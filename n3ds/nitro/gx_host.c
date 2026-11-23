/* SPDX-License-Identifier: GPL-3.0-or-later */
/* GX_Init does not program a DS display. Register writes from game C land in
 * pbw2_io (see include/types.h). The logo and the title background are drawn
 * from NitroFS a/0/2/6 by title_pic.c, not from these registers. */
#include "nitro/gx.h"

#include <string.h>

extern u8 pbw2_io[];

#if defined(PBW2_LINK_GAME)
#include "pad_host.h"
#include "vram_host.h"
#include "nitro/pm.h"

static BOOL s_is_disp_on = TRUE;
static u32 s_disp_mode = GX_DISPMODE_GRAPHICS;
#endif

void GX_Init(void) {
    /* Display control and the two engines' first kilobyte. */
    memset(pbw2_io, 0, 0x2000);
#if defined(PBW2_LINK_GAME)
    PbW2_VramHostInit();
    PbW2_PadHostInit();
#endif
}

s32 GX_VBlankIntr(BOOL enable) {
    (void)enable;
    return 0;
}

s32 GX_HBlankIntr(BOOL enable) {
    (void)enable;
    return 0;
}

void GX_SetVCountEqVal(s32 vcount) {
    (void)vcount;
}

#if defined(PBW2_LINK_GAME)
void GX_DispOff(void) {
    u32 dispcnt = reg_GX_DISPCNT;
    s_is_disp_on = FALSE;
    s_disp_mode = (dispcnt & REG_GX_DISPCNT_MODE_MASK) >> REG_GX_DISPCNT_MODE_SHIFT;
    reg_GX_DISPCNT = dispcnt & ~REG_GX_DISPCNT_MODE_MASK;
    PMi_SetDispOffCount();
}

void GX_DispOn(void) {
    s_is_disp_on = TRUE;
    if (s_disp_mode != 0) {
        reg_GX_DISPCNT = (reg_GX_DISPCNT & ~REG_GX_DISPCNT_MODE_MASK) | (s_disp_mode << REG_GX_DISPCNT_MODE_SHIFT);
    } else {
        reg_GX_DISPCNT |= GX_DISPMODE_GRAPHICS << REG_GX_DISPCNT_MODE_SHIFT;
    }
}

void GX_SetGraphicsMode(GXDispMode dispMode, GXBGMode bgMode, GXBG0As bg0As3D) {
    u32 dispcnt = reg_GX_DISPCNT;
    s_disp_mode = dispMode;
    if (!s_is_disp_on) {
        dispMode = 0;
    }
    reg_GX_DISPCNT = (dispcnt & ~REG_GX_DISPCNT_GRAPHICS_MASK) | (dispMode << REG_GX_DISPCNT_MODE_SHIFT) | bgMode |
                     (bg0As3D << REG_GX_DISPCNT_BG02D3D_SHIFT);
    if (s_disp_mode == 0) {
        s_is_disp_on = FALSE;
    }
}

void GXS_SetGraphicsMode(GXBGMode bgMode) {
    reg_GXS_DB_DISPCNT = (reg_GXS_DB_DISPCNT & ~REG_GXS_DB_DISPCNT_BGMODE_MASK) | bgMode;
}

void GXx_SetMasterBrightness_(vu16 *reg, int brightness) {
    if (brightness == 0) {
        *reg = 0;
    } else if (brightness > 0) {
        *reg = (u16)(REG_GX_MASTER_BRIGHT_MODE_UP | brightness);
    } else {
        *reg = (u16)(REG_GX_MASTER_BRIGHT_MODE_DOWN | -brightness);
    }
}
#endif
