/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "nitro/fx.h"
#include "nitro/cp.h"

#include <stdint.h>

/* The software divider completes before returning to the game thread. */
void FX_DivAsync(fx32 numer, fx32 denom) {
    CP_SetDiv64_32((u64)(u32)numer << 32, (u32)denom);
}

void FX_InvAsync(fx32 denom) {
    FX_DivAsync(FX32_ONE, denom);
}

fx64c FX_GetDivResultFx64c(void) {
    return (fx64c)reg_CP_DIV_RESULT;
}

fx32 FX_GetDivResult(void) {
    return (fx32)((FX_GetDivResultFx64c() + 0x80000) >> 20);
}

fx64c FX_InvFx64c(fx32 denom) {
    FX_InvAsync(denom);
    return FX_GetDivResultFx64c();
}

fx32 FX_Div(fx32 numer, fx32 denom) {
    if (denom == 0 || (numer == INT32_MIN && denom == -1)) {
        return 0;
    }
    /* Nitro rounds the divider's Q32 quotient when converting it back to Q12. */
    s64 quotient = ((s64)numer * 0x100000000LL) / denom;
    return (fx32)((quotient + 0x80000) >> 20);
}

fx32 FX_Inv(fx32 denom) {
    return FX_Div(FX32_ONE, denom);
}

fx32 FX_Sqrt(fx32 x) {
    if (x <= 0) {
        return 0;
    }
    return (fx32)((PbW2_CpSqrt64((u64)(u32)x << 32) + 0x200u) >> 10);
}

/* FX_ModS32: integer remainder, on the field path (field_g3d_mapper.c map
 * chunking). fx_cp.c is excluded from the link because it also defines
 * FX_Div/FX_Sqrt/FX_Inv, which the host already provides. */
s32 FX_ModS32(s32 numer, s32 denom) {
    return denom ? (numer % denom) : 0;
}

fx32 FX_InvSqrt(fx32 x) {
    if (x <= 0) {
        return 0;
    }
    u64 inverse = ((u64)FX32_ONE << 32) / (u32)x;
    u64 root = PbW2_CpSqrt64((u64)(u32)x << 32);
    return (fx32)((inverse * root + 0x20000000000ULL) >> 42);
}
