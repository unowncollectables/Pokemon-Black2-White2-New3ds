/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Followers Stage 1 (followers-design.md SS6.1): a port-layer,
 * render-only follower for Black 2 / White 2, shaped after Nate's
 * proven Platinum model (pspoke-patches/p3ds_follower.h):
 *
 * - NOT a game FieldActor. No actor is created, nothing collides,
 *   scripts cannot find it, and it writes NOTHING to the party, the
 *   event work, or the save. All state is port RAM below.
 * - Identity and field state come only from the game-side
 *   publication feed (include/pbw2_follower_feed.h, feed v2).
 * - Movement is a one-tile breadcrumb: when the player's tile
 *   changes, the follower walks to the tile the player just left,
 *   interpolated by the player's own step progress (derived from
 *   world positions, exactly like Platinum).
 * - Drawn as one textured quad through the normal G3 path right
 *   after the actor billboard draw (the hook in
 *   FieldRenderPhase2_Fieldmap), so stereo depth and wide view come
 *   for free. The texture is port-owned, served by the rasterizer
 *   through the magic TEXIMAGE address (see g3_raster.h).
 * - Stage 1 scope fence: field-only. On a zone change the trail
 *   resets and the follower hides for that frame (the SS5 respawn +
 *   ball pop are Stage 3); bike/surf/dive and script-owned-field
 *   frames hide it (their full hide/return rules are Stage 4).
 *
 * Sprite sourcing (SS3, BYO-ROM): the coverage gate below decides
 * the tier BEFORE any lookup -- the native TPoke lookup is never
 * called for an uncovered combination (SS2.8's OOB trap). Floor
 * species (the 103 TPoke-missing) get their strip converted at
 * RUNTIME from the user's own ROM filesystem (the staged nitrofs
 * copy of a/0/0/4 + a/0/1/6), using the floor pipeline's conversion
 * rules (floor-pipeline/PIPELINE.md): front battle sprite down,
 * back up, mirrored front right, front left, 1 px bob as frame 2,
 * shiny palette from battle data. No extracted asset is ever
 * committed or shipped; the strip exists only in port RAM.
 * Native-tier species resolve through PbW2_FollowerLoadNative(),
 * the Stage 2 entry point (TPoke data path); in Stage 1 it reports
 * "not loaded yet" and the follower stays in its Ball for those
 * species rather than drawing anything wrong.
 */
#include "follower.h"

#include "config.h"
#include "g3_raster.h"
#include "log.h"
#include "pbw2_follower_feed.h"

#include "nitro/fx.h"
#include "nitro/gx.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The feed instance the game side publishes into. */
PbW2FollowerFeed g_PbW2FollowerFeed;

/* NNS_G3dGlb's head, layout from lib/nnsys/include/nnsys/g3d.h --
 * only the camera matrix is needed (world -> view for the quad). */
typedef struct {
    u32 cmd0;
    u32 mtxmodeProj;
    MtxFx44 projMtx;
    u32 mtxmodePosVec;
    MtxFx43 cameraMtx;
} PbW2GlbHead;
extern PbW2GlbHead NNS_G3dGlb;
_Static_assert(offsetof(PbW2GlbHead, cameraMtx) == 76, "NNS_G3dGlb cameraMtx offset");

extern void NNS_G3dGeFlushBuffer(void);

#define TILE_FX (16 * FX32_ONE)
#define DIR_UP 0
#define DIR_DOWN 1
#define DIR_LEFT 2
#define DIR_RIGHT 3
/* FLD_PLAYER_EXSTATE_* (include/field/player/player_state.h) */
#define EXSTATE_NONE 0

/* ------------------------- coverage gate ------------------------- */
/* SS2.8: the gate is a port-owned table checked before any sprite
 * lookup. The floor set is exactly the 103 TPoke-missing species of
 * followers-design.md SS2.4. Everything else in 1..649 is native
 * TPoke tier; anything outside is no follower at all. */
