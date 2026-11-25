/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "vram_host.h"

#if defined(PBW2_LINK_GAME)

#include "types.h"
#include "nitro/gx.h"
#include <stdint.h>
#include <string.h>

#define PBW2_PLTT_BASE 0x05000000u
#define PBW2_PLTT_SIZE 0x00001000u
#define PBW2_BG_BASE 0x06000000u
/* 0x06000000–0x06800000 covers main BG, sub BG (0x06200000), main OBJ
 * (0x06400000) and sub OBJ (0x06600000). Title maps bank C as sub BG. */
#define PBW2_BG_SIZE 0x00680000u
#define PBW2_LCDC_BASE 0x06800000u
#define PBW2_LCDC_SIZE 0x000a4000u
#define PBW2_OAM_BASE 0x07000000u
#define PBW2_OAM_SIZE 0x00000800u

static u8 s_pltt[PBW2_PLTT_SIZE];
static u8 s_bg[PBW2_BG_SIZE];
static u8 s_lcdc[PBW2_LCDC_SIZE];
static u8 s_oam[PBW2_OAM_SIZE];
static u32 s_load_bytes;
static u32 s_lcdcRevision[7];
enum { REV_PAGE_BYTES = 256 };
static u32 s_pageRevision[(HW_LCDC_VRAM_H - HW_LCDC_VRAM_A) / REV_PAGE_BYTES];
static u32 s_revision;
static u32 s_bg_scr_upload_off[4];
static int s_host_ready;

void PbW2_VramHostInit(void) {
    memset(s_pltt, 0, sizeof s_pltt);
    memset(s_bg, 0, sizeof s_bg);
    memset(s_lcdc, 0, sizeof s_lcdc);
    PbW2_VramNoteLcdcWrite(HW_LCDC_VRAM_A, sizeof s_lcdc);
    memset(s_oam, 0, sizeof s_oam);
    s_load_bytes = 0;
    memset(s_bg_scr_upload_off, 0, sizeof s_bg_scr_upload_off);
    PbW2_AffineTrackInit();
    s_host_ready = 1;
}

u32 PbW2_VramLcdcBankMask(u32 address, u32 bytes) {
    static const u32 boundary[] = {
        HW_LCDC_VRAM_A, HW_LCDC_VRAM_B, HW_LCDC_VRAM_C, HW_LCDC_VRAM_D,
        HW_LCDC_VRAM_E, HW_LCDC_VRAM_F, HW_LCDC_VRAM_G, HW_LCDC_VRAM_H,
    };
    if (!bytes || address >= HW_LCDC_VRAM_H || (u64)address + bytes <= HW_LCDC_VRAM_A) return 0;
    u32 mask = 0;
    for (unsigned bank = 0; bank < 7; ++bank) {
        if (address < boundary[bank + 1] && (u64)address + bytes > boundary[bank]) mask |= 1u << bank;
    }
    return mask;
}

void PbW2_VramNoteLcdcWrite(u32 address, u32 bytes) {
    if (!bytes || address >= HW_LCDC_VRAM_H || (u64)address + bytes <= HW_LCDC_VRAM_A) return;
    u32 revision = __atomic_add_fetch(&s_revision, 1, __ATOMIC_RELAXED);
    u32 first = address < HW_LCDC_VRAM_A ? HW_LCDC_VRAM_A : address;
    u64 end = (u64)address + bytes;
    if (end > HW_LCDC_VRAM_H) end = HW_LCDC_VRAM_H;
    for (unsigned page = (first - HW_LCDC_VRAM_A) / REV_PAGE_BYTES;
         page <= (end - HW_LCDC_VRAM_A - 1) / REV_PAGE_BYTES; ++page) {
        __atomic_store_n(&s_pageRevision[page], revision, __ATOMIC_RELEASE);
    }
    u32 mask = PbW2_VramLcdcBankMask(address, bytes);
    for (unsigned bank = 0; bank < 7; ++bank) {
        if (mask & (1u << bank)) {
            __atomic_store_n(&s_lcdcRevision[bank], revision, __ATOMIC_RELEASE);
        }
    }
}

