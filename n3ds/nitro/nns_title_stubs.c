/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "nitro/fs.h"
#include "nnsys/snd.h"
#include "stub_log.h"

u32 FS_SetDefaultDMA(u32 dma) {
    (void)dma;
    PBW2_STUB_LOG("FS", "SetDefaultDMA");
    return 0;
}

BOOL NNS_SndArcInit(NNSSndArc *arc, const char *filePath, NNSSndHeapHandle heap, BOOL symbolLoadFlag) {
    (void)arc;
    (void)filePath;
    (void)heap;
    (void)symbolLoadFlag;
    PBW2_STUB_LOG("SND", "SndArcInit");
    return TRUE;
}

void NNS_SndArcSetup(void) {
    PBW2_STUB_LOG("SND", "SndArcSetup");
}