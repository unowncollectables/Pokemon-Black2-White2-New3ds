/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "g3_raster.h"

#include "nitro/gx.h"
#include "vram_host.h"
#ifdef PBW2_GPU_GEOMETRY
#include "g3_pica.h"
#endif

#include <math.h>
#include <string.h>

enum { WIDTH = 256, HEIGHT = 192, PIXELS = WIDTH * HEIGHT };

typedef struct {
    float x, y, z, w, s, t;
    float r, g, b;
} Vertex;

typedef struct {
    const u8 *tiles[4];
    const u8 *palette[6];
    unsigned param, format, width, height, address, paletteAddress;
#ifdef PBW2_GPU_GEOMETRY
    PicaTextureKey key;
#endif
} Texture;

typedef struct {
    u16 color[PIXELS];
    float depth[PIXELS];
    u8 translucentId[PIXELS];
} Frame;

static Frame s_drawing;
static u16 s_display[PIXELS];
/* The game publishes a complete frame. Only the presentation thread reads it. */
static unsigned char s_displayLock;
static int s_frameStarted;
static unsigned s_primitive, s_count;
static int s_inPrimitive;
static Vertex s_strip[4];
static s16 s_xyz[3];
/* GX texture coordinates use signed 12.4 fixed point. */
static s16 s_rawTexcoord[2], s_texcoord[2];
static u16 s_vertexColor, s_diffuse, s_ambient;
static u32 s_polygon, s_texture, s_palette, s_viewport;
static u32 s_clearColor;
static unsigned s_clearDepth;
#ifdef PBW2_GPU_GEOMETRY
enum { GPU_TRIANGLES_MAX = 8192, GPU_TEXTURES_MAX = 128 };
static struct {
    Vertex vertex[3];
    u32 polygon;
    unsigned material;
} s_gpuTriangles[GPU_TRIANGLES_MAX];
static Texture s_gpuMaterials[GPU_TEXTURES_MAX];
static unsigned s_gpuCount, s_gpuMaterialCount, s_lastMaterial;
static u32 s_gpuSourceBanks;
static bool s_gpuFrame;
#endif

static void lock_display(void) {
    while (__atomic_test_and_set(&s_displayLock, __ATOMIC_ACQUIRE)) {}
}

static void unlock_display(void) {
    __atomic_clear(&s_displayLock, __ATOMIC_RELEASE);
}

void PbW2_G3RasterReset(void) {
    lock_display();
    memset(s_display, 0, sizeof s_display);
    unlock_display();
    s_frameStarted = s_inPrimitive = 0;
#ifdef PBW2_GPU_GEOMETRY
    s_gpuCount = 0;
#endif
    s_count = 0;
    memset(s_xyz, 0, sizeof s_xyz);
    memset(s_rawTexcoord, 0, sizeof s_rawTexcoord);
    memset(s_texcoord, 0, sizeof s_texcoord);
    s_polygon = (31u << 16) | (3u << 6);
    s_vertexColor = s_diffuse = 0x7fff;
    s_ambient = 0;
    s_texture = s_palette = 0;
    s_viewport = GX_PACK_VIEWPORT_PARAM(0, 0, WIDTH - 1, HEIGHT - 1);
}

static void clear_software_frame(void) {
    u16 color = s_clearColor & 0x7fff;
    if ((s_clearColor >> 16) & 31) color |= 0x8000;
    float depth = s_clearDepth / 32767.0f;
    /* The default cleared depth admits fragments on the far plane. */
    if (depth >= 1.0f) depth = 2.0f;
    memset(s_drawing.translucentId, 0, sizeof s_drawing.translucentId);
    for (unsigned i = 0; i < PIXELS; ++i) {
        s_drawing.color[i] = color;
        s_drawing.depth[i] = depth;
    }
}

static void start_frame(void) {
    if (s_frameStarted) return;
    s_clearColor = reg_G3X_CLEAR_COLOR;
    s_clearDepth = reg_G3X_CLEAR_DEPTH & 0x7fff;
    s_frameStarted = 1;
#ifdef PBW2_GPU_GEOMETRY
    s_gpuFrame = PbW2_PicaEnabled();
    s_gpuCount = s_gpuMaterialCount = s_lastMaterial = 0;
    s_gpuSourceBanks = 0;
    if (s_gpuFrame) return;
#endif
    clear_software_frame();
}

