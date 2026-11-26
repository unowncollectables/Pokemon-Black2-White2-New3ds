/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef PBW2_SETTINGS_H
#define PBW2_SETTINGS_H

#include <stdbool.h>
#include <stdint.h>

enum {
    DISPLAY_LCD_FIT = 1u << 0,
    DISPLAY_WIDE_FILL = 1u << 1,
    DISPLAY_SHARP_PIXELS = 1u << 2,
    DISPLAY_PICA_GEOMETRY = 1u << 3,
    DISPLAY_PICA_NO_EDGES = 1u << 4,
};

/* Load preferences from a path that remains valid while the app runs. */
void Settings_Init(const char *path);
/* L+R+Y opens Settings. Returns true when the game should ignore input. */
bool Settings_Handle(uint32_t down, uint32_t held);
bool Settings_Open(void);
uint32_t Settings_DisplayFlags(void);
void Settings_Draw(void);

#endif