u32 PbW2_VramBankRevision(unsigned bank) {
    return __atomic_load_n(&s_lcdcRevision[bank], __ATOMIC_ACQUIRE);
}

u32 PbW2_VramRangeRevision(const void *mapped, u32 bytes) {
    uintptr_t address = (uintptr_t)mapped;
    uintptr_t start = (uintptr_t)s_lcdc;
    unsigned span = HW_LCDC_VRAM_H - HW_LCDC_VRAM_A;
    if (address < start || address - start >= span || !bytes) return 0;
    unsigned offset = address - start;
    if (bytes > span - offset) bytes = span - offset;
    u32 latest = 0;
    for (unsigned page = offset / REV_PAGE_BYTES; page <= (offset + bytes - 1) / REV_PAGE_BYTES; ++page) {
        u32 revision = __atomic_load_n(&s_pageRevision[page], __ATOMIC_ACQUIRE);
        if (revision > latest) latest = revision;
    }
    return latest;
}

void PbW2_VramClearLcdcAliases(void) {
    /* A reset through the LCDC window clears the same banks used by BG and OBJ. */
    if (reg_GX_VRAMCNT_A == GX_VRAMCNT_LCDC && reg_GX_VRAMCNT_B == GX_VRAMCNT_LCDC &&
        reg_GX_VRAMCNT_C == GX_VRAMCNT_LCDC && reg_GX_VRAMCNT_D == GX_VRAMCNT_LCDC &&
        reg_GX_VRAMCNT_E == GX_VRAMCNT_LCDC && reg_GX_VRAMCNT_F == GX_VRAMCNT_LCDC &&
        reg_GX_VRAMCNT_G == GX_VRAMCNT_LCDC && reg_GX_VRAMCNT_H == GX_VRAMCNT_LCDC &&
        reg_GX_VRAMCNT_I == GX_VRAMCNT_LCDC) {
        memset(s_bg, 0, sizeof s_bg);
    }
}

void PbW2_NoteBgScrUpload(int bg, u32 arm_dest) {
    u32 off;

    if ((unsigned)bg >= 4u || arm_dest < PBW2_BG_BASE || arm_dest >= PBW2_BG_BASE + PBW2_BG_SIZE) {
        return;
    }
    off = arm_dest - PBW2_BG_BASE;
    off &= ~0x3fffu;
    s_bg_scr_upload_off[bg] = off;
}

u32 PbW2_BgScrUploadByteOff(int bg) {
    if ((unsigned)bg >= 4u) {
        return 0;
    }
    return s_bg_scr_upload_off[bg];
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

int PbW2_VramResolveArm(const void *ptr, u32 *arm_out) {
    uintptr_t h;
    uintptr_t o;
    u32 arm_base;

    if (ptr == NULL || arm_out == NULL) {
        return 0;
    }
    h = (uintptr_t)ptr;
    if (h >= (uintptr_t)s_bg && h < (uintptr_t)s_bg + PBW2_BG_SIZE) {
        o = h - (uintptr_t)s_bg;
        arm_base = 0x05000000u;
        arm_base += 0x01000000u;
        *arm_out = arm_base + (u32)o;
        return 1;
    }
    if (h >= (uintptr_t)s_pltt && h < (uintptr_t)s_pltt + PBW2_PLTT_SIZE) {
        o = h - (uintptr_t)s_pltt;
        *arm_out = PBW2_PLTT_BASE + (u32)o;
        return 1;
    }
    if (h >= (uintptr_t)s_lcdc && h < (uintptr_t)s_lcdc + PBW2_LCDC_SIZE) {
        o = h - (uintptr_t)s_lcdc;
        *arm_out = PBW2_LCDC_BASE + (u32)o;
        return 1;
    }
    if (h >= (uintptr_t)s_oam && h < (uintptr_t)s_oam + PBW2_OAM_SIZE) {
        o = h - (uintptr_t)s_oam;
        *arm_out = PBW2_OAM_BASE + (u32)o;
        return 1;
    }
    return 0;
}

#endif
