/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "save_shim.h"

#include "log.h"
#include "pbw2_port.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

enum { SAVE_FLASH_BYTES = 512 * 1024, SAVE_ERASE_CHUNK = 4096 };
#ifndef PBW2_SAVE_PATH
#define PBW2_SAVE_PATH PBW2_SD_DIR "/" PBW2_SAVE_FILE
#endif

static const char s_savePath[] = PBW2_SAVE_PATH;
static const char s_temporaryPath[] = PBW2_SAVE_PATH ".tmp";

extern uint64_t svcGetSystemTick(void);
static volatile uint32_t s_saveGeneration;
static volatile uint64_t s_lastWriteTick;

uint32_t Save_Generation(void) {
    return s_saveGeneration;
}

uint64_t Save_LastWriteTick(void) {
    return s_lastWriteTick;
}

void Save_Init(void) {
    FILE *file = fopen(s_savePath, "rb");
    if (!file) {
        if (errno == ENOENT)
            Log_Line("SAVE", "no %s", PBW2_SAVE_FILE);
        else
            Log_Line("SAVE", "cannot open %s (errno %d)", PBW2_SAVE_FILE, errno);
        return;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        Log_Line("SAVE", "found %s, left untouched", PBW2_SAVE_FILE);
        return;
    }
    long bytes = ftell(file);
    fclose(file);
    Log_Line("SAVE", "found %s, %ld bytes, left untouched", PBW2_SAVE_FILE, bytes);
}

static bool flash_range(uint32_t offset, uint32_t size) {
    return offset <= SAVE_FLASH_BYTES && size <= SAVE_FLASH_BYTES - offset;
}

static bool seek_flash(FILE *file, uint32_t offset) {
    return fseek(file, 0, SEEK_END) == 0 && ftell(file) == SAVE_FLASH_BYTES &&
           fseek(file, offset, SEEK_SET) == 0;
}

bool Save_GameRead(uint32_t offset, void *data, uint32_t size) {
    if (!flash_range(offset, size) || (size && !data)) return false;
    if (!size) return true;
    FILE *file = fopen(s_savePath, "rb");
    if (!file) {
        if (errno != ENOENT) return false;
        memset(data, 0xff, size);
        return true;
    }
    bool read = seek_flash(file, offset) && fread(data, 1, size, file) == size;
    return fclose(file) == 0 && read;
}

static bool write_erased_flash(FILE *file) {
    uint8_t erased[SAVE_ERASE_CHUNK];
    memset(erased, 0xff, sizeof erased);
    for (unsigned i = 0; i < SAVE_FLASH_BYTES / sizeof erased; ++i) {
        if (fwrite(erased, 1, sizeof erased, file) != sizeof erased) return false;
    }
    return true;
}

bool Save_GameWrite(uint32_t offset, const void *data, uint32_t size) {
    if (!flash_range(offset, size) || (size && !data)) return false;
    if (!size) return true;
    FILE *file = fopen(s_savePath, "r+b");
    bool newSave = !file && errno == ENOENT;
    if (newSave) file = fopen(s_temporaryPath, "wb");
    if (!file) return false;

    /* Existing saves keep the game's incremental write ordering. Publish
     * a new file only after its full erased flash and first write are saved. */
    bool written = (!newSave || write_erased_flash(file)) && seek_flash(file, offset) &&
                   fwrite(data, 1, size, file) == size && fflush(file) == 0 && fsync(fileno(file)) == 0;
    written = fclose(file) == 0 && written;
    if (newSave) {
        if (written) written = rename(s_temporaryPath, s_savePath) == 0;
        if (!written) remove(s_temporaryPath);
        if (written) Log_Line("SAVE", "created %s (%u bytes)", PBW2_SAVE_FILE, SAVE_FLASH_BYTES);
    }
    if (written) {
        ++s_saveGeneration;
        s_lastWriteTick = svcGetSystemTick();
    }
    return written;
}
