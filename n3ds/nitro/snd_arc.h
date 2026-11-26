/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef PBW2_SND_ARC_H
#define PBW2_SND_ARC_H

#include "types.h"
#include "nnsys/snd.h"

/* Real SND archive file access against the staged SDAT (the path
 * NNS_SndArcInit recorded). Parses the SDAT INFO wave-arc table and
 * FAT once per archive and caches the directory; reads go through the
 * host FS layer, the same one StrmPcm uses for streams. */

#ifdef PBW2_LINK_GAME
/* Called from NNS_SndArcInit with the archive path the game opened. */
void SndArc_SetPath(const char *path);
const NNSSndArcSeqInfo *SndArc_GetSeqInfo(int seqNo);
const NNSSndArcBankInfo *SndArc_GetBankInfo(int bankNo);
const NNSSndArcPlayerInfo *SndArc_GetPlayerInfo(int playerNo);
/* The file id of a wave archive, or 0xFFFFFFFF when it has none. */
u32 SndArc_GetWaveArcFileId(int waveArcNo);
/* The size of a file in the archive, or 0 when it has none. */
u32 SndArc_GetFileSize(u32 fileId);
/* Bytes read from a file at an offset, or -1 on error. */
s32 SndArc_ReadFile(u32 fileId, void *buffer, s32 size, s32 offset);
#endif

#endif
