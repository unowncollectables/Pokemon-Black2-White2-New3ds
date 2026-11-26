/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "audio.h"
#include "log.h"
#include "nnsys/snd.h"
#include "stub_log.h"
#include "wave_out.h"

#include <3ds.h>
#include <string.h>

#ifdef PBW2_LINK_GAME

/* Real NNS_SndWaveOut on NDSP. The game's model is the DS one: a wave
 * plays on a locked DS hardware channel, one buffer, and the game
 * polls IsPlaying until it ends. The host maps DS channel N to NDSP
 * channel N+1 (the stream player owns NDSP 0). NDSP reads from
 * linear memory that stays alive until the DSP is shut down.
 *
 * Playback rate: the game passes the wave's native rate plus the DS
 * timer "speed" (0x8000 = 1.0x; see the in-tree snd_waveout.c
 * CalcTimer — speed recovers the SWAV timer, and the hardware rate is
 * rate * speed / 0x8000). NDSP gets that effective rate directly.
 * Volume is 0..127, pan 0..127 with 64 at center.
 *
 * Honest failure: when NDSP never came up (HLE, or no DSP service),
 * Start returns FALSE and IsPlaying FALSE, exactly like the silent
 * backend did — the game reports the cry as failed and moves on.
 * Stop / WaitForChannelStop / FreeChannel tolerate a NULL handle and
 * a never-started one, because PokeVoice_ReleaseCore calls all three
 * unconditionally. */

#define WAVE_DS_CHANNELS 16
#define WAVE_NDSP_BASE 1 /* DS ch N -> NDSP ch N+1 */
#define WAVE_PCM_CAPACITY (512u * 1024u) /* 512 KiB: the largest SDAT wave decodes to 451 KiB */
enum { WAVE_FORMAT_IMA_ADPCM = 2, ADPCM_HEADER_SAMPLES = 8 };

struct NNSSndWaveOut {
    int dsCh;
    int ndspCh;
    BOOL inUse;
    BOOL active;
    bool sequenceNote;
    int sampleRate;
    int speed;
    int volume;
    int pan;
    u64 startTick;
    ndspWaveBuf buf;
    ndspWaveBuf loopBuf;
};

static struct NNSSndWaveOut s_channels[WAVE_DS_CHANNELS];
static void *s_pcm[WAVE_DS_CHANNELS];
/* Serializes the channel table and PCM buffers between the game thread
 * (cry/mic paths) and the audio thread (the sequence tick, which holds
 * the seq lock when it calls in here — order is seq -> wave, and no
 * wave entry point calls back into the sequence player). A
 * zero-initialized LightLock is unlocked, so this needs no init hook. */
static LightLock s_waveLock;

void WaveOut_Shutdown(void) {
    LightLock_Lock(&s_waveLock);
    for (unsigned i = 0; i < WAVE_DS_CHANNELS; ++i) {
        if (s_pcm[i]) linearFree(s_pcm[i]);
        s_pcm[i] = NULL;
    }
    LightLock_Unlock(&s_waveLock);
}

static float wave_effective_rate(int sampleRate, int speed) {
    float rate = (float)sampleRate * (float)speed / 32768.0f;
    if (rate < 4000.0f) rate = 4000.0f;
    if (rate > 48000.0f) rate = 48000.0f;
    return rate;
}

static void wave_set_mix(struct NNSSndWaveOut *waveOut) {
    float volume = (float)waveOut->volume / 127.0f;
    float left = waveOut->pan >= 64 ? (127.0f - (float)waveOut->pan) / 63.0f : 1.0f;
    float right = waveOut->pan <= 64 ? (float)waveOut->pan / 64.0f : 1.0f;
    float mix[12] = {volume * left, volume * right};
    ndspChnSetMix(waveOut->ndspCh, mix);
}

