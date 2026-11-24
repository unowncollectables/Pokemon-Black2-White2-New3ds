/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Files from the staged NitroFS tree on the card:
 *   sdmc:/3ds/black2_3ds/nitrofs/   or white2_3ds
 * which is a copy of extract/<game>_us/files. The bytes are not in git.
 * Opening is by path. The ROM's archive tables are not required for that.
 * No Frigate source is copied. */
#include "nitro/fs.h"
#include "log.h"

#if defined(PBW2_WHITE2)
#define PBW2_SD_DIR "sdmc:/3ds/white2_3ds"
#else
#define PBW2_SD_DIR "sdmc:/3ds/black2_3ds"
#endif

#include <stdint.h>
#include <stdio.h>
#include <string.h>

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

#if defined(PBW2_LINK_GAME)
void FSi_InitOverlay(void);
#endif

void FS_Init(u32 dma) {
    (void)dma;
    root_path();
    s_ready = 1;
#if defined(PBW2_LINK_GAME)
    FSi_InitOverlay();
#endif
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
    FILE *fp;
    long end = -1;
    FS_InitFile(file);
    if (!s_ready) {
        FS_Init(0);
    }
    bare = bare_path(path);
    if (!bare || !bare[0]) {
        file->error = FS_RESULT_INVALID_PARAMETER;
        return FALSE;
    }
    snprintf(full, sizeof full, "%s/%s", s_root, bare);
    fp = fopen(full, "rb");
    if (!fp) {
        file->error = FS_RESULT_NO_ENTRY;
        Log_Line("FS", "open failed: %s", bare);
        return FALSE;
    }
    if (fseek(fp, 0, SEEK_END) == 0) {
        end = ftell(fp);
    }
    if (end < 0 || end > INT32_MAX || fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        file->error = FS_RESULT_FAILURE;
        return FALSE;
    }
    file->argument = fp;
    file->body.file.bottom = (s32)end;
    file->prop = &file->body;
    file->stat = FS_FILE_STATUS_IS_FILE;
    file->error = FS_RESULT_SUCCESS;
    return TRUE;
}

s32 FS_ReadFile(FSFile *file, void *dest, s32 size) {
    if (!file || !FS_IsFile(file) || size < 0 || (size > 0 && !dest)) {
        if (file) {
            file->error = FS_RESULT_INVALID_PARAMETER;
        }
        return -1;
    }
    FSFileRange *range = &file->body.file;
    s32 remaining = range->bottom - range->position;
    if (size > remaining) {
        size = remaining;
    }
    file->error = FS_RESULT_SUCCESS;
    if (size == 0) {
        return 0;
    }
    FILE *fp = file->argument;
    if (fp) {
        size = (s32)fread(dest, 1, (size_t)size, fp);
        if (ferror(fp)) {
            file->error = FS_RESULT_FAILURE;
        }
    } else {
        /* Memory files borrow their bytes for the lifetime of the handle. */
        const u8 *bytes = file->arg.readFile.buffer;
        memmove(dest, bytes + range->position, (size_t)size);
    }
    range->position += size;
    return size;
}

BOOL FS_SeekFile(FSFile *file, s32 offset, FSSeekFileMode whence) {
    if (!file || !FS_IsFile(file)) {
        return FALSE;
    }
    FSFileRange *range = &file->body.file;
    s64 position = offset;
    if (whence == FS_SEEK_END) {
        position += range->bottom;
    } else if (whence != FS_SEEK_SET) {
        position += range->position;
    }
    if (position < 0 || position > range->bottom) {
        file->error = FS_RESULT_INVALID_PARAMETER;
        return FALSE;
    }
    FILE *fp = file->argument;
    if (fp && fseek(fp, (long)position, SEEK_SET) != 0) {
        file->error = FS_RESULT_FAILURE;
        return FALSE;
    }
    range->position = (s32)position;
    file->error = FS_RESULT_SUCCESS;
    return TRUE;
}

BOOL FS_CloseFile(FSFile *file) {
    if (!file || !FS_IsFile(file)) {
        return FALSE;
    }
    FILE *fp = file->argument;
    int result = fp ? fclose(fp) : 0;
    FS_InitFile(file);
    file->error = result == 0 ? FS_RESULT_SUCCESS : FS_RESULT_FAILURE;
    return result == 0;
}

u32 FS_GetFileLength(FSFile *file) {
    return file && FS_IsFile(file) ? (u32)file->body.file.bottom : 0;
}

u32 FS_GetLength(FSFile *file) {
    return FS_GetFileLength(file);
}

u32 FS_GetFilePosition(FSFile *file) {
    return file && FS_IsFile(file) ? (u32)file->body.file.position : 0;
}

BOOL FS_CreateFileFromMemory(FSFile *file, void *data, u32 size) {
    FS_InitFile(file);
    if ((data == NULL && size != 0) || size > INT32_MAX) {
        file->error = FS_RESULT_INVALID_PARAMETER;
        return FALSE;
    }
    file->arg.readFile.buffer = data;
    file->body.file.bottom = (s32)size;
    file->prop = &file->body;
    file->stat = FS_FILE_STATUS_IS_FILE;
    file->error = FS_RESULT_SUCCESS;
    return TRUE;
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
