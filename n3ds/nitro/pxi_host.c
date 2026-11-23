/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "nitro/pxi.h"
#include "stub_log.h"

void PXI_Init(void) {
    PBW2_STUB_LOG("PXI", "Init");
}

void PXI_InitFifo(void) {
}

void PXI_SetFifoRecvCallback(int fifoTag, PXIFifoCallback callback) {
    (void)fifoTag;
    (void)callback;
}

int PXI_SendWordByFifo(int fifoTag, u32 data, BOOL err) {
    (void)fifoTag;
    (void)data;
    (void)err;
    return 0;
}

void PXIi_HandlerRecvFifoNotEmpty(void) {
}

BOOL PXI_IsCallbackReady(int fifoTag, PXIProc processor) {
    (void)fifoTag;
    (void)processor;
    return TRUE;
}