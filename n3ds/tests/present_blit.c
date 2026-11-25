/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "present.h"

#include <3ds.h>
#include <stdio.h>
#include <string.h>

enum { LCD_HEIGHT = 240, TOP_WIDTH = 400, BOTTOM_WIDTH = 320 };
static uint16_t framebuffer[TOP_WIDTH * LCD_HEIGHT];
static uint16_t picture[BOTTOM_WIDTH * LCD_HEIGHT];

u8 *__wrap_gfxGetFramebuffer(gfxScreen_t screen, gfx3dSide_t side, u16 *width, u16 *height) {
    (void)side;
    *width = LCD_HEIGHT;
    *height = screen == GFX_TOP ? TOP_WIDTH : BOTTOM_WIDTH;
    return (u8 *)framebuffer;
}

static bool check_blit(gfxScreen_t screen, unsigned picWidth, unsigned picHeight) {
    unsigned width = screen == GFX_TOP ? TOP_WIDTH : BOTTOM_WIDTH;
    unsigned left = (width - picWidth) / 2;
    unsigned top = (LCD_HEIGHT - picHeight) / 2;
    for (unsigned i = 0; i < picWidth * picHeight; i++) picture[i] = 1 + i % 65535u;
    Present_BlitRGB565(screen, picture, picWidth, picHeight);
    for (unsigned x = 0; x < width; x++) {
        for (unsigned y = 0; y < LCD_HEIGHT; y++) {
            uint16_t expected = 0;
            if (x >= left && x < left + picWidth && y >= top && y < top + picHeight)
                expected = picture[(y - top) * picWidth + x - left];
            if (framebuffer[x * LCD_HEIGHT + LCD_HEIGHT - 1 - y] != expected) return false;
        }
    }
    return true;
}

int main(void) {
    archiveMountSdmc();
    memset(framebuffer, 0xff, sizeof framebuffer);
    bool passed = check_blit(GFX_TOP, 320, 240) &&
                  check_blit(GFX_TOP, 256, 192) &&
                  check_blit(GFX_BOTTOM, 255, 191);
    FILE *out = fopen("sdmc:/present-blit-test.txt", "w");
    if (out) {
        fprintf(out, "%s\n", passed ? "PASS" : "FAIL");
        fclose(out);
    }
    for (;;) svcSleepThread(1000000000ll);
}
