/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "present.h"

#include <3ds.h>

void Pbw2_GspVblank(void) {
    gspWaitForVBlank();
}

#include "log.h"
#include "pbw2_port.h"
#include "settings.h"
#if defined(PBW2_LINK_GAME)
#include "ds2d_compose.h"
#include "game_present.h"
#include "g3_raster.h"
#include "vram_host.h"
#endif

#include <3ds.h>
#include <3ds/services/gspgpu.h>
#include <string.h>

static uint32_t s_flags;
static bool s_logged;
enum { GAME_WIDTH = 256, GAME_HEIGHT = 192 };

typedef struct {
    unsigned x, y, width, height;
} GameRect;

static GameRect game_rect(gfxScreen_t screen, unsigned lcdWidth, unsigned lcdHeight, uint32_t options) {
    unsigned width = GAME_WIDTH;
    unsigned height = GAME_HEIGHT;
    if ((options & DISPLAY_LCD_FIT) || (screen == GFX_TOP && (options & (DISPLAY_TOP_FIT | DISPLAY_WIDE_FIELD)))) {
        width = lcdHeight * GAME_WIDTH / GAME_HEIGHT;
        height = lcdHeight;
    }
    return (GameRect){(lcdWidth - width) / 2, (lcdHeight - height) / 2, width, height};
}

void Present_Commit(bool top, bool bottom) {
    s_flags = (top ? PBW2_SCREEN_TOP : 0u) | (bottom ? PBW2_SCREEN_BOTTOM : 0u);
}

uint32_t Present_Flags(void) {
    return s_flags;
}

#if defined(PBW2_LINK_GAME)
static void draw_scaled(gfxScreen_t screen, const u16 *src, unsigned sourceWidth, uint32_t options);
#endif

