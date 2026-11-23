/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "cp_div_host.h"
#include "nitro/cp.h"
#include "stub_log.h"

#include <string.h>

static u16 s_div_cnt;
static s32 s_numer_l;
static s32 s_denom;
static s32 s_result_l;
static s32 s_rem_l;
static int s_logged;

void PbW2_CpDivHostInit(void) {
    memset(&s_div_cnt, 0, sizeof s_div_cnt);
    s_numer_l = 0;
    s_denom = 0;
    s_result_l = 0;
    s_rem_l = 0;
    *(vu16 *)(void *)PBW2_IO(0x04000280) = 0;
}

void PbW2_CpSetDivCnt(u16 mode) {
    s_div_cnt = mode;
    *(vu16 *)(void *)PBW2_IO(0x04000280) = (u16)(mode | REG_CP_BUSY_MASK);
}

void PbW2_CpSetNumerL(s32 numer_l) {
    s_numer_l = numer_l;
    *(vu32 *)(void *)PBW2_IO(0x04000290) = (u32)numer_l;
}

void PbW2_CpSetDenom32(u32 denom) {
    s_denom = (s32)denom;
    if (denom == 0) {
        s_result_l = 0;
        s_rem_l = 0;
    } else {
        s_result_l = s_numer_l / (s32)denom;
        s_rem_l = s_numer_l % (s32)denom;
    }
    *(vu32 *)(void *)PBW2_IO(0x04000298) = denom;
    *(vu32 *)(void *)PBW2_IO(0x040002a0) = (u32)s_result_l;
    *(vu32 *)(void *)PBW2_IO(0x040002a8) = (u32)s_rem_l;
    s_div_cnt &= ~REG_CP_BUSY_MASK;
    *(vu16 *)(void *)PBW2_IO(0x04000280) = s_div_cnt;
    if (!s_logged) {
        s_logged = 1;
        PBW2_STUB_LOG("CP", "software div host");
    }
}

u32 PbW2_CpGetResultL(void) {
    return (u32)s_result_l;
}

u32 PbW2_CpGetDivCnt(void) {
    return s_div_cnt;
}