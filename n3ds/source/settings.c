/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "settings.h"

#include "log.h"
#ifdef PBW2_GPU_GEOMETRY
#include "g3_pica.h"
#include "g3_raster.h"
#endif

#include <3ds.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

static bool s_open;
static unsigned s_row;
static uint32_t s_display;
static uint32_t s_persisted;
static const char *s_path;
static bool s_pending;
static char s_tempPath[128];
static const char s_configHeader[] = "PBW2-DISPLAY 1\n";
static const char s_hex[] = "0123456789abcdef";

static const struct {
    const char *label;
    uint32_t flag;
} s_options[] = {
    {"LCD fit (4:3)", DISPLAY_LCD_FIT},
    {"Top screen fit (4:3)", DISPLAY_TOP_FIT},
    {"Sharp scaling", DISPLAY_SHARP_PIXELS},
    {"Sharp game text", DISPLAY_SHARP_TEXT},
#ifdef PBW2_GPU_GEOMETRY
    {"GPU geometry (experimental)", DISPLAY_PICA_GEOMETRY},
    {"GPU 3D without outlines", DISPLAY_PICA_NO_EDGES},
    {"Widescreen 3D", DISPLAY_WIDE_FIELD},
#endif
};

enum {
    ROWS = sizeof s_options / sizeof s_options[0],
    DISPLAY_MASK = DISPLAY_SHARP_TEXT | DISPLAY_WIDE_FIELD | DISPLAY_LCD_FIT | DISPLAY_TOP_FIT | DISPLAY_SHARP_PIXELS | DISPLAY_PICA_GEOMETRY | DISPLAY_PICA_NO_EDGES,
    ROW_X = 20, ROW_WIDTH = 280, ROW_Y = 49, ROW_HEIGHT = 20, ROW_SPACING = 22,
};

static bool load_display(const char *path, uint32_t *flags) {
    FILE *file = fopen(path, "rb");
    if (!file) return false;
    char entry[sizeof s_configHeader + 2];
    bool complete = fread(entry, 1, sizeof entry, file) == sizeof entry && fgetc(file) == EOF && !ferror(file);
    fclose(file);
    const unsigned start = sizeof s_configHeader - 1;
    if (!complete || memcmp(entry, s_configHeader, start) || entry[start + 2] != '\n') return false;
    const char *high = entry[start] ? strchr(s_hex, entry[start]) : NULL;
    const char *low = entry[start + 1] ? strchr(s_hex, entry[start + 1]) : NULL;
    if (!high || !low) return false;
    *flags = ((high - s_hex) * 16 + (low - s_hex)) & DISPLAY_MASK;
    return true;
}

void Settings_Init(const char *path) {
    s_display = 0;
    s_path = NULL;
    s_pending = false;
    int length = snprintf(s_tempPath, sizeof s_tempPath, "%s.tmp", path);
    if (length > 0 && (unsigned)length < sizeof s_tempPath) {
        s_path = path;
        s_pending = load_display(s_tempPath, &s_display);
        if (!s_pending) load_display(s_path, &s_display);
    }
    s_persisted = s_display;
    Log_Line("CFG", "display flags loaded: %02lx", (unsigned long)s_display);
#ifdef PBW2_GPU_GEOMETRY
    PbW2_G3SetWide((s_display & DISPLAY_WIDE_FIELD) != 0);
    PbW2_PicaSetOptions((s_display & (DISPLAY_PICA_GEOMETRY | DISPLAY_PICA_NO_EDGES)) != 0,
                        (s_display & DISPLAY_PICA_NO_EDGES) != 0);
#endif
}

static bool commit_display(void) {
    /* SDMC rename requires a vacant destination. A complete staged file
     * remains recoverable if power is lost between removal and rename. */
    if (remove(s_path) != 0 && errno != ENOENT) return false;
    if (rename(s_tempPath, s_path) != 0) return false;
    s_pending = false;
    return true;
}

static void save_display(void) {
    if (!s_path) return;
    if (s_pending && !commit_display()) {
        Log_Line("CFG", "could not finalize staged display preferences");
        return;
    }
    if (s_display == s_persisted) return;
    FILE *file = fopen(s_tempPath, "wb");
    if (file) {
        bool written = fprintf(file, "%s%02lx\n", s_configHeader, (unsigned long)s_display) > 0;
        bool closed = fclose(file) == 0;
        if (written && closed) {
            s_persisted = s_display;
            s_pending = true;
            if (commit_display()) return;
        } else {
            remove(s_tempPath);
        }
    }
    Log_Line("CFG", "could not save display preferences");
}

bool Settings_Open(void) {
    return s_open;
}

bool Settings_Handle(uint32_t down, uint32_t held) {
    if ((held & KEY_L) && (held & KEY_R) && (down & KEY_Y)) {
        s_open = !s_open;
        if (!s_open) save_display();
        Log_Line("CFG", s_open ? "settings open" : "settings close");
        return true;
    }
    if (s_open && (down & KEY_B)) {
        s_open = false;
        save_display();
        Log_Line("CFG", "settings close");
        return true;
    }
    if (!s_open) return false;
    if (down & KEY_DUP) s_row = (s_row + ROWS - 1) % ROWS;
    if (down & KEY_DDOWN) s_row = (s_row + 1) % ROWS;
    bool toggle = (down & KEY_A) != 0;
    if (down & KEY_TOUCH) {
        touchPosition touch;
        hidTouchRead(&touch);
        if (touch.px >= ROW_X && touch.px < ROW_X + ROW_WIDTH && touch.py >= ROW_Y) {
            unsigned y = touch.py - ROW_Y;
            if (y / ROW_SPACING < ROWS && y % ROW_SPACING < ROW_HEIGHT) {
                s_row = y / ROW_SPACING;
                toggle = true;
            }
        }
    }
    if (toggle) {
        s_display ^= s_options[s_row].flag;
#ifdef PBW2_GPU_GEOMETRY
        PbW2_G3SetWide((s_display & DISPLAY_WIDE_FIELD) != 0);
        PbW2_PicaSetOptions((s_display & (DISPLAY_PICA_GEOMETRY | DISPLAY_PICA_NO_EDGES)) != 0,
                            (s_display & DISPLAY_PICA_NO_EDGES) != 0);
#endif
        save_display();
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
            unsigned rowY = ROW_Y + ROW_SPACING * s_row;
            if (x >= ROW_X && x < ROW_X + ROW_WIDTH && y >= rowY && y < rowY + ROW_HEIGHT)
                color = 0x32ac;
            fb[x * stride + stride - 1 - y] = color;
        }
    }
    draw_text(fb, stride, 24, 25, "Display settings", 0xffff);
    for (unsigned i = 0; i < ROWS; ++i) {
        draw_text(fb, stride, 26, ROW_Y + 6 + ROW_SPACING * i, s_options[i].label, 0xffff);
        draw_text(fb, stride, 265, ROW_Y + 6 + ROW_SPACING * i, s_display & s_options[i].flag ? "On" : "Off", 0xffdf);
    }
    draw_text(fb, stride, 24, 208, "D-pad  Select   A  Toggle   B  Close", 0xffff);
#else
    printf("\x1b[1;0HSETTINGS");
    printf("\x1b[3;0HDisplay settings in linked builds.");
    printf("\x1b[5;0HB closes.");
#endif
}