static const u16 s_floorSpecies[] = {
    494, 495, 496, 497, 498, 499, 500, 501, 502, 503, 504, 506, 508, 509,
    512, 514, 516, 517, 518, 520, 521, 522, 524, 526, 528, 530, 532, 534,
    536, 537, 540, 541, 543, 544, 547, 549, 551, 552, 554, 555, 557, 560,
    562, 563, 565, 566, 567, 568, 570, 571, 573, 574, 576, 577, 579, 581,
    582, 584, 585, 586, 589, 590, 591, 592, 593, 595, 597, 598, 599, 601,
    602, 603, 604, 606, 608, 609, 611, 612, 614, 615, 617, 620, 623, 625,
    627, 629, 633, 634, 635, 636, 637, 638, 639, 640, 641, 642, 643, 644,
    645, 646, 647, 648, 649,
};

enum PbW2FollowerTier {
    TIER_NONE = 0,
    TIER_NATIVE_TPOKE,
    TIER_FLOOR,
};

static enum PbW2FollowerTier PbW2_FollowerCoverage(u16 species, u8 form, u8 sex) {
    (void)form;
    (void)sex;
    if (species < 1 || species > 649) return TIER_NONE;
    for (unsigned i = 0; i < sizeof s_floorSpecies / sizeof s_floorSpecies[0]; ++i)
        if (s_floorSpecies[i] == species) return TIER_FLOOR;
    return TIER_NATIVE_TPOKE;
}

/* ------------------------- follower state ------------------------- */
/* Port RAM only (the Platinum P3DS_FolMem pattern): survives the
 * overlay reloads around battles because nothing here is the game's. */
static struct {
    int valid;          /* follower placed on the trail */
    u16 zone;           /* zone the trail belongs to */
    s16 px, pz;         /* player tile at last update */
    VecFx32 ppos;       /* player position at last update */
    VecFx32 lpos;       /* player position when it last stood on (px,pz) */
    VecFx32 from, to;   /* follower walks from -> to during this step */
    VecFx32 cur;        /* follower's current world position */
    int steps;          /* tiles in the current step (1, 2 for a ledge) */
    int dir;            /* facing, DIR_* */
    unsigned anim;      /* frame clock */
    int moving;
    /* Loaded sprite identity. */
    int loaded;         /* a strip is registered with the rasterizer */
    u16 species;
    u8 form, sex;
    int shiny;
    int failedSpecies;  /* species whose load failed; don't retry every frame */
} s_fol;

static u8 s_stripIndices[256 * 32 / 2];
static u16 s_stripPalNormal[16], s_stripPalShiny[16];
static u32 s_stripGen;

static fx32 fol_abs(fx32 v) { return v < 0 ? -v : v; }

/* ------------------- runtime floor extraction ------------------- */
/* The floor pipeline (extract_floor.py) re-implemented for runtime:
 * NARC member access by index via fseek (the archives stay on SD;
 * only the members for one species are read), LZ10/LZ11, single-cell
 * NCGR decode, NCLR palettes, and the PIPELINE.md conversion rules.
 * Paths follow the staged nitrofs layout of the game's SD dir. */
#ifdef PBW2_WHITE2
#define FOL_SD_DIR "sdmc:/3ds/white2_3ds"
#else
#define FOL_SD_DIR "sdmc:/3ds/black2_3ds"
#endif
#define FOL_POKEGRA_PATH FOL_SD_DIR "/nitrofs/a/0/0/4"
#define FOL_PERSONAL_PATH FOL_SD_DIR "/nitrofs/a/0/1/6"

typedef struct {
    FILE *file;
    u32 count;
    u32 gmif; /* absolute offset of the GMIF data */
    u32 *entries; /* (start,end) pairs relative to gmif */
} Narc;

static void narc_close(Narc *narc) {
    if (narc->file) fclose(narc->file);
    free(narc->entries);
    memset(narc, 0, sizeof *narc);
}

