/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "io_thread.h"

#include "log.h"
#include "save_backup.h"
#include "workers.h"

#include <3ds.h>

/* Background housekeeping on core 2: currently the save-backup
 * poll. It opens the save read-only, and only to copy it after the
 * game has finished writing (see save_backup.c). */
static void Io_Thread(void *arg) {
    Worker *worker = arg;
    while (Worker_Run(worker)) {
        SaveBackup_Poll();
        svcSleepThread(20000000LL);
    }
}

void Io_Start(void) {
    if (Workers_Start("io", Io_Thread, 0x28, 2, NULL)) {
        Log_Line("3DS", "IO worker running (save backup poll)");
    }
}
