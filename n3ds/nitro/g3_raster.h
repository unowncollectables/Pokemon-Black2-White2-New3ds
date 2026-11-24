/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include "nitro/fx.h"

void PbW2_G3RasterReset(void);
void PbW2_G3RasterCommand(u32 opcode, const u32 *args, const MtxFx44 *clip, const MtxFx44 *texture);
void PbW2_G3RasterComposite(u16 *rgb565);