static int narc_open(Narc *narc, const char *path) {
    memset(narc, 0, sizeof *narc);
    narc->file = fopen(path, "rb");
    if (!narc->file) return 0;
    u8 header[16];
    if (fread(header, 1, sizeof header, narc->file) != sizeof header || memcmp(header, "NARC", 4) != 0) goto fail;
    u32 pos = 16;
    for (;;) {
        u8 chunk[8];
        if (fseek(narc->file, (long)pos, SEEK_SET) != 0 || fread(chunk, 1, 8, narc->file) != 8) goto fail;
        u32 size;
        memcpy(&size, chunk + 4, 4);
        if (size < 8) goto fail;
        if (memcmp(chunk, "BTAF", 4) == 0) {
            u8 countBuf[4];
            if (fread(countBuf, 1, 4, narc->file) != 4) goto fail;
            memcpy(&narc->count, countBuf, 4);
            narc->entries = malloc(narc->count * 8);
            if (!narc->entries) goto fail;
            if (fread(narc->entries, 8, narc->count, narc->file) != narc->count) goto fail;
        } else if (memcmp(chunk, "GMIF", 4) == 0) {
            narc->gmif = pos + 8;
            break;
        }
        pos += size;
    }
    return narc->entries != NULL;
fail:
    narc_close(narc);
    return 0;
}

static u8 *narc_member(Narc *narc, u32 index, u32 *outSize) {
    if (index >= narc->count) return NULL;
    u32 start = narc->entries[2 * index], end = narc->entries[2 * index + 1];
    if (end < start) return NULL;
    u8 *buf = malloc(end - start ? end - start : 1);
    if (!buf) return NULL;
    if (fseek(narc->file, (long)(narc->gmif + start), SEEK_SET) != 0 ||
        (end > start && fread(buf, 1, end - start, narc->file) != end - start)) {
        free(buf);
        return NULL;
    }
    *outSize = end - start;
    return buf;
}

/* Nitro LZ10/LZ11 (the decomp's tools/data/nitro_2d.py algorithm). */
static u8 *lz_decode(const u8 *src, u32 srcSize, u32 *outSize) {
    if (srcSize < 4 || (src[0] != 0x10 && src[0] != 0x11)) {
        u8 *copy = malloc(srcSize ? srcSize : 1);
        if (copy) memcpy(copy, src, srcSize);
        *outSize = srcSize;
        return copy;
    }
    int kind = src[0];
    u32 size = src[1] | (src[2] << 8) | (src[3] << 16);
    u32 cursor = 4;
    if (size == 0) {
        if (srcSize < 8) return NULL;
        memcpy(&size, src + 4, 4);
        cursor = 8;
    }
    u8 *out = malloc(size ? size : 1);
    if (!out) return NULL;
    u32 len = 0;
    while (len < size) {
        if (cursor >= srcSize) goto fail;
        u8 flags = src[cursor++];
        for (int bit = 7; bit >= 0 && len < size; --bit) {
            if (!(flags & (1u << bit))) {
                if (cursor >= srcSize) goto fail;
                out[len++] = src[cursor++];
                continue;
            }
            if (cursor + 2 > srcSize) goto fail;
            unsigned first = src[cursor], second = src[cursor + 1];
            cursor += 2;
            unsigned length, distance;
            if (kind == 0x10) {
                length = (first >> 4) + 3;
                distance = (((first & 15) << 8) | second) + 1;
            } else if (first >> 4 == 0) {
                if (cursor >= srcSize) goto fail;
                unsigned third = src[cursor++];
                length = (((first & 15) << 4) | (second >> 4)) + 0x11;
                distance = (((second & 15) << 8) | third) + 1;
            } else if (first >> 4 == 1) {
                if (cursor + 2 > srcSize) goto fail;
                unsigned third = src[cursor], fourth = src[cursor + 1];
                cursor += 2;
                length = (((first & 15) << 12) | (second << 4) | (third >> 4)) + 0x111;
                distance = (((third & 15) << 8) | fourth) + 1;
            } else {
                length = (first >> 4) + 1;
                distance = (((first & 15) << 8) | second) + 1;
            }
            if (distance > len) goto fail;
            for (unsigned i = 0; i < length && len < size; ++i) {
                out[len] = out[len - distance];
                ++len;
            }
        }
    }
    *outSize = size;
    return out;
fail:
    free(out);
    return NULL;
}