static Texture texture_state(void) {
    Texture t = {0};
    t.param = s_texture;
    t.format = (s_texture >> 26) & 7;
    t.width = 8u << ((s_texture >> 20) & 7);
    t.height = 8u << ((s_texture >> 23) & 7);
    t.address = (s_texture & 0xffffu) << 3;
    t.paletteAddress = (s_palette & 0x1fffu) << (t.format == GX_TEXFMT_PLTT4 ? 3 : 4);
    if (t.format == GX_TEXFMT_NONE) return t;

    const u8 texBanks[] = {reg_GX_VRAMCNT_A, reg_GX_VRAMCNT_B, reg_GX_VRAMCNT_C, reg_GX_VRAMCNT_D};
    for (unsigned bank = 0; bank < 4; ++bank) {
        u8 control = texBanks[bank];
        if ((control & 0x87) == 0x83) {
            t.tiles[(control >> 3) & 3] = PbW2_VramMap((void *)(uintptr_t)(HW_LCDC_VRAM_A + bank * 0x20000));
        }
    }
    if ((reg_GX_VRAMCNT_E & 0x87) == 0x83) {
        const u8 *base = PbW2_VramMap((void *)HW_LCDC_VRAM_E);
        for (unsigned i = 0; i < 4; ++i) t.palette[i] = base + 0x4000 * i;
    }
    const u8 palBanks[] = {reg_GX_VRAMCNT_F, reg_GX_VRAMCNT_G};
    for (unsigned bank = 0; bank < 2; ++bank) {
        u8 control = palBanks[bank];
        if ((control & 0x87) == 0x83) {
            unsigned offset = (control >> 3) & 3;
            unsigned slot = (offset & 1) + ((offset & 2) << 1);
            t.palette[slot] = PbW2_VramMap((void *)(uintptr_t)(HW_LCDC_VRAM_F + bank * 0x4000));
        }
    }
#ifdef PBW2_GPU_GEOMETRY
    if (s_gpuFrame) {
        const u8 regs[] = {texBanks[0], texBanks[1], texBanks[2], texBanks[3],
                           reg_GX_VRAMCNT_E, palBanks[0], palBanks[1]};
        t.key.param = t.param;
        t.key.palette = t.paletteAddress;
        for (unsigned bank = 0; bank < 7; ++bank) {
            if ((regs[bank] & 0x87) == 0x83) {
                t.key.banks |= (((regs[bank] >> 3) & 3) | 4) << (4 * bank);
                t.key.revision[bank] = PbW2_VramBankRevision(bank);
            }
        }
    }
#endif
    return t;
}

static u8 texture_byte(const Texture *t, unsigned addr) {
    unsigned slot = addr >> 17;
    return slot < 4 && t->tiles[slot] ? t->tiles[slot][addr & 0x1ffff] : 0;
}

static u16 palette_color(const Texture *t, unsigned addr) {
    unsigned slot = addr >> 14;
    if (slot >= 6 || !t->palette[slot]) return 0;
    const u8 *p = t->palette[slot] + (addr & 0x3ffe);
    return (p[0] | (p[1] << 8)) & 0x7fff;
}

static u16 weighted_color(u16 a, u16 b, unsigned wa, unsigned wb, unsigned shift) {
    u16 result = 0;
    for (unsigned c = 0; c < 15; c += 5) {
        unsigned channel = (((a >> c) & 31) * wa + ((b >> c) & 31) * wb) >> shift;
        result |= (channel > 31 ? 31 : channel) << c;
    }
    return result;
}

static int texture_coord(float coord, int size, unsigned repeat, unsigned flip) {
    int n = (int)floorf(coord);
    if (!repeat) {
        if (n < 0) return 0;
        return n >= size ? size - 1 : n;
    }
    if (flip && (n & size)) return (size - 1) - (n & (size - 1));
    return n & (size - 1);
}

