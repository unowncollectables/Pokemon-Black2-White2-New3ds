/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Software DS 2D compositor. Adapted from widecomp.c (GPL-3.0, 3ds-platinum). */
#include "ds2d_compose.h"

#if defined(PBW2_LINK_GAME)

#include "log.h"
#include "nitro/hw.h"
#include "nitro/gx.h"
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

static const u8 *vram_ptr(u32 addr) {
    return (const u8 *)(void *)PbW2_VramMap((void *)(uintptr_t)addr);
}

static const u16 *engine_bg_pltt(int is_sub) {
    if (is_sub) {
        return (const u16 *)(void *)PbW2_VramMap((void *)HW_DB_BG_PLTT);
    }
    return (const u16 *)(void *)PbW2_VramMap((void *)HW_BG_PLTT);
}

static const u16 *engine_obj_pltt(int is_sub) {
    if (is_sub) {
        return (const u16 *)(void *)PbW2_VramMap((void *)HW_DB_OBJ_PLTT);
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

static u32 bg_screen_addr(u32 dc, u16 cnt, int is_sub, u32 bgmode, int bg) {
    u32 base = engine_bg_vram_base(is_sub);
    u32 screen_base = ((u32)cnt >> REG_G2_BG0CNT_SCREENBASE_SHIFT) & 0x1fu;
    u32 off;

    if (bg >= 2 && bgmode >= GX_BGMODE_3 && bgmode <= GX_BGMODE_5 && (cnt & REG_G2_BG0CNT_COLORMODE_MASK)) {
        off = screen_base * 0x4000u;
        if (!is_sub) {
            return main_bg_linear_addr(off);
        }
        return base + off;
    }
    off = screen_base * 0x800u;
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

static void read_bg_affine(const u8 *regs, int bg, s16 *pa, s16 *pb, s16 *pc, s16 *pd, s32 *rx, s32 *ry) {
    u32 b = (bg == 2) ? 0x20u : 0x30u;
    u32 m01 = reg_u32(regs, b);
    u32 m23 = reg_u32(regs, b + 4u);

    *pa = (s16)(m01 & 0xffffu);
    *pb = (s16)(m01 >> 16);
    *pc = (s16)(m23 & 0xffffu);
    *pd = (s16)(m23 >> 16);
    *rx = affine_ref28(reg_u32(regs, b + 8u));
    *ry = affine_ref28(reg_u32(regs, b + 12u));
    if (*pa == 0 && *pd == 0) {
        *pa = (s16)0x0100;
        *pd = (s16)0x0100;
    }
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
    u32 base = engine_bg_vram_base(is_sub);
    u32 off = ((u32)(cnt >> 2) & 0xfu) * 0x4000u;
    if (!is_sub) {
        off += ((dc >> REG_GX_DISPCNT_BGCHAROFFSET_SHIFT) & 7u) * 0x10000u;
    }
    return base + off;
}

static void compose_text_bg(const u8 *regs, u32 dc, int is_sub, int bg, u32 bgmode, u16 *out) {
    u16 cnt = reg_u16(regs, 8 + bg * 2);
    int pw = ((cnt >> 14) & 1) ? 64 : 32;
    int ph = ((cnt >> 14) & 2) ? 64 : 32;
    u32 mapw = (u32)pw * 8;
    const u16 *map = (const u16 *)(void *)vram_ptr(bg_screen_addr(dc, cnt, is_sub, bgmode, bg));
    const u8 *chars = vram_ptr(bg_char_addr(dc, cnt, is_sub));
    const u16 *pltt = bg_layer_pltt(dc, cnt, is_sub, bg);
    u32 hofs_base = reg_u16(regs, 0x10u + (u32)bg * 4u) & 0x1ffu;
    u32 vofs_base = reg_u16(regs, 0x12u + (u32)bg * 4u) & 0x1ffu;
    int y;
    int x;

    if ((dc & (0x100u << bg)) == 0) {
        return;
    }
    if (bg == 0 && (dc & 8u)) {
        return;
    }

    for (y = 0; y < 192; y++) {
        u32 vofs = (y + vofs_base) & (ph * 8 - 1);
        u32 row = vofs / 8;
        u32 ty = vofs & 7;
        for (x = 0; x < 256; x++) {
            u32 hofs = (x + hofs_base) & (pw * 8 - 1);
            u32 col = hofs / 8;
            u32 tx = hofs & 7;
            u16 sc = map[row * (mapw / 8) + col];
            u32 tile = sc & 0x3ffu;
            u32 pal = sc >> 12;
            u32 flip_h = sc & 0x0400u;
            u32 flip_v = sc & 0x0800u;
            u32 px;
            u8 nib;
            u8 pix;

            if (flip_h) {
                tx = 7 - tx;
            }
            if (flip_v) {
                ty = 7 - ty;
            }
            if (cnt & 0x80u) {
                pix = chars[tile * 64u + ty * 8u + tx];
            } else {
                px = tx >> 1;
                nib = chars[tile * 32u + ty * 4u + px];
                pix = (tx & 1) ? (nib >> 4) : (nib & 0x0f);
            }
            if (pix == 0) {
                continue;
            }
            if (cnt & 0x80u) {
                out[y * 256 + x] = rgb555_to_565(pltt[pix]);
            } else {
                out[y * 256 + x] = rgb555_to_565(pltt[pal * 16u + pix]);
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
    u8 vram_on;
    u8 vram_delay;
} s_mode5_diag;

static u32 count_opaque16_at_base(u16 cnt, int bw, int bh) {
    u32 screen_base = ((u32)cnt >> REG_G2_BG0CNT_SCREENBASE_SHIFT) & 0x1fu;
    u32 addr = main_bg_linear_addr(screen_base * 0x4000u);
    const u16 *p = (const u16 *)(void *)vram_ptr(addr);
    u32 opaque = 0;
    u32 i;
    u32 pixels = (u32)bw * (u32)bh;

    for (i = 0; i < pixels; i++) {
        if ((p[i] & 0x7fffu) != 0) {
            opaque++;
        }
    }
    return opaque;
}

static void log_mode5_bitmap(int bg, u16 cnt, u32 bmp_addr, int is16, u32 opaque, const char *phase) {
    u32 screen_base = ((u32)cnt >> REG_G2_BG0CNT_SCREENBASE_SHIFT) & 0x1fu;
    const u16 *samp;

    samp = NULL;
    if (is16) {
        samp = (const u16 *)(void *)vram_ptr(bmp_addr);
    }
    Log_Line("GX",
             "mode5-%s bg%d cnt=0x%04x prio=%u is16=%d screen=%u addr=0x%08x vram=%u samp=0x%04x,0x%04x,0x%04x,"
             "0x%04x opaque=%u",
             phase, bg, (unsigned)cnt, (unsigned)(cnt & 3u), is16, (unsigned)screen_base, (unsigned)bmp_addr,
             (unsigned)PbW2_VramLoadBytes(), samp ? (unsigned)samp[0] : 0u, samp ? (unsigned)samp[1] : 0u,
             samp ? (unsigned)samp[2] : 0u, samp ? (unsigned)samp[3] : 0u, (unsigned)opaque);
}

static void maybe_log_mode5(const u8 *regs, u32 dc, u32 bgmode, int bg, u16 cnt, u32 bmp_addr, int is16, int bw,
                            int bh) {
    u32 opaque;
    u16 cnt3;
    u32 addr3;

    if (bgmode != GX_BGMODE_5 || bg != 2) {
        return;
    }
    opaque = is16 ? count_opaque16_at_base(cnt, bw, bh) : 0u;
    cnt3 = reg_u16(regs, 0x0e);
    addr3 = bg_screen_addr(dc, cnt3, 0, bgmode, 3);
    if (!s_mode5_diag.logged_at_on) {
        if (!s_mode5_diag.vram_on) {
            log_vramcnt_banks("on");
            s_mode5_diag.vram_on = 1;
        }
        s_mode5_diag.mode5_on_frame = s_mode5_diag.compose_frames;
        s_mode5_diag.logged_at_on = 1;
        log_mode5_bitmap(2, cnt, bmp_addr, is16, opaque, "on");
        log_mode5_bitmap(3, cnt3, addr3, 1, count_opaque16_at_base(cnt3, 256, 256), "on");
        return;
    }
    if (!s_mode5_diag.logged_delayed && s_mode5_diag.compose_frames >= s_mode5_diag.mode5_on_frame + 60u) {
        if (!s_mode5_diag.vram_delay) {
            log_vramcnt_banks("+60");
            s_mode5_diag.vram_delay = 1;
        }
        s_mode5_diag.logged_delayed = 1;
        log_mode5_bitmap(2, cnt, bmp_addr, is16, opaque, "+60");
        log_mode5_bitmap(3, cnt3, addr3, 1, count_opaque16_at_base(cnt3, 256, 256), "+60");
    }
}

static u16 sample_bitmap16(const u8 *regs, int bg, u16 cnt, int is_sub, int bw, int bh, int x, int y) {
    s16 pa;
    s16 pb;
    s16 pc;
    s16 pd;
    s32 rx;
    s32 ry;
    s32 tx;
    s32 ty;
    u32 screen_base;
    u32 byte_off;
    u32 addr;
    int wrap;
    const u16 *p;
    u16 c;

    read_bg_affine(regs, bg, &pa, &pb, &pc, &pd, &rx, &ry);
    tx = (s32)((((s64)rx + (s64)pa * x + (s64)pb * y) >> 8));
    ty = (s32)((((s64)ry + (s64)pc * x + (s64)pd * y) >> 8));
    wrap = (cnt & (1u << REG_G2_BG2CNT_AREAOVER_SHIFT)) == 0;
    if (wrap) {
        tx &= (s32)(bw - 1);
        ty &= (s32)(bh - 1);
    } else if (tx < 0 || ty < 0 || tx >= bw || ty >= bh) {
        return 0;
    }
    screen_base = ((u32)cnt >> REG_G2_BG0CNT_SCREENBASE_SHIFT) & 0x1fu;
    byte_off = screen_base * 0x4000u + ((u32)ty * (u32)bw + (u32)tx) * 2u;
    addr = is_sub ? (engine_bg_vram_base(1) + byte_off) : main_bg_linear_addr(byte_off);
    p = (const u16 *)(void *)vram_ptr(addr);
    c = p[0];
    if ((c & 0x7fffu) == 0) {
        return 0;
    }
    return rgb555_to_565(c & 0x7fffu);
}

static u16 sample_bitmap8(const u8 *regs, int bg, u16 cnt, int is_sub, int bw, int bh, int x, int y,
                          const u16 *pltt) {
    s16 pa;
    s16 pb;
    s16 pc;
    s16 pd;
    s32 rx;
    s32 ry;
    s32 tx;
    s32 ty;
    u32 screen_base;
    u32 byte_off;
    u32 addr;
    int wrap;
    const u8 *p;
    u8 ci;

    read_bg_affine(regs, bg, &pa, &pb, &pc, &pd, &rx, &ry);
    tx = (s32)((((s64)rx + (s64)pa * x + (s64)pb * y) >> 8));
    ty = (s32)((((s64)ry + (s64)pc * x + (s64)pd * y) >> 8));
    wrap = (cnt & (1u << REG_G2_BG2CNT_AREAOVER_SHIFT)) == 0;
    if (wrap) {
        tx &= (s32)(bw - 1);
        ty &= (s32)(bh - 1);
    } else if (tx < 0 || ty < 0 || tx >= bw || ty >= bh) {
        return 0;
    }
    screen_base = ((u32)cnt >> REG_G2_BG0CNT_SCREENBASE_SHIFT) & 0x1fu;
    byte_off = screen_base * 0x4000u + (u32)ty * (u32)bw + (u32)tx;
    addr = is_sub ? (engine_bg_vram_base(1) + byte_off) : main_bg_linear_addr(byte_off);
    p = vram_ptr(addr);
    ci = p[0];
    if (ci == 0) {
        return 0;
    }
    return rgb555_to_565(pltt[ci]);
}

static void compose_bitmap_bg(const u8 *regs, u32 dc, int is_sub, int bg, u32 bgmode, u16 *out) {
    u16 cnt = reg_u16(regs, 8 + bg * 2);
    const u16 *pltt;
    u32 bmp_addr;
    int bw;
    int bh;
    int is16;
    int y;
    int x;

    if ((dc & (0x100u << bg)) == 0) {
        return;
    }
    if ((cnt & REG_G2_BG0CNT_COLORMODE_MASK) == 0) {
        return;
    }
    /* bit7=1 bitmap; bit2=1 direct color 16bpp (ABGR1555, bit15 opaque). */
    is16 = (cnt & 0x0004u) != 0;
    bmp_addr = bg_screen_addr(dc, cnt, is_sub, bgmode, bg);
    pltt = bg_layer_pltt(dc, cnt, is_sub, bg);
    bitmap_dims(cnt, &bw, &bh);

    maybe_log_mode5(regs, dc, bgmode, bg, cnt, bmp_addr, is16, bw, bh);

    if (is16) {
        for (y = 0; y < 192; y++) {
            for (x = 0; x < 256; x++) {
                u16 pix = sample_bitmap16(regs, bg, cnt, is_sub, bw, bh, x, y);

                if (pix != 0) {
                    out[y * 256 + x] = pix;
                }
            }
        }
    } else {
        for (y = 0; y < 192; y++) {
            for (x = 0; x < 256; x++) {
                u16 pix = sample_bitmap8(regs, bg, cnt, is_sub, bw, bh, x, y, pltt);

                if (pix != 0) {
                    out[y * 256 + x] = pix;
                }
            }
        }
    }
}

static void draw_bg_layer(const u8 *regs, u32 dc, int is_sub, int bg, u32 bgmode, u16 *out) {
    u16 cnt = reg_u16(regs, 8 + bg * 2);

    if (bg >= 2 && bgmode >= 3 && (cnt & REG_G2_BG0CNT_COLORMODE_MASK)) {
        compose_bitmap_bg(regs, dc, is_sub, bg, bgmode, out);
    } else {
        compose_text_bg(regs, dc, is_sub, bg, bgmode, out);
    }
}

static void draw_obj_layer(const u8 *regs, u32 dc, int is_sub, u16 *out) {
    static const u8 spw[3][4] = {{8, 16, 32, 64}, {16, 32, 32, 64}, {8, 8, 16, 32}};
    static const u8 sph[3][4] = {{8, 16, 32, 64}, {8, 8, 16, 32}, {16, 32, 32, 64}};
    u32 oam_base = is_sub ? HW_DB_OAM : HW_OAM;
    const u16 *oam = (const u16 *)(void *)PbW2_VramMap((void *)(uintptr_t)oam_base);
    const u8 *ovram = vram_ptr(engine_obj_vram_base(is_sub));
    const u16 *pltt = (dc & REG_GX_DISPCNT_O_MASK) != 0 ? (const u16 *)(void *)vram_ptr(HW_LCDC_VRAM_G)
                                                      : engine_obj_pltt(is_sub);
    u32 obj1d = dc & 0x10u;
    u32 obj1d_shift = (dc >> 20) & 3u;
    int pri;
    int i;

    if ((dc & 0x1000u) == 0) {
        return;
    }
    for (pri = 3; pri >= 0; pri--) {
        for (i = 127; i >= 0; i--) {
            const u16 *a = oam + i * 4;
            u32 shape = a[0] >> 14;
            u32 size = a[1] >> 14;
            int width;
            int height;
            int xs;
            int ypos;
            int xo;
            int yo;

            if (shape > 2) {
                continue;
            }
            if ((a[0] & 0x300) == 0x200) {
                continue;
            }
            if (((a[2] >> 10) & 3) != (u32)pri) {
                continue;
            }
            if (a[0] & 0x100) {
                continue;
            }
            width = spw[shape][size];
            height = sph[shape][size];
            xs = a[1] & 0x1ff;
            ypos = a[0] & 255;
            if (xs >= 256) {
                xs -= 512;
            }
            for (yo = 0; yo < height; yo++) {
                int y = ypos + yo;
                if (y < 0 || y >= 192) {
                    continue;
                }
                for (xo = 0; xo < width; xo++) {
                    int x = xs + xo;
                    u32 tile = a[2] & 0x3ff;
                    u32 bpp8 = (a[0] & 0x2000) != 0;
                    u32 addr;
                    u32 px;
                    u32 py;
                    u8 nib;
                    u32 ci;

                    if (x < 0 || x >= 256) {
                        continue;
                    }
                    px = (a[1] & 0x1000) ? (width - 1 - xo) : xo;
                    py = (a[1] & 0x2000) ? (height - 1 - yo) : yo;
                    if (obj1d) {
                        tile <<= obj1d_shift;
                        addr = tile + (((u32)py >> 3) * (width >> 3) << (bpp8 ? 1 : 0));
                    } else {
                        addr = tile + ((py >> 3) * 0x20);
                    }
                    addr <<= 5;
                    if (bpp8) {
                        addr += ((py & 7) << 3) + ((px & ~7) << 3) + (px & 7);
                        ci = ovram[addr & 0x3ffffu];
                        if (ci == 0) {
                            continue;
                        }
                        out[y * 256 + x] = rgb555_to_565(pltt[ci]);
                    } else {
                        addr += ((py & 7) << 2) + ((px & ~7) << 2) + ((px & 7) >> 1);
                        nib = ovram[addr & 0x3ffffu];
                        ci = (px & 1) ? (nib >> 4) : (nib & 0x0f);
                        if (ci == 0) {
                            continue;
                        }
                        out[y * 256 + x] = rgb555_to_565(pltt[(a[2] >> 12) * 16u + ci]);
                    }
                }
            }
        }
    }
}

static void apply_master_bright(u16 *fb, u16 reg) {
    unsigned mode = (reg >> 14) & 3u;
    unsigned amt = reg & 0x1fu;
    u32 i;
    u32 n = 256u * 192u;

    if (mode == 0 || amt == 0) {
        return;
    }
    for (i = 0; i < n; i++) {
        u16 p = fb[i];
        u32 r = (p >> 11) & 0x1f;
        u32 g = (p >> 6) & 0x1f;
        u32 b = p & 0x1f;
        if (mode == 2) {
            r = r + amt > 31 ? 31 : r + amt;
            g = g + amt > 31 ? 31 : g + amt;
            b = b + amt > 31 ? 31 : b + amt;
        } else if (mode == 3) {
            r = r > amt ? r - amt : 0;
            g = g > amt ? g - amt : 0;
            b = b > amt ? b - amt : 0;
        }
        fb[i] = (u16)((r << 11) | (g << 6) | b);
    }
}

static void compose_engine(const u8 *regs, int is_sub, u16 *out) {
    u32 dc = reg_u32(regs, 0);
    u32 bgmode = dc & REG_GX_DISPCNT_BGMODE_MASK;
    u16 backdrop;
    int pri;
    int bg;
    u32 i;

    if (dc & 0x80u) {
        memset(out, 0, 256u * 192u * 2u);
        return;
    }
    if ((dc & REG_GX_DISPCNT_MODE_MASK) == 0) {
        memset(out, 0, 256u * 192u * 2u);
        return;
    }
    backdrop = rgb555_to_565(engine_bg_pltt(is_sub)[0]);
    for (i = 0; i < 256u * 192u; i++) {
        out[i] = backdrop;
    }
    for (pri = 3; pri >= 0; pri--) {
        for (bg = 0; bg < 4; bg++) {
            u16 cnt = reg_u16(regs, 8 + bg * 2);
            if ((dc & (0x100u << bg)) == 0) {
                continue;
            }
            if ((cnt & 3u) != (u32)pri) {
                continue;
            }
            draw_bg_layer(regs, dc, is_sub, bg, bgmode, out);
        }
    }
    draw_obj_layer(regs, dc, is_sub, out);
    apply_master_bright(out, reg_u16(regs, 0x6cu));
}

int PbW2_ComposeScreens(u16 *top_rgb565, u16 *bottom_rgb565) {
    int main_upper = (*(const u16 *)(pbw2_io + 0x304) & 0x8000u) != 0;
    u16 *main_fb;
    u16 *sub_fb;

    s_mode5_diag.compose_frames++;
    if (main_upper) {
        main_fb = top_rgb565;
        sub_fb = bottom_rgb565;
    } else {
        main_fb = bottom_rgb565;
        sub_fb = top_rgb565;
    }
    if (main_fb) {
        compose_engine(pbw2_io + 0, 0, main_fb);
    }
    if (sub_fb) {
        compose_engine(pbw2_io + 0x1000, 1, sub_fb);
    }
    return (top_rgb565 != NULL || bottom_rgb565 != NULL);
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
    static int s_logged_ready;

    s_frames++;
    if ((s_frames % 60u) != 0u && s_logged_ready) {
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
                 PbW2_GameDisplayReady());
    }
    if (PbW2_GameDisplayReady()) {
        s_logged_ready = 1;
    }
}

#endif