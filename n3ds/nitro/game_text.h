/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include "gfl/bmp.h"

void PbW2_TextMarkGlyph(const GFLBitmap *bitmap, const GFLBitmap *glyph, int x, int y, unsigned width, unsigned height);
void PbW2_TextForgetBitmap(const GFLBitmap *bitmap);
void PbW2_TextUpload(const GFLBitmap *bitmap, const void *base, unsigned first, unsigned size);
u8 PbW2_TextTileRow(const u8 *tile, unsigned size, unsigned row);