static u16 texture_pixel(const Texture *t, unsigned x, unsigned y, unsigned *alpha) {
    *alpha = 31;
    if (!t->format) return 0x7fff;
    unsigned i = y * t->width + x;
    unsigned index;
    switch (t->format) {
    case GX_TEXFMT_DIRECT: {
        unsigned addr = t->address + 2 * i;
        u16 rgba = texture_byte(t, addr) | (texture_byte(t, addr + 1) << 8);
        *alpha = (rgba & 0x8000) ? 31 : 0;
        return rgba & 0x7fff;
    }
    case GX_TEXFMT_COMP4x4: {
        unsigned block = t->address + ((y / 4) * (t->width / 4) + x / 4) * 4;
        unsigned info = 0x20000 + ((block & 0x1ffff) >> 1) + ((block & 0x40000) ? 0x10000 : 0);
        unsigned descriptor = texture_byte(t, info) | (texture_byte(t, info + 1) << 8);
        unsigned addr = t->paletteAddress + (descriptor & 0x3fff) * 4;
        unsigned mode = descriptor >> 14;
        index = (texture_byte(t, block + (y & 3)) >> (2 * (x & 3))) & 3;
        if (index == 3 && mode < 2) { *alpha = 0; return 0; }
        if (index >= 2 && (mode & 1)) {
            u16 a = palette_color(t, addr), b = palette_color(t, addr + 2);
            if (mode == 1) return weighted_color(a, b, 1, 1, 1);
            return index == 2 ? weighted_color(a, b, 5, 3, 3) : weighted_color(a, b, 3, 5, 3);
        }
        return palette_color(t, addr + 2 * index);
    }
    case GX_TEXFMT_PLTT4:
        index = (texture_byte(t, t->address + i / 4) >> (2 * (i & 3))) & 3;
        break;
    case GX_TEXFMT_PLTT16:
        index = (texture_byte(t, t->address + i / 2) >> (4 * (i & 1))) & 15;
        break;
    case GX_TEXFMT_A3I5:
        index = texture_byte(t, t->address + i);
        *alpha = (index >> 5) * 4 + (index >> 6);
        return palette_color(t, t->paletteAddress + 2 * (index & 31));
    case GX_TEXFMT_A5I3:
        index = texture_byte(t, t->address + i);
        *alpha = index >> 3;
        return palette_color(t, t->paletteAddress + 2 * (index & 7));
    default:
        index = texture_byte(t, t->address + i);
        break;
    }
    if (index == 0 && (t->param & (1u << 29))) *alpha = 0;
    return palette_color(t, t->paletteAddress + 2 * index);
}

static u16 sample_texture(const Texture *t, float s, float v, unsigned *alpha) {
    if (!t->format) {
        *alpha = 31;
        return 0x7fff;
    }
    unsigned x = texture_coord(s, t->width, t->param & (1u << 16), t->param & (1u << 18));
    unsigned y = texture_coord(v, t->height, t->param & (1u << 17), t->param & (1u << 19));
    return texture_pixel(t, x, y, alpha);
}

static float plane_distance(const Vertex *v, unsigned plane) {
    float coordinate = plane < 2 ? v->x : (plane < 4 ? v->y : v->z);
    return v->w + ((plane & 1) ? -coordinate : coordinate);
}

static Vertex interpolate(Vertex a, Vertex b, float t) {
    return (Vertex){
        a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t,
        a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t,
        a.s + (b.s - a.s) * t, a.t + (b.t - a.t) * t,
        a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t,
    };
}

static float edge(Vertex a, Vertex b, float x, float y) {
    return (b.x - a.x) * (y - a.y) - (b.y - a.y) * (x - a.x);
}

static bool textured_decal(const Texture *texture, u32 polygon) {
    return texture->format && ((polygon >> 4) & 3) == GX_POLYGONMODE_DECAL;
}

static unsigned translucent_id(u32 polygon) {
    /* Leave zero for clear and opaque pixels, including polygons with ID zero. */
    return 64 | ((polygon >> 24) & 63);
}

static unsigned decal_channel(unsigned texture, unsigned vertex, unsigned alpha) {
    if (alpha == 0) return vertex;
    if (alpha == 31) return texture;
    /* GX interpolates the nonzero channels in six bits before truncation. */
    unsigned tex6 = 2 * texture + (texture != 0);
    unsigned vtx6 = 2 * vertex + (vertex != 0);
    return (tex6 * alpha + vtx6 * (31 - alpha)) >> 6;
}

