/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "nitro/rtc.h"

#define RTC_2000_EPOCH_SECONDS 3155673600LL

static RTCWork s_work;

/* libctru reports milliseconds since 1900-01-01. */
extern u64 osGetTime(void);

void RTC_Init(void) {
    /* libctru already provides the system clock. */
}

RTCResult RTC_GetDateTime(RTCDate *date, RTCTime *time) {
    s64 seconds = (s64)(osGetTime() / 1000ull) - RTC_2000_EPOCH_SECONDS;

    RTC_ConvertSecondToDateTime(date, time, seconds);
    return RTC_RESULT_SUCCESS;
}

RTCResult RTC_GetDate(RTCDate *date) {
    RTCTime time;

    return RTC_GetDateTime(date, &time);
}

RTCResult RTC_GetTime(RTCTime *time) {
    RTCDate date;

    return RTC_GetDateTime(&date, time);
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
