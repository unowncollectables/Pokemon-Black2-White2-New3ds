/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "io_thread.h"

#include "log.h"
#include "workers.h"

#include <3ds.h>

/* Idle until the game's ROM and save reads move off the main thread.
 * It does not open the save. */
static void Io_Thread(void *arg) {
    Worker *worker = arg;
    while (Worker_Run(worker)) {
        svcSleepThread(20000000LL);
    }
}

void Io_Start(void) {
    if (Workers_Start("io", Io_Thread, 0x28, 2, NULL)) {
        Log_Line("3DS", "IO worker idle (no ROM read in this build)");
    }
}
