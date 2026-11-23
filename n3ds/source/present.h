/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef PBW2_PRESENT_H
#define PBW2_PRESENT_H

#include <stdbool.h>
#include <stdint.h>

/* Commit which screens this frame shows, then draw and present from that
 * word. A future sharp-text layer reads Present_Flags() and does not sample
 * the frame again. */
void Present_Commit(bool top, bool bottom);
uint32_t Present_Flags(void);
void GxShim_Present(uint32_t flags);

#endif
