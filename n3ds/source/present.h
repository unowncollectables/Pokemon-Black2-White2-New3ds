/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef PBW2_PRESENT_H
#define PBW2_PRESENT_H

#include <3ds/gfx.h>
#include <stdbool.h>
#include <stdint.h>

/* Commit which screens this frame shows, then draw and present from that
 * word. A future sharp-text layer reads Present_Flags() and does not sample
 * the frame again. */
void Present_Commit(bool top, bool bottom);
uint32_t Present_Flags(void);
void GxShim_Present(uint32_t flags);
#if defined(PBW2_LINK_GAME)
void Present_Clear(uint32_t flags);
#endif

/* Center pic_w x pic_h RGB565 on the chosen screen. Uses the GSP framebuffer
 * size (400x240 top, 320x240 bottom) from gfxGetFramebuffer. */
void Present_BlitRGB565(gfxScreen_t screen, const uint16_t *pic, int pic_w, int pic_h);
void Present_DrawTitle(uint32_t flags, const uint16_t *top, const uint16_t *bottom);

#endif