static void raster_triangle(Vertex a, Vertex b, Vertex c, const Texture *texture, u32 polygon) {
    float area = edge(a, b, c.x, c.y);
    if (fabsf(area) < 0.001f || !(polygon & (area < 0 ? 0x80u : 0x40u))) return;
    unsigned polygonAlpha = (polygon >> 16) & 31;
    if (!polygonAlpha) return;
    bool decal = textured_decal(texture, polygon);
    unsigned translucentId = translucent_id(polygon);
    int left = (int)fmaxf(0, floorf(fminf(a.x, fminf(b.x, c.x))));
    int right = (int)fminf(WIDTH - 1, ceilf(fmaxf(a.x, fmaxf(b.x, c.x))));
    int top = (int)fmaxf(0, floorf(fminf(a.y, fminf(b.y, c.y))));
    int bottom = (int)fminf(HEIGHT - 1, ceilf(fmaxf(a.y, fmaxf(b.y, c.y))));
    float invArea = 1.0f / area;
    for (int y = top; y <= bottom; ++y) {
        for (int x = left; x <= right; ++x) {
            float u = edge(b, c, x + 0.5f, y + 0.5f) * invArea;
            float v = edge(c, a, x + 0.5f, y + 0.5f) * invArea;
            float w = 1.0f - u - v;
            if (u < 0.0f || v < 0.0f || w < 0.0f) continue;
            unsigned pixel = y * WIDTH + x;
            float depth = a.z * u + b.z * v + c.z * w;
            if (depth > s_drawing.depth[pixel]) continue;

            float reciprocal = 1.0f / (a.w * u + b.w * v + c.w * w);
            unsigned ta;
            u16 texel = sample_texture(texture, (a.s * u + b.s * v + c.s * w) * reciprocal,
                                        (a.t * u + b.t * v + c.t * w) * reciprocal, &ta);
            unsigned alpha = decal ? polygonAlpha : (ta * polygonAlpha + 15) / 31;
            if (!alpha) continue;
            if (alpha < 31 && s_drawing.translucentId[pixel] == translucentId) continue;

            unsigned r = (unsigned)((a.r * u + b.r * v + c.r * w) * reciprocal + 0.5f);
            unsigned g = (unsigned)((a.g * u + b.g * v + c.g * w) * reciprocal + 0.5f);
            unsigned bl = (unsigned)((a.b * u + b.b * v + c.b * w) * reciprocal + 0.5f);
            u16 color;
            if (decal) {
                color = decal_channel(texel & 31, r, ta)
                      | decal_channel((texel >> 5) & 31, g, ta) << 5
                      | decal_channel((texel >> 10) & 31, bl, ta) << 10;
            } else {
                color = ((texel & 31) * r + 15) / 31
                      | ((((texel >> 5) & 31) * g + 15) / 31) << 5
                      | ((((texel >> 10) & 31) * bl + 15) / 31) << 10;
            }
            u16 below = s_drawing.color[pixel];
            if (alpha < 31 && (below & 0x8000)) color = weighted_color(color, below, alpha + 1, 31 - alpha, 5);
            s_drawing.color[pixel] = color | 0x8000;
            s_drawing.translucentId[pixel] = alpha < 31 ? translucentId : 0;
            if (alpha == 31 || (polygon & (1u << 11))) s_drawing.depth[pixel] = depth;
        }
    }
}

#ifdef PBW2_GPU_GEOMETRY
static void replay_on_cpu(void) {
    clear_software_frame();
    for (unsigned i = 0; i < s_gpuCount; ++i) {
        const Vertex *v = s_gpuTriangles[i].vertex;
        raster_triangle(v[0], v[1], v[2], &s_gpuMaterials[s_gpuTriangles[i].material], s_gpuTriangles[i].polygon);
    }
    s_gpuCount = 0;
    s_gpuFrame = false;
}

static uint32_t gpu_texel(const void *source, unsigned x, unsigned y) {
    unsigned alpha;
    u16 pixel = texture_pixel(source, x, y, &alpha);
    return ((pixel & 31) * 255u / 31) << 24 | (((pixel >> 5) & 31) * 255u / 31) << 16
           | (((pixel >> 10) & 31) * 255u / 31) << 8 | alpha * 255 / 31;
}

static u32 range_revision(const u8 *const *banks, unsigned bankSize, unsigned count, unsigned address, unsigned size) {
    u32 latest = 0;
    while (size && address / bankSize < count) {
        unsigned slot = address / bankSize;
        unsigned offset = address % bankSize;
        unsigned chunk = bankSize - offset;
        if (chunk > size) chunk = size;
        if (banks[slot]) {
            u32 revision = PbW2_VramRangeRevision(banks[slot] + offset, chunk);
            if (revision > latest) latest = revision;
        }
        address += chunk;
        size -= chunk;
    }
    return latest;
}

