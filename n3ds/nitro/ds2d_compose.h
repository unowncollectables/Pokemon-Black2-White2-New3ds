/* SPDX-License-Identifier: GPL-3.0-or-later */
/* 2D engine software compositor. Logic adapted from Pokemon Platinum 3DS
 * widecomp.c (GPL-3.0, unowncollectables/3ds-platinum). */

#pragma once

#include "types.h"

#if defined(PBW2_LINK_GAME)

int PbW2_ComposeScreens(u16 *top_rgb565, u16 *bottom_rgb565);
const u16 *PbW2_TopSides(void);
int PbW2_GameDisplayReady(void);
void PbW2_GxDisplayTrace(void);

#endif