/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Software DS 2D compositor. Adapted from widecomp.c (GPL-3.0, 3ds-platinum). */
#include "ds2d_compose.h"

#if defined(PBW2_LINK_GAME)

#include "g3_intercept.h"
#include "g3_raster.h"
#include "game_text.h"
#include "log.h"
#include "scene.h"
#include "wide_layout.h"
#include "nitro/hw.h"
#include "nitro/gx.h"
#include "types.h"
#include "vram_host.h"

#include <string.h>

extern u8 pbw2_io[];

static u16 rgb555_to_565(u16 c) {
    u32 r = c & 0x1f;
    u32 g = (c >> 5) & 0x1f;
    u32 b = (c >> 10) & 0x1f;
    return (u16)((r << 11) | (g << 6) | b);
}

static u16 reg_u16(const u8 *regs, u32 off) {
    return *(const u16 *)(regs + off);
}

static u32 reg_u32(const u8 *regs, u32 off) {
    return *(const u32 *)(regs + off);
}

enum { SUB_BG_CACHE_BYTES = 0x20000, OBJ_VRAM_BYTES = 0x40000, ENGINE_PIXELS = 256 * 192 };
typedef struct {
    u32 regs[0x70 / sizeof(u32)];
    u32 bgAddress, bgBytes, objAddress, textRevision, hasText;
} SubInputs;

static struct {
    SubInputs inputs;
    u8 bg[SUB_BG_CACHE_BYTES], obj[OBJ_VRAM_BYTES];
    u16 palette[512], oam[512], pixels[ENGINE_PIXELS];
    u32 ink[ENGINE_PIXELS];
    bool valid, replay;
    unsigned cooldown, misses;
} s_subCache;

static const u8 *live_vram_ptr(u32 addr) {
    return PbW2_VramMap((void *)(uintptr_t)addr);
}

static const u8 *vram_ptr(u32 addr) {
    if (s_subCache.replay) {
        const SubInputs *in = &s_subCache.inputs;
        if (addr == HW_DB_BG_PLTT) return (const u8 *)s_subCache.palette;
        if (addr == HW_DB_OBJ_PLTT) return (const u8 *)(s_subCache.palette + 256);
        if (addr == HW_DB_OAM) return (const u8 *)s_subCache.oam;
        if (in->objAddress && addr == in->objAddress) return s_subCache.obj;
        if (addr >= in->bgAddress && addr - in->bgAddress < in->bgBytes)
            return s_subCache.bg + addr - in->bgAddress;
    }
    return live_vram_ptr(addr);
}

static const u16 *engine_bg_pltt(int is_sub) {
    if (is_sub) {
        return (const u16 *)vram_ptr(HW_DB_BG_PLTT);
    }
    return (const u16 *)(void *)PbW2_VramMap((void *)HW_BG_PLTT);
}

static const u16 *engine_obj_pltt(int is_sub) {
    if (is_sub) {
        return (const u16 *)vram_ptr(HW_DB_OBJ_PLTT);
    }
    return (const u16 *)(void *)PbW2_VramMap((void *)HW_OBJ_PLTT);
}

static u32 engine_bg_vram_base(int is_sub) {
    return is_sub ? HW_DB_BG_VRAM : HW_BG_VRAM;
}

static u32 engine_obj_vram_base(int is_sub) {
    return is_sub ? HW_DB_OBJ_VRAM : HW_OBJ_VRAM;
}

static u32 vramcnt_mst(u8 bank) {
    return bank & 7u;
}

static u32 vramcnt_ofs(u8 bank) {
    return ((u32)bank >> REG_GX_VRAMCNT_A_OFS_SHIFT) & 3u;
}

static u32 vramcnt_map_addr(u8 bank, u32 bank_index) {
    u32 mst;
    u32 ofs;

    if ((bank & REG_GX_VRAMCNT_A_E_MASK) == 0) {
        return 0;
    }
    mst = vramcnt_mst(bank);
    ofs = vramcnt_ofs(bank);
    if (bank_index < 2u) {
        if (mst == 1u) {
            return HW_BG_VRAM + ofs * 0x20000u;
        }
        if (mst == 2u) {
            return HW_OBJ_VRAM + ofs * 0x20000u;
        }
        return 0;
    }
    if (bank_index < 4u) {
        if (mst == 1u) {
            return HW_BG_VRAM + ofs * 0x20000u;
        }
        if (mst == 2u) {
            return HW_OBJ_VRAM + ofs * 0x20000u;
        }
        if (mst == 4u) {
            return HW_DB_BG_VRAM + ofs * 0x20000u;
        }
        return 0;
    }
    return 0;
}

static u32 main_bg_linear_addr(u32 byte_off) {
    (void)byte_off;
    return HW_BG_VRAM + byte_off;
}

static void log_vramcnt_banks(const char *tag) {
    u32 vramcnt = *(const u32 *)(pbw2_io + 0x240);
    u32 i;

    for (i = 0; i < 4u; i++) {
        u8 bank = (u8)((vramcnt >> (i * 8u)) & 0xffu);

        Log_Line("GX", "vramcnt %s %c raw=0x%02x mst=%u ofs=%u map=0x%08x", tag, (char)('A' + (int)i),
                 (unsigned)bank, (unsigned)vramcnt_mst(bank), (unsigned)vramcnt_ofs(bank),
                 (unsigned)vramcnt_map_addr(bank, i));
    }
}

#define BMP16_SLOT_BYTES 0x4000u

static u32 bitmap_vram_base_byte(u16 cnt) {
    return (((u32)cnt >> REG_G2_BG0CNT_SCREENBASE_SHIFT) & 0x1fu) * BMP16_SLOT_BYTES;
}

static u32 bitmap_texel_addr(u16 cnt, int is_sub, int width, int tx, int ty, u32 pixel_bytes) {
    return engine_bg_vram_base(is_sub) + bitmap_vram_base_byte(cnt) + ((u32)ty * (u32)width + (u32)tx) * pixel_bytes;
}

static u32 bg_screen_addr(u32 dc, u16 cnt, int is_sub, u32 bgmode, int bg) {
    u32 base = engine_bg_vram_base(is_sub);
    u32 off;

    if (bg >= 2 && bgmode >= GX_BGMODE_3 && bgmode <= GX_BGMODE_5 && (cnt & REG_G2_BG0CNT_COLORMODE_MASK)) {
        off = bitmap_vram_base_byte(cnt);
        if (!is_sub) {
            return main_bg_linear_addr(off);
        }
        return base + off;
    }
    off = ((u32)cnt >> REG_G2_BG0CNT_SCREENBASE_SHIFT) & 0x1fu;
    off *= 0x800u;
    if (!is_sub) {
        off += ((dc >> REG_GX_DISPCNT_BGSCREENOFFSET_SHIFT) & 7u) * 0x10000u;
    }
    return base + off;
}

static s32 affine_ref28(u32 v) {
    v &= 0x0fffffffu;
    if (v & 0x08000000u) {
        v |= 0xf0000000u;
    }
    return (s32)v;
}

static void identity_affine(s16 *pa, s16 *pb, s16 *pc, s16 *pd, s32 *rx, s32 *ry) {
    *pa = (s16)0x0100;
    *pb = 0;
    *pc = 0;
    *pd = (s16)0x0100;
    *rx = 0;
    *ry = 0;
}