static PicaTextureKey texture_cache_key(const Texture *tex) {
    PicaTextureKey key = tex->key;
    memset(key.revision, 0, sizeof key.revision);
    unsigned count = tex->width * tex->height;
    unsigned bytes = count, paletteBytes = 0, palette = tex->paletteAddress;
    switch (tex->format) {
    case GX_TEXFMT_PLTT4: bytes = count / 4; paletteBytes = 8; break;
    case GX_TEXFMT_PLTT16: bytes = count / 2; paletteBytes = 32; break;
    case GX_TEXFMT_PLTT256: paletteBytes = 512; break;
    case GX_TEXFMT_A3I5: paletteBytes = 64; break;
    case GX_TEXFMT_A5I3: paletteBytes = 16; break;
    case GX_TEXFMT_DIRECT: bytes = 2 * count; break;
    case GX_TEXFMT_COMP4x4: bytes = count / 4; break;
    }
    key.revision[0] = range_revision(tex->tiles, 0x20000, 4, tex->address, bytes);
    if (tex->format == GX_TEXFMT_COMP4x4) {
        /* Each compressed block names its own palette. Track the referenced
         * interval and the descriptor table as well as the texel indices. */
        unsigned minPalette = 0x10000, maxPalette = 0;
        for (unsigned block = tex->address; block < tex->address + bytes; block += 4) {
            unsigned descriptor = 0x20000 + ((block & 0x1ffff) >> 1) + ((block & 0x40000) ? 0x10000 : 0);
            unsigned base = (texture_byte(tex, descriptor) | texture_byte(tex, descriptor + 1) << 8) & 0x3fff;
            if (4 * base < minPalette) minPalette = 4 * base;
            if (4 * base + 8 > maxPalette) maxPalette = 4 * base + 8;
        }
        palette += minPalette;
        paletteBytes = maxPalette - minPalette;
        for (unsigned block = tex->address; block < tex->address + bytes;) {
            unsigned chunk = 0x20000 - (block & 0x1ffff);
            if (chunk > tex->address + bytes - block) chunk = tex->address + bytes - block;
            unsigned descriptor = 0x20000 + ((block & 0x1ffff) >> 1) + ((block & 0x40000) ? 0x10000 : 0);
            u32 revision = range_revision(tex->tiles, 0x20000, 4, descriptor, chunk / 2);
            if (revision > key.revision[1]) key.revision[1] = revision;
            block += chunk;
        }
    }
    key.revision[2] = range_revision(tex->palette, 0x4000, 6, palette, paletteBytes);
    return key;
}

static bool same_texture(const Texture *a, const Texture *b) {
    return !memcmp(&a->key, &b->key, sizeof a->key);
}

static unsigned gpu_material(const Texture *texture) {
    if (s_gpuMaterialCount && same_texture(texture, &s_gpuMaterials[s_lastMaterial])) return s_lastMaterial;
    unsigned i = 0;
    while (i < s_gpuMaterialCount && !same_texture(texture, &s_gpuMaterials[i])) ++i;
    if (i == GPU_TEXTURES_MAX) return i;
    if (i == s_gpuMaterialCount) {
        s_gpuMaterials[s_gpuMaterialCount++] = *texture;
        for (unsigned bank = 0; bank < 7; ++bank) {
            if (texture->key.banks & (4u << (4 * bank))) s_gpuSourceBanks |= 1u << bank;
        }
    }
    s_lastMaterial = i;
    return i;
}

static float repeat_shift(float a, float b, float c, float period) {
    float min = fminf(a, fminf(b, c));
    return min < 0.0f ? ceilf(-min / period) * period : 0.0f;
}

