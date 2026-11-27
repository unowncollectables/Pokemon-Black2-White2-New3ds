/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "g3_pica.h"

#include "log.h"

#include <3ds.h>
#include <citro3d.h>
#include <string.h>

enum { WIDTH = 256, HEIGHT = 192, LCD_WIDTH = 400, LCD_HEIGHT = 240, VERTICES = 24576, BATCHES = 1024, TEXTURES = 256 };
enum { TEXTURE_CACHE_BYTES = 8 * 1024 * 1024 };

extern const u8 _binary_g3_pass_shbin_start[], _binary_g3_pass_shbin_end[];

typedef struct {
    unsigned first, count;
    int texture;
    PicaAlphaTest test;
    unsigned translucentId;
    bool decal, writeDepth;
} Batch;

static DVLB_s *s_shader;
static shaderProgram_s s_program;
static C3D_RenderTarget *s_target;
static unsigned s_renderWidth = WIDTH;
static u16 *s_native;
static C3D_RenderTarget *s_wideTarget;
static u32 *s_widePixels;

void PbW2_PicaSetFrameWidth(unsigned width, u16 *native) {
    s_renderWidth = width;
    s_native = native;
}
static PicaVertex *s_vertices;
static u32 *s_pixels;
static struct {
    C3D_Tex texture;
    PicaTextureKey key;
    uint64_t frame;
    unsigned bytes, alpha;
} s_texture[TEXTURES];
static Batch s_batch[BATCHES];
static unsigned s_vertexCount, s_batchCount, s_cacheBytes, s_uploads, s_hits;
static unsigned s_alphaRef;
static uint64_t s_frame;
static bool s_ready;
static unsigned s_options;
enum { PICA_ENABLED = 1u << 0, PICA_SKIP_EDGES = 1u << 1 };

