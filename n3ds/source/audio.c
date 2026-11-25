/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "audio.h"

#include "log.h"
#include "workers.h"

#include <3ds.h>

#if defined(PBW2_LINK_GAME)

#include "strm_pcm.h"

#include <stdio.h>
#include <string.h>

enum { STREAM_CHANNEL = 0, BUFFER_COUNT = 3, BUFFER_FRAMES = 2048 };
typedef enum { STREAM_IDLE, STREAM_START, STREAM_STOP } StreamCommand;

static struct {
    LightLock lock;
    StreamCommand command;
    char archive[128];
    unsigned id, offsetMs, fadeFrames, positionMs;
    bool ready;
} s_request;

static bool s_ndsp;
static s16 *s_pcm;

bool Audio_StreamStart(const char *archive, unsigned stream, unsigned offsetMs) {
    if (!s_request.ready || !archive || strlen(archive) >= sizeof s_request.archive) return false;
    LightLock_Lock(&s_request.lock);
    strcpy(s_request.archive, archive);
    s_request.id = stream;
    s_request.offsetMs = offsetMs;
    s_request.fadeFrames = 0;
    s_request.positionMs = offsetMs ? offsetMs : 1;
    s_request.command = STREAM_START;
    LightLock_Unlock(&s_request.lock);
    return true;
}

void Audio_StreamStop(unsigned fadeFrames) {
    if (!s_request.ready) return;
    LightLock_Lock(&s_request.lock);
    s_request.fadeFrames = fadeFrames;
    if (!fadeFrames) s_request.positionMs = 0;
    /* The title can request a fade before the worker has opened the stream. */
    if (s_request.command != STREAM_START || !fadeFrames) s_request.command = STREAM_STOP;
    LightLock_Unlock(&s_request.lock);
}

unsigned Audio_StreamPosition(void) {
    if (!s_request.ready) return 0;
    LightLock_Lock(&s_request.lock);
    unsigned pos = s_request.positionMs;
    LightLock_Unlock(&s_request.lock);
    return pos;
}

static void set_mix(float volume) {
    float mix[12] = {volume, volume};
    ndspChnSetMix(STREAM_CHANNEL, mix);
}

static u64 monotonic_ms(void) {
    return svcGetSystemTick() / (SYSCLOCK_ARM11 / 1000u);
}

