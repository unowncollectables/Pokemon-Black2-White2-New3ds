/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "nitro/os.h"
#include "stub_log.h"

#include <string.h>

#if defined(__3DS__)
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

void OS_CreateThread(OSThread *thread, void (*func)(void *), void *arg, void *stack, u32 stackSize, u32 prio) {
    (void)func;
    (void)arg;
    (void)stack;
    (void)stackSize;
    memset(thread, 0, sizeof *thread);
    thread->id = (u32)prio + 16u;
    PBW2_STUB_LOG("OS_CreateThread", "prio=%lu (no worker)", (unsigned long)prio);
}

BOOL OS_SetThreadPriority(OSThread *thread, u32 prio) {
    (void)thread;
    (void)prio;
    return TRUE;
}

BOOL OS_IsThreadTerminated(const OSThread *thread) {
    (void)thread;
    return FALSE;
}

void OS_SleepThreadDirect(OSThread *thread, OSThreadQueue *queue) {
    (void)thread;
    OS_SleepThread(queue);
}

void OS_JoinThread(OSThread *thread) {
    (void)thread;
}

void OS_RescheduleThread(void) {
}

void OS_YieldThread(void) {
}
