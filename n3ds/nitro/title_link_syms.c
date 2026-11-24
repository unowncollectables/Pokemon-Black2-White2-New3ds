/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Data symbols that must not be emitted as void function stubs in link_stubs.c. */
#include "dsprot/dsprot.h"
#include "nitro/gx.h"
#include "nitro/os.h"
#include "nnsys/g2d.h"
#include "nnsys/g3d.h"
#include "nnsys/gfd.h"
#include "gfl/proc.h"

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

/* NAME_ENTRY_PROC_FUNCTIONS, NAME_ENTRY_SOUNDS and NAME_ENTRY_SOUND_COUNT
 * are the real ones from src/app/name_input/name_input.c now (that slice
 * is in the link closure). FIELDMAP_PROC_FUNCS is the real one from
 * src/field/map/fieldmap_proc.c now (Slice 2) — no stub here. */