/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef PBW2_PORT_H
#define PBW2_PORT_H

#include <3ds.h>

#include "pbw2_build_status.h"

/* One tree, two games. The link chooses the game; the save directory follows. */
#if defined(PBW2_WHITE2)
#define PBW2_GAME_NAME "White 2"
#define PBW2_SD_DIR "sdmc:/3ds/white2_3ds"
#define PBW2_SAVE_FILE "white2.sav"
#define PBW2_PRODUCT "CTR-H-W23D"
#define PBW2_TITLE_ID "000400000F7D4300"
#define PBW2_GAME_C_OK PBW2_W2_GAME_C_OK
#define PBW2_GAME_C_TOTAL PBW2_W2_GAME_C_TOTAL
#define PBW2_LIB_C_OK PBW2_W2_LIB_C_OK
#define PBW2_LIB_C_TOTAL PBW2_W2_LIB_C_TOTAL
#else
#define PBW2_GAME_NAME "Black 2"
#define PBW2_SD_DIR "sdmc:/3ds/black2_3ds"
#define PBW2_SAVE_FILE "black2.sav"
#define PBW2_PRODUCT "CTR-H-B23D"
#define PBW2_TITLE_ID "000400000F7D4200"
#define PBW2_GAME_C_OK PBW2_B2_GAME_C_OK
#define PBW2_GAME_C_TOTAL PBW2_B2_GAME_C_TOTAL
#define PBW2_LIB_C_OK PBW2_B2_LIB_C_OK
#define PBW2_LIB_C_TOTAL PBW2_B2_LIB_C_TOTAL
#endif

/* A 3DS process gets about 32 threads. Workers stay well under that, and
 * every one is stopped and joined before gfx/ndsp teardown. */
#define PBW2_THREAD_BUDGET 32

/* Screen bits committed once per frame. A later sharp-text layer has to
 * present with these same bits, not a second read of the frame. */
#define PBW2_SCREEN_TOP 1u
#define PBW2_SCREEN_BOTTOM 2u

#endif
