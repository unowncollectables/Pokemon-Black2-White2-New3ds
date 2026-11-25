/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "snd_arc.h"

#include "log.h"
#include "nitro/fs.h"

#include <stdlib.h>
#include <string.h>

#ifdef PBW2_LINK_GAME

/* SDAT layout (little-endian, BOM 0xFEFF), the same parsing style as
 * strm_pcm.c: the file header holds the INFO and FAT block offsets;
 * the INFO block holds the wave-arc table offset at +0x14; a wave-arc
 * table is a count followed by per-archive record offsets, each
 * record a 24-bit file id plus flags; the FAT block is a count
 * followed by 16-byte entries (offset, size, ...). */

static uint32_t le16(const uint8_t *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8;
}

static uint32_t le32(const uint8_t *p) {
    return le16(p) | le16(p + 2) << 16;
}

static int read_at(FSFile *file, uint64_t offset, void *out, unsigned size) {
    return offset + size <= FS_GetLength(file) && FS_SeekFile(file, (s32)offset, FS_SEEK_SET)
           && FS_ReadFile(file, out, (s32)size) == (s32)size;
}

static int read_entry(FSFile *file, uint32_t base, uint32_t sectionSize, uint32_t offset, void *out, unsigned size) {
    return (uint64_t)offset + size <= sectionSize && read_at(file, (uint64_t)base + offset, out, size);
}

static struct {
    char path[128];
    FSFile file;
    int open;
    uint32_t *waveArcFileIds;
    uint32_t waveArcCount;
    uint32_t *fatOffsets;
    uint32_t *fatSizes;
    uint32_t fatCount;
} s_arc;

static void snd_arc_clear(void) {
    if (s_arc.open) FS_CloseFile(&s_arc.file);
    free(s_arc.waveArcFileIds);
    free(s_arc.fatOffsets);
    free(s_arc.fatSizes);
    memset(&s_arc, 0, sizeof s_arc);
}

void SndArc_SetPath(const char *path) {
    if (!path) return;
    if (s_arc.path[0] && strcmp(s_arc.path, path) == 0) return;
    snd_arc_clear();
    strncpy(s_arc.path, path, sizeof s_arc.path - 1);
}

/* Parses the directory on first use. Returns FALSE when the archive
 * cannot be opened or is not an SDAT. */
static int snd_arc_ensure(void) {
    uint8_t archive[0x30], info[0x28], fat[12], entry[16];
    uint32_t infoOffset, fatOffset, infoSize, fatSize, table, count, i;

    if (s_arc.waveArcFileIds) return 1;
    if (!s_arc.path[0]) return 0;
    if (!FS_OpenFile(&s_arc.file, s_arc.path)) {
        Log_Line("AUDIO", "SndArc: cannot open %s", s_arc.path);
        return 0;
    }
    s_arc.open = 1;
    if (!read_at(&s_arc.file, 0, archive, sizeof archive) || memcmp(archive, "SDAT", 4)
        || le16(archive + 4) != 0xfeff) {
        Log_Line("AUDIO", "SndArc: %s is not an SDAT", s_arc.path);
        goto fail;
    }
    infoOffset = le32(archive + 0x18);
    fatOffset = le32(archive + 0x20);
    if (!read_at(&s_arc.file, infoOffset, info, sizeof info) || memcmp(info, "INFO", 4)
        || !read_at(&s_arc.file, fatOffset, fat, sizeof fat) || memcmp(fat, "FAT ", 4)) {
        Log_Line("AUDIO", "SndArc: INFO/FAT block missing");
        goto fail;
    }
    infoSize = le32(info + 4);
    fatSize = le32(fat + 4);
    table = le32(info + 0x14); /* wave-arc table */
    if (!read_entry(&s_arc.file, infoOffset, infoSize, table, entry, 4)) goto fail;
    count = le32(entry);
    s_arc.waveArcFileIds = calloc(count ? count : 1, sizeof *s_arc.waveArcFileIds);
    if (!s_arc.waveArcFileIds) goto fail;
    s_arc.waveArcCount = count;
    for (i = 0; i < count; ++i) {
        uint32_t record;
        if (!read_entry(&s_arc.file, infoOffset, infoSize, table + 4 + 4 * i, entry, 4)) goto fail;
        record = le32(entry);
        if (!record || !read_entry(&s_arc.file, infoOffset, infoSize, record, entry, 4)) {
            s_arc.waveArcFileIds[i] = 0xFFFFFFFFu;
            continue;
        }
        s_arc.waveArcFileIds[i] = le32(entry) & 0xFFFFFFu;
    }
    s_arc.fatCount = le32(fat + 8);
    s_arc.fatOffsets = calloc(s_arc.fatCount ? s_arc.fatCount : 1, sizeof *s_arc.fatOffsets);
    s_arc.fatSizes = calloc(s_arc.fatCount ? s_arc.fatCount : 1, sizeof *s_arc.fatSizes);
    if (!s_arc.fatOffsets || !s_arc.fatSizes) goto fail;
    for (i = 0; i < s_arc.fatCount; ++i) {
        if (!read_entry(&s_arc.file, fatOffset, fatSize, 12 + 16 * i, entry, sizeof entry)) goto fail;
        s_arc.fatOffsets[i] = le32(entry);
        s_arc.fatSizes[i] = le32(entry + 4);
    }
    Log_Line("AUDIO", "SndArc: %lu wave arcs, %lu files", (unsigned long)s_arc.waveArcCount,
             (unsigned long)s_arc.fatCount);
    return 1;
fail:
    snd_arc_clear();
    return 0;
}

u32 SndArc_GetWaveArcFileId(int waveArcNo) {
    if (!snd_arc_ensure() || waveArcNo < 0 || (uint32_t)waveArcNo >= s_arc.waveArcCount) return 0xFFFFFFFFu;
    return s_arc.waveArcFileIds[waveArcNo];
}

u32 SndArc_GetFileSize(u32 fileId) {
    if (!snd_arc_ensure() || fileId >= s_arc.fatCount) return 0;
    return s_arc.fatSizes[fileId];
}

s32 SndArc_ReadFile(u32 fileId, void *buffer, s32 size, s32 offset) {
    uint32_t fileSize;
    if (!buffer || size < 0 || offset < 0) return -1;
    if (!snd_arc_ensure() || fileId >= s_arc.fatCount) return -1;
    fileSize = s_arc.fatSizes[fileId];
    if ((uint32_t)offset > fileSize || (uint32_t)size > fileSize - (uint32_t)offset) return -1;
    if (!read_at(&s_arc.file, (uint64_t)s_arc.fatOffsets[fileId] + (uint32_t)offset, buffer, (unsigned)size)) return -1;
    return size;
}

#endif