static bool render_with_pica(void) {
    if (!s_gpuCount || !PbW2_PicaBegin()) return false;
    int gpuSlots[GPU_TEXTURES_MAX];
    for (unsigned i = 0; i < s_gpuMaterialCount; ++i) {
        const Texture *tex = &s_gpuMaterials[i];
        gpuSlots[i] = -1;
        if (tex->format) {
            /* An upload between submission and replay must not cache the new
             * pixels under the revision recorded for the triangle. */
            for (unsigned bank = 0; bank < 7; ++bank) {
                if ((tex->key.banks & (4u << (4 * bank)))
                    && tex->key.revision[bank] != PbW2_VramBankRevision(bank)) return false;
            }
            PicaTexture image = {tex->width, tex->height, tex->param, tex, gpu_texel, texture_cache_key(tex)};
            gpuSlots[i] = PbW2_PicaUpload(&image);
            if (gpuSlots[i] < 0) return false;
        }
    }
    for (unsigned i = 0; i < s_gpuCount; ++i) {
        unsigned material = s_gpuTriangles[i].material;
        const Texture *tex = &s_gpuMaterials[material];
        u32 polygon = s_gpuTriangles[i].polygon;
        unsigned alpha = (polygon >> 16) & 31;
        PicaVertex vertex[3];
        for (unsigned j = 0; j < 3; ++j) {
            const Vertex *p = &s_gpuTriangles[i].vertex[j];
            /* Recreate clip-space coordinates so PICA performs the same
             * perspective division for colors and texture coordinates. */
            float w = 1.0f / p->w;
            vertex[j] = (PicaVertex){
                (p->x / (WIDTH / 2.0f) - 1.0f) * w, (p->y / (HEIGHT / 2.0f) - 1.0f) * w,
                -p->z * w, w, p->s * w / tex->width, p->t * w / tex->height,
                p->r * w / 31.0f, p->g * w / 31.0f, p->b * w / 31.0f, alpha / 31.0f,
            };
        }
        /* Keep repeating coordinates nonnegative. Moving an entire
         * triangle by a whole repeat period preserves perspective and
         * the DS floor rule for texels at negative coordinates. */
        if (tex->param & (1u << 16)) {
            float shift = repeat_shift(vertex[0].s, vertex[1].s, vertex[2].s,
                                       tex->param & (1u << 18) ? 2.0f : 1.0f);
            for (unsigned j = 0; j < 3; ++j) vertex[j].s += shift;
        }
        if (tex->param & (1u << 17)) {
            float shift = repeat_shift(vertex[0].t, vertex[1].t, vertex[2].t,
                                       tex->param & (1u << 19) ? 2.0f : 1.0f);
            for (unsigned j = 0; j < 3; ++j) vertex[j].t += shift;
        }
        int slot = gpuSlots[material];
        unsigned textureAlpha = PbW2_PicaTextureAlpha(slot);
        bool translucentDepth = (polygon & (1u << 11)) != 0;
        unsigned translucentId = translucent_id(polygon);
        if (textured_decal(tex, polygon)) {
            /* PICA interpolation agrees at the endpoints. Fractional GX decals
             * need the software path for their fixed-point color rounding. */
            if (textureAlpha & PICA_TEX_ALPHA_FRACTIONAL) return false;
            if (!PbW2_PicaTriangle(vertex, slot, PICA_ALPHA_VISIBLE, true, alpha < 31 ? translucentId : 0,
                                   alpha == 31 || translucentDepth)) return false;
            continue;
        }
        if (!textureAlpha) continue;
        bool blend = alpha < 31 || (textureAlpha & PICA_TEX_ALPHA_FRACTIONAL);
        if (alpha == 31 &&
            textureAlpha == (PICA_TEX_ALPHA_OPAQUE | PICA_TEX_ALPHA_FRACTIONAL)) {
            /* Reject alpha zero in the first pass. The opaque pass replaces
             * full-alpha pixels and resets their stencil even if they drew
             * in the first pass. */
            if (!PbW2_PicaTriangle(vertex, slot, PICA_ALPHA_VISIBLE, false, translucentId, translucentDepth)
                || !PbW2_PicaTriangle(vertex, slot, PICA_ALPHA_OPAQUE, false, 0, true)) return false;
        } else if (!PbW2_PicaTriangle(vertex, slot, PICA_ALPHA_VISIBLE, false, blend ? translucentId : 0,
                                     !blend || translucentDepth)) {
            return false;
        }
    }
    u32 clear = s_clearColor;
    u32 color = ((clear & 31) * 255 / 31) << 24 | (((clear >> 5) & 31) * 255 / 31) << 16
                | (((clear >> 10) & 31) * 255 / 31) << 8 | ((clear >> 16) & 31) * 255 / 31;
    return PbW2_PicaFinish(color, s_clearDepth / 32767.0f, s_drawing.color);
}
#endif

static void draw_triangle(Vertex a, Vertex b, Vertex c, const Texture *texture) {
#ifdef PBW2_GPU_GEOMETRY
    if (s_gpuFrame) {
        float area = edge(a, b, c.x, c.y);
        if (fabsf(area) < 0.001f || !(s_polygon & (area < 0 ? 0x80u : 0x40u)) || !(s_polygon & (31u << 16))) return;
        unsigned material = gpu_material(texture);
        if (s_gpuCount < GPU_TRIANGLES_MAX && material < GPU_TEXTURES_MAX) {
            unsigned i = s_gpuCount++;
            s_gpuTriangles[i].vertex[0] = a;
            s_gpuTriangles[i].vertex[1] = b;
            s_gpuTriangles[i].vertex[2] = c;
            s_gpuTriangles[i].material = material;
            s_gpuTriangles[i].polygon = s_polygon;
            return;
        }
        replay_on_cpu();
    }
#endif
    raster_triangle(a, b, c, texture, s_polygon);
}

