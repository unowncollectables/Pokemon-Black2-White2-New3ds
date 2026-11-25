/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "snd_arc.h"

#include "log.h"
#include "nitro/fs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef PBW2_LINK_GAME

/* SDAT uses little-endian INFO record offsets and 16-byte FAT entries.
 * INFO stays resident so metadata returned to the game remains valid. */

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

static struct {
    char path[128];
    FSFile file;
    int open;
    uint8_t *info;
    uint32_t infoSize;
    uint32_t *waveArcFileIds;
    uint32_t waveArcCount;
    uint32_t *fatOffsets;
    uint32_t *fatSizes;
    uint32_t fatCount;
} s_arc;

static void snd_arc_clear(void) {
    if (s_arc.open) FS_CloseFile(&s_arc.file);
    free(s_arc.info);
    free(s_arc.waveArcFileIds);
    free(s_arc.fatOffsets);
    free(s_arc.fatSizes);
    s_arc.open = 0;
    s_arc.info = NULL;
    s_arc.infoSize = 0;
    s_arc.waveArcFileIds = NULL;
    s_arc.waveArcCount = 0;
    s_arc.fatOffsets = NULL;
    s_arc.fatSizes = NULL;
    s_arc.fatCount = 0;
}

void SndArc_SetPath(const char *path) {
    if (!path) return;
    if (s_arc.path[0] && strcmp(s_arc.path, path) == 0) return;
    snd_arc_clear();
    snprintf(s_arc.path, sizeof s_arc.path, "%s", path);
}

/* Read the two directory blocks once. Thousands of small seeks here can
 * delay the first cry on a physical SD card. */
static int snd_arc_ensure(void) {
    uint8_t archive[0x30];
    uint8_t *info = NULL, *fat = NULL;

    if (s_arc.info) return 1;
    if (!s_arc.path[0]) return 0;
    if (!FS_OpenFile(&s_arc.file, s_arc.path)) {
        Log_Line("AUDIO", "SndArc: cannot open %s", s_arc.path);
        return 0;
    }
    s_arc.open = 1;
    if (!read_at(&s_arc.file, 0, archive, sizeof archive) || memcmp(archive, "SDAT", 4)
        || le16(archive + 4) != 0xfeff) goto fail;

    uint32_t infoOffset = le32(archive + 0x18), infoBytes = le32(archive + 0x1c);
    uint32_t fatOffset = le32(archive + 0x20), fatBytes = le32(archive + 0x24);
    uint32_t total = FS_GetLength(&s_arc.file);
    if (infoBytes < 0x28 || fatBytes < 12 || (uint64_t)infoOffset + infoBytes > total
        || (uint64_t)fatOffset + fatBytes > total) goto fail;
    info = malloc(infoBytes);
    fat = malloc(fatBytes);
    if (!info || !fat || !read_at(&s_arc.file, infoOffset, info, infoBytes)
        || !read_at(&s_arc.file, fatOffset, fat, fatBytes) || memcmp(info, "INFO", 4) || memcmp(fat, "FAT ", 4)) goto fail;

    uint32_t infoSize = le32(info + 4), fatSize = le32(fat + 4);
    uint32_t table = le32(info + 0x14);
    if (infoSize > infoBytes || fatSize > fatBytes || fatSize < 12 || table < 0x28
        || (uint64_t)table + 4 > infoSize) goto fail;
    uint32_t waveCount = le32(info + table), fatCount = le32(fat + 8);
    if (waveCount > (infoSize - table - 4) / 4 || fatCount > (fatSize - 12) / 16) goto fail;

    s_arc.waveArcFileIds = calloc(waveCount ? waveCount : 1, sizeof *s_arc.waveArcFileIds);
    s_arc.fatOffsets = calloc(fatCount ? fatCount : 1, sizeof *s_arc.fatOffsets);
    s_arc.fatSizes = calloc(fatCount ? fatCount : 1, sizeof *s_arc.fatSizes);
    if (!s_arc.waveArcFileIds || !s_arc.fatOffsets || !s_arc.fatSizes) goto fail;
    s_arc.waveArcCount = waveCount;
    s_arc.fatCount = fatCount;
    for (uint32_t i = 0; i < waveCount; ++i) {
        uint32_t record = le32(info + table + 4 + 4 * i);
        s_arc.waveArcFileIds[i] = record >= 0x28 && record <= infoSize - 4
                                  ? le32(info + record) & 0xFFFFFFu : 0xFFFFFFFFu;
    }
    for (uint32_t i = 0; i < fatCount; ++i) {
        const uint8_t *entry = fat + 12 + 16 * i;
        uint32_t offset = le32(entry), size = le32(entry + 4);
        if ((uint64_t)offset + size <= total) {
            s_arc.fatOffsets[i] = offset;
            s_arc.fatSizes[i] = size;
        }
    }
    s_arc.info = info;
    s_arc.infoSize = infoSize;
    free(fat);
    Log_Line("AUDIO", "SndArc: %lu wave arcs, %lu files", (unsigned long)waveCount, (unsigned long)fatCount);
    return 1;
fail:
    free(info);
    free(fat);
    snd_arc_clear();
    Log_Line("AUDIO", "SndArc: invalid or unreadable SDAT directory");
    return 0;
}

static const void *info_record(unsigned slot, int index, unsigned size) {
    if (index < 0 || !snd_arc_ensure()) return NULL;
    uint32_t table = le32(s_arc.info + slot);
    if (table < 0x28 || table > s_arc.infoSize - 4) return NULL;
    uint32_t count = le32(s_arc.info + table);
    if (count > (s_arc.infoSize - table - 4) / 4 || (uint32_t)index >= count) return NULL;
    uint32_t offset = le32(s_arc.info + table + 4 + 4 * (uint32_t)index);
    if ((offset & 3) || offset < 0x28 || offset > s_arc.infoSize - size) return NULL;
    return s_arc.info + offset;
}

const NNSSndArcSeqInfo *SndArc_GetSeqInfo(int seqNo) {
    const NNSSndArcSeqInfo *seq = info_record(0x08, seqNo, sizeof *seq);
    return seq && seq->fileId < s_arc.fatCount ? seq : NULL;
}

const NNSSndArcBankInfo *SndArc_GetBankInfo(int bankNo) {
    const NNSSndArcBankInfo *bank = info_record(0x10, bankNo, sizeof *bank);
    return bank && bank->fileId < s_arc.fatCount ? bank : NULL;
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
