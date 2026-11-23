/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef PBW2_SETTINGS_H
#define PBW2_SETTINGS_H

#include <stdbool.h>
#include <stdint.h>

/* L+R+Y opens the settings screen. B closes it. No toggle changes play,
 * and nothing is written. */
bool Settings_Handle(uint32_t down, uint32_t held);
bool Settings_Open(void);
void Settings_Draw(void);

#endif
