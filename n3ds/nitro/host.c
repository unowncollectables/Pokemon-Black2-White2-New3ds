/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "host.h"

#if defined(PBW2_LINK_GAME)
#include "pad_host.h"
#endif

#include "nitro/fs.h"
#include "nitro/gx.h"
#include "nitro/os.h"
#include "nitro/rtc.h"

#include <stdio.h>

extern int pbw2_fs_probe(u32 *size_out);
extern void pbw2_vblank(void);

static u32 s_pad;
static int s_nitrofs;
static u32 s_probe;

void NitroHost_Init(void) {
    RTCDate date;
    RTCTime time;
    OS_Init();
    OS_InitTick();
    OS_InitAlarm();
    RTC_Init();
    GX_Init();
    FS_Init(0);
    (void)FS_TryLoadTable(NULL, 0);
    s_nitrofs = pbw2_fs_probe(&s_probe);
    RTC_GetDateTime(&date, &time);
    printf("nitro host: rtc %lu-%02lu-%02lu %02lu:%02lu  nitrofs %s", 2000ul + date.year,
           (unsigned long)date.month, (unsigned long)date.day, (unsigned long)time.hour, (unsigned long)time.minute,
           s_nitrofs ? "a/0/0/2 open" : "not staged");
    if (s_nitrofs) {
        printf(" (%lu bytes)", (unsigned long)s_probe);
    }
    printf("\n");
}

void NitroHost_Frame(uint32_t pad_held) {
    s_pad = pad_held;
#if defined(PBW2_LINK_GAME)
    PbW2_PadHostUpdate((u32)pad_held);
#endif
    pbw2_vblank();
}

#if defined(PBW2_LINK_GAME)
void NitroHost_SetTouch(uint16_t x, uint16_t y, bool pressed) {
    PbW2_TouchHostUpdate(x, y, pressed);
}
#endif

bool NitroHost_Nitrofs(void) {
    return s_nitrofs != 0;
}

uint32_t NitroHost_ProbeBytes(void) {
    return s_probe;
}

uint32_t NitroHost_Pad(void) {
    return s_pad;
}

void NitroHost_Rtc(int *year, int *month, int *day, int *hour, int *minute) {
    RTCDate date;
    RTCTime time;
    RTC_GetDateTime(&date, &time);
    if (year) {
        *year = 2000 + (int)date.year;
    }
    if (month) {
        *month = (int)date.month;
    }
    if (day) {
        *day = (int)date.day;
    }
    if (hour) {
        *hour = (int)time.hour;
    }
    if (minute) {
        *minute = (int)time.minute;
    }
}
