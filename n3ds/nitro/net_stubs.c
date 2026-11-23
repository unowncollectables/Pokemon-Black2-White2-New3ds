/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "gfl/heap.h"
#include "gfl/net.h"
#include "stub_log.h"

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

void GFL_NetErrShowFatal(void) {
    PBW2_STUB_LOG("net", "GFL_NetErrShowFatal");
}

void AssertFailErrorDisp(void) {
    PBW2_STUB_LOG("net", "AssertFailErrorDisp");
}