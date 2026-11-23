/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "vram_host.h"

#if defined(PBW2_LINK_GAME)

#include "types.h"

#include <stdint.h>

extern u8 pbw2_io[];

/* engine 0=main, 1=sub; index 0=BG2, 1=BG3 */
static u8 s_affine_written[2][2];

void PbW2_AffineTrackInit(void) {
    memset(s_affine_written, 0, sizeof s_affine_written);
}

void PbW2_NoteBgAffineWrite(u32 addr) {
    uintptr_t base = (uintptr_t)pbw2_io;
    uintptr_t p = (uintptr_t)addr;
    u32 off;
    int engine;
    u32 eoff;

    if (p < base || p >= base + 0x110000u) {
        return;
    }
    off = (u32)(p - base);
    engine = (off >= 0x1000u) ? 1 : 0;
    eoff = off & 0xfffu;
    if (eoff == 0x20u) {
        s_affine_written[engine][0] = 1;
    } else if (eoff == 0x30u) {
        s_affine_written[engine][1] = 1;
    }
}

int PbW2_BgAffineWritten(int is_sub, int bg) {
    int engine;
    int idx;

    if (bg != 2 && bg != 3) {
        return 0;
    }
    engine = is_sub ? 1 : 0;
    idx = bg - 2;
    return s_affine_written[engine][idx] != 0;
}

#endif