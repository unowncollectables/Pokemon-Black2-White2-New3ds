/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "save_shim.h"

#include "log.h"
#include "pbw2_port.h"

#include <stdio.h>

void Save_Init(void) {
    char path[128];
    snprintf(path, sizeof path, "%s/%s", PBW2_SD_DIR, PBW2_SAVE_FILE);
    FILE *file = fopen(path, "rb");
    if (!file) {
        Log_Line("SAVE", "no %s (the game has not written one; the port will not create it)", PBW2_SAVE_FILE);
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

bool Save_GameWrite(const void *data, uint32_t size) {
    (void)data;
    Log_Line("SAVE", "refused a %u-byte write; the game is not linked in this build", size);
    return false;
}
