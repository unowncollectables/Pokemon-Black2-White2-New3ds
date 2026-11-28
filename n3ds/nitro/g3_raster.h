/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include "nitro/fx.h"
#include <stdbool.h>

enum { PBW2_NATIVE_TOP_WIDTH = 400, PBW2_NATIVE_TOP_HEIGHT = 240 };

void PbW2_G3SetWide(bool enabled);
bool PbW2_G3WideCamera(void);
/* The wide toggle itself (DISPLAY_WIDE_FIELD), without the per-frame
 * camera conditions: the 2D layout path keys off the toggle alone. */
bool PbW2_G3WideEnabled(void);
/* The published frame's shape, for compose-time decisions. */
bool PbW2_G3PublishedWide(void);
bool PbW2_G3PublishedNative(void);

void PbW2_G3RasterReset(void);
/* A new G3D system (zone/scene transition): drop the PICA texture cache
 * and the raster side's cross-zone survivors. Called from
 * GFL_G3DSysCreate, which FieldRoutine_Open runs once per zone entry. */
void PbW2_G3RasterInvalidateZone(void);
/* Software rasterization can begin before the GX swap command. */
void PbW2_G3RasterSetWBuffer(bool enabled);
/* Hold a queued GPU swap until the VBlank texture transfers finish. */
void PbW2_G3RasterDeferSwap(bool enabled);
/* Restart bounded renderer diagnostics at the start of a field zone. */
void PbW2_G3RasterTraceScene(void);
void PbW2_G3RasterCommand(u32 opcode, const u32 *args, const MtxFx44 *clip,
                         const MtxFx44 *texture, const MtxFx44 *vector);
/* The single presenter can borrow the native frame until it finishes composing. */
/* eyeShiftPx (stereo, present-driven): 0 = flat (the historical path,
 * byte-identical). >0 = right eye, <0 = left eye; magnitude is the
 * per-eye disparity in DS pixels at the far plane. The 3D layer is
 * sampled with a depth-graded offset when the published frame carries
 * depth (CPU-rasterized Z-buffer frames), and with a uniform offset
 * otherwise (GPU frames, W-buffer frames) — the SoulSilver layer-shift
 * model: the 3D world sits behind the screen plane, 2D layers and HUD
 * stay at screen depth. */
bool PbW2_G3RasterComposite(u16 *rgb565, const u8 *window, u8 *planes, u16 *rawColors, u16 *sides, u16 backdrop,
                            const u16 **native, u32 *ink, int eyeShiftPx);
/* Stereo: ask the rasterizer to publish per-pixel depth alongside the
 * colour frame at swap time (CPU Z-buffer frames only). Off by
 * default; present enables it only while the 3D slider is up. */
void PbW2_G3RasterSetDepthPublish(bool enabled);
/* Monotonic counter of published-frame replacements. While it is
 * unchanged, the published 3D layer (PbW2_G3RasterComposite's source)
 * is unchanged. */
unsigned PbW2_G3PublishedGeneration(void);
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
 * (gpu_texel funnels through texture_pixel). The strip contains eight
 * square PLTT16 frames, with 32- or 64-pixel sides.
 * `generation` increments on every identity/palette change and is
 * folded into the PICA cache key so a cached upload can never go
 * stale across a follower change. */
#define PBW2_FOLLOWER_TEX_ADDR 0x7FFF8u
void PbW2_G3FollowerSetTexture(const u8 *indices, const u16 *palNormal, const u16 *palShiny,
                               unsigned frameSize, bool shiny, u32 generation);
