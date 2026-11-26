/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "g3_raster.h"

#include "nitro/gx.h"
#include "vram_host.h"
#ifdef PBW2_GPU_GEOMETRY
#include "g3_pica.h"
#include "log.h"
#endif

#include <math.h>
#include <string.h>

enum { WIDTH = 256, HEIGHT = 192, PIXELS = WIDTH * HEIGHT };

typedef struct {
    float x, y, z, w, s, t;
    float r, g, b;
} Vertex;

typedef struct {
    Vertex vertex;
    unsigned outside;
} ClipVertex;

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
    u8 opaqueId[PIXELS];
    u8 shadowMask[PIXELS];
    u8 fogAlpha[PIXELS];
    u32 fogDepthW[PIXELS];
} Frame;

static Frame s_drawing;
static u16 s_display[PIXELS];
/* The game publishes a complete frame. Only the presentation thread reads it. */
static unsigned char s_displayLock;
static int s_frameStarted;
static unsigned s_primitive, s_count;
static int s_inPrimitive;
static ClipVertex s_strip[4];
static s16 s_xyz[3];
/* GX texture coordinates use signed 12.4 fixed point. */
static s16 s_rawTexcoord[2], s_texcoord[2];
static u16 s_vertexColor, s_diffuse, s_ambient, s_specular, s_emission;
static unsigned s_lightMask;
static bool s_useShininess;
static u8 s_shininess[128];
static struct {
    s16 direction[3];
    s32 specularReciprocal;
    u16 color;
} s_lights[4];
static u32 s_polygon, s_texture, s_palette, s_viewport;
static u32 s_clearColor;
static unsigned s_clearDepth;
static bool s_previousShadowMask;
static bool s_alphaTest;
static unsigned s_alphaRef;
static bool s_highlight;
static u16 s_toon[32];
static bool s_edgeEnabled;
static u16 s_edgeColors[8];
static struct {
    bool enabled, alphaOnly;
    unsigned offset, shift;
    u32 color;
    u8 density[32];
} s_fog;
enum { FOG_ENABLED = 0x80, FOG_CLEAR_DEPTH = 0x40 };
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
enum {
    GPU_FALLBACK_EDGE = 1u << 0,
    GPU_FALLBACK_FOG = 1u << 1,
    GPU_FALLBACK_TOON = 1u << 2,
    GPU_FALLBACK_SHADOW = 1u << 3,
    GPU_FALLBACK_CAPACITY = 1u << 4,
    GPU_FALLBACK_BATCH = 1u << 5,
};

static void report_gpu_fallback(unsigned reason, const char *name) {
    static unsigned reported;
    if (reported & reason) return;
    reported |= reason;
    Log_Line("GPU", "software geometry fallback: %s", name);
}

static void report_geometry_rate(bool gpu) {
    extern u64 svcGetSystemTick(void);
    const u64 ticksPerSecond = 268111856;
    static u64 windowStart;
    static unsigned frames, gpuFrames;
    u64 now = svcGetSystemTick();
    if (!windowStart) {
        windowStart = now;
        return;
    }
    gpuFrames += gpu;
    ++frames;
    u64 elapsed = now - windowStart;
    if (elapsed >= 4 * ticksPerSecond) {
        unsigned rate = (unsigned)(frames * ticksPerSecond * 10 / elapsed);
        Log_Line("PERF", "GX swaps %u.%u fps (%u GPU, %u CPU)", rate / 10, rate % 10, gpuFrames, frames - gpuFrames);
        windowStart = now;
        frames = gpuFrames = 0;
    }
}
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
    s_vertexColor = 0x7fff;
    s_diffuse = s_ambient = s_specular = s_emission = 0;
    s_lightMask = 0;
    s_useShininess = false;
    memset(s_shininess, 0, sizeof s_shininess);
    memset(s_lights, 0, sizeof s_lights);
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
    memset(s_drawing.opaqueId, (s_clearColor >> 24) & 63, sizeof s_drawing.opaqueId);
    memset(s_drawing.shadowMask, 0, sizeof s_drawing.shadowMask);
    s_previousShadowMask = false;
    if (s_fog.enabled) {
        unsigned fogAlpha = ((s_clearColor >> 16) & 31) | FOG_CLEAR_DEPTH;
        if (s_clearColor & REG_G3X_CLEAR_COLOR_F_MASK) fogAlpha |= FOG_ENABLED;
        memset(s_drawing.fogAlpha, fogAlpha, sizeof s_drawing.fogAlpha);
    }
    for (unsigned i = 0; i < PIXELS; ++i) {
        s_drawing.color[i] = color;
        s_drawing.depth[i] = depth;
    }
}

