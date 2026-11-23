/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include "types.h"

#ifdef PBW2_N3DS

BOOL PbW2_TryDmaCopy32(const void *src, void *dest, u32 size);
BOOL PbW2_TryDmaCopy16(const void *src, void *dest, u32 size);
BOOL PbW2_TryDmaFill32(void *dest, u32 data, u32 size);

#endif