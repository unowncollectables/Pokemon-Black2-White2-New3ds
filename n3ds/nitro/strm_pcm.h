/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef PBW2_STRM_PCM_H
#define PBW2_STRM_PCM_H

#include <stdint.h>

typedef struct StrmPcm StrmPcm;

StrmPcm *StrmPcm_Open(const char *path, unsigned stream, unsigned offsetMs);
void StrmPcm_Close(StrmPcm *stream);
unsigned StrmPcm_Rate(const StrmPcm *stream);
unsigned StrmPcm_Volume(const StrmPcm *stream);
unsigned StrmPcm_ReadStereo(StrmPcm *stream, int16_t *samples, unsigned frames);

#endif
