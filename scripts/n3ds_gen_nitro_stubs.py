#!/usr/bin/env python3
"""Emit Nitro/GX shims used by the trimmed game link."""

from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "n3ds" / "nitro" / "nitro_stubs.c"

STUBS = r"""
#include "nitro/gx.h"
#include "nitro/mi.h"
#include "nitro/os.h"
#include "stub_log.h"
#include <3ds.h>
#include <string.h>

void *__memcpy(void *dst, const void *src, size_t n) {
    return memcpy(dst, src, n);
}

void gspWaitForVBlank(void) {
    gspWaitForVBlank0();
}

void MI_CpuCopy(void *dest, const void *src, u32 size) {
    MI_CpuCopy8(src, dest, size);
}
void MI_CpuMove(void *dest, const void *src, u32 size) {
    MI_CpuCopy8(src, dest, size);
}
void MIi_CpuCopy16(const void *src, void *dest, u32 size) {
    memcpy(dest, src, size);
}
void MIi_CpuClear16(u32 value, void *dest, u32 size) {
    MIi_CpuClear32(value, dest, size);
}
void MIi_CpuCopyFast(const void *src, void *dest, u32 size) {
    memcpy(dest, src, size);
}
void MIi_CpuSend32(const void *src, void *dest, u32 size) {
    memcpy(dest, src, size);
}
void MI_CpuComp8(void *dest, const void *src, u32 size) {
    MI_CpuCopy8(src, dest, size);
}
void MI_UncompressLZ8(const void *src, void *dest) {
    (void)src;
    (void)dest;
}
void MI_Copy36B(void *dest, const void *src) {
    MI_CpuCopy8(src, dest, 36);
}
void MI_Copy64B(void *dest, const void *src) {
    MI_CpuCopy8(src, dest, 64);
}
void MI_Zero36B(void *dest) {
    MI_CpuFill8(dest, 0, 36);
}

void OS_JoinThread(OSThread *thread) {
    (void)thread;
    PBW2_STUB_LOG("OS_JoinThread", "");
}
u32 OS_GetThreadPriority(OSThread *thread) {
    (void)thread;
    return 16;
}
void OS_ExitThread(void) {
    PBW2_STUB_LOG("OS_ExitThread", "");
    for (;;) {
    }
}
void OS_DestroyThread(OSThread *thread) {
    (void)thread;
}
void OS_KillThread(OSThread *thread) {
    (void)thread;
}
void OS_YieldThread(void) {
}
void OS_RescheduleThread(void) {
}
void OS_EnableScheduler(void) {
}
void OS_DisableScheduler(void) {
}
s32 OS_LockCard(u16 lockId) {
    (void)lockId;
    return 0;
}
s32 OS_UnLockCard(u16 lockId) {
    (void)lockId;
    return 0;
}
void OS_CreateAlarm(OSAlarm *alarm, OSAlarmHandler handler, u32 tag) {
    (void)alarm;
    (void)handler;
    (void)tag;
}
void OS_SetAlarm(OSAlarm *alarm, OSTime time, OSAlarmHandler handler) {
    (void)alarm;
    (void)time;
    (void)handler;
}
void OS_CancelAlarm(OSAlarm *alarm) {
    (void)alarm;
}
void OS_IrqHandler(void) {
}
void OS_WaitInterrupt(void) {
}
OSIntrMode OS_DisableInterrupts_IrqAndFiq(void) {
    return 0;
}
OSIntrMode OS_RestoreInterrupts_IrqAndFiq(OSIntrMode state) {
    return state;
}
void OSi_UnlockVram(void) {
}

u8 SDK_AUTOLOAD_DTCM_START[1];

void GX_SendFifo48B(const void *data) {
    (void)data;
}
void GX_SendFifo64B(const void *data) {
    (void)data;
}

void G3X_Init(void) {
    PBW2_STUB_LOG("G3X", "Init");
}
void G3X_Reset(void) {
}
void G3X_InitMtxStack(void) {
}
void G3X_SetClearColor(GXRgb rgb) {
    (void)rgb;
}
void G3X_SetFog(BOOL enable, GXFogMode mode, GXRgb colorS, GXRgb colorE, s32 density) {
    (void)enable;
    (void)mode;
    (void)colorS;
    (void)colorE;
    (void)density;
}
void G3X_SetFogTable(const GXFogTable *table) {
    (void)table;
}
void G3X_SetEdgeColorTable(const GXEdgeColor *table) {
    (void)table;
}
void G3X_SetDisp1DotDepth(BOOL enable) {
    (void)enable;
}
void G3X_GetClipMtx(MtxFx43 *dst) {
    (void)dst;
}
void G3X_GetVectorMtx(MtxFx33 *dst) {
    (void)dst;
}
BOOL G3X_GetBoxTestResult(void) {
    return FALSE;
}

void MATH_QSort(void *base, u32 nmemb, u32 size, MATHCompareFunc compare) {
    (void)base;
    (void)nmemb;
    (void)size;
    (void)compare;
}
u32 MATH_CountPopulation(u32 mask) {
    u32 count = 0;
    while (mask) {
        count += mask & 1u;
        mask >>= 1;
    }
    return count;
}

u32 _u32_div_f(u32 a, u32 b) {
    return b ? a / b : 0;
}
void SVC_GetCRC16(u16 *crc, const void *data, u32 size) {
    if (crc) {
        *crc = 0;
    }
    (void)data;
    (void)size;
}

"""


def main() -> int:
    OUT.write_text("/* SPDX-License-Identifier: GPL-3.0-or-later */\n" + STUBS.strip() + "\n")
    print(f"wrote {OUT.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())