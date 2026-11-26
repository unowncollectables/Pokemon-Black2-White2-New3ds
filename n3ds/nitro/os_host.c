/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Host OS, IRQ, and arenas. Tick and VBlank come from the 3DS, not a DS timer.
 * Frigate's port/hal/os is the reference for which calls a host replaces.
 * This file is new; it does not copy that code. */
#include "nitro/os.h"
#include "nitro/hw.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* 0x04000000 through the card data port at 0x04100010. */
#define PBW2_IO_SIZE 0x110000u

u8 pbw2_io[PBW2_IO_SIZE] ATTRIBUTE_ALIGN(32);

static OSSystemWork s_work;
/* The VBlank counter. Backs HW_VBLANK_COUNT_BUF (see nitro/hw.h): the DSProt
 * check macro reads that address raw, so it must be a mapped word the host
 * increments every VBlank, not the unmapped DSi mirror address. */
u32 PbW2_VBlankCountBuf = 0;
static u32 s_irq_check;
static u32 s_irq_mask;
static OSIrqFunction s_irq[32];
static int s_irq_on = 1;
static int s_tick_ready;

typedef struct {
    u8 *base;
    u8 *lo;
    u8 *hi;
} Arena;

static Arena s_arena[OS_ARENA_MAX];
static u8 *s_main_block;

OSSystemWork *pbw2_system_work(void) {
    return &s_work;
}

u32 pbw2_vblank_count(void) {
    return __atomic_load_n(&PbW2_VBlankCountBuf, __ATOMIC_ACQUIRE);
}

void pbw2_irq_check_or(u32 mask) {
    __atomic_fetch_or(&s_irq_check, mask, __ATOMIC_RELAXED);
}

void pbw2_vblank(void) {
    /* MCSS palette transfers require the first VBlank scanlines. */
    reg_GX_VCOUNT = 192;
    pbw2_irq_check_or(OS_IE_V_BLANK);
    OSIrqFunction handler = __atomic_load_n(&s_irq[0], __ATOMIC_ACQUIRE);
    if (handler) {
        handler();
    }
    __atomic_fetch_add(&PbW2_VBlankCountBuf, 1, __ATOMIC_RELEASE);
}

u64 OS_GetTick(void) {
    /* svcGetSystemTick is the 3DS system tick, about 268.1 MHz. The DS tick
     * counts every 64 cycles of a 33.514 MHz clock. */
    extern u64 svcGetSystemTick(void);
    return svcGetSystemTick() * (OS_SYSTEM_CLOCK / 64) / 268111856ull;
}

u16 OS_GetTickLo(void) {
    return (u16)OS_GetTick();
}

BOOL OS_IsTickAvailable(void) {
    return s_tick_ready ? TRUE : FALSE;
}

void OS_InitTick(void) {
    s_tick_ready = 1;
}

void OSi_SetTimerReserved(int timerNo) {
    (void)timerNo;
}

void OSi_ReferSymbol(void *symbol) {
    (void)symbol;
}

void OS_InitArena(void) {
    if (!s_main_block) {
        s_main_block = malloc(16 * 1024 * 1024);
        if (!s_main_block) {
            return;
        }
        memset(s_main_block, 0, 16 * 1024 * 1024);
    }
    s_arena[OS_ARENA_MAIN].base = s_main_block;
    s_arena[OS_ARENA_MAIN].lo = s_main_block;
    s_arena[OS_ARENA_MAIN].hi = s_main_block + 16 * 1024 * 1024;
}

void OS_InitArenaEx(void) {
}

void *OS_GetArenaLo(OSArenaId arena) {
    if (arena >= OS_ARENA_MAX) {
        return NULL;
    }
    return s_arena[arena].lo;
}

void *OS_GetArenaHi(OSArenaId arena) {
    if (arena >= OS_ARENA_MAX) {
        return NULL;
    }
    return s_arena[arena].hi;
}

void OS_SetArenaLo(OSArenaId arena, void *start) {
    if (arena < OS_ARENA_MAX) {
        s_arena[arena].lo = start;
    }
}

void OS_SetArenaHi(OSArenaId arena, void *end) {
    if (arena < OS_ARENA_MAX) {
        s_arena[arena].hi = end;
    }
}

void *OS_AllocFromArenaLo(OSArenaId arena, u32 size, u32 alignment) {
    Arena *a;
    uintptr_t lo;
    u8 *next;
    if (arena >= OS_ARENA_MAX || !s_arena[arena].lo) {
        return NULL;
    }
    if (alignment < 4) {
        alignment = 4;
    }
    a = &s_arena[arena];
    lo = ((uintptr_t)a->lo + alignment - 1u) & ~(uintptr_t)(alignment - 1u);
    next = (u8 *)lo + size;
    if (next > a->hi) {
        return NULL;
    }
    a->lo = next;
    return (void *)lo;
}

