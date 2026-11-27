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

/* Save-completion sensing for the host's save backup: the generation
 * bumps on every successful game write, and the tick records when
 * the last one landed. "A save completed" is inferred by the backup
 * poll (generation stable + quiet for a debounce window), because
 * the shim is the only write path. If a game-side save-finished
 * hook is ever identified, it can feed the same pair. */
uint32_t Save_Generation(void);
uint64_t Save_LastWriteTick(void);

#endif
