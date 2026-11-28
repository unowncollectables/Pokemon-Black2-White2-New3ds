/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include "types.h"

#include <stddef.h>

void PbW2_G3FifoWord(u32 word);
void PbW2_G3Command(u32 opcode, const u32 *params);

#if defined(PBW2_LINK_GAME)

void PbW2_G3InterceptInit(void);
void PbW2_G3RecordFifo(const void *src, u32 words);
void PbW2_G3Compose(int is_main, u16 *fb_rgb565);

/* GX record/replay support (g3_replay.c): opaque save/restore of the
 * intercept's full matrix state, and the replay mode in which test
 * commands are suppressed and the clip matrix carries the right
 * eye's projection shift. */
size_t PbW2_G3InterceptStateSize(void);
void PbW2_G3InterceptStateSave(void *dst);
void PbW2_G3InterceptStateRestore(const void *src);
void PbW2_G3InterceptReplayBegin(float ndcShiftX);
void PbW2_G3InterceptReplayEnd(void);

#endif
