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

void GFL_NetErrShowFatal(void) {
    PBW2_STUB_LOG("net", "GFL_NetErrShowFatal");
}

/* No network on this platform: timing sync and searches never complete.
 * IsTimingRecv returns FALSE so callers that wait on a peer's timing do
 * not proceed on phantom data. */
void GFL_NetErrClear(void) {
    PBW2_STUB_LOG("net", "GFL_NetErrClear");
}

void GFL_NetErrMarkShown(void) {
    PBW2_STUB_LOG("net", "GFL_NetErrMarkShown");
}

void GFL_NetStartSearch(int unused) {
    (void)unused;
    PBW2_STUB_LOG("net", "GFL_NetStartSearch");
}

void GFL_NetSendTiming(NetHandle *handle, u16 timing) {
    (void)handle;
    (void)timing;
    PBW2_STUB_LOG("net", "GFL_NetSendTiming");
}

BOOL GFL_NetIsTimingRecv(NetHandle *handle, u16 timing) {
    (void)handle;
    (void)timing;
    return FALSE;
}

void GFL_NetSetDisconnectError(BOOL isError) {
    (void)isError;
    PBW2_STUB_LOG("net", "GFL_NetSetDisconnectError");
}

/* Slice 2e: the rest of GFL net (src/gfl/net.c + src/system/net_err.c).
 * The real implementations wrap DWC/WM, which do not exist on this
 * platform, so the whole family is host stubs. BOOL/int returns are
 * honest "no network" answers: nothing is negotiated, no timing is
 * received, the send queue is empty (nothing queued), no friends, no
 * parent packet. */
void GFL_NetErrAbort(void) {
    PBW2_STUB_LOG("net", "GFL_NetErrAbort");
}

void GFL_NetErrApplyBrightness(void) {
    PBW2_STUB_LOG("net", "GFL_NetErrApplyBrightness");
}

void GFL_NetErrClearKeepDark(void) {
    PBW2_STUB_LOG("net", "GFL_NetErrClearKeepDark");
}

void GFL_NetErrRecord(void) {
    PBW2_STUB_LOG("net", "GFL_NetErrRecord");
}

void GFL_NetErrRecordWifi(u32 message) {
    (void)message;
    PBW2_STUB_LOG("net", "GFL_NetErrRecordWifi");
}

void GFL_NetErrSetKeepDark(BOOL keepDark) {
    (void)keepDark;
    PBW2_STUB_LOG("net", "GFL_NetErrSetKeepDark");
}

void GFL_NetErrShowScreen(void) {
    PBW2_STUB_LOG("net", "GFL_NetErrShowScreen");
}

void GFL_NetErrShowWifi(u32 message) {
    (void)message;
    PBW2_STUB_LOG("net", "GFL_NetErrShowWifi");
}

BOOL GFL_NetErrShutdown(void) {
    PBW2_STUB_LOG("net", "GFL_NetErrShutdown (FALSE)");
    return FALSE;
}

int GFL_NetGetFriendData(void) {
    PBW2_STUB_LOG("net", "GFL_NetGetFriendData (0)");
    return 0;
}

u16 GFL_NetGetParentPacketSize(void) {
    PBW2_STUB_LOG("net", "GFL_NetGetParentPacketSize (0)");
    return 0;
}

int GFL_NetGetUserData(void) {
    PBW2_STUB_LOG("net", "GFL_NetGetUserData (0)");
    return 0;
}

BOOL GFL_NetIsNegotiated(u8 netId) {
    (void)netId;
    PBW2_STUB_LOG("net", "GFL_NetIsNegotiated (FALSE)");
    return FALSE;
}

BOOL GFL_NetIsSendQueueEmpty(NetHandle *handle) {
    (void)handle;
    /* TRUE: the queue is empty because nothing was ever queued. */
    return TRUE;
}

void GFL_NetIwChangeoverConnect(void (*callback)(void *work), void (*linkChanged)(void *work, BOOL linked),
                                void (*irEnded)(void *work)) {
    (void)callback;
    (void)linkChanged;
    (void)irEnded;
    PBW2_STUB_LOG("net", "GFL_NetIwChangeoverConnect");
}

