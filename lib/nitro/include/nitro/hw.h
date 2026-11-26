#ifndef POKEBW2_NITRO_HW_H
#define POKEBW2_NITRO_HW_H

#include "types.h"

#ifdef PBW2_N3DS
#define reg_OS_IME PBW2_VU16(0x04000208)
#else
#define reg_OS_IME (*(vu16 *)0x04000208)
#endif
#ifdef PBW2_N3DS
#define reg_OS_IE PBW2_VU32(0x04000210)
#else
#define reg_OS_IE (*(vu32 *)0x04000210)
#endif
// The interrupt request flags
#ifdef PBW2_N3DS
#define reg_OS_IF PBW2_VU32(0x04000214)
#else
#define reg_OS_IF (*(vu32 *)0x04000214)
#endif
// The counters and controls of the timers that the OS uses, the system timer (0) and the alarms' (1). A control
// register's bits: running, interrupting when it wraps and counting every 64 cycles
#ifdef PBW2_N3DS
#define reg_OS_TM0CNT_L PBW2_VU16(0x04000100)
#else
#define reg_OS_TM0CNT_L (*(vu16 *)0x04000100)
#endif
#ifdef PBW2_N3DS
#define reg_OS_TM0CNT_H PBW2_VU16(0x04000102)
#else
#define reg_OS_TM0CNT_H (*(vu16 *)0x04000102)
#endif
#ifdef PBW2_N3DS
#define reg_OS_TM1CNT_L PBW2_VU16(0x04000104)
#else
#define reg_OS_TM1CNT_L (*(vu16 *)0x04000104)
#endif
#ifdef PBW2_N3DS
#define reg_OS_TM1CNT_H PBW2_VU16(0x04000106)
#else
#define reg_OS_TM1CNT_H (*(vu16 *)0x04000106)
#endif
#define REG_OS_TMCNT_H_E_MASK 0x80
#define REG_OS_TMCNT_H_I_MASK 0x40
#define OS_TIMER_PRESCALER_64 1

// The state of the buttons, a bit for each that is up
#ifdef PBW2_N3DS
#define reg_PAD_KEYINPUT PBW2_VU16(0x04000130)
#else
#define reg_PAD_KEYINPUT (*(vu16 *)0x04000130)
#endif
// The card's SPI bus to its backup memory (and the infrared chip): control and data
#ifdef PBW2_N3DS
#define reg_MI_MCCNT0 PBW2_VU16(0x040001a0)
#else
#define reg_MI_MCCNT0 (*(vu16 *)0x040001a0)
#endif
#ifdef PBW2_N3DS
#define reg_MI_MCD0 PBW2_VU16(0x040001a2)
#else
#define reg_MI_MCD0 (*(vu16 *)0x040001a2)
#endif
#define REG_MI_MCCNT0_E_MASK 0x8000
#define REG_MI_MCCNT0_I_MASK 0x4000
#define REG_MI_MCCNT0_SEL_MASK 0x2000
#define REG_MI_MCCNT0_BUSY_MASK 0x0080
#define REG_MI_MCCNT0_MODE_MASK 0x0040
// The card's registers: the control of the transfer that a command starts, the command itself in two words that the
// card reads in the other byte order, and the data that comes back
#ifdef PBW2_N3DS
#define reg_MI_MCCNT1 PBW2_VU32(0x040001a4)
#else
#define reg_MI_MCCNT1 (*(vu32 *)0x040001a4)
#endif
#ifdef PBW2_N3DS
#define reg_MI_MCCMD0 PBW2_VU32(0x040001a8)
#else
#define reg_MI_MCCMD0 (*(vu32 *)0x040001a8)
#endif
#ifdef PBW2_N3DS
#define reg_MI_MCCMD1 PBW2_VU32(0x040001ac)
#else
#define reg_MI_MCCMD1 (*(vu32 *)0x040001ac)
#endif
#ifdef PBW2_N3DS
#define reg_MI_MCD1 PBW2_VU32(0x04100010)
#else
#define reg_MI_MCD1 (*(vu32 *)0x04100010)
#endif
// A command is running while START is set, and READY says that the card has a word for MCD1. The control word that the
// ROM header gives for the normal commands has the rest: the size of the answer (PC) and the clock cycles before it
// (L1). RESB is the card's reset line
#define REG_MI_MCCNT1_START_MASK 0x80000000
#define REG_MI_MCCNT1_RESB_MASK 0x20000000
#define REG_MI_MCCNT1_RDY_MASK 0x00800000
#define REG_MI_MCCNT1_PC_MASK 0x07000000
#define REG_MI_MCCNT1_PC_512B 0x01000000
#define REG_MI_MCCNT1_PC_4B 0x07000000
#define REG_MI_MCCNT1_L1_MASK 0x00001fff

