/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include "types.h"

#if defined(PBW2_LINK_GAME)

void PbW2_VramHostInit(void);
void *PbW2_VramMap(const void *addr);
void PbW2_VramNoteLoad(u32 bytes);
u32 PbW2_VramLoadBytes(void);

#endif