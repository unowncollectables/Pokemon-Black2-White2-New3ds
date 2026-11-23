/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "vram_host.h"

#if defined(PBW2_LINK_GAME)

#include <stdint.h>
#include <string.h>

#define PBW2_PLTT_BASE 0x05000000u
#define PBW2_PLTT_SIZE 0x00001000u
#define PBW2_BG_BASE 0x06000000u
#define PBW2_BG_SIZE 0x00100000u
#define PBW2_LCDC_BASE 0x06800000u
#define PBW2_LCDC_SIZE 0x000a4000u
#define PBW2_OAM_BASE 0x07000000u
#define PBW2_OAM_SIZE 0x00000400u

static u8 s_pltt[PBW2_PLTT_SIZE];
static u8 s_bg[PBW2_BG_SIZE];
static u8 s_lcdc[PBW2_LCDC_SIZE];
static u8 s_oam[PBW2_OAM_SIZE];
static u32 s_load_bytes;
static int s_host_ready;

void PbW2_VramHostInit(void) {
    memset(s_pltt, 0, sizeof s_pltt);
    memset(s_bg, 0, sizeof s_bg);
    memset(s_lcdc, 0, sizeof s_lcdc);
    memset(s_oam, 0, sizeof s_oam);
    s_load_bytes = 0;
    s_host_ready = 1;
}

int PbW2_VramHostReady(void) {
    return s_host_ready != 0;
}

void *PbW2_VramMap(const void *addr) {
    uintptr_t a;

    if (addr == NULL) {
        return NULL;
    }
    a = (uintptr_t)addr;
    if (a >= PBW2_PLTT_BASE && a < PBW2_PLTT_BASE + PBW2_PLTT_SIZE) {
        return s_pltt + (a - PBW2_PLTT_BASE);
    }
    if (a >= PBW2_BG_BASE && a < PBW2_BG_BASE + PBW2_BG_SIZE) {
        return s_bg + (a - PBW2_BG_BASE);
    }
    if (a >= PBW2_LCDC_BASE && a < PBW2_LCDC_BASE + PBW2_LCDC_SIZE) {
        return s_lcdc + (a - PBW2_LCDC_BASE);
    }
    if (a >= PBW2_OAM_BASE && a < PBW2_OAM_BASE + PBW2_OAM_SIZE) {
        return s_oam + (a - PBW2_OAM_BASE);
    }
    return (void *)addr;
}

void PbW2_VramNoteLoad(u32 bytes) {
    s_load_bytes += bytes;
}

u32 PbW2_VramLoadBytes(void) {
    return s_load_bytes;
}

#endif