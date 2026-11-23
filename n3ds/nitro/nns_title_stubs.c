/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "nitro/fs.h"
#include "nnsys/fnd.h"
#include "nnsys/g2d.h"
#include "nnsys/snd.h"
#include "stub_log.h"

void *NNS_FndAllocFromExpHeapEx(NNSFndHeapHandle heap, u32 size, int alignment) {
    (void)heap;
    (void)size;
    (void)alignment;
    PBW2_STUB_LOG("NNS", "FndAllocFromExpHeapEx");
    return NULL;
}

BOOL NNS_G2dGetUnpackedCellBank(void *file, NNSG2dCellDataBank **cells) {
    (void)file;
    (void)cells;
    PBW2_STUB_LOG("NNS", "G2dGetUnpackedCellBank");
    return FALSE;
}

BOOL NNS_G2dGetUnpackedCharacterData(void *file, NNSG2dCharacterData **character) {
    (void)file;
    (void)character;
    PBW2_STUB_LOG("NNS", "G2dGetUnpackedCharacterData");
    return FALSE;
}

BOOL NNS_G2dGetUnpackedMultiCellBank(void *file, NNSG2dMultiCellDataBank **cells) {
    (void)file;
    (void)cells;
    PBW2_STUB_LOG("NNS", "G2dGetUnpackedMultiCellBank");
    return FALSE;
}

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