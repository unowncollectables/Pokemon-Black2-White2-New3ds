/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Wide 2D layout engine + Gen 5 tables. See wide_layout.h.
 *
 * Scene facts used below, and their sources (nothing here is guessed):
 * - Overlay 11 is the field-menu overlay group: REQ-11's survey log
 *   (runs/b2w2-tip84/boot1-log.txt) shows overlay 11 loading as the X
 *   menu opens and unloading as it closes, over the field overlay 12.
 *   The X menu and the Bag both live under it.
 * - The Bag swaps the engines: ItemMenuDisp_InitBGs calls
 *   GX_SetDispSelect(GX_DISP_SELECT_SUB_MAIN) (src/app/bag/
 *   itemmenu_disp.c), so with overlay 11 resident and the sub engine
 *   on top (mainUpper == 0) the visible app is the Bag; with the main
 *   engine on top the Bag is closed and the X menu (bottom screen) is
 *   the 2D scene, the top screen being the field's 3D.
 * - Both screens' v1 layouts are the backdrop-continuation shape:
 *   the X menu's tiles sit over a grid backdrop that reaches both
 *   edges, and the Bag's red diamond-dot backdrop is full-bleed with
 *   its panel centred (REQ-11 screenshots menu1/bag1). The centred
 *   content stays exactly where the game puts it; the side strips
 *   continue the backdrop's edge columns, which is SoulSilver's own
 *   treatment for a screen whose panels stay centred (its options
 *   layout is two FILL rows of exactly this shape).
 * - A panel-split layout (MOVE the panel halves apart, COPY a clear
 *   interior column across the gap, per-BG masks) needs the per-BG
 *   assignment of panel / text / backdrop on each screen. That fact
 *   is NOT established for Gen 5's bag or menu (wide-matrix.md 4.5/4.6
 *   list it as the emulator check still owed); the completion survey
 *   filed with this stage asks for it, and the tables below are the
 *   seam it will land in. Battle, Summary and PC boxes have no table
 *   at all: their facts are unestablished (REQ-11 never reached
 *   them), so they keep the centred picture until the survey lands.
 */
#include "wide_layout.h"

#include "scene.h"

#include <stddef.h>

void PbW2_WideBuildColumnMap(const WlPart *parts, unsigned count, unsigned row, short map[WL_WIDE_WIDTH]) {
    for (int wideX = -WL_LEFT; wideX < WL_WIDE_WIDTH - WL_LEFT; ++wideX) {
        short src = wideX < 0 ? 0 : wideX > WL_DS_WIDTH - 1 ? WL_DS_WIDTH - 1 : (short)wideX;
        map[wideX + WL_LEFT] = src;
    }
    if (!parts) return;
    for (unsigned i = 0; i < count; ++i) {
        const WlPart *part = &parts[i];
        if (part->kind == WL_KIND_END) break;
        if (row < (unsigned)part->top || row >= (unsigned)part->bottom) continue;
        if (part->kind == WL_KIND_FILL) {
            for (int wideX = part->x0; wideX < part->x1; ++wideX) {
                if (wideX >= -WL_LEFT && wideX < WL_WIDE_WIDTH - WL_LEFT) map[wideX + WL_LEFT] = part->arg;
            }
        } else if (part->kind == WL_KIND_COPY) {
            for (int wideX = part->x0; wideX < part->x1; ++wideX) {
                int src = part->arg + (wideX - part->x0);
                if (wideX >= -WL_LEFT && wideX < WL_WIDE_WIDTH - WL_LEFT && src >= 0 && src < WL_DS_WIDTH)
                    map[wideX + WL_LEFT] = (short)src;
            }
        } else if (part->kind == WL_KIND_MOVE) {
            for (int srcX = part->x0; srcX < part->x1; ++srcX) {
                int wideX = srcX + part->arg;
                if (wideX >= -WL_LEFT && wideX < WL_WIDE_WIDTH - WL_LEFT && srcX >= 0 && srcX < WL_DS_WIDTH)
                    map[wideX + WL_LEFT] = (short)srcX;
            }
        }
    }
}

void PbW2_WideApplyLayout(const uint16_t *src, uint16_t *dst, const WlPart *parts, unsigned count) {
    short map[WL_WIDE_WIDTH];
    for (unsigned y = 0; y < WL_HEIGHT; ++y) {
        const uint16_t *srcRow = src + y * WL_DS_WIDTH;
        uint16_t *dstRow = dst + y * WL_WIDE_WIDTH;
        PbW2_WideBuildColumnMap(parts, count, y, map);
        for (unsigned x = 0; x < WL_WIDE_WIDTH; ++x) dstRow[x] = srcRow[map[x]];
    }
}

/* The X menu (bottom screen): grid backdrop continues to both edges;
 * the six tiles stay centred at DS size (SoulSilver keeps game panels
 * at DS size and fills around them — wide-matrix.md 4.4/4.10). */
static const WlPart kWlMenuBottom[] = {
    WL_FILL(0, 192, -32, 0, 0),
    WL_FILL(0, 192, 256, 288, 255),
    WL_END,
};

/* The Bag, top screen (sub engine under SUB_MAIN): the description
 * panel stays centred; the diamond-dot backdrop's edge columns run
 * on to the screen edges. */
static const WlPart kWlBagTop[] = {
    WL_FILL(0, 192, -32, 0, 0),
    WL_FILL(0, 192, 256, 288, 255),
    WL_END,
};

/* The Bag, bottom screen (main engine): bag picture and item list
 * stay where the game draws them; the backdrop continues past them. */
static const WlPart kWlBagBottom[] = {
    WL_FILL(0, 192, -32, 0, 0),
    WL_FILL(0, 192, 256, 288, 255),
    WL_END,
};

enum { SCENE_OVERLAY_FIELD_MENU = 11 };

static int s_mainUpper = 1;

void PbW2_WideSetFrameContext(int mainUpper) {
    s_mainUpper = mainUpper;
}

static const WlPart *layout_for(int isTop, unsigned *count, const char **name) {
    uint32_t scene = PbW2_SceneCurrentOverlay();
    if (scene != SCENE_OVERLAY_FIELD_MENU) return NULL;
    if (!s_mainUpper) {
        /* SUB_MAIN under the field-menu overlay: the Bag. */
        if (count) *count = isTop ? (sizeof kWlBagTop / sizeof kWlBagTop[0]) : (sizeof kWlBagBottom / sizeof kWlBagBottom[0]);
        if (name) *name = isTop ? "bag top" : "bag bottom";
        return isTop ? kWlBagTop : kWlBagBottom;
    }
    /* Main engine on top: the top screen is the field (its own 3D wide
     * path owns it); the X menu is the bottom screen's 2D scene. */
    if (isTop) return NULL;
    if (count) *count = sizeof kWlMenuBottom / sizeof kWlMenuBottom[0];
    if (name) *name = "x menu";
    return kWlMenuBottom;
}

const WlPart *PbW2_WideLayoutTop(unsigned *count, const char **name) {
    return layout_for(1, count, name);
}

const WlPart *PbW2_WideLayoutBottom(unsigned *count, const char **name) {
    return layout_for(0, count, name);
}
