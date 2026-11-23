/* SPDX-License-Identifier: GPL-3.0-or-later */
/* ARM9-facing VRAM map (game objects compile with -DPBW2_N3DS). */
#pragma once

#include "types.h"

#if defined(PBW2_N3DS)
void *PbW2_VramMap(const void *addr);
void PbW2_VramNoteLoad(u32 bytes);
u32 PbW2_VramLoadBytes(void);
int PbW2_VramResolveArm(const void *ptr, u32 *arm_out);
void PbW2_NoteBgScrUpload(int bg, u32 arm_dest);
u32 PbW2_BgScrUploadByteOff(int bg);
void PbW2_AffineTrackInit(void);
void PbW2_NoteBgAffineWrite(u32 addr);
int PbW2_BgAffineWritten(int is_sub, int bg);
#endif