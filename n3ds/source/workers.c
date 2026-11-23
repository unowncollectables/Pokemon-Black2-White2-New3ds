/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "workers.h"

#include "log.h"
#include "pbw2_port.h"

#include <stdio.h>

struct Worker {
    Thread thread;
    volatile bool run;
    const char *name;
    int core;
    bool started;
};

/* Main is one thread. The rest of the budget is for workers. */
#define PBW2_WORKER_MAX 8

static Worker s_workers[PBW2_WORKER_MAX];
static int s_count;

bool Workers_Start(const char *name, ThreadFunc fn, int prio, int core, Worker **out) {
    if (s_count >= PBW2_WORKER_MAX || s_count + 1 >= PBW2_THREAD_BUDGET) {
        Log_Line("3DS", "refused worker %s (budget %d)", name, PBW2_THREAD_BUDGET);
        return false;
    }
    Worker *worker = &s_workers[s_count];
    worker->run = true;
    worker->name = name;
    worker->core = core;
    worker->thread = threadCreate(fn, worker, 32 * 1024, prio, core, false);
    if (!worker->thread && core != 0) {
        Log_Line("3DS", "core %d refused for %s, using core 0", core, name);
        worker->core = 0;
        worker->thread = threadCreate(fn, worker, 32 * 1024, prio, 0, false);
    }
    if (!worker->thread) {
        Log_Line("3DS", "threadCreate failed for %s", name);
        worker->run = false;
        return false;
    }
    worker->started = true;
    s_count++;
    if (out) {
        *out = worker;
    }
    Log_Line("3DS", "worker %s on core %d (%d threads, budget %d)", name, worker->core, s_count + 1,
             PBW2_THREAD_BUDGET);
    return true;
}

bool Worker_Run(Worker *worker) {
    return worker && worker->run;
}

int Workers_Count(void) {
    return s_count + 1;
}

void Workers_Shutdown(void) {
    for (int i = s_count - 1; i >= 0; i--) {
        Worker *worker = &s_workers[i];
        worker->run = false;
        if (!worker->started) {
            continue;
        }
        threadJoin(worker->thread, UINT64_MAX);
        threadFree(worker->thread);
        worker->started = false;
        Log_Line("3DS", "joined %s", worker->name);
    }
    s_count = 0;
}
