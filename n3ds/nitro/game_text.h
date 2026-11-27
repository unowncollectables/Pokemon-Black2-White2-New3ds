/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include "gfl/bmp.h"

void PbW2_TextMarkGlyph(const GFLBitmap *bitmap, const GFLBitmap *glyph, int x, int y, unsigned width, unsigned height);
void PbW2_TextForgetBitmap(const GFLBitmap *bitmap);
void PbW2_TextUpload(const GFLBitmap *bitmap, const void *base, unsigned first, unsigned size);
/* A visible glyph pixel and, when available, its original background and offset in the glyph. */
#define PBW2_TEXT_INK (1u << 30)
#define PBW2_TEXT_NATIVE (1u << 31)
/* The glyph pixel sits over a transparent (palette index 0) bitmap pixel, so
 * there is no in-bitmap background colour to restore: the compositor must
 * substitute the composed colour beneath the text layer instead of
 * palette[0], whose stored RGB is meaningless for a transparent entry.
 * Only carried in the per-tile details word; never in a finished ink word. */
#define PBW2_TEXT_CLEAR_UNDER (1u << 29)

/* Every fitted game screen uses the same 5:4 scale on both axes. */
static inline int PbW2_TextNativeCoordinate(int source, unsigned glyphOffset) {
    return (5 * (source - (int)glyphOffset) + 2) / 4 + (int)glyphOffset;
}

static inline unsigned PbW2_TextSourceCoordinate(unsigned lcd) {
    return (4 * lcd + 2) / 5;
}

u32 PbW2_TextVramRevision(void);
u8 PbW2_TextTileRow(const u8 *tile, const u8 *liveAddress, unsigned size, unsigned row, u32 details[8]);
