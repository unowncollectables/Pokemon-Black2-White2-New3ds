/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef PBW2_WAVE_OUT_H
#define PBW2_WAVE_OUT_H

/* Real NNS_SndWaveOut on the 3DS NDSP path (see wave_out.c): one NDSP
 * channel per DS hardware channel (DS ch N -> NDSP ch N+1; the stream
 * player owns NDSP ch 0). One-shot PCM8/PCM16 waves — cries and the
 * mic — with honest failure when NDSP never came up: Start returns
 * FALSE and IsPlaying FALSE, so the game moves on exactly as it does
 * against the silent backend instead of hanging on a sound that will
 * never play. The NNS_SndWaveOut* entry points themselves are declared
 * in nnsys/snd.h; this header only marks the module. */

#endif
