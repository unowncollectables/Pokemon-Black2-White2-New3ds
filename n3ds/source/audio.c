/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "audio.h"

#include "log.h"
#include "workers.h"

#include <3ds.h>
#include <3ds/ndsp/ndsp.h>

/* The ARM7 sound driver is not decompiled. This phase is silent. The thread
 * is where a later NDSP player will run. ndspExit is called once, on the
 * main thread, after the worker has been joined. */
static bool s_ndsp;

static void Audio_Thread(void *arg) {
    Worker *worker = arg;
    while (Worker_Run(worker)) {
        svcSleepThread(10000000LL);
    }
}

void Audio_Start(void) {
    Result rc = ndspInit();
    if (R_FAILED(rc)) {
        s_ndsp = false;
        Log_Line("AUDIO", "ndspInit failed %08lX, silent", (unsigned long)rc);
    } else {
        s_ndsp = true;
        Log_Line("AUDIO", "ndsp up, silent (no ARM7 player yet)");
    }
    Workers_Start("audio", Audio_Thread, 0x20, 2, NULL);
}

void Audio_NdspExit(void) {
    if (!s_ndsp) {
        return;
    }
    ndspExit();
    s_ndsp = false;
    Log_Line("AUDIO", "ndspExit");
}
