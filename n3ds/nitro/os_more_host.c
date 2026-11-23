/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "nitro/mi.h"
#include "nitro/os.h"
#include "stub_log.h"

#include <string.h>

OSThreadInfo OSi_ThreadInfo;
BOOL OSi_IsThreadInitialized;

void OS_SleepThread(OSThreadQueue *queue) {
    (void)queue;
    PBW2_STUB_LOG("OS_SleepThread", "");
    extern void svcSleepThread(s64 ns);
    svcSleepThread(1000000);
}

void OS_WakeupThread(OSThreadQueue *queue) {
    (void)queue;
    PBW2_STUB_LOG("OS_WakeupThread", "");
}

void OS_WakeupThreadDirect(OSThread *thread) {
    (void)thread;
    PBW2_STUB_LOG("OS_WakeupThreadDirect", "");
}

void OS_CreateThread(OSThread *thread, void (*func)(void *), void *arg, void *stack, u32 stackSize, u32 prio) {
    (void)func;
    (void)arg;
    (void)stack;
    (void)stackSize;
    (void)prio;
    memset(thread, 0, sizeof *thread);
    PBW2_STUB_LOG("OS_CreateThread", "prio=%lu", (unsigned long)prio);
}

BOOL OS_SetThreadPriority(OSThread *thread, u32 prio) {
    (void)thread;
    (void)prio;
    return TRUE;
}

BOOL OS_IsThreadTerminated(const OSThread *thread) {
    (void)thread;
    return TRUE;
}

void OS_SleepThreadDirect(OSThread *thread, OSThreadQueue *queue) {
    (void)thread;
    (void)queue;
}

BOOL OS_IsAlarmAvailable(void) {
    return FALSE;
}

s32 OS_GetLockID(void) {
    return 1;
}

void OS_ReleaseLockID(u16 lockId) {
    (void)lockId;
}

s32 OS_LockByWord(u16 lockID, OSLockWord *lockp, void (*ctrl)(void)) {
    (void)lockID;
    (void)lockp;
    (void)ctrl;
    return 0;
}

s32 OS_UnlockByWord(u16 lockID, OSLockWord *lockp, void (*ctrl)(void)) {
    (void)lockID;
    (void)lockp;
    (void)ctrl;
    return 0;
}

s32 OS_TryLockCard(u16 lockID) {
    (void)lockID;
    return 0;
}

s32 OS_UnlockCard(u16 lockID) {
    (void)lockID;
    return 0;
}

s32 OS_TryLockCartridge(u16 lockID) {
    (void)lockID;
    return 0;
}

s32 OS_UnLockCartridge(u16 lockID) {
    (void)lockID;
    return 0;
}

u16 OS_ReadOwnerOfLockWord(OSLockWord *lockp) {
    (void)lockp;
    return 0;
}

void OS_SpinWaitSysCycles(u32 cycles) {
    (void)cycles;
}

u32 OS_GetDTCMAddress(void) {
    return 0x27e0000u;
}

OSProcMode OS_GetProcMode(void) {
    return OS_PROCMODE_SYS;
}

OSIntrMode_Irq OS_GetCpsrIrq(void) {
    return OS_INTRMODE_IRQ_ENABLE;
}

void OSi_SyncWithOtherProc(int c, vu8 *p) {
    (void)c;
    (void)p;
}

void OSi_TerminateCore(void) {
}

OSMutex *OSi_RemoveMutexLinkFromQueue(OSMutexQueue *queue) {
    (void)queue;
    return NULL;
}

void SVC_WaitByLoop(s32 count) {
    (void)count;
}

void SVC_CpuSet(const void *src, void *dest, u32 size) {
    MI_CpuCopy8(src, dest, size);
}