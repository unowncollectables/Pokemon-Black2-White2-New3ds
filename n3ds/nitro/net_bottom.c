/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * net_bottom.c — Wi-Fi Stage 1 transport bottom + host primitives the
 * real network libraries (lib/dwc, lib/nitrowifi, lib/dpw, lib/poke_net)
 * need underneath them, now that scripts/n3ds_link_closure.py links them.
 *
 * What is REAL here:
 *  - CRYPTO_RC4Init/CRYPTO_RC4Encrypt: the standard RC4 stream cipher
 *    against the SDK's own context layout (lib/nitro's rc4.c is MWCC
 *    assembly and cannot compile for ARM11).
 *  - The middleware signature strings: the *_version.c files use MWCC
 *    section pragmas GCC rejects; the strings themselves are plain data
 *    and are reproduced verbatim from those files.
 *  - OS_CreateAlarm/OS_SetAlarm/OS_SetPeriodicAlarm/OS_CancelAlarm: a
 *    real alarm service. One lazily created libctru thread sleeps until
 *    the nearest deadline and runs the handler. CPS's ACK alarm, WCM's
 *    keep-alive and NVRAM's poll all ride on this.
 *  - `errno`: NitroWiFi's SOC layer references the plain symbol; newlib
 *    in this freestanding link provides none.
 *
 * What is FAIL-HONEST here (the radio seam):
 *  - The DS wireless-manager bottom — NWM_* (TWL) and the WM driver
 *    entry points WCM's wmif backend calls (WM_Init/WM_Enable/
 *    WM_PowerOn/WM_StartDCF/...) — has no DS radio to drive on a 3DS.
 *    Every entry point answers synchronously with its documented
 *    failure code (NWM_RETCODE_FAILED / WM_ERRCODE_FAILED) and never
 *    pretends a scan, association or frame succeeded, and never leaves
 *    a caller waiting for a callback that will not come: both WCM
 *    backends switch on these return codes and propagate the error up
 *    through DWC to the game's own error screens. The real backend for
 *    this seam is the Stage 1 follow-up: NWM scan/connect fabricated
 *    from the 3DS's own association via ac:u, with CPS speaking BSD
 *    sockets over soc:U (the SoulSilver port's proven shape). Until
 *    then, online features fail cleanly and offline play never touches
 *    any of this — every caller lives in the network libraries.
 *  - OS_IrqHandler exists only because wcm_cpsif.c compares a mutex
 *    owner against its address (an "in IRQ context" sentinel). It is
 *    never called.
 */
#include <stdio.h>
#include <string.h>

#include "nitro/crypto/rc4.h"
#include "nitro/os.h"
#include "nitro/wm.h"
#include "stub_log.h"
#include "twl/nwm.h"
#include "types.h"

#if defined(__3DS__)
#include <3ds/types.h>
typedef void *Pbw2ThreadHandle;
extern Pbw2ThreadHandle Pbw2_NetBottomSpawn(void (*entry)(void *), void *arg, u32 stack_size);
extern void svcSleepThread(s64 ns);
extern u64 osGetTime(void);
#endif

/* ------------------------------------------------------------------ */
/* Middleware signature strings (verbatim from the *_version.c files)  */
/* ------------------------------------------------------------------ */

const char CPSi_Signature[] = "[SDK+UBIQUITOUS:CPS]";
const char SOCLi_Signature[] = "[SDK+NINTENDO:WiFi3.3.30052.201006221129]";
const char SSLi_Signature[] = "[SDK+UBIQUITOUS:SSL]";

/* ------------------------------------------------------------------ */
/* RC4 (standard algorithm, SDK context layout)                        */
/* ------------------------------------------------------------------ */

void CRYPTO_RC4Init(CRYPTORC4Context *context, const void *key, u32 key_len) {
    const u8 *k = (const u8 *)key;
    u32 i, j;
    if (key_len > 16) {
        key_len = 16;
    }
    for (i = 0; i < 256; i++) {
        context->s[i] = (u8)i;
    }
    j = 0;
    for (i = 0; i < 256; i++) {
        u8 tmp;
        j = (j + context->s[i] + (key_len ? k[i % key_len] : 0)) & 0xff;
        tmp = context->s[i];
        context->s[i] = context->s[j];
        context->s[j] = tmp;
    }
    context->i = 0;
    context->j = 0;
}

void CRYPTO_RC4Encrypt(CRYPTORC4Context *context, const void *in, u32 length, void *out) {
    const u8 *src = (const u8 *)in;
    u8 *dst = (u8 *)out;
    u32 n;
    u8 i = context->i;
    u8 j = context->j;
    for (n = 0; n < length; n++) {
        u8 tmp, ks;
        i = (u8)(i + 1);
        j = (u8)(j + context->s[i]);
        tmp = context->s[i];
        context->s[i] = context->s[j];
        context->s[j] = tmp;
        ks = context->s[(u8)(context->s[i] + context->s[j])];
        dst[n] = (u8)(src[n] ^ ks);
    }
    context->i = i;
    context->j = j;
}

/* ------------------------------------------------------------------ */
/* errno: the SOC layer references the plain symbol                    */
/* ------------------------------------------------------------------ */

int errno;

/* ------------------------------------------------------------------ */
/* Small OS answers                                                    */
/* ------------------------------------------------------------------ */

/* Address sentinel only (see header note); never called. */
void OS_IrqHandler(void) {
}

u8 OS_GetAgreedEULAVersion(void) {
    return 0;
}

u8 OS_GetROMHeaderEULAVersion(void) {
    return 0;
}

BOOL OS_IsForceDisableWireless(void) {
    return FALSE;
}

/* The host is already preemptively scheduled by the 3DS kernel; the
 * Nitro scheduler lock only ever guarded the cooperative DS thread
 * switch. Report the previous (unlocked) state and keep a nesting
 * count so paired calls stay paired in the log. */
static volatile int s_scheduler_lock_depth;

u32 OS_DisableScheduler(void) {
    s_scheduler_lock_depth++;
    return 0;
}

u32 OS_EnableScheduler(void) {
    if (s_scheduler_lock_depth > 0) {
        s_scheduler_lock_depth--;
    }
    return 0;
}

u32 OS_GetThreadPriority(const OSThread *thread) {
    if (!thread) {
        return 16;
    }
    return thread->priority;
}

/* ------------------------------------------------------------------ */
/* Alarms: one service thread, deadlines in host milliseconds          */
/* ------------------------------------------------------------------ */

#define PBW2_MAX_ALARMS 32

typedef struct {
    OSAlarm *alarm;
    u64 deadline_ms;
    u64 period_ms;
    OSAlarmHandler handler;
    void *arg;
    int active;
} Pbw2AlarmSlot;

static Pbw2AlarmSlot s_alarms[PBW2_MAX_ALARMS];
static volatile int s_alarm_service_started;

static u64 pbw2_tick_to_ms(OSTick tick) {
    u64 us = (u64)OS_TicksToMicroSeconds(tick);
    return us / 1000;
}

static Pbw2AlarmSlot *pbw2_alarm_slot(OSAlarm *alarm, int create) {
    int i;
    for (i = 0; i < PBW2_MAX_ALARMS; i++) {
        if (s_alarms[i].alarm == alarm) {
            return &s_alarms[i];
        }
    }
    if (!create) {
        return NULL;
    }
    for (i = 0; i < PBW2_MAX_ALARMS; i++) {
        if (s_alarms[i].alarm == NULL) {
            s_alarms[i].alarm = alarm;
            return &s_alarms[i];
        }
    }
    return NULL;
}

static void pbw2_alarm_service(void *unused) {
    (void)unused;
#if defined(__3DS__)
    for (;;) {
        u64 now = osGetTime();
        u64 nearest = 0;
        int i;
        int have = 0;
        for (i = 0; i < PBW2_MAX_ALARMS; i++) {
            Pbw2AlarmSlot *slot = &s_alarms[i];
            if (!slot->active) {
                continue;
            }
            if (slot->deadline_ms <= now) {
                OSAlarmHandler handler = slot->handler;
                void *arg = slot->arg;
                if (slot->period_ms) {
                    slot->deadline_ms = now + slot->period_ms;
                } else {
                    slot->active = 0;
                }
                if (handler) {
                    handler(arg);
                }
            } else if (!have || slot->deadline_ms < nearest) {
                nearest = slot->deadline_ms;
                have = 1;
            }
        }
        {
            u64 wait_ms = have ? (nearest - osGetTime()) : 20;
            if (wait_ms > 20) {
                wait_ms = 20;
            }
            if (wait_ms == 0) {
                wait_ms = 1;
            }
            svcSleepThread((s64)wait_ms * 1000000);
        }
    }
#endif
}

static void pbw2_alarm_service_start(void) {
#if defined(__3DS__)
    if (!s_alarm_service_started) {
        s_alarm_service_started = 1;
        Pbw2_NetBottomSpawn(pbw2_alarm_service, NULL, 0x4000);
    }
#endif
}

void OS_CreateAlarm(OSAlarm *alarm) {
    Pbw2AlarmSlot *slot;
    if (!alarm) {
        return;
    }
    memset(alarm, 0, sizeof *alarm);
    slot = pbw2_alarm_slot(alarm, 1);
    if (slot) {
        slot->active = 0;
        slot->handler = NULL;
        slot->arg = NULL;
        slot->period_ms = 0;
    }
}

static void pbw2_set_alarm(OSAlarm *alarm, OSTick tick, OSTick period, OSAlarmHandler handler, void *arg) {
    Pbw2AlarmSlot *slot;
    if (!alarm || !handler) {
        return;
    }
    pbw2_alarm_service_start();
    slot = pbw2_alarm_slot(alarm, 1);
    if (!slot) {
        PBW2_STUB_LOG("net_bottom", "OS_SetAlarm: alarm table full");
        return;
    }
    alarm->handler = handler;
    alarm->arg = arg;
    alarm->period = period;
#if defined(__3DS__)
    slot->deadline_ms = osGetTime() + pbw2_tick_to_ms(tick);
#else
    slot->deadline_ms = pbw2_tick_to_ms(tick);
#endif
    slot->period_ms = pbw2_tick_to_ms(period);
    slot->handler = handler;
    slot->arg = arg;
    slot->active = 1;
}

void OS_SetAlarm(OSAlarm *alarm, OSTick tick, OSAlarmHandler handler, void *arg) {
    pbw2_set_alarm(alarm, tick, 0, handler, arg);
}

void OS_CancelAlarm(OSAlarm *alarm) {
    Pbw2AlarmSlot *slot;
    if (!alarm) {
        return;
    }
    slot = pbw2_alarm_slot(alarm, 0);
    if (slot) {
        slot->active = 0;
    }
    alarm->handler = NULL;
}

/* ------------------------------------------------------------------ */
/* NWM (TWL wireless manager) bottom: fail-honest, synchronous         */
/* ------------------------------------------------------------------ */

static int s_nwm_initialized;

NWMRetCode NWM_Init(void *sysBuf, u32 bufSize, u8 dmaNo) {
    (void)sysBuf;
    (void)bufSize;
    (void)dmaNo;
    s_nwm_initialized = 1;
    return NWM_RETCODE_SUCCESS;
}

u16 NWM_GetState(void) {
    return s_nwm_initialized ? 1 : 0;
}

u16 NWM_GetAllowedChannel(void) {
    return 0;
}

u16 NWM_GetDispersionScanPeriod(u16 scanType) {
    (void)scanType;
    return 0;
}

NWMRetCode NWM_End(void) {
    s_nwm_initialized = 0;
    return NWM_RETCODE_SUCCESS;
}

NWMRetCode NWM_LoadDevice(NWMCallbackFunc callback) {
    (void)callback;
    PBW2_STUB_LOG("net_bottom", "NWM_LoadDevice: no DS radio on this platform (FAILED)");
    return NWM_RETCODE_FAILED;
}

NWMRetCode NWM_UnloadDevice(NWMCallbackFunc callback) {
    (void)callback;
    return NWM_RETCODE_SUCCESS;
}

NWMRetCode NWM_Open(NWMCallbackFunc callback) {
    (void)callback;
    PBW2_STUB_LOG("net_bottom", "NWM_Open: no DS radio on this platform (FAILED)");
    return NWM_RETCODE_FAILED;
}

NWMRetCode NWM_Close(NWMCallbackFunc callback) {
    (void)callback;
    return NWM_RETCODE_SUCCESS;
}

NWMRetCode NWM_Connect(NWMCallbackFunc callback, u8 nwType, const NWMBssDesc *pBdesc) {
    (void)callback;
    (void)nwType;
    (void)pBdesc;
    return NWM_RETCODE_FAILED;
}

NWMRetCode NWM_Disconnect(NWMCallbackFunc callback) {
    (void)callback;
    return NWM_RETCODE_FAILED;
}

NWMRetCode NWM_StartScanEx(NWMCallbackFunc callback, const NWMScanExParam *param) {
    (void)callback;
    (void)param;
    return NWM_RETCODE_FAILED;
}

NWMInfoElements *NWM_GetInfoElements(NWMBssDesc *bssDesc, u8 elementID) {
    (void)bssDesc;
    (void)elementID;
    return NULL;
}

NWMInfoElements *NWM_GetVenderInfoElements(NWMBssDesc *bssDesc, u8 elementID, const u8 ouiType[4]) {
    (void)bssDesc;
    (void)elementID;
    (void)ouiType;
    return NULL;
}

NWMRetCode NWM_SendFrame(NWMCallbackFunc callback, const u8 *destAddr, const u16 *sendFrame, u16 sendFrameSize) {
    (void)callback;
    (void)destAddr;
    (void)sendFrame;
    (void)sendFrameSize;
    return NWM_RETCODE_FAILED;
}

NWMRetCode NWM_SetPowerSaveMode(NWMCallbackFunc callback, NWMPowerMode powerSave) {
    (void)callback;
    (void)powerSave;
    return NWM_RETCODE_FAILED;
}

NWMRetCode NWM_SetReceivingFrameBuffer(NWMCallbackFunc callback, u8 *recvBuf, u16 recvBufSize, u16 protocol) {
    (void)callback;
    (void)recvBuf;
    (void)recvBufSize;
    (void)protocol;
    return NWM_RETCODE_FAILED;
}

NWMRetCode NWM_UnsetReceivingFrameBuffer(NWMCallbackFunc callback, u16 protocol) {
    (void)callback;
    (void)protocol;
    return NWM_RETCODE_SUCCESS;
}

NWMRetCode NWM_SetWEPKey(NWMCallbackFunc callback, u16 wepmode, u16 wepkeyid, const u8 *wepkey, u16 authMode) {
    (void)callback;
    (void)wepmode;
    (void)wepkeyid;
    (void)wepkey;
    (void)authMode;
    return NWM_RETCODE_FAILED;
}

NWMRetCode NWM_SetWPAPSK(NWMCallbackFunc callback, NWMWpaParam *param) {
    (void)callback;
    (void)param;
    return NWM_RETCODE_FAILED;
}

/* ------------------------------------------------------------------ */
/* WM driver bottom (the non-API half WCM's wmif backend drives):      */
/* fail-honest, synchronous. The WM *API* family used by game code     */
/* (WM_StartParent, WM_StartMP, ...) stays in wm_stub.c/link_stubs.c   */
/* for the local-wireless stage; these are the driver-lifecycle calls  */
/* underneath WCM only.                                                */
/* ------------------------------------------------------------------ */

static WMStatus s_wm_status;

WMErrCode WM_Init(void *wmSysBuf, u16 dmaNo) {
    (void)wmSysBuf;
    (void)dmaNo;
    memset(&s_wm_status, 0, sizeof s_wm_status);
    s_wm_status.state = WM_STATE_READY;
    return WM_ERRCODE_SUCCESS;
}

const WMStatus *WMi_GetStatusAddress(void) {
    return &s_wm_status;
}

WMErrCode WM_Enable(WMCallbackFunc callback) {
    (void)callback;
    PBW2_STUB_LOG("net_bottom", "WM_Enable: no DS radio on this platform (FAILED)");
    return WM_ERRCODE_FAILED;
}

WMErrCode WM_Disable(WMCallbackFunc callback) {
    (void)callback;
    return WM_ERRCODE_SUCCESS;
}

WMErrCode WM_PowerOn(WMCallbackFunc callback) {
    (void)callback;
    PBW2_STUB_LOG("net_bottom", "WM_PowerOn: no DS radio on this platform (FAILED)");
    return WM_ERRCODE_FAILED;
}

WMErrCode WM_PowerOff(WMCallbackFunc callback) {
    (void)callback;
    return WM_ERRCODE_SUCCESS;
}

WMErrCode WM_Finish(void) {
    s_wm_status.state = WM_STATE_STOP;
    return WM_ERRCODE_SUCCESS;
}

WMErrCode WM_StartDCF(WMCallbackFunc callback, WMDcfRecvBuf *recvBuf, u16 recvBufSize) {
    (void)callback;
    (void)recvBuf;
    (void)recvBufSize;
    return WM_ERRCODE_FAILED;
}

WMErrCode WM_EndDCF(WMCallbackFunc callback) {
    (void)callback;
    return WM_ERRCODE_SUCCESS;
}

WMErrCode WM_SetDCFData(WMCallbackFunc callback, const u8 *dst, const u16 *buffer, u16 byteLength) {
    (void)callback;
    (void)dst;
    (void)buffer;
    (void)byteLength;
    return WM_ERRCODE_FAILED;
}

WMErrCode WM_SetBeaconIndication(WMCallbackFunc callback, u16 flag) {
    (void)callback;
    (void)flag;
    return WM_ERRCODE_SUCCESS;
}

WMErrCode WM_SetWEPKeyEx(WMCallbackFunc callback, u16 wepmode, u16 wepkeyid, const u8 *wepkey) {
    (void)callback;
    (void)wepmode;
    (void)wepkeyid;
    (void)wepkey;
    return WM_ERRCODE_FAILED;
}

/* MSL ctype tables (include/ctype.h documents the layout: 0x80 entries,
 * C locale). The main module's ctype.c is MWCC library code that is not
 * in the decomp tree; these tables are generated from the C-locale class
 * definitions and the bit assignments in include/ctype.h. */
const unsigned short __ctype_map[0x80] = {
    0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004,
    0x0004, 0x0106, 0x0104, 0x0104, 0x0104, 0x0104, 0x0004, 0x0004,
    0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004,
    0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004,
    0x0142, 0x00d0, 0x00d0, 0x00d0, 0x00d0, 0x00d0, 0x00d0, 0x00d0,
    0x00d0, 0x00d0, 0x00d0, 0x00d0, 0x00d0, 0x00d0, 0x00d0, 0x00d0,
    0x0458, 0x0458, 0x0458, 0x0458, 0x0458, 0x0458, 0x0458, 0x0458,
    0x0458, 0x0458, 0x00d0, 0x00d0, 0x00d0, 0x00d0, 0x00d0, 0x00d0,
    0x00d0, 0x0651, 0x0651, 0x0651, 0x0651, 0x0651, 0x0651, 0x0251,
    0x0251, 0x0251, 0x0251, 0x0251, 0x0251, 0x0251, 0x0251, 0x0251,
    0x0251, 0x0251, 0x0251, 0x0251, 0x0251, 0x0251, 0x0251, 0x0251,
    0x0251, 0x0251, 0x0251, 0x00d0, 0x00d0, 0x00d0, 0x00d0, 0x00d0,
    0x00d0, 0x0471, 0x0471, 0x0471, 0x0471, 0x0471, 0x0471, 0x0071,
    0x0071, 0x0071, 0x0071, 0x0071, 0x0071, 0x0071, 0x0071, 0x0071,
    0x0071, 0x0071, 0x0071, 0x0071, 0x0071, 0x0071, 0x0071, 0x0071,
    0x0071, 0x0071, 0x0071, 0x00d0, 0x00d0, 0x00d0, 0x00d0, 0x0004,
};
const unsigned char __lower_map[0x80] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
    16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
    32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
    48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
    64, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111,
    112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 91, 92, 93, 94, 95,
    96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111,
    112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127,
};
const unsigned char __upper_map[0x80] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
    16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
    32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
    48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
    64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
    80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95,
    96, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
    80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 123, 124, 125, 126, 127,
};
const char DWCi_Signature[] = "[SDK+NINTENDO:DWC5.3.30054.20100706.1000_TWLDWC_5_3_PATCH1_SSLGDB4]";

