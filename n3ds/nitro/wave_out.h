/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef PBW2_WAVE_OUT_H
#define PBW2_WAVE_OUT_H

/* Real NNS_SndWaveOut on the 3DS NDSP path (see wave_out.c): one NDSP
 * channel per DS hardware channel (DS ch N -> NDSP ch N+1; the stream
 * player owns NDSP ch 0). The host plays PCM8, PCM16 and DS IMA-ADPCM
 * waves and queues a separate sustaining buffer for a nonzero loop
 * point. The NNS_SndWaveOut* entry points are declared in nnsys/snd.h. */

/* Release the linear PCM buffers after ndspExit. */
void WaveOut_Shutdown(void);

#ifdef PBW2_LINK_GAME
struct NNSSndWaveOut;
struct NNSSndWaveOut *WaveOut_AllocSequenceChannel(int channel);
#endif

#endif