bool PbW2_PicaInit(void) {
    if (!C3D_Init(C3D_DEFAULT_CMDBUF_SIZE)) return false;
    s_ready = true;
    s_target = C3D_RenderTargetCreate(WIDTH, HEIGHT, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
    s_vertices = linearAlloc(sizeof(*s_vertices) * VERTICES);
    s_pixels = linearAlloc(WIDTH * HEIGHT * sizeof(*s_pixels));
    s_shader = DVLB_ParseFile((u32 *)_binary_g3_pass_shbin_start,
                              _binary_g3_pass_shbin_end - _binary_g3_pass_shbin_start);
    if (!s_target || !s_vertices || !s_pixels || !s_shader) {
        PbW2_PicaExit();
        return false;
    }
    shaderProgramInit(&s_program);
    shaderProgramSetVsh(&s_program, &s_shader->DVLE[0]);
    return true;
}

static void evict(unsigned i) {
    if (!s_texture[i].bytes) return;
    C3D_TexDelete(&s_texture[i].texture);
    s_cacheBytes -= s_texture[i].bytes;
    s_texture[i].bytes = 0;
}

void PbW2_PicaExit(void) {
    if (!s_ready) return;
    for (unsigned i = 0; i < TEXTURES; ++i) evict(i);
    if (s_shader) {
        if (s_program.vertexShader) shaderProgramFree(&s_program);
        DVLB_Free(s_shader);
    }
    if (s_widePixels) linearFree(s_widePixels);
    if (s_wideTarget) C3D_RenderTargetDelete(s_wideTarget);
    if (s_pixels) linearFree(s_pixels);
    if (s_vertices) linearFree(s_vertices);
    if (s_target) C3D_RenderTargetDelete(s_target);
    C3D_Fini();
    s_ready = false;
}

static bool s_initTried;

void PbW2_PicaSetOptions(bool enabled, bool skipEdges) {
    unsigned options = (enabled ? PICA_ENABLED : 0) | (skipEdges ? PICA_SKIP_EDGES : 0);
    __atomic_store_n(&s_options, options, __ATOMIC_RELAXED);
    /* Turning the GPU off re-arms the one-shot setup: a failed first
     * attempt used to latch for the whole session, so the toggle
     * could never bring the GPU back after a transient setup miss. */
    if (!enabled) s_initTried = false;
}

bool PbW2_PicaSkipEdges(void) {
    return (__atomic_load_n(&s_options, __ATOMIC_RELAXED) & PICA_SKIP_EDGES) != 0;
}

bool PbW2_PicaEnabled(void) {
    if (!(__atomic_load_n(&s_options, __ATOMIC_RELAXED) & PICA_ENABLED)) return false;
    if (!s_ready && !s_initTried) {
        s_initTried = true;
        if (!PbW2_PicaInit()) Log_Line("GPU", "PICA200 setup failed, using the CPU rasterizer");
    }
    return s_ready;
}

bool PbW2_PicaBegin(unsigned alphaRef) {
    if (!PbW2_PicaEnabled()) return false;
    ++s_frame;
    s_batchCount = s_vertexCount = 0;
    /* A midpoint between consecutive GX alpha values tolerates either
     * rounding direction in PICA's eight-bit alpha conversion. */
    if (alphaRef == 0) s_alphaRef = 3;
    else s_alphaRef = alphaRef == 31 ? 255 : ((2 * alphaRef + 1) * 255) / 62;
    return true;
}

static GPU_TEXTURE_WRAP_PARAM wrap(unsigned repeat, unsigned mirror) {
    return repeat ? (mirror ? GPU_MIRRORED_REPEAT : GPU_REPEAT) : GPU_CLAMP_TO_EDGE;
}

static unsigned oldest_unreferenced(void) {
    unsigned oldest = TEXTURES;
    for (unsigned i = 0; i < TEXTURES; ++i) {
        if (s_texture[i].bytes && s_texture[i].frame != s_frame
            && (oldest == TEXTURES || s_texture[i].frame < s_texture[oldest].frame)) oldest = i;
    }
    return oldest;
}

int PbW2_PicaUpload(const PicaTexture *texture) {
    unsigned width = texture->width, height = texture->height;
    if (width > 1024 || height > 1024) return -1;
    for (unsigned i = 0; i < TEXTURES; ++i) {
        if (s_texture[i].bytes && !memcmp(&s_texture[i].key, &texture->key, sizeof texture->key)) {
            s_texture[i].frame = s_frame;
            ++s_hits;
            return i;
        }
    }
    unsigned bytes = width * height * 4;
    unsigned slot = 0;
    while (slot < TEXTURES && s_texture[slot].bytes) ++slot;
    if (slot == TEXTURES || s_cacheBytes + bytes > TEXTURE_CACHE_BYTES)
        slot = oldest_unreferenced();
    if (slot == TEXTURES) return -1;

    C3D_Tex *tex = &s_texture[slot].texture;
    /* The previous frame is fenced before we get here. Refresh only a victim
     * that has no references in this frame, matching SoulSilver's in-place
     * upload without sacrificing cache hits for other live materials. */
    bool reuse = s_texture[slot].bytes && tex->width == width && tex->height == height;
    if (!reuse) {
        evict(slot);
        while (s_cacheBytes + bytes > TEXTURE_CACHE_BYTES) {
            unsigned oldest = oldest_unreferenced();
            if (oldest == TEXTURES) return -1;
            evict(oldest);
        }
        if (!C3D_TexInit(tex, width, height, GPU_RGBA8)) return -1;
        s_cacheBytes += bytes;
    }
    u32 *tiles = tex->data;
    unsigned alpha = 0;
    /* Walk complete PICA tiles so writes stay within contiguous cache lines.
     * PICA's origin is below the image while DS T starts at the top. */
    for (unsigned y = height; y > 0; y -= 8) {
        for (unsigned x = 0; x < width; x += 8) {
            for (unsigned morton = 0; morton < 64; ++morton) {
                unsigned dx = (morton & 1) | ((morton >> 1) & 2) | ((morton >> 2) & 4);
                unsigned dy = ((morton >> 1) & 1) | ((morton >> 2) & 2) | ((morton >> 3) & 4);
                u32 rgba = texture->texel(texture->source, x + dx, y - 1 - dy);
                unsigned texelAlpha = rgba & 255;
                if (texelAlpha == 255) alpha |= PICA_TEX_ALPHA_OPAQUE;
                else if (texelAlpha) alpha |= PICA_TEX_ALPHA_FRACTIONAL;
                *tiles++ = rgba;
            }
        }
    }
    C3D_TexFlush(tex);
    C3D_TexSetFilter(tex, GPU_NEAREST, GPU_NEAREST);
    unsigned mode = texture->mode;
    C3D_TexSetWrap(tex, wrap(mode & (1u << 16), mode & (1u << 18)), wrap(mode & (1u << 17), mode & (1u << 19)));
    s_texture[slot].key = texture->key;
    s_texture[slot].frame = s_frame;
    s_texture[slot].bytes = bytes;
    s_texture[slot].alpha = alpha;
    ++s_uploads;
    return slot;
}

unsigned PbW2_PicaTextureAlpha(int texture) {
    return texture < 0 ? PICA_TEX_ALPHA_OPAQUE : s_texture[texture].alpha;
}

bool PbW2_PicaTriangle(const PicaVertex vertex[3], int texture, PicaAlphaTest test,
                      bool decal, unsigned translucentId, bool writeDepth) {
    if (s_vertexCount + 3 > VERTICES) return false;
    Batch *batch = s_batchCount ? &s_batch[s_batchCount - 1] : NULL;
    if (!batch || batch->texture != texture || batch->test != test || batch->translucentId != translucentId
        || batch->writeDepth != writeDepth || batch->decal != decal) {
        if (s_batchCount == BATCHES) return false;
        batch = &s_batch[s_batchCount++];
        *batch = (Batch){s_vertexCount, 0, texture, test, translucentId, decal, writeDepth};
    }
    for (unsigned i = 0; i < 3; ++i) s_vertices[s_vertexCount++] = vertex[i];
    batch->count += 3;
    return true;
}

bool PbW2_PicaFinish(uint32_t clearColor, float clearDepth, uint16_t *result) {
    u64 start = svcGetSystemTick();
    if (!s_batchCount) return false;
    /* The ordinary DS viewport has its own smaller target. Wide capture
     * uses a 320-wide viewport in the same allocation as native widescreen. */
    bool wide = s_renderWidth > WIDTH;
    if (wide && !s_wideTarget) {
        s_widePixels = linearAlloc(LCD_WIDTH * LCD_HEIGHT * sizeof(*s_widePixels));
        if (!s_widePixels) return false;
        s_wideTarget = C3D_RenderTargetCreate(LCD_WIDTH, LCD_HEIGHT, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
        if (!s_wideTarget) {
            linearFree(s_widePixels);
            s_widePixels = NULL;
            return false;
        }
    }
    if (!C3D_FrameBegin(0)) return false;
    C3D_RenderTarget *target = wide ? s_wideTarget : s_target;
    u32 *pixels = wide ? s_widePixels : s_pixels;
    unsigned stride = wide ? LCD_WIDTH : WIDTH;
    unsigned height = s_native ? LCD_HEIGHT : HEIGHT;
    unsigned targetHeight = wide ? LCD_HEIGHT : HEIGHT;
    GSPGPU_FlushDataCache(s_vertices, s_vertexCount * sizeof(*s_vertices));
    C3D_RenderTargetClear(target, C3D_CLEAR_ALL, clearColor, (unsigned)(0xffffff * clearDepth));
    C3D_FrameDrawOn(target);
    C3D_SetViewport(0, 0, s_native ? LCD_WIDTH : s_renderWidth, height);
    C3D_BindProgram(&s_program);
    C3D_AttrInfo *attrs = C3D_GetAttrInfo();
    AttrInfo_Init(attrs);
    AttrInfo_AddLoader(attrs, 0, GPU_FLOAT, 4);
    AttrInfo_AddLoader(attrs, 1, GPU_FLOAT, 2);
    AttrInfo_AddLoader(attrs, 2, GPU_FLOAT, 4);
    C3D_BufInfo *buffers = C3D_GetBufInfo();
    BufInfo_Init(buffers);
    BufInfo_Add(buffers, s_vertices, sizeof(*s_vertices), 3, 0x210);
    C3D_CullFace(GPU_CULL_NONE);
    C3D_DepthMap(true, -1.0f, 0.0f);
    C3D_StencilOp(GPU_STENCIL_KEEP, GPU_STENCIL_KEEP, GPU_STENCIL_REPLACE);
    for (unsigned i = 1; i < 6; ++i) C3D_TexEnvInit(C3D_GetTexEnv(i));
    const Batch *previous = NULL;
    for (unsigned i = 0; i < s_batchCount; ++i) {
        const Batch *batch = &s_batch[i];
        if (!previous || batch->test != previous->test) {
            switch (batch->test) {
            case PICA_ALPHA_VISIBLE: C3D_AlphaTest(true, GPU_GREATER, s_alphaRef); break;
            case PICA_ALPHA_OPAQUE: C3D_AlphaTest(true, GPU_EQUAL, 255); break;
            }
        }
        bool textured = batch->texture >= 0;
        if (textured && (!previous || batch->texture != previous->texture)) {
            C3D_TexBind(0, &s_texture[batch->texture].texture);
        }
        if (!previous || textured != (previous->texture >= 0) || batch->decal != previous->decal) {
            C3D_TexEnv *env = C3D_GetTexEnv(0);
            C3D_TexEnvInit(env);
            if (batch->decal) {
                C3D_TexEnvSrc(env, C3D_RGB, GPU_TEXTURE0, GPU_PRIMARY_COLOR, GPU_TEXTURE0);
                C3D_TexEnvOpRgb(env, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_ALPHA);
                C3D_TexEnvFunc(env, C3D_RGB, GPU_INTERPOLATE);
                C3D_TexEnvSrc(env, C3D_Alpha, GPU_PRIMARY_COLOR, 0, 0);
                C3D_TexEnvFunc(env, C3D_Alpha, GPU_REPLACE);
            } else if (textured) {
                C3D_TexEnvSrc(env, C3D_Both, GPU_PRIMARY_COLOR, GPU_TEXTURE0, 0);
                C3D_TexEnvFunc(env, C3D_Both, GPU_MODULATE);
            } else {
                C3D_TexEnvSrc(env, C3D_Both, GPU_PRIMARY_COLOR, 0, 0);
                C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);
            }
        }
        bool blend = batch->translucentId != 0;
        if (!previous || batch->translucentId != previous->translucentId) {
            C3D_StencilTest(true, blend ? GPU_NOTEQUAL : GPU_ALWAYS, batch->translucentId, 0x7f, 0x7f);
        }
        if (!previous || blend != (previous->translucentId != 0)) {
            if (blend) {
                C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_MAX, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ONE);
            } else {
                C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_ONE, GPU_ZERO, GPU_ONE, GPU_ZERO);
            }
        }
        if (!previous || batch->writeDepth != previous->writeDepth) {
            C3D_DepthTest(true, GPU_LEQUAL, batch->writeDepth ? GPU_WRITE_ALL : GPU_WRITE_COLOR);
        }
        C3D_DrawArrays(GPU_TRIANGLES, batch->first, batch->count);
        previous = batch;
    }
    u32 dims = GX_BUFFER_DIM(stride, targetHeight);
    u32 flags = GX_TRANSFER_FLIP_VERT(1) | GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8)
                | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGBA8);
    /* Vertex and texture writes are flushed where they are made. Flush
     * only the command list here, then enqueue the readback after it. */
    C3D_FrameSplit(GX_CMDLIST_FLUSH);
    GX_DisplayTransfer(target->frameBuf.colorBuf, dims, pixels, dims, flags);
    C3D_FrameEnd(GX_CMDLIST_FLUSH);
    /* Beginning the next frame waits for its predecessor, including the transfer. */
    if (!C3D_FrameBegin(0)) return false;
    C3D_FrameEnd(GX_CMDLIST_FLUSH);
    GSPGPU_InvalidateDataCache(pixels, stride * height * sizeof(*pixels));
    u16 *output = s_native ? s_native : result;
    unsigned outputWidth = s_native ? LCD_WIDTH : s_renderWidth;
    /* Prefetch the next row before converting the current one. The render
     * targets are exactly as tall as the longest viewport they serve. */
    for (unsigned y = 0; y < height; ++y) {
        if (y + 1 < height) __builtin_prefetch(pixels + (y + 1) * stride, 0, 0);
        for (unsigned x = 0; x < outputWidth; x += 8) {
            unsigned startPixel = y * stride + x;
            for (unsigned j = 0; j < 8; ++j) {
                u32 rgba = pixels[startPixel + j];
                output[y * outputWidth + x + j] = ((rgba >> 27) & 31) | ((rgba >> 14) & 0x3e0) | ((rgba >> 1) & 0x7c00)
                                       | ((rgba & 255) > 3 ? 0x8000 : 0);
            }
        }
    }
    if (s_native) {
        /* Map DS pixel centers to the nearest native sample for legacy capture. */
        for (unsigned y = 0; y < HEIGHT; ++y) {
            const u16 *row = s_native + ((10 * y + 5) / 8) * LCD_WIDTH;
            for (unsigned x = 0; x < s_renderWidth; ++x)
                result[y * s_renderWidth + x] = row[(10 * x + 5) / 8];
        }
    }
    static bool reported;
    static u64 totalTicks;
    static unsigned sampleCount;
    totalTicks += svcGetSystemTick() - start;
    ++sampleCount;
    if (!reported) {
        Log_Line("GPU", "first PICA200 frame (%u triangles, %u batches, %u uploads)", s_vertexCount / 3,
                 s_batchCount, s_uploads);
        reported = true;
    }
    if (s_frame == 2 || sampleCount == 120) {
        unsigned averageUs = totalTicks * 1000000ull / ((u64)sampleCount * SYSCLOCK_ARM11);
        Log_Line("GPU", "texture cache: %u hits, %u uploads, frame and readback avg %u us (%u frames)",
                 s_hits, s_uploads, averageUs, sampleCount);
        totalTicks = 0;
        sampleCount = 0;
    }
    return true;
}
