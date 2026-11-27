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
    unsigned fps = Present_Fps();
    if (!fps) return;
    u16 stride, width;
    u16 *fb = (u16 *)gfxGetFramebuffer(GFX_TOP, GFX_LEFT, &stride, &width);
    if (!fb || stride != 240 || width != 400) return;
    char text[16];
    snprintf(text, sizeof text, "FPS %u", fps);
    draw_text(fb, stride, 9, 9, text, 0x0000);
    draw_text(fb, stride, 8, 8, text, 0xffff);
}
