/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "log.h"

#include "pbw2_port.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static FILE *s_log;

static bool line_is_failure(const char *line) {
    return strstr(line, "exception") || strstr(line, "Exception") ||
           strstr(line, "crash") || strstr(line, "Crash") ||
           strstr(line, "failed") || strstr(line, "Failed");
}

/* The previous run's log is evidence: a crash report is useless if
 * the next boot destroys it. Before replacing log.txt, judge the
 * previous run - a run that never reached Log_Close exited dirty,
 * and a run that logged a failure is worth keeping even if it
 * exited clean - and preserve it as log_crash.txt. Ordinary runs
 * rotate to log_prev.txt. Every boot also appends one line to the
 * append-only health record, so a pattern of dirty exits is visible
 * across sessions instead of overwriting itself. */
static void preserve_previous(const char *dir, const char **prevState, unsigned *bootCount) {
    char path[128], prevPath[128], crashPath[128], healthPath[128];
    snprintf(path, sizeof path, "%s/log.txt", dir);
    snprintf(prevPath, sizeof prevPath, "%s/log_prev.txt", dir);
    snprintf(crashPath, sizeof crashPath, "%s/log_crash.txt", dir);
    snprintf(healthPath, sizeof healthPath, "%s/health.txt", dir);
    *prevState = "first";
    FILE *old = fopen(path, "r");
    if (old) {
        bool closed = false, failure = false;
        char line[256];
        while (fgets(line, sizeof line, old)) {
            if (strstr(line, "[LOG] close")) closed = true;
            if (line_is_failure(line)) failure = true;
        }
        fclose(old);
        *prevState = failure ? "failed" : closed ? "clean" : "dirty";
        rename(path, failure ? crashPath : prevPath);
    }
    unsigned boots = 0;
    FILE *health = fopen(healthPath, "r");
    if (health) {
        int c;
        while ((c = fgetc(health)) != EOF)
            if (c == '\n') ++boots;
        fclose(health);
    }
    health = fopen(healthPath, "a");
    if (health) {
        fprintf(health, "boot %u prev=%s\n", boots + 1, *prevState);
        fclose(health);
    }
    *bootCount = boots + 1;
}

bool Log_Init(void) {
    mkdir("sdmc:/3ds", 0777);
    mkdir(PBW2_SD_DIR, 0777);
    const char *prevState;
    unsigned bootCount;
    preserve_previous(PBW2_SD_DIR, &prevState, &bootCount);
    char path[128];
    snprintf(path, sizeof path, "%s/log.txt", PBW2_SD_DIR);
    s_log = fopen(path, "w");
    Log_Line("3DS", "%s port log", PBW2_GAME_NAME);
    Log_Line("LOG", "previous run: %s (boot %u)", prevState, bootCount);
    return s_log != NULL;
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
