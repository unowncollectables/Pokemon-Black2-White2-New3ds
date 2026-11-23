/* SPDX-License-Identifier: GPL-3.0-or-later */
/* ARM9-facing VRAM map (game objects compile with -DPBW2_N3DS). */
#pragma once

#include "types.h"

#if defined(PBW2_N3DS)
void *PbW2_VramMap(const void *addr);
void PbW2_VramNoteLoad(u32 bytes);
u32 PbW2_VramLoadBytes(void);
int PbW2_VramResolveArm(const void *ptr, u32 *arm_out);
#endif