static void read_bg_affine(int is_sub, int bg, s16 *pa, s16 *pb, s16 *pc, s16 *pd, s32 *rx, s32 *ry) {
    u32 hw = 0x04000000u + (is_sub ? 0x1000u : 0u) + ((bg == 2) ? 0x20u : 0x30u);

    if (!PbW2_BgAffineWritten(is_sub, bg)) {
        identity_affine(pa, pb, pc, pd, rx, ry);
        return;
    }
    *pa = (s16)PBW2_VU16(hw);
    *pb = (s16)PBW2_VU16(hw + 2u);
    *pc = (s16)PBW2_VU16(hw + 4u);
    *pd = (s16)PBW2_VU16(hw + 6u);
    *rx = affine_ref28(PBW2_VU32(hw + 8u));
    *ry = affine_ref28(PBW2_VU32(hw + 12u));
}

static void map_bitmap_uv(s16 pa, s16 pb, s16 pc, s16 pd, s32 rx, s32 ry, int x, int y, s32 *tx, s32 *ty) {
    /* 8.8 affine coeffs; ref in 1/256 px â€” identity => 1:1 screen to texel. */
    if (pa == (s16)0x0100 && pb == 0 && pc == 0 && pd == (s16)0x0100) {
        *tx = x + (rx >> 8);
        *ty = y + (ry >> 8);
        return;
    }
    *tx = (s32)((((s64)rx + (s64)pa * x + (s64)pb * y) >> 8));
    *ty = (s32)((((s64)ry + (s64)pc * x + (s64)pd * y) >> 8));
}

static const u16 *bg_layer_pltt(u32 dc, u16 cnt, int is_sub, int bg) {
    if ((dc & REG_GX_DISPCNT_BG_MASK) != 0) {
        u32 slot = (cnt >> REG_G2_BG0CNT_BGPLTTSLOT_SHIFT) & 1u;
        u32 bank = slot ? HW_LCDC_VRAM_G : HW_LCDC_VRAM_F;
        (void)bg;
        (void)is_sub;
        return (const u16 *)(void *)vram_ptr(bank);
    }
    return engine_bg_pltt(is_sub);
}

static u32 bg_char_addr(u32 dc, u16 cnt, int is_sub) {
    u32 off = ((u32)(cnt >> 2) & 0xfu) * 0x4000u;

    if (!is_sub) {
        off += ((dc >> REG_GX_DISPCNT_BGCHAROFFSET_SHIFT) & 7u) * 0x10000u;
    }
    return engine_bg_vram_base(is_sub) + off;
}

typedef struct {
    u16 colors[256 * 192];
    u8 planes[256 * 192];
    const u8 *window;
    bool track;
    u32 *ink;
    bool nativeText;
    u16 first;
    u16 second;
    u16 eva;
    u16 evb;
} BgBlend;

static unsigned blend_channel(unsigned a, unsigned b, const BgBlend *blend) {
    unsigned color = (a * blend->eva + b * blend->evb) >> 4;
    return color > 31 ? 31 : color;
}

static inline void put_bg_pixel(u16 *out, BgBlend *blend, unsigned i, unsigned plane, u16 color) {
    u16 visible = color;
    if (blend != NULL) {
        u8 mask = blend->window ? blend->window[i] : 0x3f;
        if (!(mask & plane)) return;
        if ((mask & G2_WND_EFFECT) && (blend->first & plane) && (blend->second & blend->planes[i])) {
            u16 below = blend->colors[i];
            unsigned r = blend_channel(color >> 11, below >> 11, blend);
            unsigned g = blend_channel((color >> 6) & 31, (below >> 6) & 31, blend);
            unsigned b = blend_channel(color & 31, below & 31, blend);
            visible = (u16)((r << 11) | (g << 6) | b);
        }
        /* A foreground layer blends against the original color of the layer beneath it. */
        if (blend->track) {
            if (blend->first) blend->colors[i] = color;
            blend->planes[i] = (u8)plane;
        }
    }
    out[i] = visible;
    if (blend && blend->ink) blend->ink[i] = 0;
}

static void put_text_pixel(u16 *out, BgBlend *blend, unsigned i, unsigned plane, u16 color, u32 ink, const u16 *palette) {
    put_bg_pixel(out, blend, i, plane, color);
    if (!ink || !blend || !blend->ink || (blend->window && !(blend->window[i] & plane))) return;
    blend->ink[i] = PBW2_TEXT_INK;
    if (blend->nativeText && (ink & PBW2_TEXT_NATIVE))
        blend->ink[i] = (ink & ~0xffu) | PBW2_TEXT_INK | palette[ink & 255];
}

static void compose_text_bg(const u8 *regs, u32 dc, int is_sub, int bg, u32 bgmode, u16 *out, BgBlend *blend) {
    u16 cnt = reg_u16(regs, 8 + bg * 2);
    int pw = ((cnt >> 14) & 1) ? 64 : 32;
    int ph = ((cnt >> 14) & 2) ? 64 : 32;
    const u16 *map = (const u16 *)(void *)vram_ptr(bg_screen_addr(dc, cnt, is_sub, bgmode, bg));
    u32 charAddress = bg_char_addr(dc, cnt, is_sub);
    const u8 *chars = vram_ptr(charAddress);
    const u8 *liveChars = live_vram_ptr(charAddress);
    const u16 *pltt = bg_layer_pltt(dc, cnt, is_sub, bg);
    u32 hofs_base = reg_u16(regs, 0x10u + (u32)bg * 4u) & 0x1ffu;
    u32 vofs_base = reg_u16(regs, 0x12u + (u32)bg * 4u) & 0x1ffu;
    const bool indexed256 = (cnt & 0x80u) != 0;
    const unsigned plane = 1u << bg;

    if ((dc & (0x100u << bg)) == 0) {
        return;
    }
    if (bg == 0 && (dc & REG_GX_DISPCNT_BG02D3D_MASK)) {
        return;
    }

    u16 colors[256];
    for (unsigned i = 0; i < 256; i++) colors[i] = rgb555_to_565(pltt[i]);
    for (unsigned y = 0; y < 192; y++) {
        u32 vofs = (y + vofs_base) & (ph * 8 - 1);
        u32 row = vofs / 8;
        const u16 *mapRow = map + (row >> 5) * (pw >> 5) * 1024 + (row & 31) * 32;
        for (unsigned x = 0; x < 256;) {
            u32 hofs = (x + hofs_base) & (pw * 8 - 1);
            /* Large text maps are laid out in 32-by-32 tile screen blocks. */
            u16 sc = mapRow[(hofs >> 8) * 1024 + ((hofs >> 3) & 31)];
            unsigned begin = hofs & 7;
            unsigned count = 8 - begin;
            if (count > 256 - x) count = 256 - x;
            unsigned tile = sc & 1023;
            unsigned ty = (vofs & 7) ^ ((sc & 0x0800) ? 7 : 0);
            unsigned flip = (sc & 0x0400) ? 7 : 0;
            unsigned dst = y * 256 + x;
            x += count;
            if (indexed256) {
                const u8 *source = chars + tile * 64 + ty * 8;
                u32 words[2];
                memcpy(words, source, sizeof words);
                if (!(words[0] | words[1])) continue;
                u32 details[8];
                u8 ink = blend && blend->ink ? PbW2_TextTileRow(source - ty * 8, liveChars + tile * 64, 64, ty, details) : 0;
                bool unflipped = (sc & 0x0c00) == 0;
                unsigned tx = begin ^ flip;
                int step = flip ? -1 : 1;
                if (!blend) {
                    for (unsigned i = 0; i < count; ++i, tx += step) {
                        unsigned color = source[tx];
                        if (color) out[dst + i] = colors[color];
                    }
                } else {
                    for (unsigned i = 0; i < count; ++i, tx += step) {
                        unsigned color = source[tx];
                        if (color) put_text_pixel(out, blend, dst + i, plane, colors[color],
                                                      (ink & (1u << tx)) ? (unflipped ? details[tx] | PBW2_TEXT_INK : PBW2_TEXT_INK) : 0,
                                                      colors);
                    }
                }
            } else {
                const u8 *source = chars + tile * 32 + ty * 4;
                u32 nibbles;
                memcpy(&nibbles, source, sizeof nibbles);
                if (!nibbles) continue;
                u32 details[8];
                u8 ink = blend && blend->ink ? PbW2_TextTileRow(source - ty * 4, liveChars + tile * 32, 32, ty, details) : 0;
                bool unflipped = (sc & 0x0c00) == 0;
                const u16 *palette = colors + (sc >> 12) * 16;
                for (unsigned i = 0; i < count; i++) {
                    unsigned tx = (begin + i) ^ flip;
                    unsigned color = (nibbles >> (tx * 4)) & 15;
                    if (color) put_text_pixel(out, blend, dst + i, plane, palette[color],
                                              (ink & (1u << tx)) ? (unflipped ? details[tx] | PBW2_TEXT_INK : PBW2_TEXT_INK) : 0,
                                              palette);
                }
            }
        }
    }
}