static void start_frame(void) {
    if (s_frameStarted) return;
    s_clearColor = reg_G3X_CLEAR_COLOR;
    s_clearDepth = reg_G3X_CLEAR_DEPTH & 0x7fff;
    u16 control = reg_G3X_DISP3DCNT;
    s_alphaTest = (control & REG_G3X_DISP3DCNT_ATE_MASK) != 0;
    s_alphaRef = s_alphaTest ? PBW2_VU16(0x04000340) & 31 : 0;
    s_highlight = (control & REG_G3X_DISP3DCNT_THS_MASK) != 0;
    for (unsigned i = 0; i < 32; ++i) s_toon[i] = PBW2_VU16(0x04000380 + 2 * i) & 0x7fff;
    s_edgeEnabled = (control & REG_G3X_DISP3DCNT_EME_MASK) != 0;
    if (s_edgeEnabled) {
        for (unsigned i = 0; i < 8; ++i) s_edgeColors[i] = PBW2_VU16(0x04000330 + 2 * i) & 0x7fff;
    }
    s_fog.enabled = false;
    if (control & REG_G3X_DISP3DCNT_FME_MASK) {
        s_fog.alphaOnly = (control & REG_G3X_DISP3DCNT_FMOD_MASK) != 0;
        s_fog.shift = (control & REG_G3X_DISP3DCNT_FOG_SHIFT_MASK) >> REG_G3X_DISP3DCNT_FOG_SHIFT_SHIFT;
        s_fog.offset = (reg_G3X_FOG_OFFSET & 0x7fff) << 9;
        s_fog.color = reg_G3X_FOG_COLOR;
        for (unsigned i = 0; i < 32; ++i) {
            s_fog.density[i] = PBW2_VU8(0x04000360 + i) & 127;
            s_fog.enabled |= s_fog.density[i] != 0;
        }
    }
    s_frameStarted = 1;
#ifdef PBW2_GPU_GEOMETRY
    s_gpuFrame = PbW2_PicaEnabled();
    if (s_gpuFrame && s_edgeEnabled && !PbW2_PicaSkipEdges()) {
        report_gpu_fallback(GPU_FALLBACK_EDGE, "edge marking");
        s_gpuFrame = false;
    }
    if (s_gpuFrame && s_fog.enabled && (s_clearColor & REG_G3X_CLEAR_COLOR_F_MASK)) {
        report_gpu_fallback(GPU_FALLBACK_FOG, "fog");
        s_gpuFrame = false;
    }
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
    /* Interpolated coordinates stay within the signed GX texture range.
     * Correct the truncate for negative fractional coordinates. */
    int n = (int)coord;
    n -= coord < n;
    if (!repeat) {
        if (n < 0) return 0;
        return n >= size ? size - 1 : n;
    }
    if (flip && (n & size)) return (size - 1) - (n & (size - 1));
    return n & (size - 1);
}

