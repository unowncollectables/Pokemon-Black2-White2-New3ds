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

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/lock.h>

static int s_ready;
static char s_root[96];

/* Nitro can keep many logical handles open. Keep a smaller pool of SD
 * handles so field archives leave room for audio, overlays and the log.
 * The pool must stay well under the 3DS FS service's own open-file
 * budget (shared with audio, overlays, the save and the log): a raise
 * to 32 wedged Continue on hardware (Nov 27, 2026) - the title flow
 * pinned pool handles until the FS service refused the field's first
 * archive open (a/1/2/6, zone entities) before the pool itself was
 * full, so no eviction ever ran. The emulator's host FS has no such
 * budget, which is why it never reproduced there. */
enum { SD_CACHE_SIZE = 8 };
typedef struct {
    char *path;
    FILE *fp;
    uint64_t used;
    long length; /* -1 until first measured; files never change size. */
} CachedFile;

static CachedFile s_cache[SD_CACHE_SIZE];
static uint64_t s_useCount;
__LOCK_INIT(static, s_cacheLock)

static CachedFile *find_file(const char *path) {
    for (unsigned i = 0; i < SD_CACHE_SIZE; ++i) {
        if (s_cache[i].fp && strcmp(s_cache[i].path, path) == 0) return &s_cache[i];
    }
    return NULL;
}

/* A free slot if one exists, else the LRU entry. With occupiedOnly,
 * the LRU occupied entry even when a slot is free. */
static CachedFile *cache_slot(int occupiedOnly) {
    CachedFile *lru = NULL;
    for (unsigned i = 0; i < SD_CACHE_SIZE; ++i) {
        if (!s_cache[i].fp) {
            if (!occupiedOnly) return &s_cache[i];
        } else if (!lru || s_cache[i].used < lru->used) {
            lru = &s_cache[i];
        }
    }
    return lru;
}

static void cache_evict(CachedFile *entry) {
    if (entry->fp) fclose(entry->fp);
    free(entry->path);
    *entry = (CachedFile){0};
}

static CachedFile *cache_file(const char *path) {
    CachedFile *entry = find_file(path);
    if (entry) {
        entry->used = ++s_useCount;
        return entry;
    }
    char *copy = strdup(path);
    if (!copy) return NULL;
    FILE *fp = NULL;
    for (int attempt = 0; attempt < 2 && !fp; ++attempt) {
        /* First attempt takes a free slot or evicts the LRU entry.
         * If the open is still refused, the FS service's handle
         * budget is the likely cause, so the retry evicts an
         * occupied entry to free a real handle first. A genuine
         * error (e.g. no such file) simply fails twice. */
        entry = cache_slot(attempt > 0);
        if (!entry) break;
        cache_evict(entry);
        fp = fopen(path, "rb");
    }
    if (!fp) {
        int error = errno;
        free(copy);
        errno = error;
        return NULL;
    }
    *entry = (CachedFile){copy, fp, ++s_useCount, -1};
    return entry;
}

static void root_path(void) {
    snprintf(s_root, sizeof s_root, "%s/nitrofs", PBW2_SD_DIR);
}

/* Content cache: NitroFS files are immutable at runtime, so their
 * bytes can be cached forever. 256 blocks of 32 KB (8 MB of the New
 * 3DS's 256 MB) with LRU eviction and one block of readahead -
 * archive-style access re-reads headers and directories constantly,
 * and every one of those reads used to hit the SD card. All users
 * hold s_cacheLock, like the handle cache. */
enum { BLOCK_BYTES = 32 * 1024, BLOCK_COUNT = 256 };
typedef struct {
    char path[160];
    uint32_t block;
    uint32_t bytes;
    uint64_t used;
    uint8_t data[BLOCK_BYTES];
} ContentBlock;
static ContentBlock s_blocks[BLOCK_COUNT];
static uint64_t s_blockUse;
static uint32_t s_blockHits, s_blockFills;

static ContentBlock *block_find(const char *path, uint32_t block) {
    for (unsigned i = 0; i < BLOCK_COUNT; ++i) {
        if (s_blocks[i].bytes && s_blocks[i].block == block && strcmp(s_blocks[i].path, path) == 0) {
            s_blocks[i].used = ++s_blockUse;
            ++s_blockHits;
            return &s_blocks[i];
        }
    }
    return NULL;
}

static ContentBlock *block_victim(void) {
    ContentBlock *victim = &s_blocks[0];
    for (unsigned i = 0; i < BLOCK_COUNT; ++i) {
        if (!s_blocks[i].bytes) return &s_blocks[i];
        if (s_blocks[i].used < victim->used) victim = &s_blocks[i];
    }
    return victim;
}

/* Fill one block from the file; returns NULL when the block starts
 * at or past EOF or the read fails (caller falls back to fread). */