/* ------------------------------------------------------------------ */
/* GameSpy support pieces whose SDK files are not in the decomp tree   */
/* ------------------------------------------------------------------ */

#include "dwc/gbucket.h"
#include "dwc/gsCrypt.h"
#include "dwc/gsMemory.h"
#include "dwc/hashtable.h"

/* MWCC string extras GameSpy calls. Plain case-insensitive compares. */
static int pbw2_tolower(int c) {
    return (c >= 'A' && c <= 'Z') ? c + ('a' - 'A') : c;
}

int strcmpi(const char *a, const char *b) {
    for (;;) {
        int ca = pbw2_tolower((unsigned char)*a);
        int cb = pbw2_tolower((unsigned char)*b);
        if (ca != cb || ca == 0) {
            return ca - cb;
        }
        a++;
        b++;
    }
}

int strnicmp(const char *a, const char *b, size_t n) {
    size_t i;
    for (i = 0; i < n; i++) {
        int ca = pbw2_tolower((unsigned char)a[i]);
        int cb = pbw2_tolower((unsigned char)b[i]);
        if (ca != cb || ca == 0) {
            return ca - cb;
        }
    }
    return 0;
}

/* gsPlatformThread guards its thread table with the TWL interrupt
 * lock. The 3DS kernel gives real preemptive threads and the table's
 * other accesses go through OS mutexes in this port; the lock is
 * reported as previously-enabled and otherwise left alone, the same
 * treatment as OS_DisableScheduler above. */
