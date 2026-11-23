/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "input.h"

#include "log.h"
#include "settings.h"

#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>

static bool s_irrst;
static bool s_cstickWasLive;
static uint32_t s_down;
static uint32_t s_held;

bool Input_Init(void) {
    Result rc = irrstInit();
    s_irrst = R_SUCCEEDED(rc);
    if (!s_irrst) {
        Log_Line("BTN", "irrstInit failed %08lX, C-stick reads as 0", (unsigned long)rc);
    } else {
        Log_Line("BTN", "C-stick via IRRST, ZL/ZR via HID");
    }
    return s_irrst;
}

void Input_Exit(void) {
    if (!s_irrst) {
        return;
    }
    irrstExit();
    s_irrst = false;
}

static void Log_Edge(uint32_t mask, const char *name) {
    if (s_down & mask) {
        Log_Line("BTN", "%s down", name);
    }
}

void Input_Scan(void) {
    hidScanInput();
    if (s_irrst) {
        irrstScanInput();
    }
    s_down = hidKeysDown();
    s_held = hidKeysHeld();
    Log_Edge(KEY_ZL, "ZL");
    Log_Edge(KEY_ZR, "ZR");
    if ((s_down & KEY_ZL) == 0 && (hidKeysUp() & KEY_ZL)) {
        Log_Line("BTN", "ZL up");
    }
    if ((s_down & KEY_ZR) == 0 && (hidKeysUp() & KEY_ZR)) {
        Log_Line("BTN", "ZR up");
    }
    Settings_Handle(s_down, s_held);

    if (!s_irrst) {
        return;
    }
    circlePosition stick;
    irrstCstickRead(&stick);
    bool live = abs(stick.dx) > 40 || abs(stick.dy) > 40;
    if (live && !s_cstickWasLive) {
        Log_Line("BTN", "c-stick %d %d", stick.dx, stick.dy);
    }
    s_cstickWasLive = live;
}

void Input_Draw(void) {
    circlePosition stick = {0, 0};
    if (s_irrst) {
        irrstCstickRead(&stick);
    }
    printf("\x1b[1;0HINPUT");
    printf("\x1b[3;0HZL %s   ZR %s", (s_held & KEY_ZL) ? "held" : "    ", (s_held & KEY_ZR) ? "held" : "    ");
    printf("\x1b[4;0HC-stick %4d %4d", stick.dx, stick.dy);
    printf("\x1b[6;0HL+R+Y settings");
    printf("\x1b[7;0HSTART exits the shell");
}

bool Input_Quit(void) {
    return (s_down & KEY_START) != 0;
}
