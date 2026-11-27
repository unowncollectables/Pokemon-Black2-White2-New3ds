/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include <stdbool.h>

/* One key=value config store (config.txt in the game's SD directory),
 * modelled on the SoulSilver port's config system:
 *
 * - The file is loaded once at boot into a RAM cache of lines. Unknown
 *   keys, comments and line order are preserved verbatim on rewrite.
 * - Reads resolve file value -> built-in defaults table -> the
 *   caller's fallback, and are cached per key; a read never touches
 *   the SD card after boot (PbW2_ConfigFileReads proves it).
 * - Writes are write-through: the setter updates the cached line and
 *   rewrites the whole file (staged .tmp + rename, like display.cfg),
 *   so the file and the cache can never disagree. A failed write is
 *   logged loudly, never silent.
 *
 * This store is the root dependency for the ported QOL features: new
 * settings register a key here instead of growing display.cfg's
 * fixed two-hex-digit format, which cannot grow past eight flags.
 */

void PbW2_ConfigInit(const char *path);
int PbW2_ConfigGetInt(const char *key, int fallback);
bool PbW2_ConfigGetBool(const char *key, bool fallback);
void PbW2_ConfigSetInt(const char *key, int value);
void PbW2_ConfigSetBool(const char *key, bool value);
bool PbW2_ConfigHas(const char *key);
unsigned PbW2_ConfigFileReads(void);