u32 OS_DisableInterrupts_IrqAndFiq(void) {
    return 1;
}

u32 OS_RestoreInterrupts_IrqAndFiq(u32 state) {
    (void)state;
    return 1;
}

/* Bucket sets: gbucket.c in the tree has every operation except the
 * set's birth, death and dump (its header says gstats.c's callers were
 * dead-stripped in the ROM and the SDK kept them in the same file).
 * These follow the same hashtable contract DoFind relies on: elements
 * are bucket_t, looked up by name. */
static int pbw2_bucket_hash(const void *element, int bucketCount) {
    const bucket_t *bucket = (const bucket_t *)element;
    const char *s = bucket->name;
    unsigned int h = 0;
    while (s && *s) {
        h = h * 31u + (unsigned char)*s++;
    }
    return (int)(h % (unsigned int)bucketCount);
}

static int pbw2_bucket_compare(const void *a, const void *b) {
    const bucket_t *ba = (const bucket_t *)a;
    const bucket_t *bb = (const bucket_t *)b;
    if (ba->name == bb->name) {
        return 0;
    }
    if (!ba->name || !bb->name) {
        return 1;
    }
    return strcmp(ba->name, bb->name) == 0 ? 0 : 1;
}

static void pbw2_bucket_free(void *element) {
    bucket_t *bucket = (bucket_t *)element;
    if (bucket->name) {
        gsifree(bucket->name);
    }
    if (bucket->type == bt_string && bucket->vals.sval) {
        gsifree(bucket->vals.sval);
    }
}

