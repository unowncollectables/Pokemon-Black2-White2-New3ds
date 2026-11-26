/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include "nitro/fx.h"
#include <stdbool.h>

enum { PBW2_NATIVE_TOP_WIDTH = 400, PBW2_NATIVE_TOP_HEIGHT = 240 };

void PbW2_G3SetWide(bool enabled);
bool PbW2_G3WideCamera(void);

void PbW2_G3RasterReset(void);
void PbW2_G3RasterCommand(u32 opcode, const u32 *args, const MtxFx44 *clip,
                         const MtxFx44 *texture, const MtxFx44 *vector);
bool PbW2_G3RasterComposite(u16 *rgb565, const u8 *window, u8 *planes, u16 *rawColors, u16 *sides, u16 backdrop, u16 *native, bool *haveNative);
/* Copy the DS 3D layer, including the validity bit used for capture. */
void PbW2_G3RasterCapture(u16 *rgb555);
/* Called on the geometry thread before an upload overwrites LCDC memory. */
void PbW2_G3RasterBeforeLcdcWrite(u32 address, u32 bytes);
