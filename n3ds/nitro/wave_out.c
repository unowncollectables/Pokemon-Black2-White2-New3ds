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
#define WAVE_PCM_CAPACITY (64u * 1024u)

struct NNSSndWaveOut {
    int dsCh;
    int ndspCh;
    BOOL inUse;
    BOOL active;
    int sampleRate;
    int speed;
    int volume;
    int pan;
    u64 startTick;
    ndspWaveBuf buf;
};

static struct NNSSndWaveOut s_channels[WAVE_DS_CHANNELS];
static void *s_pcm[WAVE_DS_CHANNELS];

void WaveOut_Shutdown(void) {
    for (unsigned i = 0; i < WAVE_DS_CHANNELS; ++i) {
        if (s_pcm[i]) linearFree(s_pcm[i]);
        s_pcm[i] = NULL;
    }
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

NNSSndWaveOutHandle NNS_SndWaveOutAllocChannel(int channel) {
    struct NNSSndWaveOut *waveOut;
    PBW2_STUB_LOG("snd", "NNS_SndWaveOutAllocChannel (wave player)");
    Log_Line("AUDIO", "WaveOutAllocChannel ch=%d", channel);
    if (channel < 0 || channel >= WAVE_DS_CHANNELS) return NULL;
    waveOut = &s_channels[channel];
    if (waveOut->inUse) return NULL;
    memset(waveOut, 0, sizeof *waveOut);
    waveOut->dsCh = channel;
    waveOut->ndspCh = WAVE_NDSP_BASE + channel;
    waveOut->inUse = TRUE;
    return waveOut;
}

void NNS_SndWaveOutFreeChannel(NNSSndWaveOutHandle handle) {
    struct NNSSndWaveOut *waveOut = handle;
    if (!waveOut) return;
    Log_Line("AUDIO", "WaveOutFreeChannel ch=%d", waveOut->dsCh);
    if (waveOut->active) ndspChnWaveBufClear(waveOut->ndspCh);
    waveOut->active = FALSE;
    waveOut->inUse = FALSE;
}

BOOL NNS_SndWaveOutStart(NNSSndWaveOutHandle handle, NNSSndWaveFormat format, const void *data, BOOL loop,
                         int loopStart, int samples, int rate, int volume, int speed, int pan) {
    struct NNSSndWaveOut *waveOut = handle;
    PBW2_STUB_LOG("snd", "NNS_SndWaveOutStart (wave player)");
    Log_Line("AUDIO", "WaveOutStart fmt=%d loop=%d loopStart=%d samples=%d rate=%d vol=%d speed=%d pan=%d",
             (int)format, (int)loop, loopStart, samples, rate, volume, speed, pan);
    if (!waveOut || !waveOut->inUse || !data || samples <= 0 || rate <= 0 || speed <= 0
        || (loop && loopStart != 0)) return FALSE;
    if (format != NNS_SND_WAVE_FORMAT_PCM8 && format != NNS_SND_WAVE_FORMAT_PCM16) return FALSE;
    if (!Audio_IsNdspReady()) return FALSE;
    unsigned sampleBytes = format == NNS_SND_WAVE_FORMAT_PCM8 ? 1 : 2;
    if ((unsigned)samples > WAVE_PCM_CAPACITY / sampleBytes) return FALSE;
    void **pcm = &s_pcm[waveOut->dsCh];
    if (!*pcm) *pcm = linearAlloc(WAVE_PCM_CAPACITY);
    if (!*pcm) return FALSE;
    if (waveOut->active) ndspChnWaveBufClear(waveOut->ndspCh);
    unsigned bytes = (unsigned)samples * sampleBytes;
    memcpy(*pcm, data, bytes);
    DSP_FlushDataCache(*pcm, bytes);
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
    waveOut->buf.data_vaddr = *pcm;
    waveOut->buf.nsamples = (u32)samples;
    waveOut->buf.looping = loop ? true : false; /* NDSP loops from the buffer start; no loopStart field */
    ndspChnWaveBufAdd(waveOut->ndspCh, &waveOut->buf);
    waveOut->active = TRUE;
    waveOut->startTick = svcGetSystemTick();
    Log_Line("AUDIO", "WaveOutStart ch=%d playing at %.0f Hz, %d samples", waveOut->ndspCh,
             (double)wave_effective_rate(rate, speed), samples);
    return TRUE;
}

BOOL NNS_SndWaveOutIsPlaying(NNSSndWaveOutHandle handle) {
    struct NNSSndWaveOut *waveOut = handle;
    if (!waveOut || !waveOut->active) return FALSE;
    if (waveOut->buf.status == NDSP_WBUF_QUEUED || waveOut->buf.status == NDSP_WBUF_PLAYING) return TRUE;
    Log_Line("AUDIO", "WaveOutIsPlaying ch=%d ended status=%d samplePos=%lu after %llu ms", waveOut->dsCh,
             (int)waveOut->buf.status, (unsigned long)ndspChnGetSamplePos(waveOut->ndspCh),
             (unsigned long long)((svcGetSystemTick() - waveOut->startTick) / (SYSCLOCK_ARM11 / 1000)));
    waveOut->active = FALSE;
    return FALSE;
}

void NNS_SndWaveOutStop(NNSSndWaveOutHandle handle) {
    struct NNSSndWaveOut *waveOut = handle;
    if (!waveOut) return;
    Log_Line("AUDIO", "WaveOutStop ch=%d status=%d samplePos=%lu after %llu ms", waveOut->dsCh,
             (int)waveOut->buf.status, (unsigned long)ndspChnGetSamplePos(waveOut->ndspCh),
             (unsigned long long)((svcGetSystemTick() - waveOut->startTick) / (SYSCLOCK_ARM11 / 1000)));
    if (waveOut->active) ndspChnWaveBufClear(waveOut->ndspCh);
    waveOut->active = FALSE;
}

void NNS_SndWaveOutWaitForChannelStop(NNSSndWaveOutHandle handle) {
    struct NNSSndWaveOut *waveOut = handle;
    int i;
    if (!waveOut || !waveOut->active) return;
    ndspChnWaveBufClear(waveOut->ndspCh);
    for (i = 0; i < 200; ++i) {
        if (waveOut->buf.status == NDSP_WBUF_FREE || waveOut->buf.status == NDSP_WBUF_DONE) break;
        svcSleepThread(1000000LL);
    }
    waveOut->active = FALSE;
}

void NNS_SndWaveOutSetVolume(NNSSndWaveOutHandle handle, int volume) {
    struct NNSSndWaveOut *waveOut = handle;
    if (!waveOut || !waveOut->active) return;
    waveOut->volume = volume < 0 ? 0 : volume > 127 ? 127 : volume;
    wave_set_mix(waveOut);
}

void NNS_SndWaveOutSetSpeed(NNSSndWaveOutHandle handle, int speed) {
    struct NNSSndWaveOut *waveOut = handle;
    if (!waveOut || !waveOut->active) return;
    waveOut->speed = speed;
    ndspChnSetRate(waveOut->ndspCh, wave_effective_rate(waveOut->sampleRate, speed));
}

#endif