static void Audio_Thread(void *arg) {
    Worker *worker = arg;
    ndspWaveBuf buffers[BUFFER_COUNT] = {0};
    u64 bufferStart[BUFFER_COUNT] = {0};
    StrmPcm *stream = NULL;
    unsigned writeIndex = 0, rate = 0, baseMs = 0;
    u64 framesQueued = 0, fadeBegin = 0, fadeEnd = 0;
    float volume = 0;
    bool eof = false;
    ndspChnSetFormat(STREAM_CHANNEL, NDSP_FORMAT_STEREO_PCM16);
    ndspChnSetInterp(STREAM_CHANNEL, NDSP_INTERP_LINEAR);

    while (Worker_Run(worker)) {
        LightLock_Lock(&s_request.lock);
        StreamCommand command = s_request.command;
        unsigned fadeFrames = s_request.fadeFrames;
        unsigned id = s_request.id;
        unsigned offsetMs = s_request.offsetMs;
        char archive[sizeof s_request.archive];
        if (command == STREAM_START) memcpy(archive, s_request.archive, sizeof archive);
        s_request.command = STREAM_IDLE;
        s_request.fadeFrames = 0;
        LightLock_Unlock(&s_request.lock);

        if (command == STREAM_START || (command == STREAM_STOP && !fadeFrames)) {
            ndspChnWaveBufClear(STREAM_CHANNEL);
            StrmPcm_Close(stream);
            stream = NULL;
            memset(buffers, 0, sizeof buffers);
            writeIndex = 0;
            fadeEnd = framesQueued = 0;
            eof = false;
        }
        if (command == STREAM_START) {
            stream = StrmPcm_Open(archive, id, offsetMs);
            if (stream) {
                rate = StrmPcm_Rate(stream);
                baseMs = offsetMs;
                volume = StrmPcm_Volume(stream) / 127.0f;
                ndspChnSetRate(STREAM_CHANNEL, rate);
                set_mix(volume);
                Log_Line("AUDIO", "stream %u started at %u Hz", id, rate);
            } else {
                Log_Line("AUDIO", "stream %u unavailable", id);
            }
        }
        if (stream && fadeFrames && !fadeEnd) {
            fadeBegin = monotonic_ms();
            fadeEnd = fadeBegin + ((u64)fadeFrames * 1000 + 59) / 60;
        }
        if (stream && fadeEnd) {
            u64 now = monotonic_ms();
            if (now >= fadeEnd) {
                ndspChnWaveBufClear(STREAM_CHANNEL);
                StrmPcm_Close(stream);
                stream = NULL;
            } else {
                set_mix(volume * (float)(fadeEnd - now) / (float)(fadeEnd - fadeBegin));
            }
        }

        unsigned pos = 0;
        if (stream) {
            for (unsigned i = 0; i < BUFFER_COUNT && !eof; ++i) {
                ndspWaveBuf *buf = &buffers[writeIndex];
                if (buf->status != NDSP_WBUF_FREE && buf->status != NDSP_WBUF_DONE) break;
                s16 *samples = s_pcm + writeIndex * BUFFER_FRAMES * 2;
                unsigned count = StrmPcm_ReadStereo(stream, samples, BUFFER_FRAMES);
                eof = count < BUFFER_FRAMES;
                if (!count) break;
                buf->data_pcm16 = samples;
                buf->nsamples = count;
                bufferStart[writeIndex] = framesQueued;
                framesQueued += count;
                DSP_FlushDataCache(samples, count * 2 * sizeof *samples);
                ndspChnWaveBufAdd(STREAM_CHANNEL, buf);
                writeIndex = (writeIndex + 1) % BUFFER_COUNT;
            }
            bool pending = false;
            pos = baseMs ? baseMs : 1;
            for (unsigned i = 0; i < BUFFER_COUNT; ++i) {
                if (buffers[i].status == NDSP_WBUF_QUEUED || buffers[i].status == NDSP_WBUF_PLAYING) pending = true;
                if (buffers[i].status == NDSP_WBUF_PLAYING) {
                    pos = baseMs + (bufferStart[i] + ndspChnGetSamplePos(STREAM_CHANNEL)) * 1000 / rate;
                    if (!pos) pos = 1;
                }
            }
            if (eof && !pending) {
                StrmPcm_Close(stream);
                stream = NULL;
                pos = 0;
            }
        }
        LightLock_Lock(&s_request.lock);
        if (s_request.command == STREAM_IDLE) s_request.positionMs = pos;
        LightLock_Unlock(&s_request.lock);
        svcSleepThread(5000000LL);
    }

    ndspChnWaveBufClear(STREAM_CHANNEL);
    StrmPcm_Close(stream);
}

#endif

bool Audio_IsNdspReady(void) {
#ifdef PBW2_LINK_GAME
    return s_ndsp;
#else
    return false;
#endif
}

void Audio_Start(void) {
#ifdef PBW2_LINK_GAME
    Result result = ndspInit();
    if (R_FAILED(result)) {
        Log_Line("AUDIO", "ndspInit failed: 0x%08lx", (unsigned long)result);
        return;
    }
    s_ndsp = true;
    s_pcm = linearAlloc(BUFFER_COUNT * BUFFER_FRAMES * 2 * sizeof *s_pcm);
    if (!s_pcm) {
        Log_Line("AUDIO", "could not allocate PCM buffers");
        Audio_NdspExit();
        return;
    }
    ndspSetOutputMode(NDSP_OUTPUT_STEREO);
    LightLock_Init(&s_request.lock);
    s_request.ready = Workers_Start("audio", Audio_Thread, 0x20, 2, NULL);
#endif
}

void Audio_NdspExit(void) {
#ifdef PBW2_LINK_GAME
    if (s_ndsp) {
        ndspExit();
        if (s_pcm) linearFree(s_pcm);
        s_pcm = NULL;
        s_ndsp = false;
        s_request.ready = false;
    }
#endif
}