static void decode_ima(s16 *pcm, const u8 *data, unsigned samples) {
    static const u16 stepSizes[] = {
        7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31,
        34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143,
        157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544,
        598, 658, 724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552, 1707,
        1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871,
        5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
        15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767,
    };
    static const s8 indexDeltas[] = {-1, -1, -1, -1, 2, 4, 6, 8};
    int value = (s16)(data[0] | (data[1] << 8));
    int index = data[2] & 127;
    if (index > 88) index = 88;
    data += 4;
    for (unsigned i = 0; i < samples; ++i) {
        unsigned code = (data[i / 2] >> ((i & 1) * 4)) & 15;
        int step = stepSizes[index];
        int delta = step >> 3;
        if (code & 1) delta += step >> 2;
        if (code & 2) delta += step >> 1;
        if (code & 4) delta += step;
        value += (code & 8) ? -delta : delta;
        if (value > 32767) value = 32767;
        if (value < -32767) value = -32767;
        pcm[i] = value;
        index += indexDeltas[code & 7];
        if (index < 0) index = 0;
        if (index > 88) index = 88;
    }
}

static bool wave_pending(const ndspWaveBuf *buf) {
    return buf->status == NDSP_WBUF_QUEUED || buf->status == NDSP_WBUF_PLAYING;
}

static NNSSndWaveOutHandle alloc_channel(int channel, bool sequenceNote) {
    struct NNSSndWaveOut *waveOut;
    if (channel < 0 || channel >= WAVE_DS_CHANNELS) return NULL;
    waveOut = &s_channels[channel];
    if (waveOut->inUse) return NULL;
    memset(waveOut, 0, sizeof *waveOut);
    waveOut->dsCh = channel;
    waveOut->ndspCh = WAVE_NDSP_BASE + channel;
    waveOut->inUse = TRUE;
    waveOut->sequenceNote = sequenceNote;
    return waveOut;
}

NNSSndWaveOutHandle NNS_SndWaveOutAllocChannel(int channel) {
    NNSSndWaveOutHandle handle;
    PBW2_STUB_LOG("snd", "NNS_SndWaveOutAllocChannel (wave player)");
    Log_Line("AUDIO", "WaveOutAllocChannel ch=%d", channel);
    LightLock_Lock(&s_waveLock);
    handle = alloc_channel(channel, false);
    LightLock_Unlock(&s_waveLock);
    return handle;
}

NNSSndWaveOutHandle WaveOut_AllocSequenceChannel(int channel) {
    NNSSndWaveOutHandle handle;
    LightLock_Lock(&s_waveLock);
    handle = alloc_channel(channel, true);
    LightLock_Unlock(&s_waveLock);
    return handle;
}

void NNS_SndWaveOutFreeChannel(NNSSndWaveOutHandle handle) {
    struct NNSSndWaveOut *waveOut = handle;
    if (!waveOut) return;
    if (!waveOut->sequenceNote) Log_Line("AUDIO", "WaveOutFreeChannel ch=%d", waveOut->dsCh);
    LightLock_Lock(&s_waveLock);
    if (waveOut->active) ndspChnWaveBufClear(waveOut->ndspCh);
    waveOut->active = FALSE;
    waveOut->inUse = FALSE;
    LightLock_Unlock(&s_waveLock);
}

