/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Start-menu proc tables that would pull wifi/app/communication; stubbed for title link. */
#include "gfl/proc.h"
#include "stub_log.h"

static BOOL PbW2_StubProcInit(GameProc *proc, u32 *state, void *param, void *work) {
    (void)proc;
    (void)state;
    (void)param;
    (void)work;
    PBW2_STUB_LOG("proc", "init");
    return TRUE;
}

static BOOL PbW2_StubProcMain(GameProc *proc, u32 *state, void *param, void *work) {
    (void)proc;
    (void)state;
    (void)param;
    (void)work;
    return TRUE;
}

static BOOL PbW2_StubProcExit(GameProc *proc, u32 *state, void *param, void *work) {
    (void)proc;
    (void)state;
    (void)param;
    (void)work;
    return TRUE;
}

#define PBW2_STUB_PROC(name) \
    const GameProcFunctions name = { PbW2_StubProcInit, PbW2_StubProcMain, PbW2_StubProcExit }

PBW2_STUB_PROC(GAME_SYNC_SETTINGS_PROC_FUNCTIONS);
