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