static void bitmap_dims(u16 cnt, int *bw, int *bh) {
    u32 sz = (cnt >> 14) & 3u;
    switch (sz) {
    default:
    case 0:
        *bw = 128;
        *bh = 128;
        break;
    case 1:
        *bw = 256;
        *bh = 256;
        break;
    case 2:
        *bw = 512;
        *bh = 256;
        break;
    case 3:
        *bw = 512;
        *bh = 512;
        break;
    }
}

static struct {
    u32 compose_frames;
    u32 mode5_on_frame;
    u8 logged_at_on;
    u8 logged_delayed;
} s_mode5_diag;

static u32 count_opaque16_at_base(u16 cnt, int bw, int bh) {
    u32 addr = main_bg_linear_addr(bitmap_vram_base_byte(cnt));
    const u16 *p = (const u16 *)(void *)vram_ptr(addr);
    u32 opaque = 0;
    int y;
    int x;

    for (y = 0; y < bh; y++) {
        const u16 *row = &p[(u32)y * (u32)bw];
        for (x = 0; x < bw; x++) {
            if (row[x] & 0x8000u) {
                opaque++;
            }
        }
    }
    return opaque;
}

static void log_mode5_bitmap(const u8 *regs, int bg, u16 cnt, u32 bmp_addr, int is16, int bw, int bh, u32 opaque,
                              const char *phase) {
    u32 screen_base = ((u32)cnt >> REG_G2_BG0CNT_SCREENBASE_SHIFT) & 0x1fu;
    s16 pa;
    s16 pb;
    s16 pc;
    s16 pd;
    s32 rx;
    s32 ry;
    s32 mx;
    s32 my;
    const u16 *samp;
    u16 mid_c;

    read_bg_affine(0, bg, &pa, &pb, &pc, &pd, &rx, &ry);
    map_bitmap_uv(pa, pb, pc, pd, rx, ry, 128, 96, &mx, &my);
    mid_c = 0;
    samp = NULL;
    if (is16) {
        if (mx >= 0 && my >= 0 && mx < bw && my < bh) {
            u32 mid_addr = bitmap_texel_addr(cnt, 0, bw, mx, my, 2);
            const u16 *mp = (const u16 *)(void *)vram_ptr(mid_addr);
            mid_c = mp[0];
        }
        samp = (const u16 *)(void *)vram_ptr(bmp_addr);
    }
    Log_Line("GX",
             "mode5-%s bg%d cnt=0x%04x prio=%u is16=%d screen=%u addr=0x%08x aff=%d PA=%d PB=%d PC=%d PD=%d refX=%d "
             "refY=%d mid=%d,%d samp=0x%04x,0x%04x,0x%04x,0x%04x mid_c=0x%04x opaque=%u",
             phase, bg, (unsigned)cnt, (unsigned)(cnt & 3u), is16, (unsigned)screen_base, (unsigned)bmp_addr,
             PbW2_BgAffineWritten(0, bg), (int)pa, (int)pb, (int)pc, (int)pd, (int)rx, (int)ry, (int)mx, (int)my,
             samp ? (unsigned)samp[0] : 0u, samp ? (unsigned)samp[1] : 0u, samp ? (unsigned)samp[2] : 0u,
             samp ? (unsigned)samp[3] : 0u, (unsigned)mid_c, (unsigned)opaque);
}

static void maybe_log_mode5(const u8 *regs, u32 dc, u32 bgmode, int bg, u16 cnt, u32 bmp_addr, int is16, int bw,
                            int bh) {
    u32 opaque;
    u16 cnt3;
    u32 addr3;

    if (bgmode != GX_BGMODE_5 || bg != 2 || s_mode5_diag.logged_delayed ||
        (s_mode5_diag.logged_at_on && s_mode5_diag.compose_frames - s_mode5_diag.mode5_on_frame < 60u)) {
        return;
    }
    opaque = is16 ? count_opaque16_at_base(cnt, bw, bh) : 0u;
    cnt3 = reg_u16(regs, 0x0e);
    addr3 = bg_screen_addr(dc, cnt3, 0, bgmode, 3);
    if (!s_mode5_diag.logged_at_on) {
        log_vramcnt_banks("on");
        s_mode5_diag.mode5_on_frame = s_mode5_diag.compose_frames;
        s_mode5_diag.logged_at_on = 1;
        log_mode5_bitmap(regs, 2, cnt, bmp_addr, is16, bw, bh, opaque, "on");
        log_mode5_bitmap(regs, 3, cnt3, addr3, 1, 256, 256, count_opaque16_at_base(cnt3, 256, 256), "on");
        return;
    }
    log_vramcnt_banks("+60");
    s_mode5_diag.logged_delayed = 1;
    log_mode5_bitmap(regs, 2, cnt, bmp_addr, is16, bw, bh, opaque, "+60");
    log_mode5_bitmap(regs, 3, cnt3, addr3, 1, 256, 256, count_opaque16_at_base(cnt3, 256, 256), "+60");
}

