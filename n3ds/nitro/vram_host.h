/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include "types.h"
#include "vram_host_api.h"

#if defined(PBW2_LINK_GAME)

void PbW2_VramHostInit(void);
void PbW2_VramClearLcdcAliases(void);
int PbW2_VramHostReady(void);
void PbW2_VramNoteLcdcWrite(u32 address, u32 bytes);
u32 PbW2_VramLcdcBankMask(u32 address, u32 bytes);
u32 PbW2_VramBankRevision(unsigned bank);
u32 PbW2_VramRangeRevision(const void *mapped, u32 bytes);

#endif
