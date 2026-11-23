/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef PBW2_NITRO_HOST_H
#define PBW2_NITRO_HOST_H

#include <stdbool.h>
#include <stdint.h>

/* Brought up by the shell. The game's main is not called yet. No DS register
 * is touched: IO writes from game C land in pbw2_io. */
void NitroHost_Init(void);
/* One frame. pad_held uses the game's PAD_BUTTON_* / PAD_KEY_* bits. */
void NitroHost_Frame(uint32_t pad_held);

bool NitroHost_Nitrofs(void);
uint32_t NitroHost_ProbeBytes(void);
uint32_t NitroHost_Pad(void);
void NitroHost_Rtc(int *year, int *month, int *day, int *hour, int *minute);

#endif
