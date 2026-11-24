/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "nitro/os.h"

#include <string.h>

NVRAMConfig PbW2_NvramUserInfo;

void PbW2_NvramHostInit(void) {
    memset(&PbW2_NvramUserInfo, 0, sizeof PbW2_NvramUserInfo);
    /* Invert the host touch adapter's 16 raw units per game pixel. */
    PbW2_NvramUserInfo.tp.raw_x2 = 255 * 16;
    PbW2_NvramUserInfo.tp.raw_y2 = 191 * 16;
    PbW2_NvramUserInfo.tp.dx2 = 255;
    PbW2_NvramUserInfo.tp.dy2 = 191;
}
