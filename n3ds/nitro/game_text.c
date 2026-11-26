/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "game_text.h"
#include "vram_host_api.h"

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

enum { TILE_RECORDS = 4096 };
typedef struct {
    uintptr_t address;
    u32 signature;
    u64 ink;
    u32 details[64];
} TextTile;

typedef struct {
    uintptr_t address;
    u64 ink;
    u8 colors[64];
    u32 details[64];
} BitmapTile;

static BitmapTile s_bitmapTiles[TILE_RECORDS];
static TextTile s_vramTiles[TILE_RECORDS];
static unsigned char s_vramLock;

static unsigned slot_for(const void *tile) {
    uintptr_t address = (uintptr_t)tile;
    return ((address >> 5) ^ (address >> 17)) & (TILE_RECORDS - 1);
}

static u32 tile_signature(const u8 *tile, unsigned size) {
    u32 hash = size;
    for (unsigned i = 0; i < size; ++i) hash = (hash ^ tile[i]) * 16777619u;
    return hash;
}

static const u8 *bitmap_tile(const GFLBitmap *bitmap, unsigned x, unsigned y) {
    return bitmap->pixels + ((y / 8) * ((bitmap->width + 7) / 8) + x / 8) * bitmap->tileSize;
}

void PbW2_TextMarkGlyph(const GFLBitmap *bitmap, const GFLBitmap *glyph, int x, int y, unsigned width, unsigned height) {
    if ((bitmap->tileSize != 32 && bitmap->tileSize != 64) || glyph->tileSize != 32) return;
    for (unsigned gy = 0; gy < height && gy < glyph->height; ++gy) {
        int dy = y + (int)gy;
        if (dy < 0 || dy >= bitmap->height) continue;
        for (unsigned gx = 0; gx < width && gx < glyph->width; ++gx) {
            int dx = x + (int)gx;
            if (dx < 0 || dx >= bitmap->width) continue;
            const u8 *glyphTile = bitmap_tile(glyph, gx, gy);
            unsigned color = (glyphTile[(gy & 7) * 4 + (gx & 7) / 2] >> ((gx & 1) * 4)) & 15;
            if (!color) continue;
            const u8 *tile = bitmap_tile(bitmap, dx, dy);
            BitmapTile *record = &s_bitmapTiles[slot_for(tile)];
            if (record->address != (uintptr_t)tile) {
                record->address = (uintptr_t)tile;
                record->ink = 0;
            }
            unsigned pixel = (dy & 7) * 8 + (dx & 7);
            unsigned under = bitmap->tileSize == 64 ? tile[pixel] : (tile[pixel / 2] >> ((pixel & 1) * 4)) & 15;
            u64 bit = (u64)1 << pixel;
            if ((record->ink & bit) && under == record->colors[pixel]) under = record->details[pixel] & 255;
            record->details[pixel] = under | ((gx & 31) << 16) | ((gy & 31) << 21);
            if (under && gx < 32 && gy < 32) record->details[pixel] |= PBW2_TEXT_NATIVE;
            record->colors[pixel] = color;
            record->ink |= (u64)1 << pixel;
        }
    }
}

void PbW2_TextForgetBitmap(const GFLBitmap *bitmap) {
    if (!bitmap->pixels || (bitmap->tileSize != 32 && bitmap->tileSize != 64)) return;
    unsigned tiles = ((bitmap->width + 7) / 8) * ((bitmap->height + 7) / 8);
    for (unsigned i = 0; i < tiles; ++i) {
        const u8 *tile = bitmap->pixels + i * bitmap->tileSize;
        BitmapTile *record = &s_bitmapTiles[slot_for(tile)];
        if (record->address == (uintptr_t)tile) record->address = 0;
    }
}

void PbW2_TextUpload(const GFLBitmap *bitmap, const void *base, unsigned first, unsigned size) {
    unsigned stride = bitmap->tileSize;
    if (!base || (stride != 32 && stride != 64)) return;
    for (unsigned off = 0; off + stride <= size; off += stride) {
        const u8 *tile = bitmap->pixels + off;
        const BitmapTile *source = &s_bitmapTiles[slot_for(tile)];
        const u8 *dest = PbW2_VramMap((const u8 *)base + first * stride + off);
        if (!dest) continue;
        TextTile *record = &s_vramTiles[slot_for(dest)];
        u64 ink = source->address == (uintptr_t)tile ? source->ink : 0;
        /* A later bitmap write may have replaced part of the glyph. */
        for (u64 pending = ink; pending; pending &= pending - 1) {
            unsigned pixel = __builtin_ctzll(pending);
            unsigned color = stride == 64 ? tile[pixel] : (tile[pixel / 2] >> ((pixel & 1) * 4)) & 15;
            if (color != source->colors[pixel]) ink &= ~((u64)1 << pixel);
        }
        if (source->address == (uintptr_t)tile) s_bitmapTiles[slot_for(tile)].ink = ink;
        u32 signature = ink ? tile_signature(tile, stride) : 0;
        while (__atomic_test_and_set(&s_vramLock, __ATOMIC_ACQUIRE)) {}
        if (ink) {
            record->ink = ink;
            memcpy(record->details, source->details, sizeof record->details);
            record->signature = signature;
            __atomic_store_n(&record->address, (uintptr_t)dest, __ATOMIC_RELAXED);
        } else if (record->address == (uintptr_t)dest) {
            __atomic_store_n(&record->address, 0, __ATOMIC_RELAXED);
        }
        __atomic_clear(&s_vramLock, __ATOMIC_RELEASE);
    }
}

u8 PbW2_TextTileRow(const u8 *tile, unsigned size, unsigned row, u32 details[8]) {
    TextTile *record = &s_vramTiles[slot_for(tile)];
    if (__atomic_load_n(&record->address, __ATOMIC_RELAXED) != (uintptr_t)tile) return 0;
    while (__atomic_test_and_set(&s_vramLock, __ATOMIC_ACQUIRE)) {}
    u8 ink = record->address == (uintptr_t)tile ? record->ink >> (row * 8) : 0;
    if (ink) memcpy(details, record->details + row * 8, 8 * sizeof(*details));
    u32 signature = record->signature;
    __atomic_clear(&s_vramLock, __ATOMIC_RELEASE);
    return ink && signature == tile_signature(tile, size) ? ink : 0;
}
