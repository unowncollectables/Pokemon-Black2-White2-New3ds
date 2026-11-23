/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "vram_dma.h"

#if defined(PBW2_LINK_GAME)

#include "vram_host.h"

#include <string.h>

static BOOL in_vram_range(uintptr_t addr) {
    return addr >= 0x05000000u && addr < 0x08000000u;
}

BOOL PbW2_TryDmaCopy32(const void *src, void *dest, u32 size) {
    uintptr_t d;

    if (!size || !dest) {
        return FALSE;
    }
    d = (uintptr_t)dest;
    if (!in_vram_range(d) && !in_vram_range((uintptr_t)src)) {
        return FALSE;
    }
    PbW2_VramNoteLoad(size);
    memcpy(PbW2_VramMap(dest), PbW2_VramMap(src), size);
    return TRUE;
}

BOOL PbW2_TryDmaCopy16(const void *src, void *dest, u32 size) {
    return PbW2_TryDmaCopy32(src, dest, size);
}

BOOL PbW2_TryDmaFill32(void *dest, u32 data, u32 size) {
    uintptr_t d;
    u32 *p;
    u32 words;
    u32 i;

    if (!size || !dest) {
        return FALSE;
    }
    d = (uintptr_t)dest;
    if (!in_vram_range(d)) {
        return FALSE;
    }
    PbW2_VramNoteLoad(size);
    p = PbW2_VramMap(dest);
    words = size / 4u;
    for (i = 0; i < words; i++) {
        p[i] = data;
    }
    return TRUE;
}

#endif