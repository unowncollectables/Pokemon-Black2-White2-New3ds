/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "nitro/mi.h"
#include "stub_log.h"

#include <string.h>

void cardi_backup_assert(void) {
    PBW2_STUB_LOG("card", "cardi_backup_assert");
}

void SVCi_SHA1InitCore(void *ctx) {
    memset(ctx, 0, 32);
}

void SVCi_SHA1UpdateCore(void *ctx, const void *data, u32 size) {
    (void)ctx;
    (void)data;
    (void)size;
}

void SVCi_SHA1GetHashCore(void *ctx, void *digest) {
    memset(ctx, 0, 32);
    memset(digest, 0, 20);
}

void SVCi_CalcHMACSHA1Core(void *out, const void *data, u32 size, const void *key, u32 keyLen) {
    (void)data;
    (void)size;
    (void)key;
    (void)keyLen;
    memset(out, 0, 20);
}

u32 MATH_CountLeadingZerosFunc(u32 value) {
    u32 n = 0;
    if (value == 0) {
        return 32;
    }
    while ((value & 0x80000000u) == 0) {
        n++;
        value <<= 1;
    }
    return n;
}

void *__global_destructor_chain;