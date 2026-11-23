/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Data symbols that must not be emitted as void function stubs in link_stubs.c. */
#include "dsprot/dsprot.h"
#include "nitro/gx.h"
#include "nitro/os.h"
#include "nnsys/g2d.h"
#include "nnsys/g3d.h"
#include "nnsys/gfd.h"
#include "gfl/proc.h"

const u16 NNSi_objSizeWTbl[3][4] = {
    { 8, 16, 32, 64 },
    { 16, 32, 32, 64 },
    { 8, 8, 16, 32 },
};

const u16 NNSi_objSizeHTbl[3][4] = {
    { 8, 16, 32, 64 },
    { 8, 8, 16, 32 },
    { 16, 32, 32, 64 },
};

u16 GXi_VRamLockId;
u32 GXi_DmaId = GX_DEFAULT_DMAID;
volatile u64 OSi_TickCounter;

NNSG3dGlb NNS_G3dGlb;

static void *DSProt_StubNop(void *a, void *b) {
    (void)a;
    (void)b;
    return NULL;
}

u32 DSProt_NopIndex;
DSProtCallback DSProt_Callbacks[2] = { DSProt_StubNop, DSProt_StubNop };

static NNSGfdTexKey StubAllocTexVram(u32 szByte, BOOL is4x4comp, u32 opt) {
    (void)szByte;
    (void)is4x4comp;
    (void)opt;
    return 0;
}

static int StubFreeTexVram(NNSGfdTexKey key) {
    (void)key;
    return 0;
}

static NNSGfdPlttKey StubAllocPlttVram(u32 szByte, BOOL is4pltt, u32 opt) {
    (void)szByte;
    (void)is4pltt;
    (void)opt;
    return 0;
}

static int StubFreePlttVram(NNSGfdPlttKey key) {
    (void)key;
    return 0;
}

NNSGfdFuncAllocTexVram NNS_GfdDefaultFuncAllocTexVram = StubAllocTexVram;
NNSGfdFuncFreeTexVram NNS_GfdDefaultFuncFreeTexVram = StubFreeTexVram;
NNSGfdFuncAllocPlttVram NNS_GfdDefaultFuncAllocPlttVram = StubAllocPlttVram;
NNSGfdFuncFreePlttVram NNS_GfdDefaultFuncFreePlttVram = StubFreePlttVram;

static BOOL StubProcInit(GameProc *proc, u32 *state, void *param, void *work) {
    (void)proc;
    (void)state;
    (void)param;
    (void)work;
    return FALSE;
}

static BOOL StubProcMain(GameProc *proc, u32 *state, void *param, void *work) {
    (void)proc;
    (void)state;
    (void)param;
    (void)work;
    return FALSE;
}

static BOOL StubProcExit(GameProc *proc, u32 *state, void *param, void *work) {
    (void)proc;
    (void)state;
    (void)param;
    (void)work;
    return FALSE;
}

const GameProcFunctions NAME_ENTRY_PROC_FUNCTIONS = { StubProcInit, StubProcMain, StubProcExit };
const GameProcFunctions FIELDMAP_PROC_FUNCS = { StubProcInit, StubProcMain, StubProcExit };

const u32 NAME_ENTRY_SOUNDS[6];
const u32 NAME_ENTRY_SOUND_COUNT = 0;