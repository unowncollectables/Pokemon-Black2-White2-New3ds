/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "nitro/os.h"

#include <string.h>

NVRAMConfig PbW2_NvramUserInfo;

void PbW2_NvramHostInit(void) {
    memset(&PbW2_NvramUserInfo, 0, sizeof PbW2_NvramUserInfo);
    /* DS-default touch calibration: maps raw ADC to 256x192 touch space. */
    PbW2_NvramUserInfo.tp.raw_x1 = 0x0068;
    PbW2_NvramUserInfo.tp.raw_y1 = 0x00B4;
    PbW2_NvramUserInfo.tp.dx1 = 0;
    PbW2_NvramUserInfo.tp.dy1 = 0;
    PbW2_NvramUserInfo.tp.raw_x2 = 0x03D8;
    PbW2_NvramUserInfo.tp.raw_y2 = 0x0578;
    PbW2_NvramUserInfo.tp.dx2 = 255;
    PbW2_NvramUserInfo.tp.dy2 = 191;
}