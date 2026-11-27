/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include "nitro/fx.h"
#include <stdbool.h>

enum { PBW2_NATIVE_TOP_WIDTH = 400, PBW2_NATIVE_TOP_HEIGHT = 240 };

void PbW2_G3SetWide(bool enabled);
bool PbW2_G3WideCamera(void);
/* The published frame's shape, for compose-time decisions. */
bool PbW2_G3PublishedWide(void);
bool PbW2_G3PublishedNative(void);

void PbW2_G3RasterReset(void);
/* Software rasterization can begin before the GX swap command. */
void PbW2_G3RasterSetWBuffer(bool enabled);
/* Hold a queued GPU swap until the VBlank texture transfers finish. */
void PbW2_G3RasterDeferSwap(bool enabled);
/* Restart bounded renderer diagnostics at the start of a field zone. */
void PbW2_G3RasterTraceScene(void);
void PbW2_G3RasterCommand(u32 opcode, const u32 *args, const MtxFx44 *clip,
                         const MtxFx44 *texture, const MtxFx44 *vector);
/* The single presenter can borrow the native frame until it finishes composing. */
bool PbW2_G3RasterComposite(u16 *rgb565, const u8 *window, u8 *planes, u16 *rawColors, u16 *sides, u16 backdrop,
                            const u16 **native, u32 *ink);
void PbW2_G3ReleaseNative(void);
/* Copy the DS 3D layer, including the validity bit used for capture. */
void PbW2_G3RasterCapture(u16 *rgb555);
/* Called on the geometry thread before an upload overwrites LCDC memory. */
void PbW2_G3RasterBeforeLcdcWrite(u32 address, u32 bytes);

/* Followers Stage 1: the port-owned follower texture. The follower
 * quad is submitted with this magic TEXIMAGE address (a PLTT16
 * texture there would run past the end of texture VRAM, so no real
 * draw can use it -- the same trick Nate's Platinum port uses).
 * texture_pixel() serves texels from the registered strip instead of
 * VRAM, on both the CPU rasterizer and the PICA upload path
 * (gpu_texel funnels through texture_pixel). The strip is one
 * 256x32 PLTT16 image: eight 32x32 frames, controller-14 order.
 * `generation` increments on every identity/palette change and is
 * folded into the PICA cache key so a cached upload can never go
 * stale across a follower change. */
#define PBW2_FOLLOWER_TEX_ADDR 0x7FFF8u
void PbW2_G3FollowerSetTexture(const u8 *indices, const u16 *palNormal, const u16 *palShiny,
                               bool shiny, u32 generation);