bucketset_t NewBucketSet(void) {
    bucketset_t set = (bucketset_t)gsimalloc(sizeof(struct bucketset_s));
    if (!set) {
        return NULL;
    }
    set->buckets = TableNew((int)sizeof(bucket_t), 32, pbw2_bucket_hash, pbw2_bucket_compare, pbw2_bucket_free);
    if (!set->buckets) {
        gsifree(set);
        return NULL;
    }
    return set;
}

void FreeBucketSet(bucketset_t set) {
    if (!set) {
        return;
    }
    TableFree(set->buckets);
    gsifree(set);
}

typedef struct {
    char *buf;
    size_t len;
    size_t cap;
} Pbw2DumpWork;

static void pbw2_dump_one(void *element, void *clientData) {
    bucket_t *bucket = (bucket_t *)element;
    Pbw2DumpWork *work = (Pbw2DumpWork *)clientData;
    char value[64];
    const char *text;
    size_t need;
    if (bucket->type == bt_int) {
        snprintf(value, sizeof value, "%d", bucket->vals.ival);
        text = value;
    } else if (bucket->type == bt_float) {
        snprintf(value, sizeof value, "%f", bucket->vals.fval);
        text = value;
    } else {
        text = bucket->vals.sval ? bucket->vals.sval : "";
    }
    need = strlen(bucket->name) + strlen(text) + 3;
    if (work->len + need + 1 > work->cap) {
        size_t ncap = (work->len + need + 1) * 2;
        char *nbuf = (char *)gsimalloc(ncap);
        if (!nbuf) {
            return;
        }
        if (work->buf) {
            memcpy(nbuf, work->buf, work->len);
            gsifree(work->buf);
        }
        work->buf = nbuf;
        work->cap = ncap;
    }
    work->len += (size_t)snprintf(work->buf + work->len, work->cap - work->len, "\\%s\\%s", bucket->name, text);
}

