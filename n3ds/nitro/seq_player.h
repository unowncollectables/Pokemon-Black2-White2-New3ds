/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef PBW2_SEQ_PLAYER_H
#define PBW2_SEQ_PLAYER_H

/* Host NNS sequence player (see seq_player.c). NNS_SndMain drives it;
 * there is no other entry point — the game reaches the player through
 * the NNS_SndArcPlayer and NNS_SndPlayer functions defined there. */
void SeqPlayer_Tick(void);

#endif