static unsigned descriptor_address(unsigned block) {
    return 0x20000 + ((block & 0x1ffff) >> 1) + ((block & 0x40000) ? 0x10000 : 0);
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
        unsigned info = descriptor_address(block);
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

static unsigned color6(unsigned channel) {
    return 2 * channel + (channel != 0);
}

static u16 toon_color(u16 texel, float intensity, bool textured) {
    unsigned red = intensity > 0.0f ? (unsigned)(2.0f * intensity + 1.001f) : 0;
    if (red > 63) red = 63;
    u16 tableColor = s_toon[red >> 1];
    u16 color = 0;
    for (unsigned shift = 0; shift < 15; shift += 5) {
        unsigned table = color6((tableColor >> shift) & 31);
        unsigned channel = s_highlight ? red : table;
        if (textured) channel = ((color6((texel >> shift) & 31) + 1) * (channel + 1) - 1) >> 6;
        if (s_highlight) channel += table;
        color |= (channel > 63 ? 31 : channel >> 1) << shift;
    }
    return color;
}

static unsigned fog_density(unsigned depth) {
    if (depth <= s_fog.offset) return s_fog.density[0] == 127 ? 128 : s_fog.density[0];
    u32 coordinate = ((depth - s_fog.offset) >> 2) << s_fog.shift;
    unsigned step = coordinate >> 17;
    unsigned density = s_fog.density[31];
    if (step <= 31) {
        unsigned fraction = coordinate & 0x1ffff;
        unsigned left = s_fog.density[step == 0 ? 0 : step - 1];
        density = (left * (0x20000 - fraction) + s_fog.density[step] * fraction) >> 17;
    }
    return density == 127 ? 128 : density;
}

static bool edge_against(unsigned i, unsigned id, float depth) {
    return s_drawing.opaqueId[i] != id && s_drawing.depth[i] > depth;
}

static void apply_edge_colors(void) {
    if (!s_edgeEnabled) return;
    unsigned clearId = (s_clearColor >> 24) & 63;
    float clearDepth = s_clearDepth == 0x7fff ? 2.0f : s_clearDepth / 32767.0f;
    for (unsigned y = 0; y < HEIGHT; ++y) {
        for (unsigned x = 0; x < WIDTH; ++x) {
            unsigned i = y * WIDTH + x;
            if (!(s_drawing.color[i] & 0x8000)) continue;
            unsigned id = s_drawing.opaqueId[i];
            float depth = s_drawing.depth[i];
            bool perimeter = !x || !y || x == WIDTH - 1 || y == HEIGHT - 1;
            if ((perimeter && id != clearId && clearDepth > depth) ||
                (x && edge_against(i - 1, id, depth)) ||
                (x + 1 < WIDTH && edge_against(i + 1, id, depth)) ||
                (y && edge_against(i - WIDTH, id, depth)) ||
                (y + 1 < HEIGHT && edge_against(i + WIDTH, id, depth))) {
                s_drawing.color[i] = 0x8000 | s_edgeColors[id >> 3];
            }
        }
    }
}

static void apply_fog(bool wBuffer) {
    if (!s_fog.enabled) return;
    for (unsigned i = 0; i < PIXELS; ++i) {
        unsigned attr = s_drawing.fogAlpha[i];
        if (!(attr & FOG_ENABLED)) continue;
        unsigned z;
        if (attr & FOG_CLEAR_DEPTH) z = (s_clearDepth << 9) | 511;
        else if (wBuffer) z = s_drawing.fogDepthW[i];
        else z = (unsigned)fmaxf(0.0f, s_drawing.depth[i] * 16777216.0f - 512.0f);
        unsigned density = fog_density(z);
        unsigned keep = 128 - density;
        unsigned alpha = ((attr & 31) * keep + ((s_fog.color >> 16) & 31) * density) >> 7;
        u16 color = s_drawing.color[i] & 0x7fff;
        if (!s_fog.alphaOnly) {
            u16 shaded = 0;
            for (unsigned shift = 0; shift < 15; shift += 5) {
                unsigned from = color6((color >> shift) & 31);
                unsigned to = color6((s_fog.color >> shift) & 31);
                shaded |= ((from * keep + to * density) >> 8) << shift;
            }
            color = shaded;
        }
        s_drawing.color[i] = color | (alpha != 0 ? 0x8000 : 0);
    }
}

static void raster_triangle(Vertex a, Vertex b, Vertex c, const Texture *texture, u32 polygon) {
    float area = edge(a, b, c.x, c.y);
    if (fabsf(area) < 0.001f || !(polygon & (area < 0 ? 0x80u : 0x40u))) return;
    unsigned polygonAlpha = (polygon >> 16) & 31;
    if (polygonAlpha <= s_alphaRef) return;
    bool decal = textured_decal(texture, polygon);
    unsigned mode = (polygon >> 4) & 3;
    bool toon = mode == GX_POLYGONMODE_TOON;
    bool shadow = mode == GX_POLYGONMODE_SHADOW;
    unsigned polygonId = (polygon >> 24) & 63;
    bool shadowMask = shadow && polygonId == 0;
    if (shadowMask && !s_previousShadowMask)
        memset(s_drawing.shadowMask, 0, sizeof s_drawing.shadowMask);
    s_previousShadowMask = shadowMask;
    unsigned translucentId = translucent_id(polygon);
    int left = (int)fmaxf(0, floorf(fminf(a.x, fminf(b.x, c.x))));
    int right = (int)fminf(WIDTH - 1, ceilf(fmaxf(a.x, fmaxf(b.x, c.x))));
    int top = (int)fmaxf(0, floorf(fminf(a.y, fminf(b.y, c.y))));
    int bottom = (int)fminf(HEIGHT - 1, ceilf(fmaxf(a.y, fmaxf(b.y, c.y))));
    float invArea = 1.0f / area;
    float ux = c.y - b.y, vx = a.y - c.y;
    bool textured = texture->format != GX_TEXFMT_NONE;
    bool constantW = a.w == 1.0f && b.w == 1.0f && c.w == 1.0f;
    for (int y = top; y <= bottom; ++y) {
        float rowU = (c.x - b.x) * (y + 0.5f - b.y);
        float rowV = (a.x - c.x) * (y + 0.5f - c.y);
        for (int x = left; x <= right; ++x) {
            float u = (rowU - ux * (x + 0.5f - b.x)) * invArea;
            float v = (rowV - vx * (x + 0.5f - c.x)) * invArea;
            float w = 1.0f - u - v;
            if (u < 0.0f || v < 0.0f || w < 0.0f) continue;
            unsigned pixel = y * WIDTH + x;
            float depth = a.z * u + b.z * v + c.z * w;
            bool depthPass = depth <= s_drawing.depth[pixel];
            if (shadowMask) {
                if (!depthPass) s_drawing.shadowMask[pixel] = 1;
                continue;
            }
            if (!depthPass) continue;
            if (shadow) {
                unsigned last = s_drawing.translucentId[pixel];
                if (!s_drawing.shadowMask[pixel] || (last ? last & 63 : s_drawing.opaqueId[pixel]) == polygonId)
                    continue;
            }

            float reciprocal = constantW ? 1.0f : 1.0f / (a.w * u + b.w * v + c.w * w);
            unsigned ta = 31;
            u16 texel = 0x7fff;
            if (textured) {
                texel = sample_texture(texture, (a.s * u + b.s * v + c.s * w) * reciprocal,
                                       (a.t * u + b.t * v + c.t * w) * reciprocal, &ta);
            }
            unsigned alpha = decal ? polygonAlpha : ((ta + 1) * (polygonAlpha + 1) - 1) >> 5;
            if (alpha <= s_alphaRef) continue;
            if (alpha < 31 && s_drawing.translucentId[pixel] == translucentId) continue;

            float red = (a.r * u + b.r * v + c.r * w) * reciprocal;
            unsigned r = (unsigned)(red + 0.5f);
            unsigned g = (unsigned)((a.g * u + b.g * v + c.g * w) * reciprocal + 0.5f);
            unsigned bl = (unsigned)((a.b * u + b.b * v + c.b * w) * reciprocal + 0.5f);
            u16 color;
            if (toon) {
                color = toon_color(texel, red, textured);
            } else if (decal) {
                color = decal_channel(texel & 31, r, ta)
                      | decal_channel((texel >> 5) & 31, g, ta) << 5
                      | decal_channel((texel >> 10) & 31, bl, ta) << 10;
            } else if (textured) {
                color = ((texel & 31) * r + 15) / 31
                      | ((((texel >> 5) & 31) * g + 15) / 31) << 5
                      | ((((texel >> 10) & 31) * bl + 15) / 31) << 10;
            } else {
                color = r | (g << 5) | (bl << 10);
            }
            u16 below = s_drawing.color[pixel];
            if (alpha < 31 && (below & 0x8000)) color = weighted_color(color, below, alpha + 1, 31 - alpha, 5);
            bool depthWrite = alpha == 31 || (polygon & (1u << 11));
            if (s_fog.enabled) {
                unsigned old = s_drawing.fogAlpha[pixel];
                unsigned coverage = alpha;
                bool fog = (polygon & GX_POLYGON_ATTR_MISC_FOG) != 0;
                if (alpha < 31) {
                    fog = fog && (old & FOG_ENABLED);
                    if ((old & 31) > coverage) coverage = old & 31;
                }
                unsigned clearDepth = depthWrite ? 0 : old & FOG_CLEAR_DEPTH;
                s_drawing.fogAlpha[pixel] = coverage | clearDepth | (fog ? FOG_ENABLED : 0);
                if (depthWrite) s_drawing.fogDepthW[pixel] = (unsigned)fminf(16777215.0f, 4096.0f * reciprocal);
            }
            s_drawing.color[pixel] = color | 0x8000;
            s_drawing.translucentId[pixel] = alpha < 31 ? translucentId : 0;
            if (alpha == 31) s_drawing.opaqueId[pixel] = polygonId;
            if (depthWrite) s_drawing.depth[pixel] = depth;
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

/* The game often submits field materials in the same order on the next
 * frame. Only the descriptor table determines a compressed texture's
 * palette span. Palette colors and texel indices have separate revisions. */
typedef struct {
    const u8 *descriptorBank;
    unsigned address, bytes;
    u32 revision;
    unsigned paletteOffset, paletteBytes;
} CompressedPaletteSpan;

static CompressedPaletteSpan s_paletteSpans[GPU_TEXTURES_MAX];

static PicaTextureKey texture_cache_key(const Texture *tex, unsigned material) {
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
        for (unsigned block = tex->address; block < tex->address + bytes;) {
            unsigned chunk = 0x20000 - (block & 0x1ffff);
            if (chunk > tex->address + bytes - block) chunk = tex->address + bytes - block;
            unsigned descriptor = descriptor_address(block);
            u32 revision = range_revision(tex->tiles, 0x20000, 4, descriptor, chunk / 2);
            if (revision > key.revision[1]) key.revision[1] = revision;
            block += chunk;
        }
        CompressedPaletteSpan *span = &s_paletteSpans[material];
        if (span->descriptorBank != tex->tiles[1] || span->address != tex->address ||
            span->bytes != bytes || span->revision != key.revision[1]) {
            unsigned minPalette = 0x10000, maxPalette = 0;
            for (unsigned block = tex->address; block < tex->address + bytes; block += 4) {
                unsigned descriptor = descriptor_address(block);
                unsigned base = (texture_byte(tex, descriptor) | texture_byte(tex, descriptor + 1) << 8) & 0x3fff;
                if (4 * base < minPalette) minPalette = 4 * base;
                if (4 * base + 8 > maxPalette) maxPalette = 4 * base + 8;
            }
            *span = (CompressedPaletteSpan){tex->tiles[1], tex->address, bytes, key.revision[1],
                                            minPalette, maxPalette - minPalette};
        }
        palette += span->paletteOffset;
        paletteBytes = span->paletteBytes;
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
    if (!s_gpuCount || !PbW2_PicaBegin(s_alphaRef)) return false;
    int gpuSlots[GPU_TEXTURES_MAX];
    float invWidth[GPU_TEXTURES_MAX], invHeight[GPU_TEXTURES_MAX];
    for (unsigned i = 0; i < s_gpuMaterialCount; ++i) {
        const Texture *tex = &s_gpuMaterials[i];
        gpuSlots[i] = -1;
        /* GX dimensions are powers of two, so their reciprocals are exact. */
        invWidth[i] = 1.0f / tex->width;
        invHeight[i] = 1.0f / tex->height;
        if (tex->format) {
            /* An upload between submission and replay must not cache the new
             * pixels under the revision recorded for the triangle. */
            for (unsigned bank = 0; bank < 7; ++bank) {
                if ((tex->key.banks & (4u << (4 * bank)))
                    && tex->key.revision[bank] != PbW2_VramBankRevision(bank)) return false;
            }
            PicaTexture image = {tex->width, tex->height, tex->param, tex, gpu_texel, texture_cache_key(tex, i)};
            gpuSlots[i] = PbW2_PicaUpload(&image);
            if (gpuSlots[i] < 0) return false;
        }
    }
    for (unsigned i = 0; i < s_gpuCount; ++i) {
        unsigned material = s_gpuTriangles[i].material;
        const Texture *tex = &s_gpuMaterials[material];
        u32 polygon = s_gpuTriangles[i].polygon;
        unsigned alpha = (polygon >> 16) & 31;
        float alphaFloat = alpha / 31.0f;
        PicaVertex vertex[3];
        for (unsigned j = 0; j < 3; ++j) {
            const Vertex *p = &s_gpuTriangles[i].vertex[j];
            /* Recreate clip-space coordinates so PICA performs the same
             * perspective division for colors and texture coordinates. */
            float w = 1.0f / p->w;
            vertex[j] = (PicaVertex){
                (p->x / (WIDTH / 2.0f) - 1.0f) * w, (p->y / (HEIGHT / 2.0f) - 1.0f) * w,
                -p->z * w, w, p->s * w * invWidth[material], p->t * w * invHeight[material],
                p->r * w / 31.0f, p->g * w / 31.0f, p->b * w / 31.0f, alphaFloat,
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
        /* GX truncates the product in five bits. An active alpha test
         * needs the software result if both factors can be fractional. */
        if (s_alphaTest && alpha < 31 && (textureAlpha & PICA_TEX_ALPHA_FRACTIONAL)) return false;
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
        unsigned alpha = (s_polygon >> 16) & 31;
        if (fabsf(area) < 0.001f || !(s_polygon & (area < 0 ? 0x80u : 0x40u)) || alpha <= s_alphaRef) return;
        unsigned mode = (s_polygon >> 4) & 3;
        if (mode == GX_POLYGONMODE_TOON || mode == GX_POLYGONMODE_SHADOW ||
            (s_fog.enabled && (s_polygon & GX_POLYGON_ATTR_MISC_FOG))) {
            if (mode == GX_POLYGONMODE_TOON) report_gpu_fallback(GPU_FALLBACK_TOON, "toon or highlight shading");
            else if (mode == GX_POLYGONMODE_SHADOW) report_gpu_fallback(GPU_FALLBACK_SHADOW, "shadow polygons");
            else report_gpu_fallback(GPU_FALLBACK_FOG, "fog");
            replay_on_cpu();
            raster_triangle(a, b, c, texture, s_polygon);
            return;
        }
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
        report_gpu_fallback(GPU_FALLBACK_CAPACITY, "geometry batch capacity");
        replay_on_cpu();
    }
#endif
    raster_triangle(a, b, c, texture, s_polygon);
}

static void submit_triangle(const ClipVertex *a, const ClipVertex *b, const ClipVertex *c) {
    if (a->outside & b->outside & c->outside) return;
    Vertex input[12] = { a->vertex, b->vertex, c->vertex };
    unsigned count = 3;
    if (a->outside | b->outside | c->outside) {
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

static s64 matrix_dot(const MtxFx44 *matrix, const s16 vector[3], unsigned axis) {
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
            uv[i] = (s16)(s_rawTexcoord[i] + (matrix_dot(tex, s_xyz, i) >> 24));
        }
    }
    Vertex v = {c[0], c[1], c[2], c[3], uv[0] / 16.0f, uv[1] / 16.0f,
                s_vertexColor & 31, (s_vertexColor >> 5) & 31, (s_vertexColor >> 10) & 31};
    /* A strip or quad can submit a vertex more than once. Its clip
     * coordinates are already fixed at emission, so classify it once. */
    unsigned outside = (v.x < -v.w) | ((v.x > v.w) << 1)
                     | ((v.y < -v.w) << 2) | ((v.y > v.w) << 3)
                     | ((v.z < -v.w) << 4) | ((v.z > v.w) << 5);
    ClipVertex *current = &s_strip[s_count & 3];
    *current = (ClipVertex){v, outside};
    ++s_count;
    switch (s_primitive) {
    case GX_BEGIN_TRIANGLES:
        if (s_count == 3) { submit_triangle(&s_strip[0], &s_strip[1], current); s_count = 0; }
        break;
    case GX_BEGIN_QUADS:
        if (s_count == 4) {
            submit_triangle(&s_strip[0], &s_strip[1], &s_strip[2]);
            submit_triangle(&s_strip[0], &s_strip[2], current);
            s_count = 0;
        }
        break;
    case GX_BEGIN_TRIANGLE_STRIP:
        if (s_count >= 3) {
            const ClipVertex *a = &s_strip[(s_count - 3) & 3], *b = &s_strip[(s_count - 2) & 3];
            if (s_count & 1) submit_triangle(a, b, current);
            else submit_triangle(b, a, current);
        }
        break;
    case GX_BEGIN_QUAD_STRIP:
        if (s_count >= 4 && !(s_count & 1)) {
            const ClipVertex *a = &s_strip[(s_count - 4) & 3], *b = &s_strip[(s_count - 3) & 3];
            const ClipVertex *c = &s_strip[(s_count - 2) & 3];
            submit_triangle(a, b, current);
            submit_triangle(a, current, c);
        }
        break;
    }
}

static int signed_field(u32 value, unsigned bits) {
    int sign = 1 << (bits - 1);
    return (int)((value & (2 * sign - 1)) ^ sign) - sign;
}

static void unpack_vector(u32 packed, s16 xyz[3]) {
    for (unsigned i = 0; i < 3; ++i) xyz[i] = signed_field(packed >> (10 * i), 10);
}

static void set_light_vector(u32 packed, const MtxFx44 *matrix) {
    unsigned light = packed >> 30;
    s16 xyz[3];
    unpack_vector(packed, xyz);
    int z = 0;
    for (unsigned i = 0; i < 3; ++i) {
        z = matrix_dot(matrix, xyz, i) >> FX32_SHIFT;
        s_lights[light].direction[i] = signed_field(-z, 11);
    }
    int denominator = 512 - signed_field(z, 11);
    s_lights[light].specularReciprocal = denominator ? (1 << 18) / denominator : 0;
}

static u16 lit_color(const s16 xyz[3], const MtxFx44 *matrix) {
    if (!s_lightMask) return s_emission;
    int normal[3];
    for (unsigned i = 0; i < 3; ++i) normal[i] = signed_field(matrix_dot(matrix, xyz, i) >> FX32_SHIFT, 11);
    u32 channels[3] = { (s_emission & 31u) << 14, ((s_emission >> 5) & 31u) << 14, (s_emission >> 10) << 14 };
    for (unsigned mask = s_lightMask; mask; mask &= mask - 1) {
        unsigned i = __builtin_ctz(mask);
        const s16 *dir = s_lights[i].direction;
        int dot = ((normal[0] * dir[0]) >> 9) + ((normal[1] * dir[1]) >> 9) + ((normal[2] * dir[2]) >> 9);
        int diffuse = 0, shine = 0;
        if (dot > 0) {
            diffuse = signed_field(dot, 11);
            int half = signed_field(dot + normal[2], 11);
            int squared = ((half * half) >> 10) & 1023;
            int specular = ((squared * s_lights[i].specularReciprocal) >> 8) - 512;
            if (specular > 0) shine = signed_field(specular, 14);
            if (shine < 0) shine = 0;
            if (shine > 511) shine = 511;
        }
        if (s_useShininess) shine = 2 * s_shininess[shine >> 2];
        for (unsigned c = 0; c < 3; ++c) {
            unsigned shift = 5 * c;
            int light = (s_lights[i].color >> shift) & 31;
            int direct = ((s_diffuse >> shift) & 31) * light * diffuse;
            int ambient = (s_ambient >> shift) & 31;
            int specular = (s_specular >> shift) & 31;
            /* GX stores the diffuse product in 20 bits before accumulating. */
            channels[c] += ((u32)direct & 0xfffff) + light * (512 * ambient + specular * shine);
        }
    }
    u16 color = 0;
    for (unsigned c = 0; c < 3; ++c) {
        unsigned value = channels[c] >> 14;
        color |= (value > 31 ? 31 : value) << (5 * c);
    }
    return color;
}

void PbW2_G3RasterCommand(u32 op, const u32 *p, const MtxFx44 *clip,
                         const MtxFx44 *texture, const MtxFx44 *vector) {
    switch (op) {
    case 0x20: s_vertexColor = p[0] & 0x7fff; return;
    case G3OP_NORMAL: {
        s16 normal[3];
        unpack_vector(p[0], normal);
        if ((s_texture >> 30) == GX_TEXGEN_NORMAL) {
            for (unsigned i = 0; i < 2; ++i) {
                s_texcoord[i] = (s16)(s_rawTexcoord[i] + (matrix_dot(texture, normal, i) >> 21));
            }
        }
        s_vertexColor = lit_color(normal, vector);
        return;
    }
    case G3OP_TEXCOORD: {
        s_rawTexcoord[0] = (s16)p[0];
        s_rawTexcoord[1] = (s16)(p[0] >> 16);
        if ((s_texture >> 30) == GX_TEXGEN_TEXCOORD) {
            /* GX reads the texture matrix with the command, before any vertex. */
            const s16 vector[3] = { s_rawTexcoord[0], s_rawTexcoord[1], 1 };
            for (unsigned i = 0; i < 2; ++i) {
                s_texcoord[i] = (s16)((matrix_dot(texture, vector, i) + texture->m[3][i]) >> 12);
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
        unpack_vector(p[0], s_xyz);
        for (unsigned i = 0; i < 3; ++i) s_xyz[i] *= 64;
        break;
    case 0x25: s_xyz[0] = (s16)p[0]; s_xyz[1] = (s16)(p[0] >> 16); break;
    case 0x26: s_xyz[0] = (s16)p[0]; s_xyz[2] = (s16)(p[0] >> 16); break;
    case 0x27: s_xyz[1] = (s16)p[0]; s_xyz[2] = (s16)(p[0] >> 16); break;
    case 0x28:
        for (unsigned i = 0; i < 3; ++i) s_xyz[i] = (s16)(s_xyz[i] + signed_field(p[0] >> (10 * i), 10));
        break;
    case G3OP_POLYGON_ATTR: s_polygon = p[0]; return;
    case G3OP_TEXIMAGE_PARAM: s_texture = p[0]; return;
    case 0x2b: s_palette = p[0]; return;
    case G3OP_DIF_AMB:
        s_diffuse = p[0] & 0x7fff;
        s_ambient = (p[0] >> 16) & 0x7fff;
        if (p[0] & 0x8000) s_vertexColor = s_diffuse;
        return;
    case G3OP_SPE_EMI:
        s_specular = p[0] & 0x7fff;
        s_emission = (p[0] >> 16) & 0x7fff;
        s_useShininess = (p[0] & 0x8000) != 0;
        return;
    case G3OP_LIGHT_VECTOR: set_light_vector(p[0], vector); return;
    case G3OP_LIGHT_COLOR: s_lights[p[0] >> 30].color = p[0] & 0x7fff; return;
    case 0x34: /* SHININESS delivers four table entries per FIFO word. */
        for (unsigned i = 0; i < sizeof s_shininess; ++i) s_shininess[i] = p[i / 4] >> (8 * (i & 3));
        return;
    case G3OP_BEGIN:
        s_primitive = p[0] & 3;
        s_lightMask = s_polygon & 15;
        s_count = 0;
        s_inPrimitive = 1;
        return;
    case G3OP_END: s_inPrimitive = 0; return;
    case 0x50:
        start_frame();
#ifdef PBW2_GPU_GEOMETRY
        if (s_gpuFrame && !render_with_pica()) {
            if (s_gpuCount) report_gpu_fallback(GPU_FALLBACK_BATCH, "batch requires software rendering");
            replay_on_cpu();
        }
        if (!s_gpuFrame) {
#endif
            apply_edge_colors();
            apply_fog((p[0] & (GX_BUFFERMODE_W << 1)) != 0);
#ifdef PBW2_GPU_GEOMETRY
        }
        report_geometry_rate(s_gpuFrame);
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

void PbW2_G3RasterComposite(u16 *rgb565, const u8 *window, u8 *planes, u16 *rawColors) {
    lock_display();
    for (unsigned i = 0; i < PIXELS; ++i) {
        u16 pixel = s_display[i];
        if ((pixel & 0x8000) && (!window || (window[i] & GX_WND_PLANEMASK_BG0))) {
            u16 color = ((pixel & 31) << 11) | ((pixel & 0x03e0) << 1) | ((pixel >> 10) & 31);
            rgb565[i] = color;
            if (planes) planes[i] = GX_WND_PLANEMASK_BG0;
            if (rawColors) rawColors[i] = color;
        }
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
