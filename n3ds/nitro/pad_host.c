/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "pad_host.h"

#if defined(PBW2_LINK_GAME)

#include "nitro/hw.h"

/* DS KEYINPUT is active-low; PAD_Read XORs with 0x2fff. */
#define PAD_KEYINPUT_MASK 0x03ffu
#define PAD_XY_MASK 0x0c00u
#define PAD_DEBUG_RELEASED 0x2000u

vu16 PbW2_ButtonXyBuf;
static u32 s_touch;

void PbW2_PadHostInit(void) {
    PbW2_PadHostUpdate(0);
    PbW2_TouchHostUpdate(0, 0, FALSE);
}

void PbW2_TouchHostUpdate(u16 x, u16 y, BOOL pressed) {
    /* The host calibration uses 16 raw units per game pixel. Publish the whole sample at once. */
    u32 sample = pressed ? ((u32)x << 4) | ((u32)y << 16) | (1u << 24) : 0u;
    __atomic_store_n(&s_touch, sample, __ATOMIC_RELAXED);
}

void PbW2_TouchHostRead(TPData *point) {
    u32 sample = __atomic_load_n(&s_touch, __ATOMIC_RELAXED);
    point->x = (u16)(sample & 0xfffu);
    point->y = (u16)((sample >> 12) & 0xfffu);
    point->touch = (u16)(sample >> 24);
    point->validity = point->touch ? TP_VALIDITY_VALID : TP_VALIDITY_INVALID_XY;
}

void PbW2_PadHostUpdate(u32 pad_active_high) {
    /* PAD_Read ORs the two registers, so KEYINPUT leaves X/Y bits clear. */
    reg_PAD_KEYINPUT = (u16)(~pad_active_high & PAD_KEYINPUT_MASK);
    PbW2_ButtonXyBuf = (u16)((~pad_active_high & PAD_XY_MASK) | PAD_DEBUG_RELEASED);
}

#endif
