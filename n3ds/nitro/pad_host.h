/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include "types.h"

#if defined(PBW2_LINK_GAME)

extern vu16 PbW2_ButtonXyBuf;

void PbW2_PadHostInit(void);
void PbW2_PadHostUpdate(u32 pad_active_high);

#endif