BOOL NNS_SndWaveOutStart(NNSSndWaveOutHandle handle, NNSSndWaveFormat format, const void *data, BOOL loop,
                         int loopStart, int samples, int rate, int volume, int speed, int pan) {
    struct NNSSndWaveOut *waveOut = handle;
    if (waveOut && !waveOut->sequenceNote) {
        PBW2_STUB_LOG("snd", "NNS_SndWaveOutStart (wave player)");
        Log_Line("AUDIO", "WaveOutStart fmt=%d loop=%d loopStart=%d samples=%d rate=%d vol=%d speed=%d pan=%d",
                 (int)format, (int)loop, loopStart, samples, rate, volume, speed, pan);
    }
    LightLock_Lock(&s_waveLock);
    if (!waveOut || !waveOut->inUse || !data || samples <= 0 || rate <= 0 || speed <= 0
        || (loop && (loopStart < 0 || loopStart >= samples))) {
        LightLock_Unlock(&s_waveLock);
        return FALSE;
    }
    bool adpcm = (int)format == WAVE_FORMAT_IMA_ADPCM;
    if (format != NNS_SND_WAVE_FORMAT_PCM8 && format != NNS_SND_WAVE_FORMAT_PCM16 && !adpcm) {
        LightLock_Unlock(&s_waveLock);
        return FALSE;
    }
    if (!loop) loopStart = 0;
    if (adpcm) {
        if (samples <= ADPCM_HEADER_SAMPLES || (samples & 7) || (loopStart & 7)) {
            LightLock_Unlock(&s_waveLock);
            return FALSE;
        }
        samples -= ADPCM_HEADER_SAMPLES;
        loopStart = loopStart >= ADPCM_HEADER_SAMPLES ? loopStart - ADPCM_HEADER_SAMPLES : 0;
    }
    if (!Audio_IsNdspReady()) {
        LightLock_Unlock(&s_waveLock);
        return FALSE;
    }
    unsigned sampleBytes = format == NNS_SND_WAVE_FORMAT_PCM8 ? 1 : 2;
    if ((unsigned)samples > WAVE_PCM_CAPACITY / sampleBytes) {
        LightLock_Unlock(&s_waveLock);
        return FALSE;
    }
    void **pcm = &s_pcm[waveOut->dsCh];
    if (!*pcm) *pcm = linearAlloc(WAVE_PCM_CAPACITY + 3);
    if (!*pcm) {
        LightLock_Unlock(&s_waveLock);
        return FALSE;
    }
    if (waveOut->active) ndspChnWaveBufClear(waveOut->ndspCh);
    unsigned bytes = (unsigned)samples * sampleBytes;
    if (adpcm) decode_ima(*pcm, data, samples);
    else memcpy(*pcm, data, bytes);
    unsigned introBytes = (unsigned)loopStart * sampleBytes;
    unsigned loopOffset = (introBytes + 3) & ~3u;
    if (loopOffset != introBytes) {
        memmove((u8 *)*pcm + loopOffset, (u8 *)*pcm + introBytes, bytes - introBytes);
    }
    DSP_FlushDataCache(*pcm, bytes + loopOffset - introBytes);
    waveOut->sampleRate = rate;
    waveOut->speed = speed;
    waveOut->volume = volume < 0 ? 0 : volume > 127 ? 127 : volume;
    waveOut->pan = pan < 0 ? 0 : pan > 127 ? 127 : pan;
    ndspChnSetFormat(waveOut->ndspCh,
                     format == NNS_SND_WAVE_FORMAT_PCM8 ? NDSP_FORMAT_PCM8 : NDSP_FORMAT_PCM16);
    ndspChnSetInterp(waveOut->ndspCh, NDSP_INTERP_LINEAR);
    ndspChnSetRate(waveOut->ndspCh, wave_effective_rate(rate, speed));
    wave_set_mix(waveOut);
    memset(&waveOut->buf, 0, sizeof waveOut->buf);
    memset(&waveOut->loopBuf, 0, sizeof waveOut->loopBuf);
    waveOut->buf.data_vaddr = *pcm;
    waveOut->buf.nsamples = loopStart ? (u32)loopStart : (u32)samples;
    waveOut->buf.looping = loop && !loopStart;
    ndspChnWaveBufAdd(waveOut->ndspCh, &waveOut->buf);
    if (loopStart) {
        waveOut->loopBuf.data_vaddr = (u8 *)*pcm + loopOffset;
        waveOut->loopBuf.nsamples = samples - loopStart;
        waveOut->loopBuf.looping = true;
        ndspChnWaveBufAdd(waveOut->ndspCh, &waveOut->loopBuf);
    }
    waveOut->active = TRUE;
    if (!waveOut->sequenceNote) {
        waveOut->startTick = svcGetSystemTick();
        Log_Line("AUDIO", "WaveOutStart ch=%d playing at %.0f Hz, %d samples", waveOut->ndspCh,
                 (double)wave_effective_rate(rate, speed), samples);
    }
    LightLock_Unlock(&s_waveLock);
    return TRUE;
}

