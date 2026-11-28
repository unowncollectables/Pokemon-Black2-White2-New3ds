/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "fps_overlay.h"

#include "present.h"

#include <3ds.h>
#include <stdio.h>

/* Same glyph painter and column-major addressing as Settings_Draw,
 * aimed at the top screen. */
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

void FpsOverlay_Draw(void) {
    PresentStats stats;
    if (!Present_ReadStats(&stats)) return;
    u16 stride, width;
    u16 *fb = (u16 *)gfxGetFramebuffer(GFX_TOP, GFX_LEFT, &stride, &width);
    if (!fb || stride != 240 || width != 400) return;
    /* Fixed-width fields so the block does not jitter as values move:
     * FPS is the distinct-presented-frame rate; AVG / MAX are the
     * window's average and worst host frame work (see present.h). */
    char lines[3][20];
    snprintf(lines[0], sizeof lines[0], "FPS %3u", stats.fps);
    snprintf(lines[1], sizeof lines[1], "AVG %3u.%u MS", stats.avgFrameMs10 / 10, stats.avgFrameMs10 % 10);
    snprintf(lines[2], sizeof lines[2], "MAX %3u.%u MS", stats.worstFrameMs10 / 10, stats.worstFrameMs10 % 10);
    for (unsigned i = 0; i < 3; i++) {
        draw_text(fb, stride, 9, 9 + i * 10, lines[i], 0x0000);
        draw_text(fb, stride, 8, 8 + i * 10, lines[i], 0xffff);
    }
}
