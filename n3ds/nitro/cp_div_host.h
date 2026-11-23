/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include "types.h"

void PbW2_CpDivHostInit(void);
void PbW2_CpSetDivCnt(u16 mode);
void PbW2_CpSetNumerL(s32 numer_l);
void PbW2_CpSetDenom32(u32 denom);
u32 PbW2_CpGetResultL(void);
u32 PbW2_CpGetDivCnt(void);
