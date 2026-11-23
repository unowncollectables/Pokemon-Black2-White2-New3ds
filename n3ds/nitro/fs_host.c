/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Files from the staged NitroFS tree on the card:
 *   sdmc:/3ds/black2_3ds/nitrofs/   or white2_3ds
 * which is a copy of extract/<game>_us/files. The bytes are not in git.
 * Opening is by path. The ROM's archive tables are not required for that.
 * No Frigate source is copied. */
#include "nitro/fs.h"

#if defined(PBW2_WHITE2)
#define PBW2_SD_DIR "sdmc:/3ds/white2_3ds"
#else
#define PBW2_SD_DIR "sdmc:/3ds/black2_3ds"
#endif

#include <stdio.h>
#include <string.h>

#define FS_SLOTS 16

typedef struct {
    int used;
    FILE *fp;
    u32 length;
    u32 position;
} FsSlot;

static FsSlot s_slots[FS_SLOTS];
static int s_ready;
static char s_root[96];

static void root_path(void) {
    snprintf(s_root, sizeof s_root, "%s/nitrofs", PBW2_SD_DIR);
}

static const char *bare_path(const char *path) {
    if (!path) {
        return NULL;
    }
    if (strncmp(path, "rom:", 4) == 0) {
        path += 4;
    }
    while (*path == '/') {
        path++;
    }
    return path;
}

static FsSlot *slot_of(FSFile *file) {
    if (!file || !file->argument) {
        return NULL;
    }
    return file->argument;
}

void FS_Init(u32 dma) {
    (void)dma;
    root_path();
    memset(s_slots, 0, sizeof s_slots);
    s_ready = 1;
}

u32 FS_TryLoadTable(void *memory, u32 size) {
    (void)memory;
    (void)size;
    /* Path opens do not need the ROM's fat/fnt. */
    return 0;
}

void FS_InitFile(FSFile *file) {
    memset(file, 0, sizeof *file);
}

BOOL FS_OpenFile(FSFile *file, const char *path) {
    const char *bare;
    char full[256];
    FsSlot *slot = NULL;
    int i;
    long end;
    FS_InitFile(file);
    if (!s_ready) {
        FS_Init(0);
    }
    bare = bare_path(path);
    if (!bare || !bare[0]) {
        file->error = FS_RESULT_INVALID_PARAMETER;
        return FALSE;
    }
    for (i = 0; i < FS_SLOTS; i++) {
        if (!s_slots[i].used) {
            slot = &s_slots[i];
            break;
        }
    }
    if (!slot) {
        file->error = FS_RESULT_FAILURE;
        return FALSE;
    }
    snprintf(full, sizeof full, "%s/%s", s_root, bare);
    slot->fp = fopen(full, "rb");
    if (!slot->fp) {
        file->error = FS_RESULT_NO_ENTRY;
        return FALSE;
    }
    if (fseek(slot->fp, 0, SEEK_END) != 0) {
        fclose(slot->fp);
        slot->fp = NULL;
        file->error = FS_RESULT_FAILURE;
        return FALSE;
    }
    end = ftell(slot->fp);
    fseek(slot->fp, 0, SEEK_SET);
    slot->used = 1;
    slot->length = end < 0 ? 0 : (u32)end;
    slot->position = 0;
    file->argument = slot;
    file->stat = FS_FILE_STATUS_IS_FILE;
    file->error = FS_RESULT_SUCCESS;
    return TRUE;
}

s32 FS_ReadFile(FSFile *file, void *dest, s32 size) {
    FsSlot *slot = slot_of(file);
    size_t n;
    if (!slot || !slot->fp || size < 0) {
        if (file) {
            file->error = FS_RESULT_FAILURE;
        }
        return -1;
    }
    n = fread(dest, 1, (size_t)size, slot->fp);
    slot->position += (u32)n;
    file->error = FS_RESULT_SUCCESS;
    return (s32)n;
}

BOOL FS_SeekFile(FSFile *file, s32 offset, FSSeekFileMode whence) {
    FsSlot *slot = slot_of(file);
    int mode;
    if (!slot || !slot->fp) {
        return FALSE;
    }
    mode = whence == FS_SEEK_CUR ? SEEK_CUR : whence == FS_SEEK_END ? SEEK_END : SEEK_SET;
    if (fseek(slot->fp, offset, mode) != 0) {
        file->error = FS_RESULT_FAILURE;
        return FALSE;
    }
    slot->position = (u32)ftell(slot->fp);
    file->error = FS_RESULT_SUCCESS;
    return TRUE;
}

BOOL FS_CloseFile(FSFile *file) {
    FsSlot *slot = slot_of(file);
    if (!slot) {
        return FALSE;
    }
    if (slot->fp) {
        fclose(slot->fp);
    }
    memset(slot, 0, sizeof *slot);
    FS_InitFile(file);
    return TRUE;
}

u32 FS_GetFileLength(FSFile *file) {
    FsSlot *slot = slot_of(file);
    return slot ? slot->length : 0;
}

u32 FS_GetLength(FSFile *file) {
    return FS_GetFileLength(file);
}

u32 FS_GetFilePosition(FSFile *file) {
    FsSlot *slot = slot_of(file);
    return slot ? slot->position : 0;
}

BOOL FS_CreateFileFromMemory(FSFile *file, void *data, u32 size) {
    (void)data;
    (void)size;
    FS_InitFile(file);
    file->error = FS_RESULT_UNSUPPORTED;
    return FALSE;
}

int pbw2_fs_probe(u32 *size_out) {
    FSFile file;
    BOOL ok;
    if (!s_ready) {
        FS_Init(0);
    }
    ok = FS_OpenFile(&file, "a/0/0/2");
    if (!ok) {
        if (size_out) {
            *size_out = 0;
        }
        return 0;
    }
    if (size_out) {
        *size_out = FS_GetLength(&file);
    }
    FS_CloseFile(&file);
    return 1;
}
