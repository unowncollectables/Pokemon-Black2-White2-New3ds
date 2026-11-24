/* SPDX-License-Identifier: GPL-3.0-or-later */
/* C replacements for Nitro MI helpers that are normally MWCC assembly. */
#include "nitro/mi.h"

#include <string.h>

#if defined(PBW2_LINK_GAME)
#include "g3_intercept.h"
#include "nitro/gx.h"
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
#if defined(PBW2_LINK_GAME)
    if (value == 0 && (uintptr_t)dest == HW_LCDC_VRAM && size == HW_LCDC_VRAM_SIZE) {
        PbW2_VramClearLcdcAliases();
    }
#endif
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

/* MI_Copy32B: the 32-byte copy (fog-table upload, G3X_SetFogTable). The rest
 * of mi_memory.c stays MWCC assembly and is not compiled here; only this
 * one is needed, so it lives with the other host MI replacements. */
void MI_Copy32B(const void *src, void *dest) {
    if (src && dest) {
#if defined(PBW2_LINK_GAME)
        void *d = pbw2_map_ptr(dest);
        const void *s = pbw2_map_ptr(src);
        pbw2_note_copy(dest, 32);
        memcpy(d, s, 32);
#else
        memcpy(dest, src, 32);
#endif
    }
}

/* MI_IsNDmaBusy: no NDMA engine on this platform, so no channel is ever
 * busy. (The camera NDMA path that calls it has no camera hardware.) */
BOOL MI_IsNDmaBusy(u32 ndmaNo) {
    (void)ndmaNo;
    return FALSE;
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
    if ((uintptr_t)dest == (uintptr_t)REG_GXFIFO_ADDR || dest == &reg_G3X_GXFIFO) {
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
#if defined(PBW2_LINK_GAME)
    /* Host C translation of lib/nitro/src/mi/mi_uncompress.c (NitroSDK LZ10/LZ11).
       The SDK source is MWCC asm with SWPB for DS VRAM; byte stores are fine here. */
    const u8 *s;
    u8 *d;
    u32 header;
    u32 left;
    int extended;
    u32 flags;
    int bits;
    u32 base;
    u32 length;
    u32 disp;
    u8 a;
    u8 b;

    if (src == NULL || dest == NULL) {
        return;
    }
    s = pbw2_map_ptr(src);
    d = pbw2_map_ptr(dest);
    header = (u32)s[0] | ((u32)s[1] << 8) | ((u32)s[2] << 16) | ((u32)s[3] << 24);
    s += 4;
    left = header >> 8;
    extended = (header & 0xfu) != 0;
    while (left > 0) {
        flags = *s++;
        bits = 8;
        while (bits-- > 0 && left > 0) {
            if (flags & 0x80u) {
                a = *s;
                if (!extended) {
                    base = 3u;
                } else if (a & 0xe0u) {
                    base = 1u;
                } else {
                    s++;
                    base = (u32)(a & 0xfu) << 4;
                    if (a & 0x10u) {
                        base <<= 8;
                        b = *s++;
                        base += ((u32)b << 4) + 0x100u;
                    }
                    base += 0x11u;
                    a = *s;
                }
                length = base + ((u32)a >> 4);
                s++;
                disp = (((u32)a & 0xfu) << 8) | *s++;
                disp += 1u;
                left -= length;
                while (length-- > 0u) {
                    *d = *(d - disp);
                    d++;
                }
            } else {
                *d++ = *s++;
                left--;
            }
            flags <<= 1;
        }
    }
#else
    (void)src;
    (void)dest;
#endif
}

void MI_Copy36B(const void *src, void *dest) {
#if defined(PBW2_LINK_GAME)
    if (dest == (void *)&reg_G3X_GXFIFO) {
        PbW2_G3RecordFifo(src, 9);
        return;
    }
#endif
    MI_CpuCopy8(src, dest, 36);
}

void MI_Copy48B(const void *src, void *dest) {
    MI_CpuCopy8(src, dest, 48);
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
