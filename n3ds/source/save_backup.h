/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

/* Rotating save backup, polled from the IO worker (no new thread).
 *
 * After the game finishes saving, the completed save is copied to
 * <save>.bak1 so a later corrupt save or bad session can be rolled
 * back by hand. The platform never writes the live .sav: the source
 * is opened read-only, the copy is staged as .bak1.tmp, verified,
 * and published by rename, so an interrupted or failed copy always
 * leaves the previous backup byte-identical. Nothing happens at
 * boot or exit, and toggling the feature on is not a save event -
 * only the next in-game save is backed up.
 *
 * Gated by the "save_backup" config key (Settings, Features tab),
 * default off.
 */
void SaveBackup_Poll(void);

/* Ask the worker to export the current save to export/<save file>
 * at the next quiet moment. Called from the settings screen; the
 * settings screen itself does no file IO. */
void SaveBackup_RequestExport(void);
