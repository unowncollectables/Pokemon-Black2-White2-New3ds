/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "nitro/hw.h"
#include "nitro/mi.h"

#include <stdio.h>
#include <string.h>

u8 PbW2_RomHeaderBuf[HW_CARD_ROM_HEADER_SIZE];
u8 PbW2_CardRomHeader[HW_CARD_ROM_HEADER_SIZE];

#if defined(PBW2_WHITE2)
#define PBW2_SD_DIR "sdmc:/3ds/white2_3ds"
#else
#define PBW2_SD_DIR "sdmc:/3ds/black2_3ds"
#endif

void PbW2_RomHeaderHostInit(void) {
    char path[128];
    FILE *fp;

    memset(PbW2_RomHeaderBuf, 0xFF, sizeof PbW2_RomHeaderBuf);
    memset(PbW2_CardRomHeader, 0xFF, sizeof PbW2_CardRomHeader);
    snprintf(path, sizeof path, "%s/rom_header.bin", PBW2_SD_DIR);
    fp = fopen(path, "rb");
    if (fp) {
        (void)fread(PbW2_RomHeaderBuf, 1, sizeof PbW2_RomHeaderBuf, fp);
        fclose(fp);
        MI_CpuCopy8(PbW2_RomHeaderBuf, PbW2_CardRomHeader, sizeof PbW2_CardRomHeader);
    }
}
