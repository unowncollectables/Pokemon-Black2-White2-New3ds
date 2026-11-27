/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "nnsys/snd.h"
#include "wave_out.h"

#include <3ds.h>
#include <stdio.h>
#include <string.h>

static ndspWaveBuf *s_queue[2];
static unsigned s_count;
static u16 s_format;
static float s_rate, s_left, s_right;
static unsigned s_logLines;

bool Audio_IsNdspReady(void) { return true; }
void Log_Line(const char *tag, const char *fmt, ...) { (void)tag; (void)fmt; ++s_logLines; }
void __wrap_ndspChnSetFormat(int id, u16 format) { (void)id; s_format = format; }
void __wrap_ndspChnSetInterp(int id, ndspInterpType type) { (void)id; (void)type; }
void __wrap_ndspChnSetRate(int id, float rate) { (void)id; s_rate = rate; }
void __wrap_ndspChnSetMix(int id, float mix[12]) { (void)id; s_left = mix[0]; s_right = mix[1]; }
u32 __wrap_ndspChnGetSamplePos(int id) { (void)id; return 0; }
Result __wrap_DSP_FlushDataCache(const void *data, u32 size) { (void)data; (void)size; return 0; }

void __wrap_ndspChnWaveBufAdd(int id, ndspWaveBuf *buf) {
    (void)id;
    if (s_count < 2) s_queue[s_count++] = buf;
    buf->status = NDSP_WBUF_QUEUED;
}

void __wrap_ndspChnWaveBufClear(int id) {
    (void)id;
    for (unsigned i = 0; i < s_count; ++i) s_queue[i]->status = NDSP_WBUF_DONE;
    s_count = 0;
}

static const char *check_waves(void) {
    const s8 pcm[] = {-90, -20, 30, 80, 100};
    NNSSndWaveOutHandle wave = NNS_SndWaveOutAllocChannel(15);
    if (!NNS_SndWaveOutStart(wave, NNS_SND_WAVE_FORMAT_PCM8, pcm, TRUE, 3, 5, 16000, 127, 32768, 64) ||
        s_format != NDSP_FORMAT_PCM8 || s_count != 2 || s_queue[0]->looping || !s_queue[1]->looping ||
        s_queue[0]->nsamples != 3 || s_queue[1]->nsamples != 2 ||
        memcmp(s_queue[0]->data_pcm8, pcm, 3) || memcmp(s_queue[1]->data_pcm8, pcm + 3, 2) ||
        ((uintptr_t)s_queue[1]->data_vaddr & 3)) return "PCM8 intro and aligned sustain";

    if (s_left != 0.5f || s_right != 0.5f) return "centered DS pan splits gain evenly";
    WaveOut_SetPan(wave, 0);
    if (s_left != 1.0f || s_right != 0.0f) return "live hard-left pan";
    WaveOut_SetPan(wave, 127);
    if (s_left != 0.0f || s_right != 1.0f) return "live hard-right pan";

    s_queue[0]->status = NDSP_WBUF_DONE;
    if (!NNS_SndWaveOutIsPlaying(wave)) return "sustain plays after intro";
    if (NNS_SndWaveOutStart(wave, NNS_SND_WAVE_FORMAT_PCM8, pcm, TRUE, 5, 5, 16000, 127, 32768, 64) ||
        !NNS_SndWaveOutIsPlaying(wave)) return "invalid restart preserves playing wave";

    const s8 fast[] = {100, -100, 30, 70, -70, -30, 80};
    const s8 expectedIntro[] = {0, 30};
    const s8 expectedLoop[] = {0, 25};
    if (!NNS_SndWaveOutStart(wave, NNS_SND_WAVE_FORMAT_PCM8, fast, TRUE, 3, 7, 32000, 127, 65536, 64) ||
        s_count != 2 || s_rate != 32000 || s_queue[0]->nsamples != 2 || s_queue[1]->nsamples != 2 ||
        memcmp(s_queue[0]->data_pcm8, expectedIntro, 2) || memcmp(s_queue[1]->data_pcm8, expectedLoop, 2))
        return "high-rate filtering preserves the original loop start";
    if (!NNS_SndWaveOutStart(wave, NNS_SND_WAVE_FORMAT_PCM8, fast, FALSE, 0, 1, 32000, 127, 131072, 64) ||
        s_count != 1 || s_queue[0]->nsamples != 1 || s_queue[0]->data_pcm8[0] != 100)
        return "one-sample high-rate voices stay audible";

    const u8 adpcm[] = {100, 0, 0, 0, 0x71, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    const s16 lead[] = {101, 112, 114, 113, 114, 115, 116, 117};
    const s16 sustain[] = {118, 119, 119, 119, 119, 119, 119, 119};
    if (!NNS_SndWaveOutStart(wave, (NNSSndWaveFormat)2, adpcm, TRUE, 16, 24, 16000, 127, 32768, 64) ||
        s_format != NDSP_FORMAT_PCM16 || s_count != 2 || !s_queue[1]->looping ||
        s_queue[0]->nsamples != 8 || s_queue[1]->nsamples != 8 ||
        memcmp(s_queue[0]->data_pcm16, lead, sizeof lead) ||
        memcmp(s_queue[1]->data_pcm16, sustain, sizeof sustain)) return "ADPCM predictor and loop offset";

    NNS_SndWaveOutStop(wave);
    if (s_count || NNS_SndWaveOutIsPlaying(wave)) return "stopped wave";
    NNS_SndWaveOutFreeChannel(wave);

    unsigned before = s_logLines;
    wave = WaveOut_AllocSequenceChannel(15);
    if (!NNS_SndWaveOutStart(wave, NNS_SND_WAVE_FORMAT_PCM8, pcm, FALSE, 0, 5, 16000, 127, 32768, 64) ||
        s_count != 1 || !NNS_SndWaveOutIsPlaying(wave)) return "sequence channel plays";
    NNS_SndWaveOutSetVolume(wave, (3 << 8) | 64);
    if (s_left != 0.015625f || s_right != 0.015625f) return "quiet sequence voice uses its DS divider";
    NNS_SndWaveOutStop(wave);
    NNS_SndWaveOutFreeChannel(wave);
    if (s_count || s_logLines != before) return "sequence channel avoids note logs";
    WaveOut_Shutdown();
    return NULL;
}

int main(void) {
    archiveMountSdmc();
    const char *failure = check_waves();
    FILE *out = fopen("sdmc:/wave-out-test.txt", "w");
    if (out) {
        fprintf(out, "%s\n", failure ? failure : "PASS");
        fclose(out);
    }
    for (;;) svcSleepThread(1000000000ll);
}
