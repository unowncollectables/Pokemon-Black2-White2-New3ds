/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "pad_host.h"

#if defined(PBW2_LINK_GAME)

#include "nitro/hw.h"
#include "nitro/os.h"

#include <string.h>

extern u8 pbw2_io[];

/* DS KEYINPUT is active-low; PAD_Read XORs with 0x2fff. */
#define PAD_KEYINPUT_IO_OFFSET 0x130u
#define PAD_MASK (0x2fffu)

vu16 PbW2_ButtonXyBuf;

void PbW2_PadHostInit(void) {
    vu16 *key = (vu16 *)(void *)(pbw2_io + PAD_KEYINPUT_IO_OFFSET);

    /* All buttons released (active-low ones). */
    *key = 0xffffu;
    /* X/Y released (bits 10–11), lid open (bit 15 clear). */
    PbW2_ButtonXyBuf = 0x0c00u;
}

void PbW2_PadHostUpdate(u32 pad_active_high) {
    u32 down = pad_active_high & PAD_MASK;
    u32 face_xy = down & (0x400u | 0x800u);
    u32 key_down = down & ~(0x400u | 0x800u);
    vu16 *key = (vu16 *)(void *)(pbw2_io + PAD_KEYINPUT_IO_OFFSET);
    u16 xy;

    *key = (u16)(~(key_down)&0xffffu);
    xy = 0x0c00u;
    if (face_xy & 0x400u) {
        xy &= (u16) ~(1u << 10);
    }
    if (face_xy & 0x800u) {
        xy &= (u16) ~(1u << 11);
    }
    PbW2_ButtonXyBuf = xy;
}

#endif