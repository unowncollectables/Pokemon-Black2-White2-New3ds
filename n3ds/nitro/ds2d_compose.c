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

#define PBW2_SCR_W 256
#define PBW2_SCR_H 192
static u16 s_powcnt_swap_buf[PBW2_SCR_W * PBW2_SCR_H];

static u16 rgb555_to_565(u16 c) {
    u32 r = c & 0x1f;
    u32 g = (c >> 5) & 0x1f;
    u32 b = (c >> 10) & 0x1f;
    return (u16)((r << 11) | (g << 6) | b);
}

static const u16 *bg_palette(void) {
    return (const u16 *)(void *)PbW2_VramMap((void *)HW_BG_PLTT);
}

static const u16 *obj_palette(void) {
    return (const u16 *)(void *)PbW2_VramMap((void *)HW_OBJ_PLTT);
}

static const u8 *vram_ptr(u32 addr) {
    return (const u8 *)(void *)PbW2_VramMap((void *)(uintptr_t)addr);
}

static u32 bg_char_base(const u8 *regs) {
    u32 dc = *(const u32 *)(regs + 0);
    return 0x06000000u + ((dc >> 24) & 7u) * 0x10000u;
}

static u32 bg_screen_base(u32 cnt) {
    return 0x06000000u + ((cnt >> 8) & 0x1fu) * 0x800u;
}

static u32 obj_vram_base(void) {
    return 0x06400000u;
}

static u16 reg_u16(const u8 *regs, u32 off) {
    return *(const u16 *)(regs + off);
}

static void compose_text_bg(const u8 *regs, int bg, u16 *out) {
    u32 dc = *(const u32 *)(regs + 0);
    u16 cnt = *(const u16 *)(regs + 8 + bg * 2);
    int pw = ((cnt >> 14) & 1) ? 64 : 32;
    int ph = ((cnt >> 14) & 2) ? 64 : 32;
    u32 mapw = (u32)pw * 8;
    const u16 *map = (const u16 *)(void *)vram_ptr(bg_screen_base(cnt));
    const u8 *chars = vram_ptr(bg_char_base(regs) + ((cnt >> 2) & 0xf) * 0x4000u);
    const u16 *pltt = bg_palette();
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
                pix = (chars[tile * 32u + ty * 4u + px] >> ((tx & 1) ? 0 : 4)) & 0x0f;
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

static void draw_obj_layer(const u8 *regs, u16 *out) {
    static const u8 spw[3][4] = {{8, 16, 32, 64}, {16, 32, 32, 64}, {8, 8, 16, 32}};
    static const u8 sph[3][4] = {{8, 16, 32, 64}, {8, 8, 16, 32}, {16, 32, 32, 64}};
    u32 dc = *(const u32 *)(regs + 0);
    const u16 *oam = (const u16 *)(void *)PbW2_VramMap((void *)HW_OAM);
    const u8 *ovram = vram_ptr(obj_vram_base());
    const u16 *pltt = obj_palette();
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
                    if (dc & 0x10u) {
                        tile <<= (dc >> 20) & 3u;
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

static void compose_engine(const u8 *regs, u16 *out) {
    u32 dc = *(const u32 *)(regs + 0);
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
    backdrop = rgb555_to_565(bg_palette()[0]);
    for (i = 0; i < 256u * 192u; i++) {
        out[i] = backdrop;
    }
    for (pri = 3; pri >= 0; pri--) {
        for (bg = 0; bg < 4; bg++) {
            u16 cnt = *(const u16 *)(regs + 8 + bg * 2);
            if ((dc & (0x100u << bg)) == 0) {
                continue;
            }
            if ((cnt & 3u) != (u32)pri) {
                continue;
            }
            compose_text_bg(regs, bg, out);
        }
    }
    draw_obj_layer(regs, out);
    apply_master_bright(out, reg_u16(regs, 0x6cu));
}

int PbW2_ComposeScreens(u16 *top_rgb565, u16 *bottom_rgb565) {
    int swap = (*(const u16 *)(pbw2_io + 0x304) & 0x8000u) != 0;

    if (top_rgb565) {
        compose_engine(pbw2_io + 0, top_rgb565);
    }
    if (bottom_rgb565) {
        compose_engine(pbw2_io + 0x1000, bottom_rgb565);
    }
    if (swap && top_rgb565 && bottom_rgb565) {
        u32 fb_bytes = PBW2_SCR_W * PBW2_SCR_H * (u32)sizeof(u16);
        memcpy(s_powcnt_swap_buf, top_rgb565, fb_bytes);
        memcpy(top_rgb565, bottom_rgb565, fb_bytes);
        memcpy(bottom_rgb565, s_powcnt_swap_buf, fb_bytes);
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