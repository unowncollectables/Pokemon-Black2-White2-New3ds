/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include "types.h"

#if defined(PBW2_N3DS)

#include "nitro/tp.h"

extern vu16 PbW2_ButtonXyBuf;

void PbW2_PadHostInit(void);
void PbW2_PadHostUpdate(u32 pad_active_high);
void PbW2_TouchHostUpdate(u16 x, u16 y, BOOL pressed);
void PbW2_TouchHostRead(TPData *point);

#endif
