/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "game_present.h"

static volatile int s_past_system_init;

void pbw2_game_system_init_done(void) {
    s_past_system_init = 1;
}

int PbW2_ShellShouldDrawTitle(void) {
    return !s_past_system_init;
}