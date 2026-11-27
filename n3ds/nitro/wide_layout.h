/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Wide 2D layouts (port-over Stage 4, W2/W3): per-scene part tables that
 * turn a centred 256-wide DS picture into a 320-wide one (DS columns
 * -32..287), in the shape of SoulSilver's widecomp layouts (GPL-3.0,
 * ss3ds_widelayouts.h). A part is one row of the table:
 *
 *   WL_FILL(top, bottom, x0, x1, col): wide columns x0..x1-1 of rows
 *       top..bottom-1 show the picture's column col.
 *   WL_COPY(top, bottom, x0, x1, col): wide columns x0..x1-1 show the
 *       picture's columns col, col+1, ... (textured continuation).
 *   WL_MOVE(top, bottom, x0, x1, dx): the picture's columns x0..x1 move
 *       dx px in wide space (their old columns fall back to the
 *       edge-column default unless another part covers them).
 *
 * Columns not covered by any part keep the default SoulSilver gives a
 * screen with no layout: the picture's edge columns repeat into the
 * strips. Parts apply in table order; later parts win.
 *
 * This is a screen-level engine: it remaps the composed picture, so a
 * part cannot separate two layers that share a column. Layouts are
 * therefore authored only where that is safe (a centred panel over a
 * continuing backdrop); screens whose facts are not established get no
 * table and keep today's centred picture. See wide_layout.c. */
#pragma once

#include <stdbool.h>
#include <stdint.h>

enum {
    WL_KIND_END = 0,
    WL_KIND_MOVE = 1,
    WL_KIND_FILL = 2,
    WL_KIND_COPY = 3,
};

typedef struct {
    short kind;
    short top, bottom; /* rows [top, bottom) */
    short x0, x1;      /* FILL/COPY: wide columns [x0, x1); MOVE: DS columns [x0, x1) */
    short arg;         /* FILL: source column; COPY: first source column; MOVE: dx */
} WlPart;

#define WL_FILL(top, bottom, x0, x1, col) { WL_KIND_FILL, (top), (bottom), (x0), (x1), (col) }
#define WL_COPY(top, bottom, x0, x1, col) { WL_KIND_COPY, (top), (bottom), (x0), (x1), (col) }
#define WL_MOVE(top, bottom, x0, x1, dx)  { WL_KIND_MOVE, (top), (bottom), (x0), (x1), (dx) }
#define WL_END                            { WL_KIND_END, 0, 0, 0, 0, 0 }

enum { WL_WIDE_WIDTH = 320, WL_DS_WIDTH = 256, WL_HEIGHT = 192, WL_LEFT = 32 };

/* Build one row's column map: map[wideX + WL_LEFT] = DS source column. */
void PbW2_WideBuildColumnMap(const WlPart *parts, unsigned count, unsigned row, short map[WL_WIDE_WIDTH]);

/* Expand a composed 256x192 picture into a 320x192 wide picture. */
void PbW2_WideApplyLayout(const uint16_t *src, uint16_t *dst, const WlPart *parts, unsigned count);

/* Per-frame context from the compositor: which engine is on top. */
void PbW2_WideSetFrameContext(int mainUpper);

/* The layout for the physical top / bottom screen this frame, or NULL
 * when the current scene has none (or must decline). The scene is the
 * W1 overlay identity plus the display-select signature; *name, when
 * non-NULL, receives a static scene name for the log. */
const WlPart *PbW2_WideLayoutTop(unsigned *count, const char **name);
const WlPart *PbW2_WideLayoutBottom(unsigned *count, const char **name);
