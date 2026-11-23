/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "settings.h"

#include "log.h"

#include <3ds.h>
#include <stdio.h>

static bool s_open;

bool Settings_Open(void) {
    return s_open;
}

bool Settings_Handle(uint32_t down, uint32_t held) {
    if ((held & KEY_L) && (held & KEY_R) && (down & KEY_Y)) {
        s_open = !s_open;
        Log_Line("CFG", s_open ? "settings open" : "settings close");
        return true;
    }
    if (s_open && (down & KEY_B)) {
        s_open = false;
        Log_Line("CFG", "settings close");
        return true;
    }
    return false;
}

void Settings_Draw(void) {
    printf("\x1b[1;0HSETTINGS");
    printf("\x1b[3;0HNo gameplay toggles in this build.");
    printf("\x1b[5;0HLater features are added here and");
    printf("\x1b[6;0Hdefault off. Nothing is written to");
    printf("\x1b[7;0Hconfig.txt or the save.");
    printf("\x1b[9;0HB closes. L+R+Y closes.");
}
