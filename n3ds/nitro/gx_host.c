/* SPDX-License-Identifier: GPL-3.0-or-later */
/* GX_Init does not program a DS display. Register writes from game C land in
 * pbw2_io (see include/types.h). Drawing the logo from those registers is the
 * next step, after NitroFS is on the card. */
#include "nitro/gx.h"

#include <string.h>

extern u8 pbw2_io[];

void GX_Init(void) {
    /* Display control and the two engines' first kilobyte. */
    memset(pbw2_io, 0, 0x2000);
}

s32 GX_VBlankIntr(BOOL enable) {
    (void)enable;
    return 0;
}

s32 GX_HBlankIntr(BOOL enable) {
    (void)enable;
    return 0;
}
