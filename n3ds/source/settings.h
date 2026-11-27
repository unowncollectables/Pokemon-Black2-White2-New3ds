/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef PBW2_SETTINGS_H
#define PBW2_SETTINGS_H

#include <stdbool.h>
#include <stdint.h>

enum {
    DISPLAY_SHARP_TEXT = 1u << 6,
    DISPLAY_WIDE_FIELD = 1u << 5,
    DISPLAY_LCD_FIT = 1u << 0,
    DISPLAY_TOP_FIT = 1u << 1,
    DISPLAY_SHARP_PIXELS = 1u << 2,
    DISPLAY_PICA_GEOMETRY = 1u << 3,
    DISPLAY_PICA_NO_EDGES = 1u << 4,
};

/* Load preferences from the game's SD directory (config.txt inside
 * it), which must remain valid while the app runs. */
void Settings_Init(const char *dir);
/* L+R+Y opens Settings. Returns true when the game should ignore input. */
bool Settings_Handle(uint32_t down, uint32_t held);
bool Settings_Open(void);
/* Effective FPS-counter state: hotkey override if set, else the
 * fps_counter config key. */
bool Settings_FpsCounterOn(void);
uint32_t Settings_DisplayFlags(void);
/* Re-push the derived display state (wide enable, PICA options) into the
 * renderer. Called at boot (Settings_Init), on settings close, on the
 * scale hotkey, and on every field entry (GFL_G3DSysCreate) so the
 * boot -> field path applies exactly what a menu round-trip applies.
 * Idempotent: two atomic stores. */
void Settings_ApplyDisplay(void);
struct Font;
void Settings_CacheGameFont(struct Font *font);
void Settings_Draw(void);
void Settings_DrawTop(void);

#endif