static void compose_bitmap_bg(const u8 *regs, u32 dc, int is_sub, int bg, u32 bgmode, u16 *out, BgBlend *blend) {
    u16 cnt = reg_u16(regs, 8 + bg * 2);
    unsigned plane = 1u << bg;
    bool direct = (cnt & 0x0004u) != 0;
    bool wrap = (cnt & (1u << REG_G2_BG2CNT_AREAOVER_SHIFT)) != 0;
    u32 address = engine_bg_vram_base(is_sub) + bitmap_vram_base_byte(cnt);
    const u8 *bitmap = vram_ptr(address);
    const u16 *palette = bg_layer_pltt(dc, cnt, is_sub, bg);
    int width, height;
    s16 pa, pb, pc, pd;
    s32 rx, ry;
    bitmap_dims(cnt, &width, &height);
    maybe_log_mode5(regs, dc, bgmode, bg, cnt, address, direct, width, height);
    read_bg_affine(is_sub, bg, &pa, &pb, &pc, &pd, &rx, &ry);

    /* The signed 28-bit origin plus 256 steps of either 8.8 coefficient fits in s32. */
    for (unsigned y = 0; y < 192; ++y, rx += pb, ry += pd) {
        s32 sx = rx, sy = ry;
        for (unsigned x = 0; x < 256; ++x, sx += pa, sy += pc) {
            unsigned tx = (unsigned)(sx >> 8), ty = (unsigned)(sy >> 8);
            if (wrap) {
                tx &= width - 1;
                ty &= height - 1;
            } else if (tx >= (unsigned)width || ty >= (unsigned)height) {
                continue;
            }
            unsigned index = ty * width + tx;
            u16 color;
            if (direct) {
                color = ((const u16 *)(const void *)bitmap)[index];
                if (!(color & 0x8000)) continue;
            } else {
                unsigned entry = bitmap[index];
                if (!entry) continue;
                color = palette[entry];
            }
            put_bg_pixel(out, blend, y * 256 + x, plane, rgb555_to_565(color));
        }
    }
}

static void draw_bg_layer(const u8 *regs, u32 dc, int is_sub, int bg, u32 bgmode, u16 *out, BgBlend *blend) {
    u16 cnt = reg_u16(regs, 8 + bg * 2);

    if (bg >= 2 && bgmode >= 3 && (cnt & REG_G2_BG0CNT_COLORMODE_MASK)) {
        compose_bitmap_bg(regs, dc, is_sub, bg, bgmode, out, blend);
    } else {
        compose_text_bg(regs, dc, is_sub, bg, bgmode, out, blend);
    }
}

static u32 engine_obj_vram_mapped(int is_sub) {
    u32 vramcnt;
    u8 bank;
    u32 map;

    if (is_sub) {
        if (s_subCache.replay) return s_subCache.inputs.objAddress;
        vramcnt = *(const u32 *)(pbw2_io + 0x240);
        bank = (u8)((vramcnt >> 24) & 0xffu);
        map = vramcnt_map_addr(bank, 3);
        if (map != 0u && vramcnt_mst(bank) == 2u) {
            return map;
        }
        return HW_DB_OBJ_VRAM;
    }
    vramcnt = *(const u32 *)(pbw2_io + 0x240);
    bank = (u8)(vramcnt & 0xffu);
    map = vramcnt_map_addr(bank, 0);
    if (map != 0u && vramcnt_mst(bank) == 2u) {
        return map;
    }
    return HW_OBJ_VRAM;
}

enum { OBJ_WINDOW_LIST = 4, OBJ_LISTS = 5, OBJ_COUNT = 128 };

typedef struct {
    u16 oam[OBJ_COUNT * 4];
    const u16 *palette;
    const u8 *vram;
    unsigned count[OBJ_LISTS];
    u8 index[OBJ_LISTS][OBJ_COUNT];
    bool oneDimensional;
    unsigned tileShift;
} ObjScene;

static void prepare_obj_scene(ObjScene *scene, u32 dc, int is_sub) {
    memset(scene->count, 0, sizeof scene->count);
    if (!(dc & 0x1000u)) return;
    memcpy(scene->oam, vram_ptr(is_sub ? HW_DB_OAM : HW_OAM), sizeof scene->oam);
    scene->vram = vram_ptr(engine_obj_vram_mapped(is_sub));
    scene->palette = (dc & REG_GX_DISPCNT_O_MASK) ? (const u16 *)vram_ptr(HW_LCDC_VRAM_G) : engine_obj_pltt(is_sub);
    scene->oneDimensional = (dc & REG_GX_DISPCNT_OBJMAP_CH_MASK) != 0;
    scene->tileShift = 5 + (scene->oneDimensional ? (dc >> REG_GX_DISPCNT_EXOBJ_SHIFT) & 3u : 0);
    /* Descending OAM order leaves the lower index on top at a priority tie. */
    for (int i = OBJ_COUNT - 1; i >= 0; --i) {
        const u16 *attributes = scene->oam + i * 4;
        if ((attributes[0] >> 14) > 2 || (attributes[0] & 0x300) == 0x200) continue;
        unsigned mode = (attributes[0] >> 10) & 3;
        unsigned list = mode == GX_OAM_MODE_OBJWND ? OBJ_WINDOW_LIST : (attributes[2] >> 10) & 3;
        scene->index[list][scene->count[list]++] = i;
    }
}

static void draw_obj_layer(const ObjScene *scene, u16 *out, u8 *window, u8 inside, unsigned list, BgBlend *blend) {
    static const u8 spw[3][4] = {{8, 16, 32, 64}, {16, 32, 32, 64}, {8, 8, 16, 32}};
    static const u8 sph[3][4] = {{8, 16, 32, 64}, {8, 8, 16, 32}, {16, 32, 32, 64}};
    for (unsigned i = 0; i < scene->count[list]; ++i) {
        const u16 *a = scene->oam + scene->index[list][i] * 4;
        unsigned shape = a[0] >> 14;
        unsigned size = a[1] >> 14;
        int width = spw[shape][size];
        int height = sph[shape][size];
        bool affine = (a[0] & 0x0100u) != 0;
        bool doubleSize = affine && (a[0] & 0x0200u);
        int boxWidth = doubleSize ? 2 * width : width;
        int boxHeight = doubleSize ? 2 * height : height;
        const u16 *matrix = scene->oam + 16 * ((a[1] >> 9) & 31);
        int pa = affine ? (s16)matrix[3] : (a[1] & 0x1000u) ? -256 : 256;
        int pb = affine ? (s16)matrix[7] : 0;
        int pc = affine ? (s16)matrix[11] : 0;
        int pd = affine ? (s16)matrix[15] : (a[1] & 0x2000u) ? -256 : 256;
        int xCenter = affine ? width * 128 : pa < 0 ? (width - 1) * 256 : 0;
        int yCenter = affine ? height * 128 : pd < 0 ? (height - 1) * 256 : 0;
        int xOrigin = affine ? -boxWidth / 2 : 0;
        int yOrigin = affine ? -boxHeight / 2 : 0;
        bool bpp8 = (a[0] & 0x2000) != 0;
        unsigned tile_bytes = bpp8 ? 64 : 32;
        unsigned tile_base = (a[2] & 0x3ffu) << scene->tileShift;
        unsigned tile_row_bytes = scene->oneDimensional ? (width >> 3) * tile_bytes : 0x400;
        int xs = a[1] & 0x1ff;
        int ypos = a[0] & 255;
        if (xs >= 256) xs -= 512;
        if (ypos >= 192) ypos -= 256;
        /* Clip screen bounds once per sprite. Advance the affine origin to
         * the first visible pixel so offscreen work never enters the loop. */
        int left = xs < 0 ? -xs : 0;
        int right = xs + boxWidth > 256 ? 256 - xs : boxWidth;
        int top = ypos < 0 ? -ypos : 0;
        int bottom = ypos + boxHeight > 192 ? 192 - ypos : boxHeight;
        if (left >= right || top >= bottom) continue;
        int sxAtLeft = xCenter + (xOrigin + left) * pa;
        int syAtLeft = yCenter + (xOrigin + left) * pc;
        for (int yo = top; yo < bottom; ++yo) {
            int y = ypos + yo;
            int sx = sxAtLeft + (yOrigin + yo) * pb;
            int sy = syAtLeft + (yOrigin + yo) * pd;
            for (int xo = left; xo < right; ++xo, sx += pa, sy += pc) {
                int x = xs + xo;
                unsigned addr;
                unsigned px = sx >> 8, py = sy >> 8;
                unsigned ci, paletteIndex;
                if (affine && (px >= (unsigned)width || py >= (unsigned)height)) continue;
                addr = tile_base + (py >> 3) * tile_row_bytes + (py & 7u) * (tile_bytes >> 3)
                     + (px >> 3) * tile_bytes;
                if (bpp8) {
                    addr += px & 7u;
                    ci = scene->vram[addr & 0x3ffffu];
                    paletteIndex = ci;
                } else {
                    addr += (px & 7u) >> 1;
                    u8 nib = scene->vram[addr & 0x3ffffu];
                    ci = (px & 1) ? (nib >> 4) : (nib & 0x0f);
                    paletteIndex = (a[2] >> 12) * 16u + ci;
                }
                if (!ci) continue;
                unsigned pixel = y * 256 + x;
                if (!out) window[pixel] = inside;
                else if (!window || (window[pixel] & GX_WND_PLANEMASK_OBJ)) {
                    u16 color = rgb555_to_565(scene->palette[paletteIndex]);
                    out[pixel] = color;
                    if (blend && blend->ink) blend->ink[pixel] = 0;
                    if (blend && blend->track) {
                        if (blend->first) blend->colors[pixel] = color;
                        blend->planes[pixel] = GX_WND_PLANEMASK_OBJ;
                    }
                }
            }
        }
    }
}

