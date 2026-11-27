/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "save_backup.h"

#include "config.h"
#include "log.h"
#include "pbw2_port.h"
#include "save_shim.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#ifndef PBW2_SAVE_PATH
#define PBW2_SAVE_PATH PBW2_SD_DIR "/" PBW2_SAVE_FILE
#endif

enum { SAVE_BYTES = 512 * 1024, COPY_CHUNK = 32 * 1024 };
static const char s_savePath[] = PBW2_SAVE_PATH;
static const char s_backupPath[] = PBW2_SAVE_PATH ".bak1";
static const char s_stagePath[] = PBW2_SAVE_PATH ".bak1.tmp";
static const char s_exportDir[] = PBW2_SD_DIR "/export";
static const char s_exportPath[] = PBW2_SD_DIR "/export/" PBW2_SAVE_FILE;
static const char s_exportStage[] = PBW2_SD_DIR "/export/" PBW2_SAVE_FILE ".tmp";

/* svcGetSystemTick runs at this rate on the New 3DS. */
static const uint64_t kTicksPerSecond = 268111856;
extern uint64_t svcGetSystemTick(void);
extern void svcSleepThread(int64_t ns);

static uint32_t s_backedUpGeneration;
static bool s_failedGeneration;

static bool save_is_stable(uint32_t generation) {
    struct stat first, second;
    if (stat(s_savePath, &first) != 0 || first.st_size != SAVE_BYTES) return false;
    svcSleepThread(100 * 1000 * 1000LL);
    if (stat(s_savePath, &second) != 0) return false;
    return second.st_size == first.st_size && second.st_mtime == first.st_mtime &&
           Save_Generation() == generation;
}

static bool copy_save_staged(const char *stagePath, const char *destPath) {
    FILE *in = fopen(s_savePath, "rb");
    if (!in) return false;
    FILE *out = fopen(stagePath, "wb");
    if (!out) {
        fclose(in);
        return false;
    }
    static uint8_t buffer[COPY_CHUNK];
    size_t total = 0;
    bool ok = true;
    for (;;) {
        size_t read = fread(buffer, 1, sizeof buffer, in);
        if (read && fwrite(buffer, 1, read, out) != read) {
            ok = false;
            break;
        }
        total += read;
        if (read < sizeof buffer) {
            if (ferror(in)) ok = false;
            break;
        }
    }
    fclose(in);
    if (fflush(out) != 0 || fsync(fileno(out)) != 0) ok = false;
    if (fclose(out) != 0) ok = false;
    if (!ok || total != SAVE_BYTES) {
        remove(stagePath);
        return false;
    }
    /* SDMC rename requires a vacant destination. */
    remove(destPath);
    if (rename(stagePath, destPath) != 0) {
        remove(stagePath);
        return false;
    }
    return true;
}

static bool copy_backup(void) {
    return copy_save_staged(s_stagePath, s_backupPath);
}

static volatile bool s_exportRequested;

void SaveBackup_RequestExport(void) {
    s_exportRequested = true;
}

void SaveBackup_Poll(void) {
    if (s_exportRequested) {
        /* The export waits for the same quiet + stability as a
         * backup; while the save is moving, keep the request and
         * retry on the next poll, silently. */
        uint32_t generation = Save_Generation();
        if (svcGetSystemTick() - Save_LastWriteTick() >= 2 * kTicksPerSecond &&
            save_is_stable(generation)) {
            s_exportRequested = false;
            mkdir(s_exportDir, 0777);
            if (copy_save_staged(s_exportStage, s_exportPath))
                Log_Line("SAVE", "export written to export/%s", PBW2_SAVE_FILE);
            else
                Log_Line("SAVE", "export failed, previous export kept");
        }
    }
    uint32_t generation = Save_Generation();
    if (!PbW2_ConfigGetBool("save_backup", false)) {
        /* Toggling the feature on must not back up a save that
         * predates it; only the next in-game save counts. */
        s_backedUpGeneration = generation;
        s_failedGeneration = false;
        return;
    }
    if (generation == s_backedUpGeneration) return;
    if (svcGetSystemTick() - Save_LastWriteTick() < 2 * kTicksPerSecond) return;
    if (!save_is_stable(generation)) return;
    if (copy_backup()) {
        s_backedUpGeneration = generation;
        s_failedGeneration = false;
        Log_Line("SAVE", "backup updated (gen %lu)", (unsigned long)generation);
    } else {
        /* One log line per failed generation, not one per poll. */
        if (!s_failedGeneration) {
            s_failedGeneration = true;
            Log_Line("SAVE", "backup failed, previous backup kept");
        }
        s_backedUpGeneration = generation;
    }
}
