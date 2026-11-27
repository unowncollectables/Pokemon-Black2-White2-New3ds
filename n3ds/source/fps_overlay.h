/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

/* The FPS counter overlay: draws "FPS <n>" at the top-left of the
 * top screen, straight into the LCD framebuffer after compose and
 * before the flush, so it is never part of the game picture (it
 * cannot leak into screenshots the game composes, the bottom-screen
 * cache, or the wide/native paths). The number is the LCD present
 * rate from present.c's 120-frame window - the same value the PERF
 * log line reports. Gated by the "fps_counter" config key. */
void FpsOverlay_Draw(void);
