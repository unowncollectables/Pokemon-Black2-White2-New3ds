/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "present.h"
#include "game_text.h"
#include "settings.h"

#include <3ds.h>
#include <stdio.h>
#include <string.h>

enum { LCD_HEIGHT = 240, TOP_WIDTH = 400, BOTTOM_WIDTH = 320 };
static uint16_t framebuffer[TOP_WIDTH * LCD_HEIGHT];
static uint16_t picture[BOTTOM_WIDTH * LCD_HEIGHT];
static uint16_t secondBottom[BOTTOM_WIDTH * LCD_HEIGHT];
static bool useSecondBottom;
static unsigned fullFlushes, topFlushes;

void __wrap_gfxFlushBuffers(void) { ++fullFlushes; }

Result __wrap_GSPGPU_FlushDataCache(const void *address, u32 size) {
    if (address != framebuffer || size != sizeof framebuffer) return -1;
    ++topFlushes;
    return 0;
}

u8 *__wrap_gfxGetFramebuffer(gfxScreen_t screen, gfx3dSide_t side, u16 *width, u16 *height) {
    (void)side;
    *width = LCD_HEIGHT;
    *height = screen == GFX_TOP ? TOP_WIDTH : BOTTOM_WIDTH;
    return (u8 *)(screen == GFX_BOTTOM && useSecondBottom ? secondBottom : framebuffer);
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

static bool check_game_fit(void) {
    for (unsigned y = 0; y < 192; ++y) {
        for (unsigned x = 0; x < 256; ++x) picture[y * 256 + x] = x < 127 ? 0xf800 : 0x001f;
    }
    Present_DrawGame(GFX_TOP, picture, DISPLAY_TOP_FIT | DISPLAY_SHARP_PIXELS);
    for (unsigned x = 0; x < TOP_WIDTH; ++x) {
        for (unsigned y = 0; y < LCD_HEIGHT; ++y) {
            uint16_t pixel = framebuffer[x * LCD_HEIGHT + LCD_HEIGHT - 1 - y];
            if (x < 40 || x >= 360) {
                if (pixel) return false;
            } else if (x < 198) {
                if (pixel != 0xf800) return false;
            } else if (x >= 199) {
                if (pixel != 0x001f) return false;
            } else if (!(pixel & 0xf800) || !(pixel & 31)) {
                return false;
            }
        }
    }
    return true;
}

static bool check_native_text(void) {
    static u32 ink[256 * 192];
    const u16 background = 0x07c0;
    for (unsigned i = 0; i < 256 * 192; ++i) picture[i] = background;
    for (unsigned x = 0; x < 4; ++x) {
        unsigned i = 16 * 256 + 8 + x;
        picture[i] = 0xf800;
        ink[i] = PBW2_TEXT_NATIVE | PBW2_TEXT_INK | (x << 16) | background;
    }
    Present_DrawGameWithText(GFX_TOP, picture, DISPLAY_TOP_FIT | DISPLAY_SHARP_TEXT, ink);
    for (unsigned y = 18; y < 24; ++y) {
        for (unsigned x = 46; x < 57; ++x) {
            u16 expected = y == 20 && x >= 50 && x < 54 ? 0xf800 : background;
            if (framebuffer[x * LCD_HEIGHT + LCD_HEIGHT - 1 - y] != expected) return false;
        }
    }
    return true;
}

static bool check_bottom_cache(void) {
    const u16 color = 0x07c0;
    for (unsigned i = 0; i < 256 * 192; ++i) picture[i] = color;
    unsigned options = DISPLAY_LCD_FIT | DISPLAY_SHARP_PIXELS;
    Present_DrawBottomCached(picture, options, NULL);
    Present_Flush();
    useSecondBottom = true;
    Present_DrawBottomCached(picture, options, NULL);
    Present_Flush();
    if (framebuffer[0] != color || secondBottom[0] != color || fullFlushes != 2 || topFlushes) return false;

    useSecondBottom = false;
    Present_DrawBottomCached(picture, options, NULL);
    Present_Flush();
    useSecondBottom = true;
    Present_DrawBottomCached(picture, options, NULL);
    Present_Flush();
    if (fullFlushes != 2 || topFlushes != 2) return false;

    picture[191 * 256] = 0x001f;
    Present_DrawBottomCached(picture, options, NULL);
    Present_Flush();
    if (secondBottom[0] != 0x001f || fullFlushes != 3) return false;

    Present_DrawBottomCached(NULL, options, NULL); /* Settings takes the screen. */
    secondBottom[0] = 0;
    Present_DrawBottomCached(picture, options, NULL);
    Present_Flush();
    return secondBottom[0] == 0x001f && fullFlushes == 4 && topFlushes == 2;
}

int main(void) {
    archiveMountSdmc();
    memset(framebuffer, 0xff, sizeof framebuffer);
    bool passed = check_blit(GFX_TOP, 320, 240) &&
                  check_blit(GFX_TOP, 256, 192) &&
                  check_blit(GFX_BOTTOM, 255, 191) && check_game_fit() && check_native_text() && check_bottom_cache();
    FILE *out = fopen("sdmc:/present-blit-test.txt", "w");
    if (out) {
        fprintf(out, "%s\n", passed ? "PASS" : "FAIL");
        fclose(out);
    }
    for (;;) svcSleepThread(1000000000ll);
}
