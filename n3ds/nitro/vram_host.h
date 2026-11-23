/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include "types.h"
#include "vram_host_api.h"

#if defined(PBW2_LINK_GAME)

void PbW2_VramHostInit(void);
int PbW2_VramHostReady(void);

#endif