static void apply_window_rect(u8 *window, u16 xBounds, u16 yBounds, u8 mask) {
    unsigned left = xBounds >> 8, right = xBounds & 255;
    unsigned top = yBounds >> 8, bottom = yBounds & 255;
    if (left == right || top == bottom) return;
    for (unsigned y = 0; y < 192; ++y) {
        bool within = top < bottom ? y >= top && y < bottom : y >= top || y < bottom;
        if (!within) continue;
        u8 *row = window + y * 256;
        if (left < right) {
            memset(row + left, mask, right - left);
        } else {
            memset(row, mask, right);
            memset(row + left, mask, 256 - left);
        }
    }
}

static u8 *build_window_mask(const u8 *regs, u32 dc, const ObjScene *objects) {
    static u8 window[256 * 192];
    unsigned enabled = (dc >> 13) & 7;
    if (!enabled) return NULL;
    u16 outside = reg_u16(regs, 0x4a);
    memset(window, outside & 0x3f, sizeof window);
    if (enabled & GX_WNDMASK_OW) draw_obj_layer(objects, NULL, window, (outside >> 8) & 0x3f, OBJ_WINDOW_LIST, NULL);
    /* Window 0 takes precedence over window 1, then over the OBJ window. */
    u16 inside = reg_u16(regs, 0x48);
    if (enabled & GX_WNDMASK_W1) apply_window_rect(window, reg_u16(regs, 0x42), reg_u16(regs, 0x46), (inside >> 8) & 0x3f);
    if (enabled & GX_WNDMASK_W0) apply_window_rect(window, reg_u16(regs, 0x40), reg_u16(regs, 0x44), inside & 0x3f);
    return window;
}

static void apply_master_bright(u16 *fb, unsigned pixels, u16 reg) {
    u16 mode = reg & REG_GX_MASTER_BRIGHT_E_MOD_MASK;
    u32 factor = reg & REG_GX_MASTER_BRIGHT_E_VALUE_MASK;
    int brighten = mode == REG_GX_MASTER_BRIGHT_MODE_UP;

    if (factor == 0 || (!brighten && mode != REG_GX_MASTER_BRIGHT_MODE_DOWN)) {
        return;
    }
    if (factor >= 16) {
        memset(fb, brighten ? 0xff : 0, pixels * sizeof(*fb));
        return;
    }
    for (u32 i = 0; i < pixels; i++) {
        u16 p = fb[i];
        u32 r = (p >> 11) & 0x1f;
        u32 g = (p >> 5) & 0x3f;
        u32 b = p & 0x1f;
        if (brighten) {
            r += (31u - r) * factor >> 4;
            g += (63u - g) * factor >> 4;
            b += (31u - b) * factor >> 4;
        } else {
            r -= r * factor >> 4;
            g -= g * factor >> 4;
            b -= b * factor >> 4;
        }
        fb[i] = (u16)((r << 11) | (g << 5) | b);
    }
}

static unsigned layer_brightness(unsigned color, unsigned factor, bool brighten) {
    if (brighten) return color + (((63 - color) * factor + 8) >> 4);
    return color - ((color * factor + 7) >> 4);
}

static void apply_layer_brightness(u16 *out, const BgBlend *blend, u16 bldcnt, unsigned factor) {
    unsigned targets = bldcnt & 0x3f;
    if (!targets || !factor) return;
    bool brighten = (bldcnt & REG_G2_BLDCNT_EFFECT_MASK) == REG_G2_BLDCNT_EFFECT_INCREASE;
    if (factor > 16) factor = 16;
    for (unsigned i = 0; i < 256 * 192; ++i) {
        if (!(blend->planes[i] & targets) || (blend->window && !(blend->window[i] & G2_WND_EFFECT))) continue;
        u16 color = out[i];
        unsigned r = layer_brightness((color >> 11) * 2, factor, brighten);
        unsigned g = layer_brightness((color >> 5) & 63, factor, brighten);
        unsigned b = layer_brightness((color & 31) * 2, factor, brighten);
        out[i] = ((r >> 1) << 11) | (g << 5) | (b >> 1);
    }
}

