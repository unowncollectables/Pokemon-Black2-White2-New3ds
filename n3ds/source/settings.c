/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "settings.h"

#include "log.h"
#ifdef PBW2_GPU_GEOMETRY
#include "g3_pica.h"
#endif

#include <3ds.h>
#include <stdio.h>

static bool s_open;
static unsigned s_row;
static uint32_t s_display;

static const struct {
    const char *label;
    uint32_t flag;
} s_options[] = {
    {"LCD fit (4:3)", DISPLAY_LCD_FIT},
    {"Wide fill (stretch top)", DISPLAY_WIDE_FILL},
    {"Sharp pixels (point filter)", DISPLAY_SHARP_PIXELS},
#ifdef PBW2_GPU_GEOMETRY
    {"GPU geometry (experimental)", DISPLAY_PICA_GEOMETRY},
#endif
};

enum { ROWS = sizeof s_options / sizeof s_options[0] };

bool Settings_Open(void) {
    return s_open;
}

bool Settings_Handle(uint32_t down, uint32_t held) {
    if ((held & KEY_L) && (held & KEY_R) && (down & KEY_Y)) {
        s_open = !s_open;
        Log_Line("CFG", s_open ? "settings open" : "settings close");
        return true;
    }
    if (s_open && (down & KEY_B)) {
        s_open = false;
        Log_Line("CFG", "settings close");
        return true;
    }
    if (!s_open) return false;
    if (down & KEY_DUP) s_row = (s_row + ROWS - 1) % ROWS;
    if (down & KEY_DDOWN) s_row = (s_row + 1) % ROWS;
    if (down & KEY_A) {
        s_display ^= s_options[s_row].flag;
#ifdef PBW2_GPU_GEOMETRY
        PbW2_PicaSetEnabled((s_display & DISPLAY_PICA_GEOMETRY) != 0);
#endif
        Log_Line("CFG", "%s %s", s_options[s_row].label, s_display & s_options[s_row].flag ? "on" : "off");
    }
    return true;
}

uint32_t Settings_DisplayFlags(void) {
    return s_display;
}

#if defined(PBW2_LINK_GAME)
static void draw_text(u16 *fb, unsigned stride, unsigned x, unsigned y, const char *text, u16 color) {
    const ConsoleFont *font = &consoleGetDefault()->font;
    for (; *text; ++text, x += 8) {
        const u8 *glyph = font->gfx + 8 * ((unsigned char)*text - font->asciiOffset);
        for (unsigned dx = 0; dx < 8; ++dx) {
            for (unsigned dy = 0; dy < 8; ++dy) {
                if (glyph[dy] & (0x80u >> dx)) fb[(x + dx) * stride + stride - 1 - y - dy] = color;
            }
        }
    }
}
#endif

void Settings_Draw(void) {
#if defined(PBW2_LINK_GAME)
    u16 stride, width;
    u16 *fb = (u16 *)gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, &stride, &width);
    if (!fb || stride != 240 || width != 320) return;
    for (unsigned x = 0; x < width; ++x) {
        for (unsigned y = 0; y < stride; ++y) {
            u16 color = x < 12 || x >= 308 || y < 12 || y >= 228 ? 0x0842 : 0x18c6;
            if (x >= 20 && x < 300 && y >= 49 + 31 * s_row && y < 74 + 31 * s_row) color = 0x32ac;
            fb[x * stride + stride - 1 - y] = color;
        }
    }
    draw_text(fb, stride, 24, 25, "Display settings", 0xffff);
    for (unsigned i = 0; i < ROWS; ++i) {
        draw_text(fb, stride, 26, 58 + 31 * i, s_options[i].label, 0xffff);
        draw_text(fb, stride, 265, 58 + 31 * i, s_display & s_options[i].flag ? "On" : "Off", 0xffdf);
    }
    draw_text(fb, stride, 24, 178, "Sharp pixels applies when scaled.", 0xbdff);
    draw_text(fb, stride, 24, 208, "D-pad  Select   A  Toggle   B  Close", 0xffff);
#else
    printf("\x1b[1;0HSETTINGS");
    printf("\x1b[3;0HDisplay settings in linked builds.");
    printf("\x1b[5;0HB closes.");
#endif
}
