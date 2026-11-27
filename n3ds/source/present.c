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
#include "game_text.h"
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
static void draw_native_text(gfxScreen_t screen, const u16 *src, unsigned sourceWidth, const u32 *ink,
                             bool restoreLCD);
static void draw_scaled(gfxScreen_t screen, const u16 *src, unsigned sourceWidth, uint32_t options, const u32 *ink);
#endif

void GxShim_Present(uint32_t flags) {
#if defined(PBW2_LINK_GAME)
    static u16 s_top[256 * 192];
    static u16 s_bottom[256 * 192];
    static u32 s_topInk[256 * 192], s_bottomInk[256 * 192];
    if (PbW2_VramHostReady()) {
        PbW2_GxDisplayTrace();
        /* Readiness gates the first game frame. Later frames can intentionally blank the LCDs. */
        if (PbW2_GameHasPresented() || PbW2_GameDisplayReady()) {
            u16 *top = (flags & PBW2_SCREEN_TOP) ? s_top : NULL;
            u16 *bottom = (flags & PBW2_SCREEN_BOTTOM) && !Settings_Open() ? s_bottom : NULL;
            uint32_t options = Settings_DisplayFlags();
            bool sharpText = (options & DISPLAY_SHARP_TEXT) != 0;
            u32 *topInk = top && sharpText && (options & (DISPLAY_TOP_FIT | DISPLAY_LCD_FIT | DISPLAY_WIDE_FIELD))
                             ? s_topInk : NULL;
            u32 *bottomInk = bottom && sharpText && (options & DISPLAY_LCD_FIT) ? s_bottomInk : NULL;
            PbW2_ComposeScreensWithText(top, bottom, topInk, bottomInk);
            if (top) {
                const u16 *sides = PbW2_TopSides();
                const u16 *native = PbW2_NativeTop();
                const u16 *wideTop = PbW2_WideTop();
                if (wideTop && (options & DISPLAY_WIDE_FIELD)) {
                    /* Wide 2D layout picture (320 wide, centre unmoved).
                     * No native ink: glyph coordinates are DS-space and
                     * a layout screen's text rides the scaled path until
                     * the REQ-14 map says which of its layers are native. */
                    draw_scaled(GFX_TOP, wideTop, 320, options, NULL);
                } else if (native && (options & DISPLAY_WIDE_FIELD)) {
                    Present_BlitRGB565(GFX_TOP, native, PBW2_NATIVE_TOP_WIDTH, PBW2_NATIVE_TOP_HEIGHT);
                    if (topInk) draw_native_text(GFX_TOP, top, GAME_WIDTH, topInk, true);
                } else if (sides && (options & DISPLAY_WIDE_FIELD)) {
                    static u16 wide[320 * GAME_HEIGHT];
                    for (unsigned y = 0; y < GAME_HEIGHT; ++y) {
                        memcpy(wide + y * 320, sides + y * 64, 32 * sizeof(u16));
                        memcpy(wide + y * 320 + 32, top + y * GAME_WIDTH, GAME_WIDTH * sizeof(u16));
                        memcpy(wide + y * 320 + 288, sides + y * 64 + 32, 32 * sizeof(u16));
                    }
                    draw_scaled(GFX_TOP, wide, 320, options, topInk);
                } else Present_DrawGameWithText(GFX_TOP, top, options, topInk);
            }
            {
                const u16 *wideBottom = PbW2_WideBottom();
                if (bottom && wideBottom && (options & DISPLAY_WIDE_FIELD)) {
                    draw_scaled(GFX_BOTTOM, wideBottom, 320, options, NULL);
                } else {
                    Present_DrawBottomCached(bottom, options, bottomInk);
                }
            }
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
    /* Under a wide bottom layout the picture is 320 wide with the DS
     * centre unmoved at +32: invert that mapping, or taps on the X
     * menu's tiles would land a strip-width off. */
    if (PbW2_WideBottom() && (Settings_DisplayFlags() & DISPLAY_WIDE_FIELD)) {
        if (px < 32 || px >= 288) return false;
        *x = px - 32;
        *y = (2 * py + 1) * GAME_HEIGHT / (2 * 240);
        return true;
    }
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

static void draw_scaled(gfxScreen_t screen, const u16 *src, unsigned sourceWidth, uint32_t options, const u32 *ink) {
    u16 stride, width;
    u16 *fb = landscape_fb(screen, &stride, &width);
    if (!fb || stride != 240 || width < 320) return;

    GameRect rect = game_rect(screen, width, stride, options);
    if (sourceWidth == 320) rect = (GameRect){0, 0, width, stride};
    /* Scaled modes fill the LCD vertically. Only the side margins need clearing. */
    clear_side_margins(fb, stride, width, rect.x, rect.x + rect.width);
    const u16 *glyphColors = src;
    if (ink) {
        static u16 background[320 * GAME_HEIGHT];
        unsigned left = (sourceWidth - GAME_WIDTH) / 2;
        memcpy(background, src, sourceWidth * GAME_HEIGHT * sizeof(*src));
        for (unsigned y = 0; y < GAME_HEIGHT; ++y) {
            for (unsigned x = 0; x < GAME_WIDTH; ++x) {
                u32 pixel = ink[y * GAME_WIDTH + x];
                if (pixel & PBW2_TEXT_NATIVE) background[y * sourceWidth + left + x] = (u16)pixel;
            }
        }
        src = background;
    }
    bool sharp = (options & DISPLAY_SHARP_PIXELS) != 0;
    unsigned sampleY[240], nearestRow[240];
    unsigned offset = (sourceWidth - GAME_WIDTH) / 2;
    for (unsigned y = 0; y < rect.height; ++y) {
        sampleY[y] = sample_axis(y, rect.height, GAME_HEIGHT, sharp);
        if (ink) nearestRow[y] = ((2 * y + 1) * GAME_HEIGHT / (2 * rect.height)) * sourceWidth;
    }
    for (unsigned x = 0; x < rect.width; ++x) {
        unsigned sx = sample_axis(x, rect.width, sourceWidth, sharp);
        u16 *out = fb + (rect.x + x) * stride + stride - 1 - rect.y;
        unsigned xi = sx >> 5;
        unsigned xn = xi + (xi < sourceWidth - 1);
        bool canSampleInk = ink && xi >= offset && xn < offset + GAME_WIDTH;
        unsigned nearX = canSampleInk ? (2 * x + 1) * sourceWidth / (2 * rect.width) : 0;
        for (unsigned y = 0; y < rect.height; ++y) {
            unsigned sy = sampleY[y];
            unsigned yi = sy >> 5;
            unsigned yn = yi + (yi < GAME_HEIGHT - 1);
            if (canSampleInk &&
                (ink[yi * GAME_WIDTH + xi - offset] == PBW2_TEXT_INK || ink[yi * GAME_WIDTH + xn - offset] == PBW2_TEXT_INK ||
                 ink[yn * GAME_WIDTH + xi - offset] == PBW2_TEXT_INK || ink[yn * GAME_WIDTH + xn - offset] == PBW2_TEXT_INK)) {
                out[-(int)y] = src[nearestRow[y] + nearX];
                continue;
            }
            u16 a = mix565(src[yi * sourceWidth + xi], src[yi * sourceWidth + xn], sx & 31);
            u16 b = mix565(src[yn * sourceWidth + xi], src[yn * sourceWidth + xn], sx & 31);
            out[-(int)y] = mix565(a, b, sy & 31);
        }
    }
    if (ink) draw_native_text(screen, glyphColors, sourceWidth, ink, false);
}

static void draw_native_text(gfxScreen_t screen, const u16 *src, unsigned sourceWidth, const u32 *ink,
                             bool restoreLCD) {
    u16 stride, width;
    u16 *fb = landscape_fb(screen, &stride, &width);
    if (!fb || stride != 240 || width < 320) return;
    unsigned left = (width - 320) / 2;
    unsigned offset = (sourceWidth - GAME_WIDTH) / 2;
    if (restoreLCD) {
        /* Restore the background at each LCD sample copied from a glyph pixel. */
        for (unsigned y = 0; y < GAME_HEIGHT; ++y) {
            for (unsigned x = 0; x < GAME_WIDTH; ++x) {
                u32 pixel = ink[y * GAME_WIDTH + x];
                if (!(pixel & PBW2_TEXT_NATIVE)) continue;
                for (unsigned ny = (5 * y + 1) / 4; ny < (5 * y + 6) / 4; ++ny) {
                    for (unsigned nx = (5 * x + 1) / 4; nx < (5 * x + 6) / 4; ++nx)
                        fb[(left + nx) * stride + stride - 1 - ny] = (u16)pixel;
                }
            }
        }
    }
    for (unsigned y = 0; y < GAME_HEIGHT; ++y) {
        for (unsigned x = 0; x < GAME_WIDTH; ++x) {
            u32 pixel = ink[y * GAME_WIDTH + x];
            if (!(pixel & PBW2_TEXT_NATIVE)) continue;
            unsigned nx = PbW2_TextNativeCoordinate(x, (pixel >> 16) & 31);
            unsigned ny = PbW2_TextNativeCoordinate(y, (pixel >> 21) & 31);
            fb[(left + nx) * stride + stride - 1 - ny] = src[y * sourceWidth + offset + x];
        }
    }
}

void Present_DrawGameWithText(gfxScreen_t screen, const u16 *src, uint32_t options, const u32 *ink) {
    if (!(options & (DISPLAY_LCD_FIT | DISPLAY_TOP_FIT | DISPLAY_WIDE_FIELD)) ||
        (screen == GFX_BOTTOM && !(options & DISPLAY_LCD_FIT))) {
        Present_BlitRGB565(screen, src, GAME_WIDTH, GAME_HEIGHT);
        return;
    }
    draw_scaled(screen, src, GAME_WIDTH, options, ink);
}

void Present_DrawGame(gfxScreen_t screen, const u16 *src, uint32_t options) {
    Present_DrawGameWithText(screen, src, options, NULL);
}

/* Keep track of which of libctru's two LCD buffers contains the latest bottom image. */
static struct {
    u16 image[GAME_WIDTH * GAME_HEIGHT];
    u32 ink[GAME_WIDTH * GAME_HEIGHT];
    u16 *buffers[2];
    uint32_t options;
    bool hasInk, reused;
} s_bottomCache;

void Present_DrawBottomCached(const u16 *src, uint32_t options, const u32 *ink) {
    s_bottomCache.reused = false;
    u16 stride, width;
    u16 *fb = src ? landscape_fb(GFX_BOTTOM, &stride, &width) : NULL;
    if (!fb || stride != 240 || width != 320) {
        s_bottomCache.buffers[0] = s_bottomCache.buffers[1] = NULL;
        if (src) Present_DrawGameWithText(GFX_BOTTOM, src, options, ink);
        return;
    }
    options &= DISPLAY_LCD_FIT | DISPLAY_SHARP_PIXELS;
    bool same = s_bottomCache.buffers[0] && options == s_bottomCache.options &&
                (ink != NULL) == s_bottomCache.hasInk &&
                memcmp(src, s_bottomCache.image, sizeof s_bottomCache.image) == 0 &&
                (!ink || memcmp(ink, s_bottomCache.ink, sizeof s_bottomCache.ink) == 0);
    if (same && (fb == s_bottomCache.buffers[0] || fb == s_bottomCache.buffers[1])) {
        s_bottomCache.reused = true;
        return;
    }
    if (same) {
        s_bottomCache.buffers[1] = fb;
    } else {
        memcpy(s_bottomCache.image, src, sizeof s_bottomCache.image);
        if (ink) memcpy(s_bottomCache.ink, ink, sizeof s_bottomCache.ink);
        s_bottomCache.hasInk = ink != NULL;
        s_bottomCache.options = options;
        s_bottomCache.buffers[0] = fb;
        s_bottomCache.buffers[1] = NULL;
    }
    Present_DrawGameWithText(GFX_BOTTOM, src, options, ink);
}

/* Present-rate window: fed once per main-loop present, emits the
 * PERF line every 120 frames and keeps the average for the FPS
 * overlay. This is LCD present rate, not game-logic rate. */
static unsigned s_presentFps;

void Present_NoteFrame(uint64_t start, uint64_t flushStart, uint64_t finish) {
    static uint64_t windowStart;
    static uint64_t drawTicks, flushTicks;
    static unsigned frames;
    if (!frames) windowStart = start;
    drawTicks += flushStart - start;
    flushTicks += finish - flushStart;
    if (++frames == 120) {
        uint64_t elapsed = finish - windowStart;
        unsigned fps = elapsed ? frames * (uint64_t)SYSCLOCK_ARM11 / elapsed : 0;
        unsigned drawUs = drawTicks * 1000000ull / ((uint64_t)frames * SYSCLOCK_ARM11);
        unsigned flushUs = flushTicks * 1000000ull / ((uint64_t)frames * SYSCLOCK_ARM11);
        Log_Line("PERF", "LCD %u fps, update and compose avg %u us, flush and swap avg %u us", fps, drawUs, flushUs);
        s_presentFps = fps;
        frames = 0;
        drawTicks = flushTicks = 0;
    }
}

unsigned Present_Fps(void) {
    return s_presentFps;
}

void Present_Flush(void) {
    if (s_bottomCache.reused) {
        /* The game uses a single RGB565 image at the top. The retained bottom
         * buffer was already flushed when it was last written. */
        u16 stride, width;
        u16 *top = landscape_fb(GFX_TOP, &stride, &width);
        if (top && stride == 240 && width == 400 &&
            R_SUCCEEDED(GSPGPU_FlushDataCache(top, stride * width * sizeof(*top)))) return;
    }
    gfxFlushBuffers();
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
        Present_DrawBottomCached(NULL, 0, NULL);
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
