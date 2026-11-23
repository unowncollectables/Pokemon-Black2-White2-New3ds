/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include "types.h"

#if defined(PBW2_LINK_GAME)

void PbW2_G3InterceptInit(void);
void PbW2_G3RecordFifo(const void *src, u32 words);
void PbW2_G3Compose(int is_main, u16 *fb_rgb565);
unsigned PbW2_G3FifoWords(void);
void PbW2_G3GeProvideDefaultBuffer(void);

#endif