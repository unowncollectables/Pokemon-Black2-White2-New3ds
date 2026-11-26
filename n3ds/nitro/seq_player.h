/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef PBW2_SEQ_PLAYER_H
#define PBW2_SEQ_PLAYER_H

/* Host NNS sequence player (see seq_player.c). NNS_SndMain drives it
 * from the game thread; when the audio thread owns the tick (see
 * SeqPlayer_SetAudioThreadTick), NNS_SndMain is a no-op and the audio
 * thread calls SeqPlayer_Tick when it services buffers, so a game-thread
 * stall cannot starve the music. */
void SeqPlayer_Init(void);
void SeqPlayer_Tick(void);
/* True once the core-2 audio thread has started: the game-thread
 * NNS_SndMain stops ticking and the audio thread owns SeqPlayer_Tick.
 * False (the default) keeps the game-thread tick, so sequences still
 * advance when NDSP / the audio worker never came up. */
void SeqPlayer_SetAudioThreadTick(bool on);

#endif