static ContentBlock *block_fill(CachedFile *entry, const char *path, uint32_t block) {
    uint64_t start = (uint64_t)block * BLOCK_BYTES;
    if (entry->length < 0 || start >= (uint64_t)entry->length) return NULL;
    if (fseek(entry->fp, (long)start, SEEK_SET) != 0) return NULL;
    ContentBlock *slot = block_victim();
    size_t want = BLOCK_BYTES;
    if (start + want > (uint64_t)entry->length) want = (size_t)(entry->length - start);
    clearerr(entry->fp);
    size_t got = fread(slot->data, 1, want, entry->fp);
    if (!got || ferror(entry->fp)) return NULL;
    snprintf(slot->path, sizeof slot->path, "%s", path);
    slot->block = block;
    slot->bytes = (uint32_t)got;
    slot->used = ++s_blockUse;
    if (++s_blockFills % 1024 == 0)
        Log_Line("FS", "content cache: %u hits, %u fills", s_blockHits, s_blockFills);
    return slot;
}

static ContentBlock *block_get(CachedFile *entry, const char *path, uint32_t block) {
    ContentBlock *found = block_find(path, block);
    if (found) return found;
    found = block_fill(entry, path, block);
    if (found) block_fill(entry, path, block + 1); /* sequential readahead */
    return found;
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
    if (snprintf(full, sizeof full, "%s/%s", s_root, bare) >= sizeof full) {
        file->error = FS_RESULT_INVALID_PARAMETER;
        return FALSE;
    }
    __lock_acquire(s_cacheLock);
    CachedFile *entry = cache_file(full);
    int error = entry ? 0 : errno;
    if (entry && entry->length < 0 && fseek(entry->fp, 0, SEEK_END) == 0) {
        entry->length = ftell(entry->fp);
    }
    if (entry) end = entry->length;
    __lock_release(s_cacheLock);
    if (!entry) {
        file->error = error == ENOENT ? FS_RESULT_NO_ENTRY : FS_RESULT_FAILURE;
        Log_Line("FS", "open failed: %s (errno %d)", bare, error);
        return FALSE;
    }
    if (end < 0 || end > INT32_MAX || !(file->argument = strdup(full))) {
        file->error = FS_RESULT_FAILURE;
        return FALSE;
    }
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
    if (file->argument) {
        __lock_acquire(s_cacheLock);
        /* One-entry memo: reads are heavily sequential on one file,
         * so the last resolved handle is usually this one. The check
         * anchors on the live entry's own path string - an evicted
         * or reused slot simply fails the compare. */
        static CachedFile *memo;
        CachedFile *entry = NULL;
        if (memo && memo->fp && strcmp(memo->path, file->argument) == 0) {
            entry = memo;
            entry->used = ++s_useCount;
        } else {
            entry = cache_file(file->argument);
            if (entry) memo = entry;
        }
        if (!entry) {
            __lock_release(s_cacheLock);
            file->error = FS_RESULT_FAILURE;
            return -1;
        }
        /* Serve from the content cache when the length is known;
         * any block that cannot be filled falls back to fread for
         * the remainder of the read. */
        s32 done = 0;
        if (entry->length >= 0 && strlen(file->argument) < sizeof ((ContentBlock *)0)->path) {
            while (done < size) {
                uint32_t at = (uint32_t)(range->position + done);
                ContentBlock *block = block_get(entry, file->argument, at / BLOCK_BYTES);
                if (!block) break;
                unsigned offset = at % BLOCK_BYTES;
                if (offset >= block->bytes) break;
                unsigned chunk = block->bytes - offset;
                if (chunk > (unsigned)(size - done)) chunk = (unsigned)(size - done);
                memcpy((u8 *)dest + done, block->data + offset, chunk);
                done += (s32)chunk;
            }
        }
        if (done < size) {
            if (fseek(entry->fp, range->position + done, SEEK_SET) != 0) {
                __lock_release(s_cacheLock);
                file->error = FS_RESULT_FAILURE;
                range->position += done;
                return done ? done : -1;
            }
            clearerr(entry->fp);
            s32 rest = (s32)fread((u8 *)dest + done, 1, (size_t)(size - done), entry->fp);
            if (ferror(entry->fp)) file->error = FS_RESULT_FAILURE;
            done += rest;
        }
        size = done;
        __lock_release(s_cacheLock);
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
    range->position = (s32)position;
    file->error = FS_RESULT_SUCCESS;
    return TRUE;
}

BOOL FS_CloseFile(FSFile *file) {
    if (!file || !FS_IsFile(file)) {
        return FALSE;
    }
    free(file->argument);
    FS_InitFile(file);
    file->error = FS_RESULT_SUCCESS;
    return TRUE;
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
