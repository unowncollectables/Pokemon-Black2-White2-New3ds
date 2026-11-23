/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "log.h"

#include "pbw2_port.h"

#include <stdarg.h>
#include <stdio.h>
#include <sys/stat.h>

static FILE *s_log;

void Log_Init(void) {
    mkdir("sdmc:/3ds", 0777);
    mkdir(PBW2_SD_DIR, 0777);
    char path[128];
    snprintf(path, sizeof path, "%s/log.txt", PBW2_SD_DIR);
    s_log = fopen(path, "w");
    Log_Line("3DS", "%s port log", PBW2_GAME_NAME);
}

void Log_Close(void) {
    if (!s_log) {
        return;
    }
    Log_Line("LOG", "close");
    fclose(s_log);
    s_log = NULL;
}

void Log_Line(const char *tag, const char *fmt, ...) {
    char body[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(body, sizeof body, fmt, ap);
    va_end(ap);
    printf("[%s] %s\n", tag, body);
    if (s_log) {
        fprintf(s_log, "[%s] %s\n", tag, body);
        fflush(s_log);
    }
}
