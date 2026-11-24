/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Clock from the 3DS, not the DS RTC chip. osGetTime is milliseconds since
 * 1900-01-01. No Frigate source is copied. */
#include "nitro/rtc.h"
#include "gfl/rtc_cache.h"

static RTCDate s_date;
static RTCTime s_time;
static RTCWork s_work;
static int s_ready;

/* libctru. Not included as a header: its u32 disagrees with the decomp's. */
extern u64 osGetTime(void);

static int leap(int year) {
    return (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)) ? 1 : 0;
}

static void read_clock(void) {
    static const u8 mdays[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    u64 total;
    u64 days;
    u64 sod;
    int year;
    int month;
    total = osGetTime() / 1000ull;
    days = total / 86400ull;
    sod = total % 86400ull;
    /* 1900-01-01 was a Monday. */
    s_date.week = (RTCWeek)((1 + (u32)(days % 7ull)) % 7u);
    year = 1900;
    while (days >= (u64)(365 + leap(year))) {
        days -= (u64)(365 + leap(year));
        year++;
    }
    month = 1;
    for (int i = 0; i < 12; i++) {
        int dim = mdays[i] + (i == 1 ? leap(year) : 0);
        if (days < (u64)dim) {
            break;
        }
        days -= (u64)dim;
        month++;
    }
    s_date.year = (u32)year;
    s_date.month = (u32)month;
    s_date.day = (u32)days + 1u;
    s_time.hour = (u32)(sod / 3600ull);
    s_time.minute = (u32)((sod % 3600ull) / 60ull);
    s_time.second = (u32)(sod % 60ull);
    s_ready = 1;
}

void RTC_Init(void) {
    read_clock();
}

void RTC_GetCachedDate(RTCDate *date) {
    if (!s_ready) {
        read_clock();
    }
    *date = s_date;
}

void RTC_GetCachedTime(RTCTime *time) {
    if (!s_ready) {
        read_clock();
    }
    *time = s_time;
}

/* The pure functions the game needs from gfl/rtc_cache.c, computed from
 * the 3DS clock instead of the DS RTC chip's cache (rtc_cache.c itself is
 * excluded from the closure: its cache would read unmapped hardware and
 * return zeros, breaking the field clock in ev_time.c). */
void RTC_GetCachedDateTime(RTCDate *date, RTCTime *time) {
    if (!s_ready) {
        read_clock();
    }
    *date = s_date;
    *time = s_time;
}

int RTC_ConvertDaySecondsCached(void) {
    if (!s_ready) {
        read_clock();
    }
    return (int)(s_time.second + s_time.minute * 60u + s_time.hour * 3600u);
}

s64 RTC_ConvertSecondsCached(void) {
    if (!s_ready) {
        read_clock();
    }
    return RTC_ConvertDateTimeToSecond(&s_date, &s_time);
}

int RTC_GetDayOfYear(const RTCDate *date) {
    static const u16 sDaysBeforeMonth[12] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };
    int days;
    RTCDate firstDay;

    days = (int)date->day;
    days += sDaysBeforeMonth[date->month - 1];
    if (date->month >= 3) {
        int year = (int)date->year;

        if ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0) {
            days++;
        }
    }
    firstDay = *date;
    firstDay.month = 1;
    firstDay.day = 1;
    RTC_ConvertDateToDay(&firstDay);
    RTC_ConvertDateToDay(date);
    return days;
}

RTCResult RTC_GetDate(RTCDate *date) {
    read_clock();
    *date = s_date;
    return RTC_RESULT_SUCCESS;
}

RTCResult RTC_GetTime(RTCTime *time) {
    read_clock();
    *time = s_time;
    return RTC_RESULT_SUCCESS;
}

RTCResult RTC_GetDateTime(RTCDate *date, RTCTime *time) {
    read_clock();
    *date = s_date;
    *time = s_time;
    return RTC_RESULT_SUCCESS;
}

RTCResult RTC_GetDateAsync(RTCDate *date, RTCCallback callback, void *arg) {
    RTCResult result = RTC_GetDate(date);
    if (callback) {
        callback(result, arg);
    }
    return result;
}

RTCResult RTC_GetTimeAsync(RTCTime *time, RTCCallback callback, void *arg) {
    RTCResult result = RTC_GetTime(time);
    if (callback) {
        callback(result, arg);
    }
    return result;
}

RTCResult RTC_GetDateTimeAsync(RTCDate *date, RTCTime *time, RTCCallback callback, void *arg) {
    RTCResult result = RTC_GetDateTime(date, time);
    if (callback) {
        callback(result, arg);
    }
    return result;
}

RTCWork *RTCi_GetSysWork(void) {
    return &s_work;
}

BOOL RTCi_ReadRawDateTimeAsync(void) {
    return TRUE;
}
BOOL RTCi_ReadRawDateAsync(void) {
    return TRUE;
}
BOOL RTCi_ReadRawTimeAsync(void) {
    return TRUE;
}
BOOL RTCi_WriteRawStatus2Async(void) {
    return FALSE;
}
