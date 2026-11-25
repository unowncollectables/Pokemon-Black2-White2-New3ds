/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "strm_pcm.h"

#include "nitro/fs.h"

#include <stdlib.h>
#include <string.h>

enum { STRM_FORMAT_PCM16 = 1, READ_FRAMES = 8192 };

struct StrmPcm {
    FSFile file;
    uint32_t data, blocks, blockBytes, blockFrames, lastBytes, lastFrames;
    uint32_t position, loopStart, end, rate;
    uint32_t cacheStart, cacheFrames;
    uint8_t channels, loop, volume;
    int16_t cache[2][READ_FRAMES];
};

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

static int load_header(StrmPcm *stream, unsigned index, unsigned offsetMs) {
    uint8_t archive[0x30], info[0x28], fat[12], entry[8], head[0x40];
    FSFile *file = &stream->file;
    if (!read_at(file, 0, archive, sizeof archive) || memcmp(archive, "SDAT", 4) || le16(archive + 4) != 0xfeff) {
        return 0;
    }
    uint32_t infoOffset = le32(archive + 0x18), fatOffset = le32(archive + 0x20);
    if (!read_at(file, infoOffset, info, sizeof info) || memcmp(info, "INFO", 4)
        || !read_at(file, fatOffset, fat, sizeof fat) || memcmp(fat, "FAT ", 4)) {
        return 0;
    }
    uint32_t infoSize = le32(info + 4), fatSize = le32(fat + 4);
    uint32_t table = le32(info + 0x24);
    if (table < sizeof info || !read_entry(file, infoOffset, infoSize, table, entry, 4)
        || index >= le32(entry) || (uint64_t)table + 4 + 4 * (uint64_t)index + 4 > infoSize) {
        return 0;
    }
    if (!read_entry(file, infoOffset, infoSize, table + 4 + 4 * index, entry, 4)) {
        return 0;
    }
    uint32_t record = le32(entry);
    if (!record || !read_entry(file, infoOffset, infoSize, record, entry, sizeof entry)) {
        return 0;
    }
    uint32_t id = le32(entry);
    stream->volume = entry[4] & 127;
    if (id >= le32(fat + 8) || (uint64_t)id * 16 + 12 + sizeof entry > fatSize
        || !read_entry(file, fatOffset, fatSize, 12 + 16 * id, entry, sizeof entry)) {
        return 0;
    }
    uint32_t offset = le32(entry), size = le32(entry + 4);
    if ((uint64_t)offset + size > FS_GetLength(file) || size < sizeof head
        || !read_at(file, offset, head, sizeof head) || memcmp(head, "STRM", 4)
        || le16(head + 4) != 0xfeff || memcmp(head + 0x10, "HEAD", 4) || head[0x18] != STRM_FORMAT_PCM16) {
        return 0;
    }
    stream->loop = head[0x19];
    stream->channels = head[0x1a];
    stream->rate = le16(head + 0x1c);
    stream->loopStart = le32(head + 0x20);
    stream->end = le32(head + 0x24);
    uint32_t dataOffset = le32(head + 0x28);
    stream->blocks = le32(head + 0x2c);
    stream->blockBytes = le32(head + 0x30);
    stream->blockFrames = le32(head + 0x34);
    stream->lastBytes = le32(head + 0x38);
    stream->lastFrames = le32(head + 0x3c);
    if (!stream->blocks || !stream->blockFrames || !stream->lastFrames || !stream->rate
        || (stream->channels != 1 && stream->channels != 2) || dataOffset < sizeof head || dataOffset > size
        || 2ull * stream->blockFrames > stream->blockBytes || 2ull * stream->lastFrames > stream->lastBytes
        || stream->lastFrames > stream->blockFrames || (stream->loop && stream->loopStart >= stream->end)) {
        return 0;
    }
    uint64_t priorBytes = (uint64_t)(stream->blocks - 1) * stream->blockBytes;
    uint64_t totalFrames = (uint64_t)(stream->blocks - 1) * stream->blockFrames + stream->lastFrames;
    uint64_t start = (uint64_t)offsetMs * stream->rate / 1000;
    if (priorBytes > size || (priorBytes + stream->lastBytes) * stream->channels > size - dataOffset
        || stream->end > totalFrames || start >= stream->end) {
        return 0;
    }
    stream->data = offset + dataOffset;
    stream->position = (uint32_t)start;
    return 1;
}

StrmPcm *StrmPcm_Open(const char *path, unsigned stream, unsigned offsetMs) {
    StrmPcm *pcm = calloc(1, sizeof *pcm);
    if (!pcm) return NULL;
    if (!FS_OpenFile(&pcm->file, path) || !load_header(pcm, stream, offsetMs)) {
        StrmPcm_Close(pcm);
        return NULL;
    }
    return pcm;
}

void StrmPcm_Close(StrmPcm *stream) {
    if (stream) {
        if (FS_IsFile(&stream->file)) FS_CloseFile(&stream->file);
        free(stream);
    }
}

unsigned StrmPcm_Rate(const StrmPcm *stream) {
    return stream->rate;
}

unsigned StrmPcm_Volume(const StrmPcm *stream) {
    return stream->volume;
}

static int refill(StrmPcm *stream) {
    uint32_t block = stream->position / stream->blockFrames;
    uint32_t within = stream->position % stream->blockFrames;
    int last = block == stream->blocks - 1;
    uint32_t count = (last ? stream->lastFrames : stream->blockFrames) - within;
    if (count > READ_FRAMES) count = READ_FRAMES;
    if (count > stream->end - stream->position) count = stream->end - stream->position;
    uint32_t stride = last ? stream->lastBytes : stream->blockBytes;
    uint64_t offset = stream->data + (uint64_t)block * stream->blockBytes * stream->channels + 2 * (uint64_t)within;

    /* Read a contiguous window per channel. A stereo STRM can put its
     * channels megabytes apart, even when they belong to the same block. */
    stream->cacheFrames = 0;
    if (!read_at(&stream->file, offset, stream->cache[0], 2 * count)
        || (stream->channels == 2 && !read_at(&stream->file, offset + stride, stream->cache[1], 2 * count))) {
        return 0;
    }
    stream->cacheStart = stream->position;
    stream->cacheFrames = count;
    return 1;
}

unsigned StrmPcm_ReadStereo(StrmPcm *stream, int16_t *samples, unsigned frames) {
    unsigned filled = 0;
    while (filled < frames) {
        if (stream->position == stream->end) {
            if (!stream->loop) break;
            stream->position = stream->loopStart;
        }
        if (stream->position - stream->cacheStart >= stream->cacheFrames && !refill(stream)) break;
        uint32_t within = stream->position - stream->cacheStart;
        uint32_t available = stream->cacheFrames - within;
        uint32_t count = frames - filled;
        if (count > available) count = available;
        const int16_t *left = stream->cache[0] + within;
        const int16_t *right = stream->cache[stream->channels - 1] + within;
        for (uint32_t i = 0; i < count; ++i) {
            *samples++ = left[i];
            *samples++ = right[i];
        }
        filled += count;
        stream->position += count;
    }
    return filled;
}