BOOL GFL_NetIwFinishCollecting(void) {
    PBW2_STUB_LOG("net", "GFL_NetIwFinishCollecting (FALSE)");
    return FALSE;
}

void GFL_NetIwGetCollectedMac(u8 *mac, int index) {
    (void)index;
    if (mac != NULL) {
        int i;
        for (i = 0; i < 6; i++) {
            mac[i] = 0;
        }
    }
    PBW2_STUB_LOG("net", "GFL_NetIwGetCollectedMac (no peers)");
}

u8 GFL_NetIwGetLinkedCount(void) {
    PBW2_STUB_LOG("net", "GFL_NetIwGetLinkedCount (0)");
    return 0;
}

void GFL_NetIwOverrideNetId(int netId) {
    (void)netId;
    PBW2_STUB_LOG("net", "GFL_NetIwOverrideNetId");
}

void GFL_NetIwSetGameCommandBase(int gameCommandBase) {
    (void)gameCommandBase;
    PBW2_STUB_LOG("net", "GFL_NetIwSetGameCommandBase");
}

void GFL_NetIwSetPeerFilter(GFLNetPeerFilter filter) {
    (void)filter;
    PBW2_STUB_LOG("net", "GFL_NetIwSetPeerFilter");
}

void GFL_NetMoveIcon(int x, int y) {
    (void)x;
    (void)y;
    PBW2_STUB_LOG("net", "GFL_NetMoveIcon");
}

void GFL_NetNotifyError(int error) {
    (void)error;
    PBW2_STUB_LOG("net", "GFL_NetNotifyError");
}

void GFL_NetNotifyFatalError(int error) {
    (void)error;
    PBW2_STUB_LOG("net", "GFL_NetNotifyFatalError");
}

void GFL_NetResetIconPos(void) {
    PBW2_STUB_LOG("net", "GFL_NetResetIconPos");
}

void GFL_NetSetAcceptConnect(NetHandle *handle, int accept) {
    (void)handle;
    (void)accept;
    PBW2_STUB_LOG("net", "GFL_NetSetAcceptConnect");
}

void GFL_NetSetChangeoverMode(int mode, int useParentMac, const u8 *mac) {
    (void)mode;
    (void)useParentMac;
    (void)mac;
    PBW2_STUB_LOG("net", "GFL_NetSetChangeoverMode");
}

void GFL_NetSetGameCommandBase(u8 gameCommandBase, u8 unused) {
    (void)gameCommandBase;
    (void)unused;
    PBW2_STUB_LOG("net", "GFL_NetSetGameCommandBase");
}

void GFL_NetSetInitData(const GFLNetInitData *pNetInit) {
    (void)pNetInit;
    PBW2_STUB_LOG("net", "GFL_NetSetInitData");
}

void GFL_NetSetMPSendEnabled(BOOL enabled) {
    (void)enabled;
    PBW2_STUB_LOG("net", "GFL_NetSetMPSendEnabled");
}

void GFL_NetSetRejectNegotiated(BOOL reject) {
    (void)reject;
    PBW2_STUB_LOG("net", "GFL_NetSetRejectNegotiated");
}

void GFL_NetSetWork(void *work) {
    (void)work;
    PBW2_STUB_LOG("net", "GFL_NetSetWork");
}

u8 GflNet_GetFriendListNum(void) {
    PBW2_STUB_LOG("net", "GflNet_GetFriendListNum (0)");
    return 0;
}

/* The error-display code asks for the server's message for a result code.
 * No server is ever contacted, so there is no message. */
u32 GFL_NetErrGetServerMessage(int result) {
    (void)result;
    PBW2_STUB_LOG("net", "GFL_NetErrGetServerMessage (0)");
    return 0;
}

void GflNet_SetAutoExit(u8 autoExit) {
    (void)autoExit;
    PBW2_STUB_LOG("net", "GflNet_SetAutoExit");
}

void AssertFailErrorDisp(void) {
    PBW2_STUB_LOG("net", "AssertFailErrorDisp");
}