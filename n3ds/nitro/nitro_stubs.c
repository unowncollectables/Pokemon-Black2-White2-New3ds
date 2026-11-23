/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "types.h"
#include "stub_log.h"
#include <string.h>

void *__memcpy(void *dst, const void *src, size_t n) {
    return memcpy(dst, src, n);
}

u32 _u32_div_f(u32 a, u32 b) {
    return b ? a / b : 0;
}

void SVC_GetCRC16(u16 *crc, const void *data, u32 size) {
    if (crc) {
        *crc = 0;
    }
    (void)data;
    (void)size;
}

typedef s32 (*MATHCompareFunc)(void *a, void *b);

void MATH_QSort(void *head, u32 num, u32 width, MATHCompareFunc comp, void *stackBuffer) {
    (void)head;
    (void)num;
    (void)width;
    (void)comp;
    (void)stackBuffer;
    PBW2_STUB_LOG("MATH", "QSort");
}

u32 MATH_CountPopulation(u32 mask) {
    u32 count = 0;
    while (mask) {
        count += mask & 1u;
        mask >>= 1;
    }
    return count;
}
