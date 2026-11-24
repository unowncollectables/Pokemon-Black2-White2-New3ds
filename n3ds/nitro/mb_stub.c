/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "nitro/mb.h"
#include "stub_log.h"

/* Multiboot (DS download play): no DS wireless download hardware on this
 * platform, so no child can ever connect and no parent session can start.
 * The BOOL/int returns are honest "unavailable" answers: MB_Init and
 * MB_StartParent fail (mbp.c checks != MB_ERRCODE_SUCCESS and takes its
 * error path), queries report nothing bootable / no segments. */
int MB_Init(void *work, const MBUserInfo *user, u32 ggid, u32 tgid, u32 dma) {
    (void)work;
    (void)user;
    (void)ggid;
    (void)tgid;
    (void)dma;
    PBW2_STUB_LOG("mb", "MB_Init: no download play on this platform");
    return -1;
}

int MB_StartParent(int channel) {
    (void)channel;
    PBW2_STUB_LOG("mb", "MB_StartParent: no download play on this platform");
    return -1;
}

void MB_End(void) {
    PBW2_STUB_LOG("mb", "MB_End");
}

void MB_DisconnectChild(u16 aid) {
    (void)aid;
    PBW2_STUB_LOG("mb", "MB_DisconnectChild");
}

BOOL MB_SetParentCommParam(u16 sendSize, u16 maxChildren) {
    (void)sendSize;
    (void)maxChildren;
    PBW2_STUB_LOG("mb", "MB_SetParentCommParam (FALSE)");
    return FALSE;
}

const MBUserInfo *MB_CommGetChildUser(u16 childAid) {
    (void)childAid;
    PBW2_STUB_LOG("mb", "MB_CommGetChildUser (NULL)");
    return NULL;
}

BOOL MB_CommIsBootable(u16 childAid) {
    (void)childAid;
    return FALSE;
}

BOOL MB_CommResponseRequest(u16 childAid, MBCommResponseRequestType ack) {
    (void)childAid;
    (void)ack;
    return FALSE;
}

void MB_CommSetParentStateCallback(MBCommPStateCallback callback) {
    (void)callback;
    PBW2_STUB_LOG("mb", "MB_CommSetParentStateCallback");
}

u32 MB_GetSegmentLength(FSFile *file) {
    (void)file;
    return 0;
}

BOOL MB_ReadSegment(FSFile *file, void *buf, u32 len) {
    (void)file;
    (void)buf;
    (void)len;
    return FALSE;
}

BOOL MB_RegisterFile(const MBGameRegistry *gameReg, const void *buf) {
    (void)gameReg;
    (void)buf;
    return FALSE;
}