static void capture_main(u16 *out, u32 dc) {
    u32 capture = reg_GX_DISPCAPCNT;
    unsigned displayBank = (dc >> 18) & 3;
    unsigned displayMode = (dc & REG_GX_DISPCNT_MODE_MASK) >> REG_GX_DISPCNT_MODE_SHIFT;
    u16 *source = PbW2_VramMap((void *)(uintptr_t)(HW_LCDC_VRAM_A + displayBank * 0x20000));

    if ((capture & REG_GX_DISPCAPCNT_E_MASK) && !(capture & (1u << REG_GX_DISPCAPCNT_SRCB_SHIFT))) {
        static const unsigned heights[] = {128, 64, 128, 192};
        unsigned size = (capture >> REG_GX_DISPCAPCNT_WSIZE_SHIFT) & 3;
        unsigned width = size ? 256 : 128;
        unsigned destBank = (capture >> REG_GX_DISPCAPCNT_DEST_SHIFT) & 3;
        const u8 banks[] = {reg_GX_VRAMCNT_A, reg_GX_VRAMCNT_B, reg_GX_VRAMCNT_C, reg_GX_VRAMCNT_D};
        unsigned mode = (capture >> REG_GX_DISPCAPCNT_MOD_SHIFT) & 3;
        unsigned eva = capture & 31;
        unsigned evb = (capture >> REG_GX_DISPCAPCNT_EVB_SHIFT) & 31;
        unsigned writeOffset = ((capture >> 18) & 3) * 0x4000;
        unsigned readOffset = displayMode == 2 ? 0 : ((capture >> 26) & 3) * 0x4000;
        static u16 capture3d[256 * 192];
        int source3d = (capture & (1u << REG_GX_DISPCAPCNT_SRCA_SHIFT)) != 0;
        if (source3d && mode != 1) {
            PbW2_G3RasterCapture(capture3d);
        }
        if (eva > 16) eva = 16;
        if (evb > 16) evb = 16;
        if (banks[destBank] == GX_VRAMCNT_LCDC) {
            u32 bankAddress = HW_LCDC_VRAM_A + destBank * 0x20000;
            u16 *dest = PbW2_VramMap((void *)(uintptr_t)bankAddress);
            for (unsigned y = 0; y < heights[size]; ++y) {
                for (unsigned x = 0; x < width; ++x) {
                    unsigned i = y * width + x;
                    u16 rgb = out[y * 256 + x];
                    u16 a = source3d && mode != 1 ? capture3d[y * 256 + x]
                                     : (rgb >> 11) | ((rgb >> 1) & 0x03e0) | ((rgb & 31) << 10) | 0x8000;
                    u16 b = banks[displayBank] == GX_VRAMCNT_LCDC ? source[(readOffset + i) & 0xffff] : 0;
                    u16 pixel = mode == 0 ? a : b;
                    if (mode >= 2) {
                        pixel = (a | b) & 0x8000;
                        unsigned weightA = (a & 0x8000) ? eva : 0;
                        unsigned weightB = (b & 0x8000) ? evb : 0;
                        for (unsigned shift = 0; shift < 15; shift += 5) {
                            unsigned channel = ((((a >> shift) & 31) * weightA) + (((b >> shift) & 31) * weightB)) >> 4;
                            pixel |= (channel > 31 ? 31 : channel) << shift;
                        }
                    }
                    dest[(writeOffset + i) & 0xffff] = pixel;
                }
            }
            unsigned written = width * heights[size];
            unsigned first = 0x10000 - writeOffset;
            if (first > written) first = written;
            PbW2_VramNoteLcdcWrite(bankAddress + 2 * writeOffset, 2 * first);
            if (written > first) PbW2_VramNoteLcdcWrite(bankAddress, 2 * (written - first));
        }
        if (reg_GX_DISPCAPCNT == capture) reg_GX_DISPCAPCNT = capture & ~REG_GX_DISPCAPCNT_E_MASK;
    }
    if (displayMode == 2) {
        for (unsigned i = 0; i < 256 * 192; ++i) out[i] = rgb555_to_565(source[i]);
    }
}

static u16 s_sides[64 * 192];
static bool s_haveSides, s_haveNativeTop;
static u16 s_nativeTop[PBW2_NATIVE_TOP_WIDTH * PBW2_NATIVE_TOP_HEIGHT];
static u16 s_wideTop[WL_WIDE_WIDTH * WL_HEIGHT], s_wideBottom[WL_WIDE_WIDTH * WL_HEIGHT];
static bool s_haveWideTop, s_haveWideBottom;

const u16 *PbW2_NativeTop(void) {
    return s_haveNativeTop ? s_nativeTop : NULL;
}

const u16 *PbW2_WideTop(void) {
    return s_haveWideTop ? s_wideTop : NULL;
}

const u16 *PbW2_WideBottom(void) {
    return s_haveWideBottom ? s_wideBottom : NULL;
}

/* A wide 2D layout declines (the centred picture stands) when the
 * engine is not in plain graphics mode, is forced blank, or carries
 * the 3D layer on BG0 — that screen belongs to the 3D wide path.
 * Display capture armed on the main engine declines both screens:
 * capture frames are transitions, and a layout must not flicker. */
static bool layout_declined(const u8 *regs) {
    u32 dc = reg_u32(regs, 0);
    if (dc & 0x80u) return true;
    if (((dc >> REG_GX_DISPCNT_MODE_SHIFT) & 3) != 1) return true;
    if (dc & REG_GX_DISPCNT_BG02D3D_MASK) return true;
    return false;
}

static void log_layout_change(int isTop, const char *name) {
    static const char *s_lastTop, *s_lastBottom;
    const char **last = isTop ? &s_lastTop : &s_lastBottom;
    if (name == *last) return;
    *last = name;
    Log_Line("WIDE", "layout %s: %s", isTop ? "top" : "bottom", name ? name : "none");
}

static void apply_wide_layouts(int main_upper, u16 *main_fb, u16 *sub_fb) {
    u16 *top_fb = main_upper ? main_fb : sub_fb;
    u16 *bottom_fb = main_upper ? sub_fb : main_fb;
    const u8 *top_regs = main_upper ? pbw2_io : pbw2_io + 0x1000;
    const u8 *bottom_regs = main_upper ? pbw2_io + 0x1000 : pbw2_io;
    unsigned count = 0;
    const char *name = NULL;
    const WlPart *parts;

    PbW2_WideSetFrameContext(main_upper);
    if (!PbW2_G3WideEnabled() || (reg_GX_DISPCAPCNT & REG_GX_DISPCAPCNT_E_MASK)) {
        log_layout_change(1, NULL);
        log_layout_change(0, NULL);
        return;
    }
    parts = PbW2_WideLayoutTop(&count, &name);
    if (top_fb && parts && !layout_declined(top_regs)) {
        PbW2_WideApplyLayout(top_fb, s_wideTop, parts, count);
        s_haveWideTop = true;
        log_layout_change(1, name);
    } else {
        log_layout_change(1, NULL);
    }
    parts = PbW2_WideLayoutBottom(&count, &name);
    if (bottom_fb && parts && !layout_declined(bottom_regs)) {
        PbW2_WideApplyLayout(bottom_fb, s_wideBottom, parts, count);
        s_haveWideBottom = true;
        log_layout_change(0, name);
    } else {
        log_layout_change(0, NULL);
    }
}

const u16 *PbW2_TopSides(void) {
    return s_haveSides ? s_sides : NULL;
}

