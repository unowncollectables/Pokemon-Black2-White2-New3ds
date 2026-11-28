/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "nitro/os.h"
#include "stub_log.h"

#include <stdlib.h>
#include <string.h>

#if defined(__3DS__)
#include <3ds/thread.h>
extern void svcSleepThread(s64 ns);
#endif

#define PBW2_MAX_QUEUE_HOST 24

typedef struct {
    OSThreadQueue *queue;
    volatile u32 wake_gen;
    volatile u32 sleep_gen;
} PbW2QueueHost;

static PbW2QueueHost s_queue_hosts[PBW2_MAX_QUEUE_HOST];
static OSThread s_main_thread;
static int s_thread_ready;

OSThreadInfo OSi_ThreadInfo;
BOOL OSi_IsThreadInitialized;

static PbW2QueueHost *queue_host(OSThreadQueue *queue) {
    int i;
    if (!queue) {
        return NULL;
    }
    for (i = 0; i < PBW2_MAX_QUEUE_HOST; i++) {
        if (s_queue_hosts[i].queue == queue) {
            return &s_queue_hosts[i];
        }
    }
    for (i = 0; i < PBW2_MAX_QUEUE_HOST; i++) {
        if (s_queue_hosts[i].queue == NULL) {
            s_queue_hosts[i].queue = queue;
            return &s_queue_hosts[i];
        }
    }
    return NULL;
}

void PbW2_OsThreadHostInit(void) {
    if (s_thread_ready) {
        return;
    }
    memset(&OSi_ThreadInfo, 0, sizeof OSi_ThreadInfo);
    memset(&s_main_thread, 0, sizeof s_main_thread);
    OSi_ThreadInfo.current = &s_main_thread;
    OSi_IsThreadInitialized = TRUE;
    s_main_thread.id = 1;
    s_thread_ready = 1;
}

void OS_InitThread(void) {
    PbW2_OsThreadHostInit();
}

BOOL OS_IsThreadAvailable(void) {
    return OSi_IsThreadInitialized;
}

void OS_SleepThread(OSThreadQueue *queue) {
    PbW2QueueHost *host;
    u32 my_gen;

    if (!queue) {
#if defined(__3DS__)
        svcSleepThread(100000);
#endif
        return;
    }
    host = queue_host(queue);
    if (!host) {
        return;
    }
    host->sleep_gen++;
    my_gen = host->sleep_gen;
    while (host->wake_gen < my_gen) {
#if defined(__3DS__)
        svcSleepThread(100000);
#else
        /* Host compile smoke only. */
        break;
#endif
    }
}

void OS_WakeupThread(OSThreadQueue *queue) {
    PbW2QueueHost *host;

    if (!queue) {
        return;
    }
    host = queue_host(queue);
    if (!host) {
        return;
    }
    host->wake_gen = host->sleep_gen;
}

void OS_WakeupThreadDirect(OSThread *thread) {
    (void)thread;
}

/* Real worker threads (Wi-Fi Stage 1). Until now OS_CreateThread only
 * recorded bookkeeping, which the DWC download client cannot live with:
 * DWC_NdInitAsync starts an SVL token thread and later joins it, and
 * NHTTP/CPS run their own workers. The only OS_CreateThread callers in
 * the linked closure are the network libraries (censused Nov 28, 2026:
 * lib/dwc auth/conntest/http/nd/nhttp, lib/nitrowifi cps/soc) — no
 * offline game path creates threads through here, so making creation
 * real cannot change offline behaviour.
 *
 * Mapping notes: the game's own stack buffer is ignored — libctru
 * allocates the worker stack (the DS stack sizes are honoured as the
 * allocation size, with a floor). DS priorities (0 highest .. 31
 * lowest) map below the game thread's 3DS priority 0x30 so a network
 * worker never preempts the game. Threads are joinable, never
 * detached: OS_JoinThread joins and frees, matching the DS contract. */
#define PBW2_MAX_WORKER_THREADS 24