#ifdef PBW2_N3DS
#define reg_GX_DISPSTAT PBW2_VU16(0x04000004)
#else
#define reg_GX_DISPSTAT (*(vu16 *)0x04000004)
#endif
#define REG_GX_DISPSTAT_HBLK_MASK 0x0002
#ifdef PBW2_N3DS
#define reg_GX_VCOUNT PBW2_VU16(0x04000006)
#else
#define reg_GX_VCOUNT (*(vu16 *)0x04000006)
#endif
#ifdef PBW2_N3DS
#define reg_GX_POWCNT PBW2_VU16(0x04000304)
#else
#define reg_GX_POWCNT (*(vu16 *)0x04000304)
#endif
// Swaps the screens, so that the main engine drives the top screen
#define REG_GX_POWCNT_DSEL_MASK 0x8000

#define REG_BLDCNT_ADDR 0x04000050
#define REG_DB_BLDCNT_ADDR 0x04001050
#define REG_MOSAIC_ADDR 0x0400004c
#define REG_DB_MOSAIC_ADDR 0x0400104c
#define REG_BLDALPHA_ADDR 0x04000052
#define REG_DB_BLDALPHA_ADDR 0x04001052

// Palette memory: the BG and OBJ palettes of the main and sub (DB) screens
#define HW_BG_PLTT 0x05000000
#define HW_OBJ_PLTT 0x05000200
#define HW_DB_BG_PLTT 0x05000400
#define HW_DB_OBJ_PLTT 0x05000600
// The main screen's OBJ VRAM, and the sub screen's
#define HW_OBJ_VRAM 0x06400000
#define HW_DB_OBJ_VRAM 0x06600000
#define REG_MASTER_BRIGHT_ADDR 0x0400006c
#define REG_DB_MASTER_BRIGHT_ADDR 0x0400106c

// The instruction TCM's image, which ends where main memory starts, and the size of the data TCM
#define HW_ITCM_IMAGE 0x01ff8000
#define HW_MAIN_MEM 0x02000000
#define HW_DTCM_SIZE 0x4000

// VBlank counter in shared memory, from the DSi mirror of main memory.
// On the 3DS port the DSi mirror is not mapped: 0x02fffc3c sits outside the
// game's memory, so a raw read there data-aborts (translation fault). That
// is what killed the first map change on hardware: the DSProt check macro
// reads this address directly (lib/dsprot/include/dsprot/dsprot.h), bypassing
// pbw2_vblank_count(). The host therefore backs the counter in a normal
// global, updated every VBlank by pbw2_vblank(), the same pattern as
// PbW2_RomHeaderBuf below.
#ifdef PBW2_N3DS
extern u32 PbW2_VBlankCountBuf;
#define HW_VBLANK_COUNT_BUF ((u32)&PbW2_VBlankCountBuf)
#else
#define HW_VBLANK_COUNT_BUF 0x02fffc3c
#endif
// The end of the instruction TCM, and where the DS's cartridge slot is mapped
#define HW_ITCM_END 0x02000000
#define HW_CTRDG_ROM 0x08000000
// The ROM header that the card's boot leaves at the end of main memory, the copy of it that the system keeps, and the
// header of a DSi game, which the system leaves in the memory the processors share
#define HW_CARD_ROM_HEADER_SIZE 0x160
#ifdef PBW2_N3DS
extern u8 PbW2_RomHeaderBuf[HW_CARD_ROM_HEADER_SIZE];
extern u8 PbW2_CardRomHeader[HW_CARD_ROM_HEADER_SIZE];
#define HW_ROM_HEADER_BUF ((u32)PbW2_RomHeaderBuf)
#define HW_CARD_ROM_HEADER ((u32)PbW2_CardRomHeader)
#define HW_TWL_ROM_HEADER_BUF ((u32)PbW2_RomHeaderBuf)
#else
#define HW_CARD_ROM_HEADER 0x02fffa80
#define HW_ROM_HEADER_BUF 0x02fffe00
#define HW_TWL_ROM_HEADER_BUF 0x02ffe000
#endif
// The low word of the DSi's title ID, its game code, in the system area
#define HW_TWL_TITLE_ID 0x02ffe230

static inline BOOL OS_DisableIrq(void) {
    u16 prev = reg_OS_IME;
    reg_OS_IME = 0;
    return prev;
}

static inline BOOL OS_EnableIrq(void) {
    u16 prev = reg_OS_IME;
    reg_OS_IME = 1;
    return prev;
}

// NitroSDK's OS_RestoreIrq, which puts back the IME that OS_DisableIrq returned. It reads IME as it writes it
static inline BOOL OS_RestoreIrq(BOOL enable) {
    u16 prev = reg_OS_IME;
    reg_OS_IME = enable;
    return prev;
}

#endif // POKEBW2_NITRO_HW_H