/* A Nitro resource (NCGR/NCLR): LZ-decode and check the section. */
static u8 *resource(const u8 *blob, u32 blobSize, const char *magic, const char *section, u32 *outSize) {
    u8 *raw = lz_decode(blob, blobSize, outSize);
    if (!raw) return NULL;
    if (*outSize < 20 || memcmp(raw, magic, 4) != 0 || memcmp(raw + 16, section, 4) != 0) {
        free(raw);
        return NULL;
    }
    return raw;
}

/* Single-cell 96x96 NCGR -> 96x96 palette indices (extract_floor.py). */
static int single_cell_pixels(const u8 *blob, u32 blobSize, u8 out[96 * 96]) {
    u32 rawSize;
    u8 *raw = resource(blob, blobSize, "RGCN", "RAHC", &rawSize);
    if (!raw) return 0;
    u32 size;
    memcpy(&size, raw + 40, 4);
    if (size != 0x1200 || rawSize < 48 + size) {
        free(raw);
        return 0;
    }
    const u8 *chars = raw + 48;
    /* The four OBJs of a 96x96 single cell, in storage order. */
    static const u8 objs[4][4] = { {0, 0, 64, 64}, {64, 0, 32, 64}, {0, 64, 64, 32}, {64, 64, 32, 32} };
    u8 tiles[144][32];
    unsigned stored = 0;
    for (unsigned o = 0; o < 4; ++o) {
        for (unsigned y = objs[o][1]; y < objs[o][1] + objs[o][3]; y += 8)
            for (unsigned x = objs[o][0]; x < objs[o][0] + objs[o][2]; x += 8) {
                unsigned tiled = (y / 8) * 12 + x / 8;
                memcpy(tiles[tiled], chars + stored * 32, 32);
                ++stored;
            }
    }
    unsigned cursor = 0;
    for (unsigned ty = 0; ty < 96; ty += 8)
        for (unsigned tx = 0; tx < 96; tx += 8)
            for (unsigned y = 0; y < 8; ++y)
                for (unsigned x = 0; x < 8; x += 2) {
                    u8 value = ((const u8 *)tiles)[cursor++];
                    out[(ty + y) * 96 + tx + x] = value & 15;
                    out[(ty + y) * 96 + tx + x + 1] = value >> 4;
                }
    free(raw);
    return 1;
}

static int palette16(const u8 *blob, u32 blobSize, u16 out[16]) {
    u32 rawSize;
    u8 *raw = resource(blob, blobSize, "RLCN", "TTLP", &rawSize);
    if (!raw) return 0;
    u32 size;
    memcpy(&size, raw + 32, 4);
    if (size < 32 || rawSize < 40 + 32) {
        free(raw);
        return 0;
    }
    memcpy(out, raw + 40, 32);
    free(raw);
    return 1;
}

/* One 96x96 view -> a 32x32 frame: crop to the shared union box,
 * nearest-neighbour on palette indices (never colours), bottom-centre
 * anchored. The bob frame is the same frame shifted 1 px up. */
static void convert_view(const u8 *pixels, const int box[4], double scale, int mirror, int bob, u8 frame[32 * 32]) {
    memset(frame, 0, 32 * 32);
    int bw = box[2] - box[0], bh = box[3] - box[1];
    int nw = (int)(bw * scale + 0.5);
    int nh = (int)(bh * scale + 0.5);
    if (nw < 1) nw = 1;
    if (nh < 1) nh = 1;
    int ox = (32 - nw) / 2, oy = 32 - nh - (bob ? 1 : 0);
    for (int y = 0; y < nh; ++y) {
        int sy = box[1] + y * bh / nh;
        for (int x = 0; x < nw; ++x) {
            int sx = box[0] + (mirror ? (nw - 1 - x) * bw / nw : x * bw / nw);
            int dx = ox + x, dy = oy + y;
            if ((unsigned)dx < 32 && (unsigned)dy < 32) frame[dy * 32 + dx] = pixels[sy * 96 + sx];
        }
    }
}