BOOL NNS_SndWaveOutIsPlaying(NNSSndWaveOutHandle handle) {
    struct NNSSndWaveOut *waveOut = handle;
    bool playing;
    if (!waveOut) return FALSE;
    LightLock_Lock(&s_waveLock);
    if (!waveOut->active) {
        LightLock_Unlock(&s_waveLock);
        return FALSE;
    }
    playing = wave_pending(&waveOut->buf) || wave_pending(&waveOut->loopBuf);
    if (!playing) {
        if (!waveOut->sequenceNote) {
            Log_Line("AUDIO", "WaveOutIsPlaying ch=%d ended status=%d samplePos=%lu after %llu ms", waveOut->dsCh,
                     (int)waveOut->buf.status, (unsigned long)ndspChnGetSamplePos(waveOut->ndspCh),
                     (unsigned long long)((svcGetSystemTick() - waveOut->startTick) / (SYSCLOCK_ARM11 / 1000)));
        }
        waveOut->active = FALSE;
    }
    LightLock_Unlock(&s_waveLock);
    return playing ? TRUE : FALSE;
}

void NNS_SndWaveOutStop(NNSSndWaveOutHandle handle) {
    struct NNSSndWaveOut *waveOut = handle;
    if (!waveOut) return;
    if (!waveOut->sequenceNote) {
        Log_Line("AUDIO", "WaveOutStop ch=%d status=%d samplePos=%lu after %llu ms", waveOut->dsCh,
                 (int)waveOut->buf.status, (unsigned long)ndspChnGetSamplePos(waveOut->ndspCh),
                 (unsigned long long)((svcGetSystemTick() - waveOut->startTick) / (SYSCLOCK_ARM11 / 1000)));
    }
    LightLock_Lock(&s_waveLock);
    if (waveOut->active) ndspChnWaveBufClear(waveOut->ndspCh);
    waveOut->active = FALSE;
    LightLock_Unlock(&s_waveLock);
}

void NNS_SndWaveOutWaitForChannelStop(NNSSndWaveOutHandle handle) {
    struct NNSSndWaveOut *waveOut = handle;
    int i;
    if (!waveOut) return;
    /* Do not hold the lock across the sleep loop: the audio thread's
     * tick takes this lock for every note, and blocking it for up to
     * 200 ms would stall the music — the very thing this change fixes.
     * The buf status is driven by NDSP, so polling it unlocked is
     * safe; only the active flag is set under the lock. */
    LightLock_Lock(&s_waveLock);
    if (!waveOut->active) {
        LightLock_Unlock(&s_waveLock);
        return;
    }
    ndspChnWaveBufClear(waveOut->ndspCh);
    LightLock_Unlock(&s_waveLock);
    for (i = 0; i < 200; ++i) {
        if (!wave_pending(&waveOut->buf) && !wave_pending(&waveOut->loopBuf)) break;
        svcSleepThread(1000000LL);
    }
    LightLock_Lock(&s_waveLock);
    waveOut->active = FALSE;
    LightLock_Unlock(&s_waveLock);
}

void NNS_SndWaveOutSetVolume(NNSSndWaveOutHandle handle, int volume) {
    struct NNSSndWaveOut *waveOut = handle;
    if (!waveOut) return;
    LightLock_Lock(&s_waveLock);
    if (!waveOut->active) {
        LightLock_Unlock(&s_waveLock);
        return;
    }
    waveOut->volume = volume < 0 ? 0 : volume > 127 ? 127 : volume;
    wave_set_mix(waveOut);
    LightLock_Unlock(&s_waveLock);
}

void NNS_SndWaveOutSetSpeed(NNSSndWaveOutHandle handle, int speed) {
    struct NNSSndWaveOut *waveOut = handle;
    if (!waveOut) return;
    LightLock_Lock(&s_waveLock);
    if (!waveOut->active) {
        LightLock_Unlock(&s_waveLock);
        return;
    }
    waveOut->speed = speed;
    ndspChnSetRate(waveOut->ndspCh, wave_effective_rate(waveOut->sampleRate, speed));
    LightLock_Unlock(&s_waveLock);
}

#endif
