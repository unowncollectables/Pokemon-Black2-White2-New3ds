/* SPDX-License-Identifier: GPL-3.0-or-later */
/* C replacements for Nitro MI helpers that are normally MWCC assembly. */
#include "nitro/mi.h"

#include <string.h>

void MI_CpuCopy8(const void *src, void *dest, u32 size) {
    if (size && src && dest) {
        memcpy(dest, src, size);
    }
}

void MI_CpuFill8(void *dest, u8 value, u32 size) {
    if (size && dest) {
        memset(dest, value, size);
    }
}

void MI_CpuFill(void *dest, u32 value, u32 size) {
    u32 i;
    u8 *bytes;
    if (!dest || !size) {
        return;
    }
    bytes = dest;
    for (i = 0; i < size; i += 4u) {
        u32 left = size - i;
        if (left >= 4u) {
            *(u32 *)(bytes + i) = value;
        } else {
            u8 pat[4] = {(u8)value, (u8)(value >> 8), (u8)(value >> 16), (u8)(value >> 24)};
            memcpy(bytes + i, pat, left);
            break;
        }
    }
}

void MIi_CpuCopy32(const void *src, void *dest, u32 size) {
    memcpy(dest, src, size);
}

void MIi_CpuClear32(u32 value, void *dest, u32 size) {
    u32 *p = dest;
    u32 words = size / 4u;
    u32 i;
    for (i = 0; i < words; i++) {
        p[i] = value;
    }
}

void MIi_CpuClearFast(u32 value, void *dest, u32 size) {
    MIi_CpuClear32(value, dest, size);
}

void MIi_UncompressBackward(void *end) {
    (void)end;
}