char *DumpBucketSet(bucketset_t set) {
    Pbw2DumpWork work;
    if (!set) {
        return NULL;
    }
    work.buf = NULL;
    work.len = 0;
    work.cap = 0;
    TableMapSafe(set->buckets, pbw2_dump_one, &work);
    if (!work.buf) {
        work.buf = (char *)gsimalloc(1);
        if (work.buf) {
            work.buf[0] = '\0';
        }
    }
    return work.buf;
}

/* RSA public-key encryption, the half of gsCrypt.c the ROM's overlay
 * did not carry (the tree has only the verify half). PKCS #1 v1.5
 * block type 2, built on the tree's own gsLargeInt power-mod, exactly
 * mirroring the verify path's byte order. The padding bytes come from
 * a tick-seeded xorshift — the DS original used the SDK's rand; this
 * is an interoperability padding, and the channel it protects is the
 * GameSpy auth service's own. */
gsi_i32 gsCryptRSAEncryptBuffer(const gsCryptRSAKey *key, const unsigned char *plainText, gsi_u32 size,
                                unsigned char cipherText[0x80]) {
    gsLargeInt_t value;
    gsi_u8 *bytes = (gsi_u8 *)value.mData;
    gsi_u32 total = key->modulus.mLength * sizeof(gsi_u32);
    u32 rng;
    gsi_u32 i;
    if (!key || !plainText || !cipherText) {
        return -1;
    }
    if (total != 0x80 || size + 11 > total) {
        return -1;
    }
#if defined(__3DS__)
    rng = (u32)osGetTime() ^ (u32)(osGetTime() >> 32) ^ 0x9e3779b9u;
#else
    rng = 0x9e3779b9u;
#endif
    if (rng == 0) {
        rng = 0x1234567u;
    }
    bytes[0] = 0;
    bytes[1] = 2;
    for (i = 2; i < total - size - 1; i++) {
        u8 b;
        do {
            rng ^= rng << 13;
            rng ^= rng >> 17;
            rng ^= rng << 5;
            b = (u8)(rng >> 24);
        } while (b == 0);
        bytes[i] = b;
    }
    bytes[total - size - 1] = 0;
    memcpy(&bytes[total - size], plainText, size);
    value.mLength = total / sizeof(gsi_u32);
    gsLargeIntReverseBytes(&value);
    gsLargeIntPowerMod(&value, &key->exponent, &key->modulus, &value);
    gsLargeIntReverseBytes(&value);
    memcpy(cipherText, bytes, total);
    return 0;
}
