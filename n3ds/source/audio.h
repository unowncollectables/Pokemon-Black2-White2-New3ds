/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef PBW2_AUDIO_H
#define PBW2_AUDIO_H

void Audio_Start(void);
/* Joins nothing itself. Call after Workers_Shutdown, once. */
void Audio_NdspExit(void);

#endif