typedef struct Pbw2Worker {
    OSThread *thread;
    void (*func)(void *);
    void *arg;
    volatile int done;
#if defined(__3DS__)
    Thread handle;
#endif
} Pbw2Worker;

static Pbw2Worker s_workers[PBW2_MAX_WORKER_THREADS];

static Pbw2Worker *worker_for(const OSThread *thread, int create) {
    int i;
    if (!thread) {
        return NULL;
    }
    for (i = 0; i < PBW2_MAX_WORKER_THREADS; i++) {
        if (s_workers[i].thread == thread) {
            return &s_workers[i];
        }
    }
    if (!create) {
        return NULL;
    }
    for (i = 0; i < PBW2_MAX_WORKER_THREADS; i++) {
        if (s_workers[i].thread == NULL) {
            s_workers[i].thread = (OSThread *)thread;
            return &s_workers[i];
        }
    }
    return NULL;
}

#if defined(__3DS__)
static void pbw2_worker_entry(void *arg) {
    Pbw2Worker *worker = (Pbw2Worker *)arg;
    worker->func(worker->arg);
    worker->done = 1;
}
#endif

/* Spawn helper for the platform layer's own service threads
 * (net_bottom.c's alarm service). Returns the libctru Thread or NULL. */
void *Pbw2_NetBottomSpawn(void (*entry)(void *), void *arg, u32 stack_size) {
#if defined(__3DS__)
    Thread t = threadCreate(entry, arg, stack_size ? stack_size : 0x4000, 0x38, -2, false);
    return (void *)t;
#else
    (void)entry;
    (void)arg;
    (void)stack_size;
    return NULL;
#endif
}

void OS_CreateThread(OSThread *thread, void (*func)(void *), void *arg, void *stack, u32 stackSize, u32 prio) {
    Pbw2Worker *worker;
    (void)stack; /* libctru allocates its own stack; see note above */
    memset(thread, 0, sizeof *thread);
    thread->id = (u32)prio + 16u;
    thread->priority = prio;
    worker = worker_for(thread, 1);
    if (!worker) {
        PBW2_STUB_LOG("OS_CreateThread", "worker table full (no worker)");
        return;
    }
    worker->func = func;
    worker->arg = arg;
    worker->done = 0;
#if defined(__3DS__)
    {
        u32 size = stackSize < 0x1000 ? 0x8000 : stackSize;
        int ctr_prio = 0x30 + (prio > 16 ? (int)(prio - 16) : 1);
        if (ctr_prio > 0x3f) {
            ctr_prio = 0x3f;
        }
        worker->handle = threadCreate(pbw2_worker_entry, worker, size, ctr_prio, -2, false);
        if (!worker->handle) {
            PBW2_STUB_LOG("OS_CreateThread", "threadCreate failed (no worker)");
            worker->done = 1;
        }
    }
#else
    worker->done = 1;
#endif
}

BOOL OS_SetThreadPriority(OSThread *thread, u32 prio) {
    if (thread) {
        thread->priority = prio;
    }
    return TRUE;
}

BOOL OS_IsThreadTerminated(const OSThread *thread) {
    Pbw2Worker *worker = worker_for(thread, 0);
    if (!worker) {
        return TRUE;
    }
    return worker->done ? TRUE : FALSE;
}

void OS_SleepThreadDirect(OSThread *thread, OSThreadQueue *queue) {
    (void)thread;
    OS_SleepThread(queue);
}

void OS_JoinThread(OSThread *thread) {
    Pbw2Worker *worker = worker_for(thread, 0);
    if (!worker) {
        return;
    }
#if defined(__3DS__)
    if (worker->handle) {
        threadJoin(worker->handle, UINT64_MAX);
        threadFree(worker->handle);
        worker->handle = NULL;
    }
#endif
    worker->done = 1;
    worker->thread = NULL;
}

void OS_RescheduleThread(void) {
}

void OS_YieldThread(void) {
}
