/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "input.h"
#include "settings.h"

#include <3ds.h>
#include <stdio.h>

static u32 s_held, s_down;
static circlePosition s_circle;

void __wrap_hidScanInput(void) {}
u32 __wrap_hidKeysDown(void) { return s_down; }
u32 __wrap_hidKeysHeld(void) { return s_held; }
u32 __wrap_hidKeysUp(void) { return 0; }
void __wrap_hidCircleRead(circlePosition *position) { *position = s_circle; }

void Log_Line(const char *tag, const char *format, ...) {
    (void)tag;
    (void)format;
}

static u32 scan(u32 held, s16 x, s16 y) {
    s_down = held & ~s_held;
    s_held = held;
    s_circle = (circlePosition){ .dx = x, .dy = y };
    Input_Scan();
    return Input_GameHeld();
}

static const char *check(void) {
    if (scan(0, 100, 0) != KEY_DRIGHT)
        return "normal analog movement";
    if (scan(KEY_L | KEY_R | KEY_Y, 100, 0) || !Settings_Open())
        return "opening settings";
    if (scan(KEY_A, 0, 100))
        return "input while the menu is open";
    if (scan(KEY_B, 0, 100) || Settings_Open())
        return "closing settings";
    if (scan(0, 0, 100))
        return "stick held after close";
    if (scan(0, -100, 0))
        return "direction change without neutral";
    if (scan(KEY_DRIGHT, 0, 0) != KEY_DRIGHT)
        return "independent digital movement";
    if (scan(0, -100, 0) != KEY_DLEFT)
        return "analog movement after neutral";
    if (scan(0, 80, -80))
        return "dead zone";
    return NULL;
}

int main(void) {
    archiveMountSdmc();
    FILE *out = fopen("sdmc:/input-capture-test.txt", "w");
    if (out) {
        const char *failure = check();
        fprintf(out, "Circle Pad settings input %s%s\n", failure ? "FAIL " : "PASS", failure ? failure : "");
        fclose(out);
    }
    for (;;) svcSleepThread(1000000000);
}
