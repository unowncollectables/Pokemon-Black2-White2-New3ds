/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "nitro/nvram.h"
#include "nitro/os.h"
#include "stub_log.h"

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

/* DS NVRAM bottom (Build 22 hotfix). There is no DS NVRAM on this
 * platform and no ARM7 to answer the SDK's SPI-over-PXI protocol: the
 * host PXI layer never delivers the completion callback the SDK's
 * synchronous NVRAM calls wait on, so the real lib/nitro nvram.o
 * blocks forever in OS_ReceiveMessage — which is exactly where
 * Build 21's boot died (GameInit -> GFL_NetBoot -> DWC library start
 * -> DWC_BM_Init -> NCFG check -> NVRAM read). These host definitions
 * satisfy the link closure the same way rtc_host.c's RTC_Init does,
 * so the SDK implementation leaves the link, and every NVRAM access
 * fails honestly and immediately (NVRAM_RESULT_FATAL_ERROR): DWC's
 * settings check reports READ_ERROR and its callers take their
 * documented failure paths instead of hanging. A real bottom (a
 * fabricated settings image) is Wi-Fi Stage 2 work. */
void NVRAM_Init(void) {
}

NVRAMResult NVRAM_ReadDataBytes(u32 address, u32 size, void *buf) {
    (void)address;
    (void)size;
    (void)buf;
    PBW2_STUB_LOG("NVRAM", "no DS NVRAM on this platform; read fails fast");
    return NVRAM_RESULT_FATAL_ERROR;
}

NVRAMResult NVRAM_WriteDataBytes(u32 address, u32 size, const void *src) {
    (void)address;
    (void)size;
    (void)src;
    PBW2_STUB_LOG("NVRAM", "no DS NVRAM on this platform; write fails fast");
    return NVRAM_RESULT_FATAL_ERROR;
}

NVRAMResult NVRAM_VerifyDataBytes(u32 address, u32 size, const void *src) {
    (void)address;
    (void)size;
    (void)src;
    PBW2_STUB_LOG("NVRAM", "no DS NVRAM on this platform; verify fails fast");
    return NVRAM_RESULT_FATAL_ERROR;
}
