/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef PBW2_SAVE_SHIM_H
#define PBW2_SAVE_SHIM_H

#include <stdbool.h>
#include <stdint.h>

/* Looks at the save file and does not create or write it. */
void Save_Init(void);

/* The only write entry. The shell does not call it. This phase refuses the
 * write so a bug here cannot create a save. The game, once linked, is the
 * caller, and the bytes stay the cartridge save. */
bool Save_GameWrite(const void *data, uint32_t size);

#endif
