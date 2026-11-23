/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "gfl/heap.h"
#include "gfl/net.h"
#include "gfl/net_whpipe.h"
#include "stub_log.h"
#include "types.h"

void GFL_NetBoot(int libraryHeapId, int unused, HeapID parentHeapId, HeapID heapId) {
    (void)libraryHeapId;
    (void)unused;
    (void)parentHeapId;
    (void)heapId;
    PBW2_STUB_LOG("net", "GFL_NetBoot");
}

void GFL_NetHeapInit(void) {
    PBW2_STUB_LOG("net", "GFL_NetHeapInit");
}

void GFL_NetErrInit(u32 heapId) {
    (void)heapId;
    PBW2_STUB_LOG("net", "GFL_NetErrInit");
}

void GFL_NetErrUpdate(void) {
}

BOOL GFL_NetErrCheck(void) {
    return FALSE;
}

BOOL GFL_NetIsWifi(void) {
    return FALSE;
}

BOOL GFL_NetIsInfrared(void) {
    return FALSE;
}

BOOL GFL_NetIsInitialized(void) {
    return FALSE;
}

BOOL GFL_NetIsNotInitialized(void) {
    return TRUE;
}

BOOL GFL_NetIsEnded(void) {
    return TRUE;
}

BOOL GFL_NetIsParent(void) {
    return FALSE;
}

u8 GFL_NetGetGameCommandBase(void) {
    return 0;
}

BOOL NetWhpipe_IsScanning(void) {
    return FALSE;
}

void GFL_NetErrShowFatal(void) {
    PBW2_STUB_LOG("net", "GFL_NetErrShowFatal");
}

void AssertFailErrorDisp(void) {
    PBW2_STUB_LOG("net", "AssertFailErrorDisp");
}