static void sprite_bbox(const u8 *pixels, int box[4]) {
    box[0] = 96; box[1] = 96; box[2] = 0; box[3] = 0;
    for (int i = 0; i < 96 * 96; ++i) {
        if (!pixels[i]) continue;
        int x = i % 96, y = i / 96;
        if (x < box[0]) box[0] = x;
        if (y < box[1]) box[1] = y;
        if (x + 1 > box[2]) box[2] = x + 1;
        if (y + 1 > box[3]) box[3] = y + 1;
    }
}

/* Build the 256x32 controller-14 strip for a floor species from the
 * user's own ROM filesystem. Returns 1 on success. */
static int PbW2_FollowerLoadFloor(u16 species, u8 form, u8 sex) {
    Narc pokegra, personal;
    int ok = 0;
    u8 *frontMember = NULL, *backMember = NULL, *palMember = NULL, *shinyMember = NULL, *recMember = NULL;
    u8 *front = malloc(96 * 96), *back = malloc(96 * 96);
    if (!front || !back) goto done;
    if (!narc_open(&pokegra, FOL_POKEGRA_PATH)) {
        Log_Line("FOL", "pokegra narc open failed (%s)", FOL_POKEGRA_PATH);
        goto done;
    }
    if (!narc_open(&personal, FOL_PERSONAL_PATH)) {
        Log_Line("FOL", "personal narc open failed");
        goto done;
    }
    u32 recSize;
    recMember = narc_member(&personal, species, &recSize);
    if (!recMember || recSize < 0x22) goto done;
    u16 spriteOffset;
    memcpy(&spriteOffset, recMember + 0x1E, 2);
    u8 formCount = recMember[0x20];
    u8 flags = recMember[0x21];
    int paletteForms = (flags & 0x80) != 0, noFlip = (flags & 0x40) != 0;
    if (form >= formCount) goto done;
    u32 base, palBase = 0;
    if (form == 0 || paletteForms) {
        base = (u32)species * 20;
        if (paletteForms && form) palBase = 0x3AB9 + (spriteOffset + form - 1) * 2;
    } else {
        base = 0x3584 + (spriteOffset + form - 1) * 20;
    }
    u32 frontSlot = base + (sex == 1 ? 1 : 0), backSlot = base + 9 + (sex == 1 ? 1 : 0);
    u32 fSize, bSize, pSize, sSize;
    frontMember = narc_member(&pokegra, frontSlot, &fSize);
    if (sex == 1 && (!frontMember || fSize == 0)) {
        free(frontMember);
        frontMember = narc_member(&pokegra, base, &fSize);
        backSlot = base + 9;
    }
    backMember = narc_member(&pokegra, backSlot, &bSize);
    if (!frontMember || !backMember) goto done;
    if (palBase) {
        palMember = narc_member(&pokegra, palBase, &pSize);
        shinyMember = narc_member(&pokegra, palBase + 1, &sSize);
    } else {
        palMember = narc_member(&pokegra, base + 18, &pSize);
        shinyMember = narc_member(&pokegra, base + 19, &sSize);
    }
    if (!palMember || !shinyMember) goto done;
    if (!single_cell_pixels(frontMember, fSize, front) || !single_cell_pixels(backMember, bSize, back)) goto done;
    if (!palette16(palMember, pSize, s_stripPalNormal) || !palette16(shinyMember, sSize, s_stripPalShiny)) goto done;
    {
        int fb[4], bb[4], uni[4];
        sprite_bbox(front, fb);
        sprite_bbox(back, bb);
        if (fb[2] == 0 || bb[2] == 0) goto done;
        uni[0] = fb[0] < bb[0] ? fb[0] : bb[0];
        uni[1] = fb[1] < bb[1] ? fb[1] : bb[1];
        uni[2] = fb[2] > bb[2] ? fb[2] : bb[2];
        uni[3] = fb[3] > bb[3] ? fb[3] : bb[3];
        double scale = 32.0 / (uni[2] - uni[0]);
        double scaleY = 32.0 / (uni[3] - uni[1]);
        if (scaleY < scale) scale = scaleY;
        if (scale > 2.0) scale = 2.0;
        /* Frame order (controller 14): up, down, right, left pairs;
         * frame 2 of each pair is the 1 px bob. Sides are the front
         * (right mirrored unless the species is noFlip). */
        u8 frames[8][32 * 32];
        convert_view(back, uni, scale, 0, 0, frames[0]);
        convert_view(back, uni, scale, 0, 1, frames[1]);
        convert_view(front, uni, scale, 0, 0, frames[2]);
        convert_view(front, uni, scale, 0, 1, frames[3]);
        convert_view(front, uni, scale, !noFlip, 0, frames[4]);
        convert_view(front, uni, scale, !noFlip, 1, frames[5]);
        convert_view(front, uni, scale, 0, 0, frames[6]);
        convert_view(front, uni, scale, 0, 1, frames[7]);
        memset(s_stripIndices, 0, sizeof s_stripIndices);
        for (unsigned f = 0; f < 8; ++f)
            for (unsigned i = 0; i < 32 * 32; ++i) {
                unsigned at = f * 32 + (i % 32) + (i / 32) * 256;
                u8 index = frames[f][i] & 15;
                if (at & 1) s_stripIndices[at / 2] |= index << 4;
                else s_stripIndices[at / 2] |= index;
            }
    }
    ok = 1;
done:
    free(frontMember);
    free(backMember);
    free(palMember);
    free(shinyMember);
    free(recMember);
    free(front);
    free(back);
    narc_close(&pokegra);
    narc_close(&personal);
    return ok;
}