static void compose_engine(const u8 *regs, int is_sub, u16 *out, u32 *ink) {
    static BgBlend bg_blend;
    static u16 underGeometry[256 * 192];
    const u16 *nativeFrame = NULL;
    BgBlend *blend = NULL;
    u32 dc = reg_u32(regs, 0);
    u32 bgmode = dc & REG_GX_DISPCNT_BGMODE_MASK;
    u16 backdrop;
    int pri;
    int bg;
    u32 i;

    if ((dc & 0x80u) || (dc & REG_GX_DISPCNT_MODE_MASK) == 0) {
        memset(out, 0xff, 256u * 192u * sizeof(*out));
        return;
    }
    backdrop = rgb555_to_565(engine_bg_pltt(is_sub)[0]);
    for (i = 0; i < 256u * 192u; i++) {
        out[i] = backdrop;
    }
    ObjScene objects;
    prepare_obj_scene(&objects, dc, is_sub);
    u8 *window = build_window_mask(regs, dc, &objects);
    u16 bldcnt = reg_u16(regs, 0x50);
    unsigned effect = bldcnt & REG_G2_BLDCNT_EFFECT_MASK;
    unsigned factor = reg_u16(regs, 0x54) & 31;
    bool layerBrightness = (effect == REG_G2_BLDCNT_EFFECT_INCREASE || effect == REG_G2_BLDCNT_EFFECT_DECREASE)
                           && (bldcnt & 0x3f) && factor;
    bool layerAlpha = effect == REG_G2_BLDCNT_EFFECT_ALPHA && (bldcnt & 0x3f) && (bldcnt & 0x3f00);
    /* Whether BG0 itself takes part in the effect. The wide side
     * extension replaces only BG0 (the 3D layer), so an effect aimed
     * at other layers must not veto it - weather and flash blends
     * strobed the wide picture off and on - while an effect that
     * does touch BG0 must, or the unmodified sides would seam
     * against the modified centre. */
    bool bg0Brightness = (effect == REG_G2_BLDCNT_EFFECT_INCREASE || effect == REG_G2_BLDCNT_EFFECT_DECREASE)
                         && (bldcnt & 0x0001) && factor;
    bool bg0Alpha = effect == REG_G2_BLDCNT_EFFECT_ALPHA && ((bldcnt & 0x0001) || (bldcnt & 0x0100));
    bg_blend.window = window;
    bg_blend.track = layerAlpha || layerBrightness;
    bg_blend.first = 0;
    bg_blend.ink = ((dc >> REG_GX_DISPCNT_MODE_SHIFT) & 3) == 1 ? ink : NULL;
    /* Master brightness no longer vetoes native text: compose applies
     * it to the finished frame (and to the native top and sides), and
     * the native stamp samples its colours from that brightened
     * frame, so glyphs fade with everything else. The old veto made
     * sharp text downgrade to the DS-resolution rendering for the
     * whole length of every fade. */
    bg_blend.nativeText = bg_blend.ink && !layerAlpha && !layerBrightness &&
                          (is_sub || !(reg_GX_DISPCAPCNT & REG_GX_DISPCAPCNT_E_MASK));
    bg_blend.track |= bg_blend.nativeText;
    if (window || bg_blend.track || bg_blend.ink) blend = &bg_blend;
    if (bg_blend.track) memset(bg_blend.planes, 1u << 5, sizeof(bg_blend.planes));
    if (layerAlpha) {
        u16 alpha = reg_u16(regs, 0x52);
        blend = &bg_blend;
        blend->first = bldcnt & 0x3f;
        blend->second = (bldcnt >> 8) & 0x3f;
        blend->eva = alpha & 31;
        blend->evb = (alpha >> 8) & 31;
        if (blend->eva > 16) {
            blend->eva = 16;
        }
        if (blend->evb > 16) {
            blend->evb = 16;
        }
        memcpy(blend->colors, out, sizeof(blend->colors));
    }
    for (pri = 3; pri >= 0; pri--) {
        /* BG0 wins a priority tie among BGs. OBJs with that priority go over them. */
        for (bg = 3; bg >= 0; bg--) {
            u16 cnt = reg_u16(regs, 8 + bg * 2);
            if ((dc & (0x100u << bg)) == 0) {
                continue;
            }
            if ((cnt & 3u) != (u32)pri) {
                continue;
            }
            if (bg == 0 && !is_sub && (dc & REG_GX_DISPCNT_BG02D3D_MASK)) {
                bool extend = PbW2_G3PublishedWide() && ((dc >> 16) & 3) == 1 &&
                              !bg0Brightness && !bg0Alpha &&
                              PbW2_G3PublishedNative();
                if (extend) memcpy(underGeometry, out, sizeof underGeometry);
                s_haveSides = PbW2_G3RasterComposite(out, window, bg_blend.track ? bg_blend.planes : NULL,
                                                     layerAlpha ? bg_blend.colors : NULL, extend ? s_sides : NULL,
                                                     backdrop, extend ? &nativeFrame : NULL, bg_blend.ink);
                if (nativeFrame) {
                    /* Subsequent layers sit in front of the 3D layer. */
                    bg_blend.track = true;
                    memset(bg_blend.planes, 0, sizeof bg_blend.planes);
                    blend = &bg_blend;
                }
            } else {
                draw_bg_layer(regs, dc, is_sub, bg, bgmode, out, blend);
            }
        }
        draw_obj_layer(&objects, out, window, 0, pri, blend);
    }
    if (nativeFrame) {
        for (unsigned y = 0; y < PBW2_NATIVE_TOP_HEIGHT; ++y) {
            unsigned dsRow = ((8 * y + 4) / 10) * 256;
            u16 *row = s_nativeTop + y * PBW2_NATIVE_TOP_WIDTH;
            const u16 *geometry = nativeFrame + y * PBW2_NATIVE_TOP_WIDTH;
            for (unsigned x = 0; x < PBW2_NATIVE_TOP_WIDTH; ++x) {
                u16 pixel = geometry[x];
                if (x >= 40 && x < 360) {
                    unsigned i = dsRow + (8 * (x - 40) + 4) / 10;
                    bool visible = !window || (window[i] & GX_WND_PLANEMASK_BG0);
                    if (bg_blend.planes[i]) row[x] = out[i];
                    else row[x] = visible && (pixel & 0x8000) ? rgb555_to_565(pixel) : underGeometry[i];
                } else {
                    unsigned edge = x < 40 ? 0 : 255;
                    bool visible = !window || (window[dsRow + edge] & GX_WND_PLANEMASK_BG0);
                    row[x] = visible && (pixel & 0x8000) ? rgb555_to_565(pixel) : backdrop;
                }
            }
        }
        PbW2_G3ReleaseNative();
        s_haveNativeTop = true;
        apply_master_bright(s_nativeTop, PBW2_NATIVE_TOP_WIDTH * PBW2_NATIVE_TOP_HEIGHT, reg_u16(regs, 0x6cu));
    }
    if (bg_blend.ink) {
        for (unsigned i = 0; i < 256 * 192; ++i) {
            u32 info = ink[i];
            if (!(info & PBW2_TEXT_NATIVE)) continue;
            int x = PbW2_TextNativeCoordinate(i % 256, (info >> 16) & 31);
            int y = PbW2_TextNativeCoordinate(i / 256, (info >> 21) & 31);
            if (!bg_blend.nativeText || !bg_blend.planes[i] || (unsigned)x >= 320 || (unsigned)y >= 240 ||
                bg_blend.planes[i] != bg_blend.planes[PbW2_TextSourceCoordinate(y) * 256 + PbW2_TextSourceCoordinate(x)])
                ink[i] = PBW2_TEXT_INK;
        }
    }
    if (layerBrightness) apply_layer_brightness(out, blend, bldcnt, factor);
    if (!is_sub) capture_main(out, dc);
    if (!is_sub && s_haveSides) apply_master_bright(s_sides, 64 * 192, reg_u16(regs, 0x6cu));
    apply_master_bright(out, 256 * 192, reg_u16(regs, 0x6cu));
}

static bool sub_inputs(SubInputs *in, const u8 *regs, bool hasText) {
    memset(in, 0, sizeof(*in));
    memcpy(in->regs, regs, sizeof in->regs);
    const u8 *snapshot = (const u8 *)in->regs;
    u32 dc = in->regs[0];
    if ((dc & (REG_GX_DISPCNT_BGMODE_MASK | REG_GX_DISPCNT_MODE_MASK | REG_GX_DISPCNT_BG_MASK |
               REG_GX_DISPCNT_O_MASK | REG_GX_DISPCNT_BG02D3D_MASK | 0x80u)) !=
        (1u << REG_GX_DISPCNT_MODE_SHIFT)) return false;

    unsigned first = 0xffffffffu, end = 0;
    for (int bg = 0; bg < 4; ++bg) {
        if (!(dc & (0x100u << bg))) continue;
        u16 cnt = reg_u16(snapshot, 8 + bg * 2);
        unsigned map = bg_screen_addr(dc, cnt, 1, 0, bg);
        unsigned chars = bg_char_addr(dc, cnt, 1);
        unsigned mapEnd = map + 2048 * (1 + ((cnt >> 14) & 1)) * (1 + (cnt >> 15));
        unsigned charsEnd = chars + ((cnt & 0x80u) ? 65536 : 32768);
        if (chars < first) first = chars;
        if (map < first) first = map;
        if (charsEnd > end) end = charsEnd;
        if (mapEnd > end) end = mapEnd;
    }
    if (end) {
        in->bgAddress = first;
        in->bgBytes = end - first;
        if (in->bgBytes > SUB_BG_CACHE_BYTES) return false;
    }
    in->objAddress = (dc & 0x1000u) ? engine_obj_vram_mapped(1) : 0;
    in->hasText = hasText;
    in->textRevision = hasText ? PbW2_TextVramRevision() : 0;
    return true;
}

