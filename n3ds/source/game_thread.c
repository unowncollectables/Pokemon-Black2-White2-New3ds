/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "game_thread.h"
#include "log.h"

#include <3ds/thread.h>
#include <stdio.h>

extern void Pbw2_GameMain(void);

static Thread s_thread;
static volatile int s_started;
static volatile int s_failed;

static void GameThread(void *arg) {
    (void)arg;
    Log_Line("game", "Pbw2_GameMain enter");
    printf("\n[game] Pbw2_GameMain enter\n");
    fflush(stdout);
    s_started = 1;
    Pbw2_GameMain();
    Log_Line("game", "Pbw2_GameMain returned (unexpected)");
    printf("\n[game] Pbw2_GameMain returned\n");
    fflush(stdout);
}

void GameThread_Start(void) {
    if (s_started || s_failed) {
        return;
    }
    s_thread = threadCreate(GameThread, NULL, 0x10000, 0x30, 3, false);
    if (!s_thread) {
        s_failed = 1;
        Log_Line("game", "thread create failed");
        printf("\n[game] thread create failed\n");
        return;
    }
    Log_Line("game", "thread created");
}

int GameThread_Started(void) {
    return s_started;
}