/* Native TPoke tier: the Stage 2 entry point. Stage 1 proves the
 * floor tier (Trubbish); the native data path (TPoke table + the
 * game's own field graphics archive) lands in Stage 2 behind this
 * same gate-checked call. Returning 0 keeps the follower in its
 * Ball for native species in Stage 1 -- never a wrong sprite. */
static int PbW2_FollowerLoadNative(u16 species, u8 form, u8 sex) {
    (void)species;
    (void)form;
    (void)sex;
    return 0;
}

/* Resolve the feed's lead identity to a registered strip, on change
 * only. Returns 1 when a sprite is ready to draw. */
static int follower_resolve_sprite(const PbW2FollowerFeed *feed) {
    if (feed->leadSlot >= PBW2_FOLLOWER_PARTY_SLOTS) {
        s_fol.loaded = 0;
        return 0;
    }
    const PbW2FollowerPartySlot *lead = &feed->party[feed->leadSlot];
    if (!lead->present || lead->egg || lead->hp == 0) {
        s_fol.loaded = 0;
        return 0;
    }
    if (s_fol.loaded && s_fol.species == lead->species && s_fol.form == lead->form &&
        s_fol.sex == lead->sex && s_fol.shiny == (lead->shiny != 0)) {
        return 1;
    }
    if (s_fol.failedSpecies == (int)lead->species && s_fol.form == lead->form && s_fol.sex == lead->sex) {
        return 0;
    }
    enum PbW2FollowerTier tier = PbW2_FollowerCoverage(lead->species, lead->form, lead->sex);
    int ok = 0;
    if (tier == TIER_FLOOR) {
        ok = PbW2_FollowerLoadFloor(lead->species, lead->form, lead->sex);
    } else if (tier == TIER_NATIVE_TPOKE) {
        ok = PbW2_FollowerLoadNative(lead->species, lead->form, lead->sex);
    }
    s_fol.species = lead->species;
    s_fol.form = lead->form;
    s_fol.sex = lead->sex;
    s_fol.shiny = lead->shiny != 0;
    if (!ok) {
        s_fol.loaded = 0;
        s_fol.failedSpecies = (int)lead->species;
        Log_Line("FOL", "species %u tier %d: no sprite loaded (stays in its Ball)", lead->species, (int)tier);
        return 0;
    }
    s_fol.failedSpecies = -1;
    ++s_stripGen;
    PbW2_G3FollowerSetTexture(s_stripIndices, s_stripPalNormal, s_stripPalShiny, s_fol.shiny != 0, s_stripGen);
    s_fol.loaded = 1;
    Log_Line("FOL", "lead species %u form %u sex %u shiny %d -> strip registered (gen %lu)",
             lead->species, lead->form, lead->sex, s_fol.shiny, (unsigned long)s_stripGen);
    return 1;
}

