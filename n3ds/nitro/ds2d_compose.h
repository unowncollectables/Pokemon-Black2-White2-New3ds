/* SPDX-License-Identifier: GPL-3.0-or-later */
/* 2D engine software compositor. Logic adapted from Pokemon Platinum 3DS
 * widecomp.c (GPL-3.0, unowncollectables/3ds-platinum). */

#pragma once

#include "types.h"

#if defined(PBW2_LINK_GAME)

void PbW2_ComposeScreensWithText(u16 *top_rgb565, u16 *bottom_rgb565, u32 *topInk, u32 *bottomInk);
int PbW2_ComposeScreens(u16 *top_rgb565, u16 *bottom_rgb565);
/* The phases PbW2_ComposeScreensWithText runs, exposed for present:
 * Begin (present thread) -> Main and Sub (may run concurrently;
 * compose scratch is per-engine) -> FinishWide (present thread,
 * after both). PbW2_ComposeMainUpper returns the LCD routing Begin
 * sampled for the frame being composed. */
void PbW2_ComposeBegin(u32 *topInk, u32 *bottomInk);
int PbW2_ComposeMainUpper(void);
void PbW2_ComposeMainScreen(u16 *top_rgb565, u16 *bottom_rgb565, u32 *topInk, u32 *bottomInk);
void PbW2_ComposeSubScreen(u16 *top_rgb565, u16 *bottom_rgb565, u32 *topInk, u32 *bottomInk);
void PbW2_ComposeFinishWide(u16 *top_rgb565, u16 *bottom_rgb565);
/* Stereo: per-eye disparity (DS pixels, signed; >0 right eye, <0 left
 * eye, 0 flat) applied to the 3D layer when the main engine is
 * composed for the top screen. Set before each compose; the flat
 * default is 0 and present restores 0 after its eye passes. */
void PbW2_ComposeSetEyeShift(int shiftPx);
const u16 *PbW2_TopSides(void);
const u16 *PbW2_NativeTop(void);
/* 320x192 wide-layout pictures for the current frame (wide_layout.c),
 * or NULL when the scene has no layout or the layout declined. */
const u16 *PbW2_WideTop(void);
const u16 *PbW2_WideBottom(void);
int PbW2_GameDisplayReady(void);
void PbW2_GxDisplayTrace(void);

#endif