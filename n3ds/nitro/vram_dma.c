/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "vram_dma.h"

#if defined(PBW2_LINK_GAME)

#include "log.h"
#include "g3_raster.h"
#include "vram_host.h"

#include <string.h>

static BOOL in_vram_range(uintptr_t addr) {
    return addr >= 0x05000000u && addr < 0x08000000u;
}

static u32 normalize_dest(const void *dest) {
    uintptr_t d = (uintptr_t)dest;
    u32 arm;

    if (in_vram_range(d)) {
        return (u32)d;
    }
    if (PbW2_VramResolveArm(dest, &arm)) {
        return arm;
    }
    return (u32)d;
}

static void log_bg_upload_once(u32 dest, u32 size) {
    static u8 s_bg2;
    static u8 s_bg3;

    if (dest >= 0x06000000u && dest < 0x06020000u && !s_bg2) {
        s_bg2 = 1;
        Log_Line("GX", "vram upload BG2 scr dest=0x%08x size=%u", (unsigned)dest, (unsigned)size);
    } else if (dest >= 0x06020000u && dest < 0x06040000u && !s_bg3) {
        s_bg3 = 1;
        Log_Line("GX", "vram upload BG3 scr dest=0x%08x size=%u", (unsigned)dest, (unsigned)size);
    }
}

BOOL PbW2_TryDmaCopy32(const void *src, void *dest, u32 size) {
    u32 darm;
    const void *arm_src;

    if (!size || !dest) {
        return FALSE;
    }
    darm = normalize_dest(dest);
    if (!in_vram_range((uintptr_t)darm) && !in_vram_range((uintptr_t)src)) {
        return FALSE;
    }
    log_bg_upload_once(darm, size);
    if (darm >= 0x06000000u && darm < 0x06020000u) {
        PbW2_NoteBgScrUpload(2, darm);
    } else if (darm >= 0x06020000u && darm < 0x06040000u) {
        PbW2_NoteBgScrUpload(3, darm);
    }
    PbW2_VramNoteLoad(size);
    arm_src = in_vram_range((uintptr_t)src) ? PbW2_VramMap(src) : src;
    PbW2_G3RasterBeforeLcdcWrite(darm, size);
    memcpy(PbW2_VramMap((void *)(uintptr_t)darm), arm_src, size);
    PbW2_VramNoteLcdcWrite(darm, size);
    return TRUE;
}

BOOL PbW2_TryDmaCopy16(const void *src, void *dest, u32 size) {
    return PbW2_TryDmaCopy32(src, dest, size);
}

BOOL PbW2_TryDmaFill32(void *dest, u32 data, u32 size) {
    u32 darm;
    u32 *p;
    u32 words;
    u32 i;

    if (!size || !dest) {
        return FALSE;
    }
    darm = normalize_dest(dest);
    if (!in_vram_range((uintptr_t)darm)) {
        return FALSE;
    }
    PbW2_VramNoteLoad(size);
    p = PbW2_VramMap((void *)(uintptr_t)darm);
    words = size / 4u;
    PbW2_G3RasterBeforeLcdcWrite(darm, size);
    for (i = 0; i < words; i++) {
        p[i] = data;
    }
    PbW2_VramNoteLcdcWrite(darm, size);
    return TRUE;
}

#endif
