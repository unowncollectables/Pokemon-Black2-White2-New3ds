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

void MI_CpuCopy(const void *src, void *dest, u32 size) {
    MI_CpuCopy8(src, dest, size);
}

void MI_CpuMove(const void *src, void *dest, u32 size) {
    MI_CpuCopy8(src, dest, size);
}

void MIi_CpuCopy16(const void *src, void *dest, u32 size) {
    memcpy(dest, src, size);
}

void MIi_CpuClear16(u16 value, void *dest, u32 size) {
    u32 i;
    u16 *p = dest;
    u32 words = size / 2u;
    for (i = 0; i < words; i++) {
        p[i] = value;
    }
}

void MIi_CpuCopyFast(const void *src, void *dest, u32 size) {
    memcpy(dest, src, size);
}

void MIi_CpuSend32(const void *src, volatile void *dest, u32 size) {
  memcpy((void *)dest, src, size);
}

int MI_CpuComp8(const void *a, const void *b, u32 size) {
    return memcmp(a, b, size);
}

void MI_UncompressLZ8(const void *src, void *dest) {
    (void)src;
    (void)dest;
}

void MI_Copy36B(const void *src, void *dest) {
    MI_CpuCopy8(src, dest, 36);
}

void MI_Copy64B(const void *src, void *dest) {
    MI_CpuCopy8(src, dest, 64);
}

void MI_Zero36B(void *dest) {
    MI_CpuFill8(dest, 0, 36);
}