static bool same_sub_inputs(const SubInputs *in) {
    return !memcmp(in, &s_subCache.inputs, sizeof(*in)) &&
           !memcmp(s_subCache.palette, live_vram_ptr(HW_DB_BG_PLTT), sizeof s_subCache.palette) &&
           (!in->objAddress || (!memcmp(s_subCache.oam, live_vram_ptr(HW_DB_OAM), sizeof s_subCache.oam) &&
                                !memcmp(s_subCache.obj, live_vram_ptr(in->objAddress), sizeof s_subCache.obj))) &&
           (!in->bgBytes || !memcmp(s_subCache.bg, live_vram_ptr(in->bgAddress), in->bgBytes));
}

static void compose_sub(const u8 *regs, u16 *out, u32 *ink) {
    SubInputs in;
    if (s_subCache.cooldown) {
        --s_subCache.cooldown;
        compose_engine(regs, 1, out, ink);
        return;
    }
    if (!sub_inputs(&in, regs, ink != NULL)) {
        s_subCache.valid = false;
        compose_engine(regs, 1, out, ink);
        return;
    }
    if (s_subCache.valid && same_sub_inputs(&in) &&
        (!ink || in.textRevision == PbW2_TextVramRevision())) {
        s_subCache.misses = 0;
    } else {
        /* Give animated screens a few frames before trying to cache again. */
        if (++s_subCache.misses == 3) {
            s_subCache.misses = 0;
            s_subCache.cooldown = 6;
            s_subCache.valid = false;
            compose_engine(regs, 1, out, ink);
            return;
        }
        s_subCache.inputs = in;
        memcpy(s_subCache.palette, live_vram_ptr(HW_DB_BG_PLTT), sizeof s_subCache.palette);
        if (in.bgBytes) memcpy(s_subCache.bg, live_vram_ptr(in.bgAddress), in.bgBytes);
        if (in.objAddress) {
            memcpy(s_subCache.oam, live_vram_ptr(HW_DB_OAM), sizeof s_subCache.oam);
            memcpy(s_subCache.obj, live_vram_ptr(in.objAddress), sizeof s_subCache.obj);
        }
        if (ink) memset(s_subCache.ink, 0, sizeof s_subCache.ink);
        /* Compose from the captured bytes so the cache always describes its own output. */
        s_subCache.replay = true;
        compose_engine((const u8 *)in.regs, 1, s_subCache.pixels, ink ? s_subCache.ink : NULL);
        s_subCache.replay = false;
        s_subCache.valid = !ink || in.textRevision == PbW2_TextVramRevision();
    }
    memcpy(out, s_subCache.pixels, sizeof s_subCache.pixels);
    if (ink) memcpy(ink, s_subCache.ink, sizeof s_subCache.ink);
}

void PbW2_ComposeScreensWithText(u16 *top_rgb565, u16 *bottom_rgb565, u32 *topInk, u32 *bottomInk) {
    int main_upper = (*(const u16 *)(pbw2_io + 0x304) & 0x8000u) != 0;
    u16 *main_fb;
    u16 *sub_fb;

    s_haveSides = s_haveNativeTop = false;
    s_haveWideTop = s_haveWideBottom = false;
    if (topInk) memset(topInk, 0, 256 * 192 * sizeof(*topInk));
    if (bottomInk) memset(bottomInk, 0, 256 * 192 * sizeof(*bottomInk));
    s_mode5_diag.compose_frames++;
    if (main_upper) {
        main_fb = top_rgb565;
        sub_fb = bottom_rgb565;
    } else {
        main_fb = bottom_rgb565;
        sub_fb = top_rgb565;
    }
    if (main_fb) {
        compose_engine(pbw2_io + 0, 0, main_fb, main_upper ? topInk : bottomInk);
    } else if (reg_GX_DISPCAPCNT & REG_GX_DISPCAPCNT_E_MASK) {
        /* A hidden main LCD still drives the DS capture unit. */
        static u16 captureMain[256 * 192];
        compose_engine(pbw2_io + 0, 0, captureMain, NULL);
    }
    if (sub_fb) {
        if (main_upper) compose_sub(pbw2_io + 0x1000, sub_fb, bottomInk);
        else compose_engine(pbw2_io + 0x1000, 1, sub_fb, topInk);
    }
    apply_wide_layouts(main_upper, main_fb, sub_fb);
}

int PbW2_ComposeScreens(u16 *top_rgb565, u16 *bottom_rgb565) {
    PbW2_ComposeScreensWithText(top_rgb565, bottom_rgb565, NULL, NULL);
    return top_rgb565 != NULL || bottom_rgb565 != NULL;
}

int PbW2_GameDisplayReady(void) {
    u32 dc = *(const u32 *)(pbw2_io + 0);
    u32 sub = *(const u32 *)(pbw2_io + 0x1000);
    u32 layers = (dc | sub) & (0x00000f00u | 0x00001000u);
    u32 main_on = (dc & REG_GX_DISPCNT_MODE_MASK) != 0;
    u32 sub_on = (sub & REG_GX_DISPCNT_MODE_MASK) != 0;

    if (PbW2_VramLoadBytes() < 64u) {
        return 0;
    }
    if (!main_on && !sub_on) {
        return 0;
    }
    if (layers == 0) {
        return 0;
    }
    return 1;
}

void PbW2_GxDisplayTrace(void) {
    static u32 s_frames;
    static int s_last_ready = -1;
    int ready = PbW2_GameDisplayReady();

    s_frames++;
    if ((s_frames % 60u) != 0u && ready == s_last_ready) {
        return;
    }
    {
        u32 dc = *(const u32 *)(pbw2_io + 0);
        u32 sub = *(const u32 *)(pbw2_io + 0x1000);
        u32 vramcnt = *(const u32 *)(pbw2_io + 0x240);
        u32 pwr = *(const u32 *)(pbw2_io + 0x304);
        Log_Line("GX",
                 "disp trace vram=%u dc=0x%08x sub=0x%08x bg0-3cnt=0x%04x%04x mbright=0x%04x vramcnt=0x%08x pow=0x%04x ready=%d",
                 (unsigned)PbW2_VramLoadBytes(), (unsigned)dc, (unsigned)sub,
                 (unsigned)*(const u16 *)(pbw2_io + 0x08), (unsigned)*(const u16 *)(pbw2_io + 0x0e),
                 (unsigned)reg_u16(pbw2_io, 0x6cu), (unsigned)vramcnt, (unsigned)(pwr & 0xffffu),
                 ready);
    }
    s_last_ready = ready;
}

#endif
