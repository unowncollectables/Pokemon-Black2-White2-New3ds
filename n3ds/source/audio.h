/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef PBW2_AUDIO_H
#define PBW2_AUDIO_H

#include <stdbool.h>

void Audio_Start(void);
/* Joins nothing itself. Call after Workers_Shutdown, once. */
void Audio_NdspExit(void);

#ifdef PBW2_LINK_GAME
bool Audio_IsNdspReady(void);
bool Audio_StreamStart(const char *archive, unsigned stream, unsigned offsetMs);
void Audio_StreamStop(unsigned fadeFrames);
unsigned Audio_StreamPosition(void);
#endif

#endif
