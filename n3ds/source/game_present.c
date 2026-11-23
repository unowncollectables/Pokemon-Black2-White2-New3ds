/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "game_present.h"

#include "log.h"

static volatile int s_game_has_presented;

void pbw2_game_system_init_done(void) {
    /* Legacy hook; shell title is gated on first present instead. */
}

#if defined(PBW2_LINK_GAME)
int PbW2_GameHasPresented(void) {
    return s_game_has_presented != 0;
}
#endif

void pbw2_game_notify_present(void) {
    if (!s_game_has_presented) {
        s_game_has_presented = 1;
        Log_Line("GX", "first game present (display on + VRAM loaded)");
    }
}

int PbW2_ShellShouldDrawTitle(void) {
    return !s_game_has_presented;
}