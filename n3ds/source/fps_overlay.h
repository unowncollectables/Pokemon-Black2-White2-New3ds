/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

/* The FPS counter overlay: draws a three-line stats block at the
 * top-left of the top screen, straight into the LCD framebuffer after
 * compose and before the flush, so it is never part of the game
 * picture (it cannot leak into screenshots the game composes, the
 * bottom-screen cache, or the wide/native paths):
 *
 *   FPS <n>          distinct presented frames per second - presents
 *                    whose composed top picture changed (present.c's
 *                    fingerprint), NOT raw host presents
 *   AVG <x.x> MS     the window's average host frame work
 *   MAX <x.x> MS     the window's worst single host frame work
 *
 * Stats come from present.c's ~500 ms window (Present_ReadStats), so
 * the block refreshes about twice a second with stable-width fields;
 * the FPS figure is the same value the PERF log line reports. Gated
 * by the "fps_counter" config key. */
void FpsOverlay_Draw(void);
