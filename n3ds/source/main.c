/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "audio.h"
#include "input.h"
#include "io_thread.h"
#include "log.h"
#include "pbw2_build_status.h"
#include "pbw2_port.h"
#include "present.h"
#include "save_shim.h"
#include "settings.h"
#include "workers.h"

#include <3ds.h>
#include <stdio.h>
#include <string.h>

static PrintConsole s_top;
static PrintConsole s_bottom;

static void Draw_Top(bool new3ds) {
    consoleSelect(&s_top);
    consoleClear();
    printf("Pokemon %s\n", PBW2_GAME_NAME);
    printf("New 3DS port, personal build\n\n");
    printf("New 3DS: %s\n", new3ds ? "yes" : "NO");
    printf("Asked for 804 MHz and L2.\n");
    printf("CIA flags: %s %s\n\n", PBW2_PRODUCT, PBW2_TITLE_ID);
    if (!new3ds) {
        printf("This port is New 3DS only.\n\n");
    }
    printf("Platform shell. The game is not\n");
    printf("linked, so this is not the logo\n");
    printf("or the title screen.\n\n");
    printf("Game C compiled: %d / %d\n", PBW2_GAME_C_OK, PBW2_GAME_C_TOTAL);
    printf("Library C compiled: %d / %d\n\n", PBW2_LIB_C_OK, PBW2_LIB_C_TOTAL);
    printf("Threads: %d / %d\n", Workers_Count(), PBW2_THREAD_BUDGET);
    printf("SD: %s\n", PBW2_SD_DIR);
    printf("Save: not written.\n");
}

static void Draw_Bottom(void) {
    consoleSelect(&s_bottom);
    consoleClear();
    if (Settings_Open()) {
        Settings_Draw();
    } else {
        Input_Draw();
    }
}

static void Shutdown(void) {
    /* Stop and join every worker before releasing ndsp or the GPU. */
    Workers_Shutdown();
    Audio_NdspExit();
    Input_Exit();
    Log_Line("3DS", "exit");
    Log_Close();
    gfxExit();
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    gfxInitDefault();
    consoleInit(GFX_TOP, &s_top);
    consoleInit(GFX_BOTTOM, &s_bottom);

    /* 3dsx: this asks the New 3DS for 804 MHz and the L2 cache.
     * CIA: the RSF in n3ds/cia sets CpuSpeed and EnableL2Cache, or the
     * process stays at 268 MHz even after this call. */
    osSetSpeedupEnable(true);

    bool new3ds = false;
    APT_CheckNew3DS(&new3ds);

    Result sd = archiveMountSdmc();
    Log_Init();
    Log_Line("3DS", "sdmc %s", R_SUCCEEDED(sd) ? "mounted" : "not mounted");
    Log_Line("3DS", "new3ds=%d speedup requested", new3ds ? 1 : 0);
    Log_Line("3DS", "game C %d/%d library C %d/%d", PBW2_GAME_C_OK, PBW2_GAME_C_TOTAL, PBW2_LIB_C_OK,
             PBW2_LIB_C_TOTAL);

    Save_Init();
    Input_Init();
    Audio_Start();
    Io_Start();

    while (aptMainLoop()) {
        Input_Scan();
        if (Input_Quit()) {
            Log_Line("3DS", "START exit");
            break;
        }

        /* Both screens, one commit, then the draw and the GX note. */
        Present_Commit(true, true);
        uint32_t flags = Present_Flags();
        if (flags & PBW2_SCREEN_TOP) {
            Draw_Top(new3ds);
        }
        if (flags & PBW2_SCREEN_BOTTOM) {
            Draw_Bottom();
        }
        GxShim_Present(flags);

        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }

    Shutdown();
    return 0;
}
