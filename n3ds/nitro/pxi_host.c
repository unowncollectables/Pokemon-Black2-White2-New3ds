/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "nitro/pxi.h"
#include "stub_log.h"

#include <string.h>

void PbW2_PxiServiceArm7(int fifoTag, u32 data);

static PXIFifoCallback s_recv[PXI_MAX_FIFO_TAG];
static int s_pending_fs_cmd;

static void deliver(int fifoTag, u32 data, BOOL err) {
    if (fifoTag < 0 || fifoTag >= PXI_MAX_FIFO_TAG) {
        return;
    }
    if (s_recv[fifoTag]) {
        s_recv[fifoTag]((PXIFifoTag)fifoTag, data, err);
    }
}

void PXI_Init(void) {
    memset(s_recv, 0, sizeof s_recv);
    s_pending_fs_cmd = -1;
    PBW2_STUB_LOG("PXI", "Init");
}

void PXI_InitFifo(void) {
}

void PXI_SetFifoRecvCallback(int fifoTag, PXIFifoCallback callback) {
    if (fifoTag >= 0 && fifoTag < PXI_MAX_FIFO_TAG) {
        s_recv[fifoTag] = callback;
    }
}

int PXI_SendWordByFifo(int fifoTag, u32 data, BOOL err) {
    if (fifoTag == PXI_FIFO_TAG_FS) {
        s_pending_fs_cmd = (int)data;
    }
    if (err) {
        u32 cmd = (fifoTag == PXI_FIFO_TAG_FS && s_pending_fs_cmd >= 0) ? (u32)s_pending_fs_cmd : data;
        PbW2_PxiServiceArm7(fifoTag, cmd);
        deliver(fifoTag, data, TRUE);
    }
    return PXI_FIFO_SUCCESS;
}

void PXIi_HandlerRecvFifoNotEmpty(void) {
}

BOOL PXI_IsCallbackReady(int fifoTag, PXIProc processor) {
    (void)fifoTag;
    (void)processor;
    return TRUE;
}