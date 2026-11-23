/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

void pbw2_game_system_init_done(void);
void pbw2_game_notify_present(void);
int PbW2_ShellShouldDrawTitle(void);
#if defined(PBW2_LINK_GAME)
int PbW2_GameHasPresented(void);
#endif