u32 OS_GetConsoleType(void) {
    return 0;
}

BOOL OS_IsRunOnEmulator(void) {
    return FALSE;
}

BOOL OSi_IsRunOnTwl(void) {
    return FALSE;
}

BOOL OS_IsRunOnTwl(void) {
    return FALSE;
}

BOOL OSi_IsCodecTwlMode(void) {
    return FALSE;
}

BOOL OS_IsAvailableWireless(void) {
    return FALSE;
}

BOOL OS_IsAgreeEULA(void) {
    return FALSE;
}

/* OS_GetOwnerInfoEx: the DS system owner's info. The 3DS system birthday
 * is not wired to the port, so the struct is zeroed: the trainer-card
 * birthday script command reads month/day 0/0 (no birthday shown) instead
 * of crashing on an unimplemented call. */
void OS_GetOwnerInfoEx(OSOwnerInfoEx *info) {
    if (info != NULL) {
        memset(info, 0, sizeof(*info));
    }
}

u32 OS_GetBootType(void) {
    return OS_BOOTTYPE_ROM;
}

OSIntrMode OS_EnableInterrupts(void) {
    OSIntrMode prev = s_irq_on ? 0 : 0x80;
    s_irq_on = 1;
    return prev;
}

OSIntrMode OS_DisableInterrupts(void) {
    OSIntrMode prev = s_irq_on ? 0 : 0x80;
    s_irq_on = 0;
    return prev;
}

OSIntrMode OS_RestoreInterrupts(OSIntrMode mask) {
    OSIntrMode prev = s_irq_on ? 0 : 0x80;
    s_irq_on = (mask & 0x80) ? 0 : 1;
    return prev;
}

OSIrqMask OS_EnableIrqMask(OSIrqMask mask) {
    OSIrqMask prev = s_irq_mask;
    s_irq_mask |= mask;
    return prev;
}

void OS_SetIrqFunction(OSIrqMask interrupts, OSIrqFunction function) {
    u32 bit;
    for (bit = 0; bit < 32; bit++) {
        if (interrupts & (1u << bit)) {
            __atomic_store_n(&s_irq[bit], function, __ATOMIC_RELEASE);
        }
    }
}

extern void Pbw2_GspVblank(void);

void OS_WaitIrq(BOOL clear, OSIrqMask interrupts) {
    if (clear) __atomic_fetch_and(&s_irq_check, ~interrupts, __ATOMIC_RELAXED);
    if (!(interrupts & OS_IE_V_BLANK) || (__atomic_load_n(&s_irq_check, __ATOMIC_RELAXED) & interrupts)) return;

    /* Only the game thread dispatches its DS interrupt handler. The LCD
     * thread waits for scanout independently to present its frame. */
    Pbw2_GspVblank();
    pbw2_vblank();
}

void OS_WaitInterrupt(BOOL clear, OSIrqMask interrupts) {
    OS_WaitIrq(clear, interrupts);
    __atomic_fetch_and(&s_irq_check, ~interrupts, __ATOMIC_RELAXED);
}

void OS_WaitVBlankIntr(void) {
    OS_WaitIrq(TRUE, OS_IE_V_BLANK);
}

void OS_Halt(void) {
}

void OS_Sleep(u32 msec) {
    extern void svcSleepThread(s64 ns);
    svcSleepThread((s64)msec * 1000000);
}

void OS_InitAlarm(void) {
}

#if defined(PBW2_LINK_GAME)
void PbW2_DtcmHostInit(void);
void PbW2_NvramHostInit(void);
void PbW2_RomHeaderHostInit(void);
void PbW2_OsThreadHostInit(void);
void PbW2_CpDivHostInit(void);
void PXI_Init(void);
void CARD_Init(void);
#endif

void OS_Init(void) {
    memset(&s_work, 0, sizeof s_work);
    memset(s_irq, 0, sizeof s_irq);
    OS_InitArena();
#if defined(PBW2_LINK_GAME)
    PbW2_OsThreadHostInit();
    PXI_Init();
    PbW2_RomHeaderHostInit();
    PbW2_DtcmHostInit();
    PbW2_NvramHostInit();
    PbW2_CpDivHostInit();
    CARD_Init();
#endif
    s_irq_on = 1;
}

/* The game must not restart itself. These stop in place. */
void OS_ResetSystem(u32 parameter) {
    (void)parameter;
    for (;;) {
    }
}

void OS_Terminate(void) {
    for (;;) {
    }
}

void DC_FlushRange(const void *addr, u32 size) {
    (void)addr;
    (void)size;
}

void DC_InvalidateRange(void *addr, u32 size) {
    (void)addr;
    (void)size;
}

void DC_StoreRange(const void *addr, u32 size) {
    (void)addr;
    (void)size;
}
