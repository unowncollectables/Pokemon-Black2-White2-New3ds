/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include "types.h"

void PbW2_G3FifoWord(u32 word);
void PbW2_G3Command(u32 opcode, const u32 *params);

#if defined(PBW2_LINK_GAME)

void PbW2_G3InterceptInit(void);
void PbW2_G3RecordFifo(const void *src, u32 words);
void PbW2_G3Compose(int is_main, u16 *fb_rgb565);

#endif
