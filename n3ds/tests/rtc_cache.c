/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>
#include "gfl/rtc_cache.h"

extern int archiveMountSdmc(void);
extern void svcSleepThread(s64 ns);

/* 2024-02-29 23:59:59, expressed in the 3DS clock's epoch and units. */
static u64 now = 3918239999000ull;

u64 __wrap_osGetTime(void) {
    return now;
}

static const char *check_cache(void) {
    RTCDate date;
    RTCTime time;

    RTCCache_Init();
    RTC_GetCachedDateTime(&date, &time);
    if (date.year != 24 || date.month != 2 || date.day != 29 || date.week != 4 ||
        time.hour != 23 || time.minute != 59 || time.second != 59 ||
        RTC_ConvertSecondsCached() != 762566399 || RTC_GetDayOfYear(&date) != 60) {
        return "initial leap-day timestamp";
    }

    now += 1000;
    for (int i = 0; i < 10; ++i) {
        RTCCache_Update();
    }
    if (RTC_ConvertDaySecondsCached() != 86399) {
        return "cache changed before its refresh frame";
    }
    RTCCache_Update();
    RTC_GetCachedDateTime(&date, &time);
    if (date.year != 24 || date.month != 3 || date.day != 1 || date.week != 5 ||
        RTC_ConvertDaySecondsCached() != 0 || RTC_ConvertSecondsCached() != 762566400 ||
        RTC_GetDayOfYear(&date) != 61) {
        return "cache refresh across midnight";
    }

    now = 0;
    RTC_GetDateTime(&date, &time);
    if (RTC_ConvertDateTimeToSecond(&date, &time) != 0) {
        return "clock earlier than the DS epoch";
    }
    return NULL;
}

int main(void) {
    archiveMountSdmc();
    const char *failure = check_cache();
    FILE *out = fopen("sdmc:/rtc-cache-test.txt", "w");
    if (out != NULL) {
        fprintf(out, "%s\n", failure != NULL ? failure : "PASS");
        fclose(out);
    }
    for (;;) {
        svcSleepThread(1000000000ll);
    }
}
