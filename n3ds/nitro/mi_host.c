/* SPDX-License-Identifier: GPL-3.0-or-later */
/* C replacements for Nitro MI helpers that are normally MWCC assembly. */
#include "nitro/mi.h"

#include <string.h>

#if defined(PBW2_LINK_GAME)
#include "g3_intercept.h"
#include "nitro/mi.h"
#include "vram_host.h"

static void *pbw2_map_ptr(const void *p) {
    void *m;

    if (p == NULL) {
        return NULL;
    }
    m = PbW2_VramMap(p);
    return m;
}

static void pbw2_note_scr_dest(const void *dest) {
    u32 arm;

    if (PbW2_VramResolveArm(dest, &arm)) {
        if (arm >= 0x06000000u && arm < 0x06020000u) {
            PbW2_NoteBgScrUpload(2, arm);
        } else if (arm >= 0x06020000u && arm < 0x06040000u) {
            PbW2_NoteBgScrUpload(3, arm);
        }
    }
}

static void pbw2_note_copy(void *dest, u32 size) {
    if ((u32)(uintptr_t)dest >= 0x05000000u && (u32)(uintptr_t)dest < 0x08000000u) {
        PbW2_VramNoteLoad(size);
        pbw2_note_scr_dest(dest);
    }
}
#endif

void MI_CpuCopy8(const void *src, void *dest, u32 size) {
    if (size && src && dest) {
#if defined(PBW2_LINK_GAME)
        void *d = pbw2_map_ptr(dest);
        const void *s = pbw2_map_ptr(src);
        pbw2_note_copy(dest, size);
        memcpy(d, s, size);
#else
        memcpy(dest, src, size);
#endif
    }
}

void MI_CpuFill8(void *dest, u8 value, u32 size) {
    if (size && dest) {
#if defined(PBW2_LINK_GAME)
        void *d = pbw2_map_ptr(dest);
        pbw2_note_copy(dest, size);
        memset(d, value, size);
#else
        memset(dest, value, size);
#endif
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
#if defined(PBW2_LINK_GAME)
    void *d = pbw2_map_ptr(dest);
    const void *s = pbw2_map_ptr(src);
    pbw2_note_copy(dest, size);
    memcpy(d, s, size);
#else
    memcpy(dest, src, size);
#endif
}

void MIi_CpuClear32(u32 value, void *dest, u32 size) {
#if defined(PBW2_LINK_GAME)
    dest = pbw2_map_ptr(dest);
#endif
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
#if defined(PBW2_LINK_GAME)
    void *d = pbw2_map_ptr(dest);
    const void *s = pbw2_map_ptr(src);
    pbw2_note_copy(dest, size);
    memcpy(d, s, size);
#else
    memcpy(dest, src, size);
#endif
}

void MIi_CpuClear16(u16 value, void *dest, u32 size) {
    u32 i;
#if defined(PBW2_LINK_GAME)
    dest = pbw2_map_ptr(dest);
#endif
    u16 *p = dest;
    u32 words = size / 2u;
    for (i = 0; i < words; i++) {
        p[i] = value;
    }
}

void MIi_CpuCopyFast(const void *src, void *dest, u32 size) {
#if defined(PBW2_LINK_GAME)
    void *d = pbw2_map_ptr(dest);
    const void *s = pbw2_map_ptr(src);
    pbw2_note_copy(dest, size);
    memcpy(d, s, size);
#else
    memcpy(dest, src, size);
#endif
}

void MIi_CpuSend32(const void *src, volatile void *dest, u32 size) {
#if defined(PBW2_LINK_GAME)
    if ((uintptr_t)dest == (uintptr_t)REG_GXFIFO_ADDR) {
        PbW2_G3RecordFifo(src, size / sizeof(u32));
        return;
    }
    {
        void *d = pbw2_map_ptr((void *)dest);
        const void *s = pbw2_map_ptr(src);
        pbw2_note_copy((void *)dest, size);
        memcpy(d, s, size);
    }
#else
    memcpy((void *)dest, src, size);
#endif
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

#if defined(PBW2_LINK_GAME)
void MI_NDmaCopy(u32 dma, const void *src, void *dest, u32 size) {
    (void)dma;
    MIi_CpuCopy32(src, dest, size);
}

void MI_NDmaFill(u32 dma, void *dest, u32 value, u32 size) {
    (void)dma;
    MIi_CpuClear32(value, dest, size);
}

void MI_NDmaCopyAsync(u32 dma, const void *src, void *dest, u32 size, void (*cb)(void *), void *arg) {
    (void)dma;
    MIi_CpuCopy32(src, dest, size);
    if (cb) {
        cb(arg);
    }
}

void MI_NDmaFillAsync(u32 dma, void *dest, u32 value, u32 size, void (*cb)(void *), void *arg) {
    (void)dma;
    MIi_CpuClear32(value, dest, size);
    if (cb) {
        cb(arg);
    }
}

#endif