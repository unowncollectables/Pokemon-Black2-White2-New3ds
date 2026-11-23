/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "present.h"

#include <3ds.h>

void Pbw2_GspVblank(void) {
    gspWaitForVBlank();
}

#include "log.h"
#include "pbw2_port.h"
#if defined(PBW2_LINK_GAME)
#include "ds2d_compose.h"
#include "game_present.h"
#include "vram_host.h"
#endif

#include <3ds.h>
#include <3ds/services/gspgpu.h>
#include <string.h>

static uint32_t s_flags;
static bool s_logged;

void Present_Commit(bool top, bool bottom) {
    s_flags = (top ? PBW2_SCREEN_TOP : 0u) | (bottom ? PBW2_SCREEN_BOTTOM : 0u);
}

uint32_t Present_Flags(void) {
    return s_flags;
}

void GxShim_Present(uint32_t flags) {
#if defined(PBW2_LINK_GAME)
    static u16 s_top[256 * 192];
    static u16 s_bottom[256 * 192];
    extern void pbw2_game_notify_present(void);

    if (PbW2_VramHostReady()) {
        (void)PbW2_ComposeScreens(s_top, s_bottom);
        if ((flags & PBW2_SCREEN_TOP) != 0) {
            Present_BlitRGB565(GFX_TOP, s_top, 256, 192);
        }
        if ((flags & PBW2_SCREEN_BOTTOM) != 0) {
            Present_BlitRGB565(GFX_BOTTOM, s_bottom, 256, 192);
        }
        if (PbW2_GameDisplayReady()) {
            pbw2_game_notify_present();
        }
    }
#endif
    if (s_logged) {
        return;
    }
    s_logged = true;
    Log_Line("GX", "present flags top=%d bottom=%d (sharp text uses these)",
             (flags & PBW2_SCREEN_TOP) != 0, (flags & PBW2_SCREEN_BOTTOM) != 0);
}

/* libctru names the GSP buffer width 240 and height 400 (top). On the card
 * that is a column-major landscape picture: landscape sx maps to column, sy to
 * row within the column flipped like consoleDrawChar (sx*stride+(stride-1-sy)). */
static u16 *landscape_fb(gfxScreen_t screen, u16 *stride, u16 *land_w) {
    u16 land_h = 0;
    u16 *fb = (u16 *)gfxGetFramebuffer(screen, GFX_LEFT, stride, &land_h);
    if (!fb || *stride == 0 || land_h == 0) {
        return NULL;
    }
    *land_w = land_h;
    return fb;
}

static void clear_landscape(u16 *fb, u16 stride, u16 land_w, u16 land_h) {
    u32 y;
    u32 x;
    for (y = 0; y < land_h; y++) {
        for (x = 0; x < land_w; x++) {
            u32 idx = x * (u32)stride + ((u32)stride - 1u - y);
            if (idx < (u32)land_w * (u32)stride) {
                fb[idx] = 0;
            }
        }
    }
}

void Present_BlitRGB565(gfxScreen_t screen, const uint16_t *pic, int pic_w, int pic_h) {
    u16 stride = 0;
    u16 land_w = 0;
    u16 *fb;
    int ox;
    int oy;
    int y;
    int x;
    u32 fb_pixels;

    if (!pic || pic_w <= 0 || pic_h <= 0) {
        return;
    }
    fb = landscape_fb(screen, &stride, &land_w);
    if (!fb) {
        return;
    }
    /* Landscape height is 240; landscape width is 400 (top) or 320 (bottom). */
    if (stride != GSP_SCREEN_WIDTH || land_w < (u16)pic_w) {
        return;
    }
    ox = ((int)land_w - pic_w) / 2;
    oy = ((int)stride - pic_h) / 2;
    fb_pixels = (u32)land_w * (u32)stride;
    clear_landscape(fb, stride, land_w, stride);

    for (y = 0; y < pic_h; y++) {
        for (x = 0; x < pic_w; x++) {
            int sx = ox + x;
            int sy = oy + y;
            u32 idx;
            if (sx < 0 || sy < 0 || sx >= (int)land_w || sy >= (int)stride) {
                continue;
            }
            idx = (u32)sx * (u32)stride + ((u32)stride - 1u - (u32)sy);
            if (idx < fb_pixels) {
                fb[idx] = pic[(u32)y * (u32)pic_w + (u32)x];
            }
        }
    }
}

void Present_DrawTitle(uint32_t flags, const uint16_t *top, const uint16_t *bottom) {
    if ((flags & PBW2_SCREEN_TOP) != 0 && top != NULL) {
        Present_BlitRGB565(GFX_TOP, top, 256, 192);
    }
    if ((flags & PBW2_SCREEN_BOTTOM) != 0 && bottom != NULL) {
        Present_BlitRGB565(GFX_BOTTOM, bottom, 256, 192);
    }
}