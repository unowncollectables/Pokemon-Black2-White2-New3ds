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
    /* No ARM7 player in this milestone, so NDSP is not started. Azahar has no
     * DSP firmware; calling ndspInit there fails and is not a port fault. */
    s_ndsp = false;
    Log_Line("AUDIO", "silent (no ARM7 player this milestone)");
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