void GxShim_Present(uint32_t flags) {
#if defined(PBW2_LINK_GAME)
    static u16 s_top[256 * 192];
    static u16 s_bottom[256 * 192];
    if (PbW2_VramHostReady()) {
        PbW2_GxDisplayTrace();
        /* Readiness gates the first game frame. Later frames can intentionally blank the LCDs. */
        if (PbW2_GameHasPresented() || PbW2_GameDisplayReady()) {
            u16 *top = (flags & PBW2_SCREEN_TOP) ? s_top : NULL;
            u16 *bottom = (flags & PBW2_SCREEN_BOTTOM) && !Settings_Open() ? s_bottom : NULL;
            (void)PbW2_ComposeScreens(top, bottom);
            uint32_t options = Settings_DisplayFlags();
            if (top) {
                const u16 *sides = PbW2_TopSides();
                const u16 *native = PbW2_NativeTop();
                if (native && (options & DISPLAY_WIDE_FIELD)) {
                    Present_BlitRGB565(GFX_TOP, native, PBW2_NATIVE_TOP_WIDTH, PBW2_NATIVE_TOP_HEIGHT);
                } else if (sides && (options & DISPLAY_WIDE_FIELD)) {
                    static u16 wide[320 * GAME_HEIGHT];
                    for (unsigned y = 0; y < GAME_HEIGHT; ++y) {
                        memcpy(wide + y * 320, sides + y * 64, 32 * sizeof(u16));
                        memcpy(wide + y * 320 + 32, top + y * GAME_WIDTH, GAME_WIDTH * sizeof(u16));
                        memcpy(wide + y * 320 + 288, sides + y * 64 + 32, 32 * sizeof(u16));
                    }
                    draw_scaled(GFX_TOP, wide, 320, options);
                } else Present_DrawGame(GFX_TOP, top, options);
            }
            if (bottom) Present_DrawGame(GFX_BOTTOM, bottom, options);
            pbw2_game_notify_present();
        }
    }
#endif
    if (s_logged) {
        return;
    }
    s_logged = true;
    Log_Line("GX", "present flags top=%d bottom=%d",
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

static void clear_side_margins(u16 *fb, unsigned stride, unsigned width, unsigned left, unsigned right) {
    if (left) memset(fb, 0, (size_t)stride * left * sizeof(*fb));
    if (right < width) memset(fb + right * stride, 0, (size_t)stride * (width - right) * sizeof(*fb));
}

#if defined(PBW2_LINK_GAME)
bool Present_TouchToGame(uint16_t px, uint16_t py, uint16_t *x, uint16_t *y) {
    GameRect rect = game_rect(GFX_BOTTOM, 320, 240, Settings_DisplayFlags());
    if (px < rect.x || px >= rect.x + rect.width || py < rect.y || py >= rect.y + rect.height) return false;
    *x = (2 * (px - rect.x) + 1) * GAME_WIDTH / (2 * rect.width);
    *y = (2 * (py - rect.y) + 1) * GAME_HEIGHT / (2 * rect.height);
    return true;
}

static u16 mix565(u16 a, u16 b, unsigned weight) {
    if (!weight) return a;
    /* 5-bit fixed point for each packed color lane. */
    unsigned other = 32 - weight;
    u32 rb = ((a & 0xf81f) * other + (b & 0xf81f) * weight) >> 5;
    u32 g = ((a & 0x07e0) * other + (b & 0x07e0) * weight) >> 5;
    return (rb & 0xf81f) | (g & 0x07e0);
}

static unsigned sample_axis(unsigned pixel, unsigned output, unsigned input, bool sharp) {
    int fixed = ((2 * pixel + 1) * input * 16) / output - 16;
    /* Linear filtering of a virtual nearest-neighbor 2x image. Only the
     * middle half of each source-pixel interval blends into its neighbor. */
    if (sharp && fixed >= 0) {
        int fraction = (fixed & 31) * 2 - 16;
        if (fraction < 0) fraction = 0;
        if (fraction > 32) fraction = 32;
        fixed = (fixed & ~31) + fraction;
    }
    if (fixed < 0) return 0;
    if (fixed > (int)(input - 1) * 32) return (input - 1) * 32;
    return fixed;
}

static void draw_scaled(gfxScreen_t screen, const u16 *src, unsigned sourceWidth, uint32_t options) {
    u16 stride, width;
    u16 *fb = landscape_fb(screen, &stride, &width);
    if (!fb || stride != 240 || width < 320) return;

    GameRect rect = game_rect(screen, width, stride, options);
    if (sourceWidth == 320) rect = (GameRect){0, 0, width, stride};
    /* Scaled modes fill the LCD vertically. Only the side margins need clearing. */
    clear_side_margins(fb, stride, width, rect.x, rect.x + rect.width);
    bool sharp = (options & DISPLAY_SHARP_PIXELS) != 0;
    unsigned sampleY[240];
    for (unsigned y = 0; y < rect.height; ++y) {
        sampleY[y] = sample_axis(y, rect.height, GAME_HEIGHT, sharp);
    }
    for (unsigned x = 0; x < rect.width; ++x) {
        unsigned sx = sample_axis(x, rect.width, sourceWidth, sharp);
        u16 *out = fb + (rect.x + x) * stride + stride - 1 - rect.y;
        unsigned xi = sx >> 5;
        unsigned xn = xi + (xi < sourceWidth - 1);
        for (unsigned y = 0; y < rect.height; ++y) {
            unsigned sy = sampleY[y];
            unsigned yi = sy >> 5;
            unsigned yn = yi + (yi < GAME_HEIGHT - 1);
            u16 a = mix565(src[yi * sourceWidth + xi], src[yi * sourceWidth + xn], sx & 31);
            u16 b = mix565(src[yn * sourceWidth + xi], src[yn * sourceWidth + xn], sx & 31);
            out[-(int)y] = mix565(a, b, sy & 31);
        }
    }
}

void Present_DrawGame(gfxScreen_t screen, const u16 *src, uint32_t options) {
    if (!(options & (DISPLAY_LCD_FIT | DISPLAY_TOP_FIT | DISPLAY_WIDE_FIELD)) ||
        (screen == GFX_BOTTOM && !(options & DISPLAY_LCD_FIT))) {
        Present_BlitRGB565(screen, src, GAME_WIDTH, GAME_HEIGHT);
        return;
    }
    draw_scaled(screen, src, GAME_WIDTH, options);
}

static void clear_screen(gfxScreen_t screen) {
    u16 stride;
    u16 width;
    u16 *fb = landscape_fb(screen, &stride, &width);
    if (fb != NULL) {
        memset(fb, 0, (size_t)stride * width * sizeof(*fb));
    }
}

void Present_Clear(uint32_t flags) {
    if (flags & PBW2_SCREEN_TOP) {
        clear_screen(GFX_TOP);
    }
    if (flags & PBW2_SCREEN_BOTTOM) {
        clear_screen(GFX_BOTTOM);
    }
}
#endif

void Present_BlitRGB565(gfxScreen_t screen, const uint16_t *pic, int pic_w, int pic_h) {
    u16 stride = 0;
    u16 land_w = 0;
    u16 *fb;
    int ox;
    int oy;
    int y;
    int x;

    if (!pic || pic_w <= 0 || pic_h <= 0) {
        return;
    }
    fb = landscape_fb(screen, &stride, &land_w);
    if (!fb) {
        return;
    }
    /* Landscape height is 240; landscape width is 400 (top) or 320 (bottom). */
    if (stride != GSP_SCREEN_WIDTH || pic_w > land_w || pic_h > stride) {
        return;
    }
    ox = ((int)land_w - pic_w) / 2;
    oy = ((int)stride - pic_h) / 2;
    clear_side_margins(fb, stride, land_w, ox, ox + pic_w);
    unsigned bottom = stride - oy - pic_h;

    for (x = 0; x < pic_w; x++) {
        u16 *column = fb + (ox + x) * stride;
        if (bottom) memset(column, 0, bottom * sizeof(*column));
        if (oy) memset(column + stride - oy, 0, oy * sizeof(*column));
        u16 *out = column + stride - 1 - oy;
        for (y = 0; y < pic_h; y++) {
            out[-y] = pic[y * pic_w + x];
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
