/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "audio.h"
#include "nitro/fs.h"
#include "nnsys/snd.h"
#include "snd_arc.h"
#include "stub_log.h"

static const char *s_archive;
static NNSSndStrmHandle *s_streamHandle;

u32 FS_SetDefaultDMA(u32 dma) {
    (void)dma;
    PBW2_STUB_LOG("FS", "SetDefaultDMA");
    return 0;
}

BOOL NNS_SndArcInit(NNSSndArc *arc, const char *filePath, NNSSndHeapHandle heap, BOOL symbolLoadFlag) {
    (void)arc;
    s_archive = filePath;
    SndArc_SetPath(filePath);
    (void)heap;
    (void)symbolLoadFlag;
    PBW2_STUB_LOG("SND", "SndArcInit");
    return TRUE;
}

void NNS_SndArcStrmInit(u32 threadPrio, NNSSndHeapHandle heap) {
    (void)threadPrio;
    (void)heap;
}

BOOL NNS_SndArcStrmAllocChannel(int playerNo) {
    return playerNo == 0;
}

void NNS_SndArcStrmFreeChannel(int playerNo) {
    if (playerNo == 0) {
        Audio_StreamStop(0);
        s_streamHandle = NULL;
    }
}

BOOL NNS_SndArcStrmStart(NNSSndStrmHandle *handle, int strmNo, u32 offset) {
    if (strmNo < 0 || !Audio_StreamStart(s_archive, (unsigned)strmNo, offset)) return FALSE;
    s_streamHandle = handle;
    return TRUE;
}

void NNS_SndArcStrmStartPrepared(NNSSndStrmHandle *handle) {
    (void)handle;
}

void NNS_SndArcStrmStop(NNSSndStrmHandle *handle, int fadeFrames) {
    if (handle == s_streamHandle) Audio_StreamStop(fadeFrames > 0 ? (unsigned)fadeFrames : 0);
}

void NNS_SndStrmHandleInit(NNSSndStrmHandle *handle) {
    handle->player = NULL;
}

void NNS_SndStrmHandleRelease(NNSSndStrmHandle *handle) {
    if (handle == s_streamHandle) s_streamHandle = NULL;
    handle->player = NULL;
}

u32 NNS_SndArcStrmGetCurrentPlayingPos(NNSSndStrmHandle *handle) {
    return handle == s_streamHandle ? Audio_StreamPosition() : 0;
}

void NNS_SndArcSetup(void) {
    PBW2_STUB_LOG("SND", "SndArcSetup");
}