static void submit_triangle(Vertex a, Vertex b, Vertex c) {
    Vertex input[12] = { a, b, c };
    unsigned count = 3;
    unsigned outside = 0;
    for (unsigned plane = 0; plane < 6; ++plane) {
        unsigned vertices = (plane_distance(&a, plane) < 0) + (plane_distance(&b, plane) < 0)
                          + (plane_distance(&c, plane) < 0);
        if (vertices == 3) return;
        outside |= vertices;
    }
    if (outside) {
        Vertex output[12];
        for (unsigned plane = 0; plane < 6; ++plane) {
            unsigned clipped = 0;
            Vertex prev = input[count - 1];
            float d = plane_distance(&prev, plane);
            for (unsigned i = 0; i < count; ++i) {
                Vertex cur = input[i];
                float e = plane_distance(&cur, plane);
                if ((d < 0) != (e < 0)) output[clipped++] = interpolate(prev, cur, d / (d - e));
                if (e >= 0) output[clipped++] = cur;
                prev = cur;
                d = e;
            }
            count = clipped;
            if (count < 3) return;
            memcpy(input, output, count * sizeof(Vertex));
        }
    }
    unsigned x0 = s_viewport & 255, y0 = (s_viewport >> 8) & 255;
    unsigned x1 = (s_viewport >> 16) & 255, y1 = (s_viewport >> 24) & 255;
    if (x0 > x1 || y0 > y1) return;
    float sx = (x1 - x0 + 1) * 0.5f, sy = (y1 - y0 + 1) * 0.5f;
    for (unsigned i = 0; i < count; ++i) {
        Vertex *v = &input[i];
        if (v->w < 0.000001f) return;
        v->w = 1.0f / v->w;
        v->x = x0 + sx * (v->x * v->w + 1.0f);
        v->y = HEIGHT - y0 - sy * (v->y * v->w + 1.0f);
        v->z = (v->z * v->w + 1.0f) * 0.5f;
        v->s *= v->w; v->t *= v->w;
        v->r *= v->w; v->g *= v->w; v->b *= v->w;
    }
    start_frame();
    Texture texture = texture_state();
    for (unsigned i = 2; i < count; ++i) draw_triangle(input[0], input[i - 1], input[i], &texture);
}

static s64 texture_dot(const MtxFx44 *matrix, const s16 vector[3], unsigned axis) {
    return (s64)vector[0] * matrix->m[0][axis] + (s64)vector[1] * matrix->m[1][axis]
         + (s64)vector[2] * matrix->m[2][axis];
}

static void emit_vertex(const MtxFx44 *clip, const MtxFx44 *tex) {
    if (!s_inPrimitive) return;
    float c[4];
    for (unsigned i = 0; i < 4; ++i) {
        s64 sum = (s64)s_xyz[0] * clip->m[0][i] + (s64)s_xyz[1] * clip->m[1][i] +
                  (s64)s_xyz[2] * clip->m[2][i] + (s64)FX32_ONE * clip->m[3][i];
        c[i] = (float)sum * (1.0f / (FX32_ONE * FX32_ONE));
    }
    s16 uv[2] = {s_texcoord[0], s_texcoord[1]};
    if ((s_texture >> 30) == GX_TEXGEN_VERTEX) {
        /* The generator uses the original vertex, before modelview and clip. */
        for (unsigned i = 0; i < 2; ++i) {
            uv[i] = (s16)(s_rawTexcoord[i] + (texture_dot(tex, s_xyz, i) >> 24));
        }
    }
    Vertex v = {c[0], c[1], c[2], c[3], uv[0] / 16.0f, uv[1] / 16.0f,
                s_vertexColor & 31, (s_vertexColor >> 5) & 31, (s_vertexColor >> 10) & 31};
    s_strip[s_count & 3] = v;
    ++s_count;
    switch (s_primitive) {
    case GX_BEGIN_TRIANGLES:
        if (s_count == 3) { submit_triangle(s_strip[0], s_strip[1], v); s_count = 0; }
        break;
    case GX_BEGIN_QUADS:
        if (s_count == 4) {
            submit_triangle(s_strip[0], s_strip[1], s_strip[2]);
            submit_triangle(s_strip[0], s_strip[2], v);
            s_count = 0;
        }
        break;
    case GX_BEGIN_TRIANGLE_STRIP:
        if (s_count >= 3) {
            Vertex a = s_strip[(s_count - 3) & 3], b = s_strip[(s_count - 2) & 3];
            if (s_count & 1) submit_triangle(a, b, v);
            else submit_triangle(b, a, v);
        }
        break;
    case GX_BEGIN_QUAD_STRIP:
        if (s_count >= 4 && !(s_count & 1)) {
            Vertex a = s_strip[(s_count - 4) & 3], b = s_strip[(s_count - 3) & 3], c = s_strip[(s_count - 2) & 3];
            submit_triangle(a, b, v);
            submit_triangle(a, v, c);
        }
        break;
    }
}

static int signed10(u32 x) {
    int n = x & 1023;
    return n >= 512 ? n - 1024 : n;
}

