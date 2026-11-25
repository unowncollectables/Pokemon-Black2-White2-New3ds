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

static void toggle_lcd_fit(void) {
    Settings_Handle(KEY_Y, KEY_L | KEY_R | KEY_Y);
    Settings_Handle(KEY_A, KEY_A);
    Settings_Handle(KEY_B, KEY_B);
}

static const char *check_preferences(void) {
    const char *path = "sdmc:/display-prefs-test.cfg";
    const char *pending = "sdmc:/display-prefs-test.cfg.tmp";
    remove(path);
    remove(pending);
    Settings_Init(path);
    if (Settings_DisplayFlags()) return "missing file uses the defaults";
    FILE *file = fopen(path, "rb");
    if (file) {
        fclose(file);
        return "launch created a preferences file";
    }

    toggle_lcd_fit();
    Settings_Init(path);
    if (Settings_DisplayFlags() != DISPLAY_LCD_FIT) return "selection survives restart";
    toggle_lcd_fit();
    Settings_Init(path);
    if (Settings_DisplayFlags()) return "a new selection replaces old settings";

    file = fopen(pending, "wb");
    if (!file) return "open staged preferences";
    fputs("PBW2-DISPLAY 1\nf4\n", file);
    if (fclose(file)) return "close staged preferences";
    Settings_Init(path);
    if (Settings_DisplayFlags()) return "complete primary takes precedence over staged data";
    remove(path);
    Settings_Init(path);
    if (Settings_DisplayFlags() != DISPLAY_SHARP_PIXELS) return "recover staged data and mask unknown flags";
    remove(pending);

    file = fopen(path, "wb");
    if (!file) return "open truncated preferences";
    fputs("PBW2-DISPLAY 1\n0", file);
    if (fclose(file)) return "close truncated preferences";
    Settings_Init(path);
    if (Settings_DisplayFlags()) return "truncated data uses defaults";
    remove(path);
    return NULL;
}

int main(void) {
    archiveMountSdmc();
    FILE *out = fopen("sdmc:/input-capture-test.txt", "w");
    if (out) {
        const char *failure = check();
        fprintf(out, "Circle Pad settings input %s%s\n", failure ? "FAIL " : "PASS", failure ? failure : "");
        failure = check_preferences();
        fprintf(out, "Display preferences %s%s\n", failure ? "FAIL " : "PASS", failure ? failure : "");
        fclose(out);
    }
    for (;;) svcSleepThread(1000000000);
}
