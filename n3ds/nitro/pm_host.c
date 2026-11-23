/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Power management: no ARM7 PXI on New 3DS. */
#include <string.h>

#include "nitro/pm.h"
#include "stub_log.h"
#include "twl/pm.h"

u32 PM_GetAmp(PMAmpSwitch *amp) {
    if (amp) {
        *amp = PM_AMP_OFF;
    }
    return PM_RESULT_SUCCESS;
}

u32 PM_SetAmp(PMAmpSwitch amp) {
    (void)amp;
    PBW2_STUB_LOG("PM", "SetAmp");
    return PM_RESULT_SUCCESS;
}

u32 PM_SetAmpGain(PMAmpGain gain) {
    (void)gain;
    return PM_RESULT_SUCCESS;
}

u32 PM_SetAmpGainLevel(u8 level) {
    (void)level;
    return PM_RESULT_SUCCESS;
}

void PM_AppendPreExitCallback(PMExitCallbackInfo *info) {
    (void)info;
}

void PM_ReadyToExit(void) {
    PBW2_STUB_LOG("PM", "ReadyToExit blocked");
}

void PMi_SetDispOffCount(void) {
}

BOOL PM_GetAutoExit(void) {
    return FALSE;
}

void PMi_InsertList(PMGenCallbackInfo **list, PMGenCallbackInfo *info, int priority, int insertType) {
    (void)list;
    (void)info;
    (void)priority;
    (void)insertType;
}

void PMi_DeleteList(PMGenCallbackInfo **list, PMGenCallbackInfo *info) {
    (void)list;
    (void)info;
}

void PMi_ExecuteList(PMGenCallbackInfo *list) {
    (void)list;
}

u32 PM_GetBatteryLevel(u16 *level) {
    if (level) {
        *level = 3;
    }
    return PM_RESULT_SUCCESS;
}

u32 PM_GetBattery(PMBattery *battery) {
    if (battery) {
        memset(battery, 0, sizeof *battery);
    }
    return PM_RESULT_SUCCESS;
}

u32 PM_SetBackLight(PMLCDTarget target, PMBackLightSwitch backlight) {
    (void)target;
    (void)backlight;
    return PM_RESULT_SUCCESS;
}

u32 PM_GetBackLight(PMBackLightSwitch *top, PMBackLightSwitch *bottom) {
    if (top) {
        *top = PM_BACKLIGHT_ON;
    }
    if (bottom) {
        *bottom = PM_BACKLIGHT_ON;
    }
    return PM_RESULT_SUCCESS;
}

void PM_GoSleepMode(u32 trigger, u32 logic, u16 keyPattern) {
    (void)trigger;
    (void)logic;
    (void)keyPattern;
}

void PM_SetAutoExit(BOOL autoExit) {
    (void)autoExit;
}

void PM_ForceToPowerOff(void) {
    PBW2_STUB_LOG("PM", "ForceToPowerOff blocked");
}

void PMi_ExecutePostExitCallbackList(void) {
}