/* SPDX-License-Identifier: GPL-3.0-or-later */
/* DS GXFIFO capture + Citro3D bridge scaffolding (title logo path).
 * Packet shortcut layout adapted from unowncollectables/Pokemon-HeartGold-SoulSilver-New3ds
 * (soulsilver3ds/platform/g3packet.c, GPL-3.0). Full renderer TBD. */
#include "g3_intercept.h"

#if defined(PBW2_LINK_GAME)

#include "log.h"
#include "nitro/gx.h"
#include "types.h"

#include <string.h>

#define G3_RING_WORDS 16384u

static u32 s_fifo[G3_RING_WORDS];
static u32 s_fifo_head;
static u32 s_fifo_count;
static int s_logged;

void PbW2_G3InterceptInit(void) {
    s_fifo_head = 0;
    s_fifo_count = 0;
    s_logged = 0;
    memset(s_fifo, 0, sizeof s_fifo);
}

void PbW2_G3RecordFifo(const void *src, u32 words) {
    const u32 *p = (const u32 *)src;
    u32 i;

    if (!src || words == 0) {
        return;
    }
    for (i = 0; i < words; i++) {
        s_fifo[s_fifo_head] = p[i];
        s_fifo_head = (s_fifo_head + 1u) % G3_RING_WORDS;
        if (s_fifo_count < G3_RING_WORDS) {
            s_fifo_count++;
        }
    }
}

unsigned PbW2_G3FifoWords(void) {
    return s_fifo_count;
}

void PbW2_G3Compose(int is_main, u16 *fb_rgb565) {
    u32 dc;

    (void)fb_rgb565;
    if (!is_main || fb_rgb565 == NULL) {
        return;
    }
    dc = PBW2_VU32(0x04000000);
    if ((dc & REG_GX_DISPCNT_BG02D3D_MASK) == 0) {
        return;
    }
    if (s_fifo_count == 0) {
        return;
    }
    if (!s_logged) {
        s_logged = 1;
        Log_Line("G3", "fifo words=%u (Citro3D replay next)", (unsigned)s_fifo_count);
    }
}

#endif