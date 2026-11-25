/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef PBW2_SAVE_SHIM_H
#define PBW2_SAVE_SHIM_H

#include <stdbool.h>
#include <stdint.h>

/* Looks at the save file and does not create or write it. */
void Save_Init(void);

/* Cartridge flash byte ranges. Only the game's backup code calls writes. */
bool Save_GameRead(uint32_t offset, void *data, uint32_t size);
bool Save_GameWrite(uint32_t offset, const void *data, uint32_t size);

#endif
