/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "nitro/os.h"

/* Stand-in for DS DTCM (16 KiB). HW_DTCM is (u32)SDK_AUTOLOAD_DTCM_START. */
u32 SDK_AUTOLOAD_DTCM_START[0x4000 / 4] ATTRIBUTE_ALIGN(32);

void PbW2_DtcmHostInit(void) {
    OS_SetArenaLo(OS_ARENA_DTCM, SDK_AUTOLOAD_DTCM_START);
    OS_SetArenaHi(OS_ARENA_DTCM, (u8 *)SDK_AUTOLOAD_DTCM_START + sizeof SDK_AUTOLOAD_DTCM_START);
}