/* ------------------------- draw ------------------------- */

void PbW2_FollowerDraw(void) {
    /* The config store caches reads in RAM, so this is a cheap
     * per-frame check and an in-menu toggle lands the moment the
     * menu closes and writes the key through. */
    static int s_cfg = -1;
    int cfg = PbW2_ConfigGetBool("follower", false) ? 1 : 0;
    if (cfg != s_cfg) {
        s_cfg = cfg;
        Log_Line("FOL", "follower=%d", s_cfg);
    }
    if (!s_cfg) {
        s_fol.valid = 0;
        return;
    }
    const PbW2FollowerFeed *feed = &g_PbW2FollowerFeed;
    if (!feed->valid) {
        s_fol.valid = 0;
        return;
    }
    /* Stage 1 hide rules: non-walk avatar states and a script/task
     * owning the field hide the follower and reset the trail. A zone
     * change does the same for this frame (Stage 3 adds the respawn
     * + ball pop; a clean hide is Stage 1's behaviour). */
    if (feed->exState != EXSTATE_NONE || feed->scriptOwnsField || feed->zoneId != s_fol.zone) {
        if (feed->zoneId != s_fol.zone) {
            s_fol.zone = feed->zoneId;
        }
        s_fol.valid = 0;
        s_fol.px = feed->gridX;
        s_fol.pz = feed->gridZ;
        s_fol.ppos = feed->worldPos;
        s_fol.lpos = feed->worldPos;
        return;
    }
    if (!follower_resolve_sprite(feed)) {
        s_fol.valid = 0;
        return;
    }

    /* Breadcrumb trail (Platinum model): on a tile change the
     * follower walks to the tile the player just left. */
    VecFx32 pos = feed->worldPos;
    int x = feed->gridX, z = feed->gridZ;
    if (x != s_fol.px || z != s_fol.pz) {
        int dx = x - s_fol.px, dz = z - s_fol.pz;
        int ad = (dx < 0 ? -dx : dx) + (dz < 0 ? -dz : dz);
        if (ad > 2 || (dx && dz)) {
            /* Warp / jump / scripted slide: hide until the next
             * normal step re-seeds the trail. */
            s_fol.valid = 0;
        } else {
            VecFx32 left = s_fol.ppos; /* where the player just stood */
            if (!s_fol.valid) {
                s_fol.cur = left;
                s_fol.valid = 1;
            }
            s_fol.from = s_fol.cur;
            s_fol.to = left;
            s_fol.steps = ad;
            fx32 mdx = s_fol.to.x - s_fol.from.x, mdz = s_fol.to.z - s_fol.from.z;
            if (fol_abs(mdx) > 3 * TILE_FX || fol_abs(mdz) > 3 * TILE_FX) s_fol.from = s_fol.to;
            mdx = s_fol.to.x - s_fol.from.x;
            mdz = s_fol.to.z - s_fol.from.z;
            if (mdx || mdz) {
                s_fol.dir = fol_abs(mdx) >= fol_abs(mdz) ? (mdx < 0 ? DIR_LEFT : DIR_RIGHT)
                                                        : (mdz < 0 ? DIR_UP : DIR_DOWN);
            } else {
                s_fol.dir = dz < 0 ? DIR_UP : dz > 0 ? DIR_DOWN : dx < 0 ? DIR_LEFT : DIR_RIGHT;
            }
            s_fol.lpos = left;
        }
        s_fol.px = x;
        s_fol.pz = z;
    }
    s_fol.ppos = pos;
    if (!s_fol.valid) return;

    /* Standing still (turning in place) turns the follower too. */
    if (s_fol.from.x == s_fol.to.x && s_fol.from.z == s_fol.to.z) {
        s_fol.dir = feed->faceDir <= DIR_RIGHT ? (int)feed->faceDir : s_fol.dir;
    }

    /* Interpolate by the player's own step progress. */
    fx32 done = fol_abs(pos.x - s_fol.lpos.x) + fol_abs(pos.z - s_fol.lpos.z);
    fx32 len = s_fol.steps * TILE_FX;
    fx32 t = len > 0 ? (fx32)(((s64)done << 12) / len) : FX32_ONE;
    if (t > FX32_ONE) t = FX32_ONE;
    if (t < 0) t = 0;
    s_fol.moving = t < FX32_ONE && (s_fol.from.x != s_fol.to.x || s_fol.from.z != s_fol.to.z);
    s_fol.cur.x = s_fol.from.x + (fx32)(((s64)(s_fol.to.x - s_fol.from.x) * t) >> 12);
    s_fol.cur.y = s_fol.from.y + (fx32)(((s64)(s_fol.to.y - s_fol.from.y) * t) >> 12);
    s_fol.cur.z = s_fol.from.z + (fx32)(((s64)(s_fol.to.z - s_fol.from.z) * t) >> 12);
    if (!s_fol.moving) s_fol.from = s_fol.cur;
    /* Never stand on the player (first frame after a reversal). */
    if (fol_abs(s_fol.cur.x - pos.x) < 4 * FX32_ONE && fol_abs(s_fol.cur.z - pos.z) < 4 * FX32_ONE && !s_fol.moving) return;
    s_fol.anim++;

    /* Draw: view-space quad at the follower's feet, bottom-centred,
     * 32x32 world units (1 texel = 1 unit), like Platinum. Frame
     * selection: strip order up/down/right/left pairs. */
    static const int kFrameBase[4] = { 0, 2, 6, 4 }; /* DIR_UP, DOWN, LEFT, RIGHT */
    int dir = s_fol.dir & 3;
    int frame = kFrameBase[dir] + ((s_fol.anim >> (s_fol.moving ? 3 : 4)) & 1);
    VecFx32 centre = s_fol.cur, view;
    centre.y += 16 * FX32_ONE;
    MTX_MultVec43(&centre, &NNS_G3dGlb.cameraMtx, &view);
    view.y += 3 * FX32_ONE; /* field sprites sit ~3 px lower in their cell */

    NNS_G3dGeFlushBuffer();
    G3_MtxMode(GX_MTXMODE_POSITION_VECTOR);
    G3_PushMtx();
    G3_Identity();
    G3_Translate(view.x, view.y, view.z);
    G3_Scale(16 * FX32_ONE, 16 * FX32_ONE, 16 * FX32_ONE);
    G3_PolygonAttr(GX_LIGHTMASK_NONE, GX_POLYGONMODE_MODULATE, GX_CULL_NONE, 0, 31, GX_POLYGON_ATTR_MISC_FOG);
    G3_TexImageParam(GX_TEXFMT_PLTT16, GX_TEXGEN_TEXCOORD, GX_TEXSIZE_S256, GX_TEXSIZE_T32,
                     GX_TEXREPEAT_NONE, GX_TEXFLIP_NONE, GX_TEXPLTTCOLOR0_TRNS, PBW2_FOLLOWER_TEX_ADDR);
    G3_TexPlttBase(0, GX_TEXFMT_PLTT16);
    G3_Color(GX_RGB(31, 31, 31));
    fx32 s0 = frame * 32 * FX32_ONE, s1 = s0 + 32 * FX32_ONE;
    fx16 hw = (fx16)FX16_ONE; /* (32/2) texels at the 16x scale below */
    G3_Begin(GX_BEGIN_QUADS);
    G3_TexCoord(s0, 0); G3_Vtx(-hw, hw, 0);
    G3_TexCoord(s0, 32 * FX32_ONE); G3_Vtx(-hw, -hw, 0);
    G3_TexCoord(s1, 32 * FX32_ONE); G3_Vtx(hw, -hw, 0);
    G3_TexCoord(s1, 0); G3_Vtx(hw, hw, 0);
    G3_End();
    G3_PopMtx(1);
}
