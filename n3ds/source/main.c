/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "audio.h"
#include "host.h"
#include "input.h"
#include "io_thread.h"
#include "log.h"
#include "pbw2_port.h"
#include "present.h"
#include "save_shim.h"
#include "settings.h"
#include "title_pic.h"
#include "workers.h"
#include "game_present.h"
#include "types.h"
#if defined(PBW2_LINK_GAME)
#include "game_thread.h"
#endif

#include <3ds.h>
#include <stdio.h>
#include <string.h>

#if !defined(PBW2_LINK_GAME)
static PrintConsole s_top;
static PrintConsole s_bottom;
#endif

/* Bits match include/gfl/key.h. Circle-pad past the dead zone counts as a d-pad
 * direction so a stick-only pad still reaches the game later. */
static u32 Pad_FromHid(u32 held) {
    u32 pad = 0;
    circlePosition circle;
    if (held & KEY_A) {
        pad |= 0x1;
    }
    if (held & KEY_B) {
        pad |= 0x2;
    }
    if (held & KEY_SELECT) {
        pad |= 0x4;
    }
    if (held & KEY_START) {
        pad |= 0x8;
    }
    if (held & KEY_DRIGHT) {
        pad |= 0x10;
    }
    if (held & KEY_DLEFT) {
        pad |= 0x20;
    }
    if (held & KEY_DUP) {
        pad |= 0x40;
    }
    if (held & KEY_DDOWN) {
        pad |= 0x80;
    }
    if (held & KEY_R) {
        pad |= 0x100;
    }
    if (held & KEY_L) {
        pad |= 0x200;
    }
    if (held & KEY_X) {
        pad |= 0x400;
    }
    if (held & KEY_Y) {
        pad |= 0x800;
    }
    hidCircleRead(&circle);
    if (circle.dx > 80) {
        pad |= 0x10;
    }
    if (circle.dx < -80) {
        pad |= 0x20;
    }
    if (circle.dy > 80) {
        pad |= 0x40;
    }
    if (circle.dy < -80) {
        pad |= 0x80;
    }
    return pad;
}

#if !defined(PBW2_LINK_GAME)
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
    printf("Platform shell. Game main is not\n");
    printf("linked, so this is not the logo\n");
    printf("or the title screen.\n");
    printf("NitroFS: %s\n", NitroHost_Nitrofs() ? "a/0/0/2 open" : "not staged");
    {
        int year, month, day, hour, minute;
        NitroHost_Rtc(&year, &month, &day, &hour, &minute);
        printf("RTC %04d-%02d-%02d %02d:%02d\n\n", year, month, day, hour, minute);
    }
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
#endif

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

#if defined(PBW2_LINK_GAME)
    /* A screen console writes directly into a framebuffer while the game renders. */
    gfxInit(GSP_RGB565_OES, GSP_RGB565_OES, false);
#else
    gfxInitDefault();
    consoleInit(GFX_TOP, &s_top);
    consoleInit(GFX_BOTTOM, &s_bottom);
#endif

    /* 3dsx: this asks the New 3DS for 804 MHz and the L2 cache.
     * CIA: the RSF in n3ds/cia sets CpuSpeed and EnableL2Cache, or the
     * process stays at 268 MHz even after this call.
     * PBW2_NO_SPEEDUP is emulator-test-only: Azahar/Lotus abort on the
     * guest's ConfigureNew3DSCPU request, so emu builds skip it and run
     * at 268 MHz. Never ship a build with this defined. */
#if !defined(PBW2_NO_SPEEDUP)
    osSetSpeedupEnable(true);
#else
    printf("Speedup skipped (PBW2_NO_SPEEDUP, emulator test build).\n");
#endif

    bool new3ds = false;
    APT_CheckNew3DS(&new3ds);

    /* The launcher often already has sdmc mounted. A second archiveMountSdmc
     * fails, but fopen("sdmc:...") still works. A written log is success. */
    Result sd = archiveMountSdmc();
    bool log_ok = Log_Init();
    if (R_SUCCEEDED(sd)) {
        Log_Line("3DS", "sdmc mounted");
    } else if (log_ok) {
        Log_Line("3DS", "sdmc already mounted");
    } else {
        Log_Line("3DS", "sdmc not mounted");
    }
    Log_Line("3DS", "new3ds=%d speedup %s", new3ds ? 1 : 0,
#if defined(PBW2_NO_SPEEDUP)
             "skipped (emu test build)"
#else
             "requested"
#endif
    );
    Log_Line("3DS", "game C %d/%d library C %d/%d", PBW2_GAME_C_OK, PBW2_GAME_C_TOTAL, PBW2_LIB_C_OK,
             PBW2_LIB_C_TOTAL);

    Save_Init();
    Input_Init();
    Audio_Start();
    Io_Start();
    NitroHost_Init();
    Log_Line("3DS", "nitrofs %s", NitroHost_Nitrofs() ? "a/0/0/2 open" : "not staged");
#if defined(PBW2_LINK_GAME)
    GameThread_Start();
    Log_Line("3DS", "game thread %s", GameThread_Started() ? "running" : "pending");
#endif
    if (TitlePic_Load()) {
        Log_Line("3DS", "title pic: %s", TitlePic_Note());
#if !defined(PBW2_LINK_GAME)
        gfxSetScreenFormat(GFX_TOP, GSP_RGB565_OES);
        gfxSetScreenFormat(GFX_BOTTOM, GSP_RGB565_OES);
        gfxSetDoubleBuffering(GFX_TOP, true);
        gfxSetDoubleBuffering(GFX_BOTTOM, true);
#endif
    } else if (NitroHost_Nitrofs()) {
        Log_Line("3DS", "title pic: %s", TitlePic_Note());
    }

    while (aptMainLoop()) {
        Input_Scan();
        /* Emulate the DS VCOUNT register. It is shadowed in pbw2_io and
         * nothing ever wrote it, so GX_GetVCount() always returned 0.
         * The MCSS palette-load task only runs on scanlines 192-200
         * (the first VBlank lines), so its load never completed and the
         * MCSS shadow palette file leaked into the intro heap — and
         * GFL_HeapDelete then exited the game at the intro/name-entry
         * transition. VBlank TCBs run at the start of the frame, i.e.
         * line 192 on a real DS, so report that here. */
        PBW2_VU16(0x04000006) = 192;
        NitroHost_Frame(Pad_FromHid(hidKeysHeld()));
#if !defined(PBW2_LINK_GAME)
        if (Input_Quit()) {
            Log_Line("3DS", "START exit");
            break;
        }
#endif

        /* Both screens, one commit. The title pictures use that same pair. */
        Present_Commit(true, true);
        uint32_t flags = Present_Flags();
        if (PbW2_ShellShouldDrawTitle() && TitlePic_Ready()) {
            Present_DrawTitle(flags, TitlePic_Top(), TitlePic_Bottom());
        } else {
#if defined(PBW2_LINK_GAME)
            if (!PbW2_GameHasPresented()) {
                Present_Clear(flags);
            }
#else
            if (flags & PBW2_SCREEN_TOP) {
                Draw_Top(new3ds);
            }
            if (flags & PBW2_SCREEN_BOTTOM) {
                Draw_Bottom();
            }
#endif
        }
        GxShim_Present(flags);

        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }

    Shutdown();
    return 0;
}