void PbW2_G3RasterCommand(u32 op, const u32 *p, const MtxFx44 *clip, const MtxFx44 *texture) {
    switch (op) {
    case 0x20: s_vertexColor = p[0] & 0x7fff; return;
    case G3OP_NORMAL:
        if ((s_texture >> 30) == GX_TEXGEN_NORMAL) {
            s16 normal[3];
            for (unsigned i = 0; i < 3; ++i) normal[i] = signed10(p[0] >> (10 * i));
            for (unsigned i = 0; i < 2; ++i) {
                s_texcoord[i] = (s16)(s_rawTexcoord[i] + (texture_dot(texture, normal, i) >> 21));
            }
        }
        if (s_polygon & 15) s_vertexColor = weighted_color(s_diffuse, s_ambient, 20, 12, 5);
        return;
    case G3OP_TEXCOORD: {
        s_rawTexcoord[0] = (s16)p[0];
        s_rawTexcoord[1] = (s16)(p[0] >> 16);
        if ((s_texture >> 30) == GX_TEXGEN_TEXCOORD) {
            /* GX reads the texture matrix with the command, before any vertex. */
            const s16 vector[3] = { s_rawTexcoord[0], s_rawTexcoord[1], 1 };
            for (unsigned i = 0; i < 2; ++i) {
                s_texcoord[i] = (s16)((texture_dot(texture, vector, i) + texture->m[3][i]) >> 12);
            }
        } else {
            memcpy(s_texcoord, s_rawTexcoord, sizeof s_texcoord);
        }
        return;
    }
    case 0x23:
        s_xyz[0] = (s16)p[0]; s_xyz[1] = (s16)(p[0] >> 16); s_xyz[2] = (s16)p[1];
        break;
    case 0x24:
        for (unsigned i = 0; i < 3; ++i) s_xyz[i] = (s16)(signed10(p[0] >> (10 * i)) * 64);
        break;
    case 0x25: s_xyz[0] = (s16)p[0]; s_xyz[1] = (s16)(p[0] >> 16); break;
    case 0x26: s_xyz[0] = (s16)p[0]; s_xyz[2] = (s16)(p[0] >> 16); break;
    case 0x27: s_xyz[1] = (s16)p[0]; s_xyz[2] = (s16)(p[0] >> 16); break;
    case 0x28:
        for (unsigned i = 0; i < 3; ++i) s_xyz[i] = (s16)(s_xyz[i] + signed10(p[0] >> (10 * i)));
        break;
    case G3OP_POLYGON_ATTR: s_polygon = p[0]; return;
    case G3OP_TEXIMAGE_PARAM: s_texture = p[0]; return;
    case 0x2b: s_palette = p[0]; return;
    case 0x30:
        s_diffuse = p[0] & 0x7fff;
        s_ambient = (p[0] >> 16) & 0x7fff;
        if (p[0] & 0x8000) s_vertexColor = s_diffuse;
        return;
    case G3OP_BEGIN: s_primitive = p[0] & 3; s_count = 0; s_inPrimitive = 1; return;
    case G3OP_END: s_inPrimitive = 0; return;
    case 0x50:
        start_frame();
#ifdef PBW2_GPU_GEOMETRY
        if (s_gpuFrame && !render_with_pica()) replay_on_cpu();
#endif
        lock_display();
        memcpy(s_display, s_drawing.color, sizeof s_display);
        unlock_display();
        s_frameStarted = s_inPrimitive = 0;
        return;
    case 0x60: s_viewport = p[0]; return;
    default: return;
    }
    emit_vertex(clip, texture);
}

void PbW2_G3RasterComposite(u16 *rgb565) {
    lock_display();
    for (unsigned i = 0; i < PIXELS; ++i) {
        u16 pixel = s_display[i];
        if (pixel & 0x8000) rgb565[i] = ((pixel & 31) << 11) | ((pixel & 0x03e0) << 1) | ((pixel >> 10) & 31);
    }
    unlock_display();
}

void PbW2_G3RasterCapture(u16 *rgb555) {
    lock_display();
    memcpy(rgb555, s_display, sizeof s_display);
    unlock_display();
}

void PbW2_G3RasterBeforeLcdcWrite(u32 address, u32 bytes) {
#ifdef PBW2_GPU_GEOMETRY
    /* The CPU backend samples at submission. Resolve queued geometry while
     * its original texture bytes are still present. */
    if (s_frameStarted && s_gpuFrame && s_gpuCount &&
        (s_gpuSourceBanks & PbW2_VramLcdcBankMask(address, bytes))) {
        replay_on_cpu();
    }
#else
    (void)address;
    (void)bytes;
#endif
}
