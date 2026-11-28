/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "g3_pica.h"

#include "log.h"

#include <3ds.h>
#include <citro3d.h>
#include <string.h>

enum { WIDTH = 256, HEIGHT = 192, LCD_WIDTH = 400, LCD_HEIGHT = 240, VERTICES = 24576, BATCHES = 1024 };
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
} s_texture[PICA_TEXTURE_SLOTS];
static Batch s_batch[BATCHES];
static unsigned s_vertexCount, s_batchCount, s_cacheBytes, s_uploads, s_hits;
static unsigned s_invalidations;
static unsigned s_alphaRef;
static uint64_t s_frame;
static bool s_ready;
static unsigned s_options;
enum { PICA_ENABLED = 1u << 0, PICA_SKIP_EDGES = 1u << 1, PICA_ASYNC = 1u << 2 };

/* Async readback state (Build 24): at most one frame is ever pending —
 * the raster resolves it at the top of the next publish, before any
 * upload or eviction, so the readback buffer is fenced before reuse
 * exactly as the sync path's own fence guarantees. The pending frame's
 * centre image converts into s_stage (never into the caller's result
 * buffer: by resolve time that buffer may hold a CPU-rendered frame);
 * its native image converts into the native buffer captured here. */
typedef struct {
    bool active;
    u32 *pixels;
    unsigned stride, height, outputWidth, renderWidth;
    u16 *native;
    uint64_t queueTicks;
} PicaPending;

static PicaPending s_pending;
static u16 s_stage[LCD_WIDTH * LCD_HEIGHT];
static bool s_forceSync;

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
    /* Leave no stale record behind: the slot must not carry the evicted
     * texture's key, frame stamp or alpha into its next life. */
    memset(&s_texture[i].key, 0, sizeof s_texture[i].key);
    s_texture[i].frame = 0;
    s_texture[i].alpha = 0;
}

/* Zone transitions rewrite texture VRAM wholesale, and the cache key is a
 * write-order stamp, not content: no resident entry can be trusted across
 * one. Drop the whole cache so each zone starts from the known-good empty
 * state (the first-entry state that always rendered textured). The
 * re-upload cost is already being paid -- keys advance on every zone load
 * either way. Hits/uploads stay cumulative so tester comparisons across
 * entries still work; invalidations counts the resets themselves. */
static bool pica_async(void) {
    return (__atomic_load_n(&s_options, __ATOMIC_RELAXED) & PICA_ASYNC) != 0;
}

/* Waits until the pending frame's draw + transfer have completed (the
 * same FrameBegin/End pair the sync path fences with: beginning a
 * frame waits for its predecessor, including the transfer). Returns
 * false when the fence could not be run, leaving the frame pending. */
static bool fence_pending(void) {
    if (!C3D_FrameBegin(0)) return false;
    C3D_FrameEnd(GX_CMDLIST_FLUSH);
    return true;
}

/* Drops the pending frame without converting it (zone transitions and
 * shutdown): the fence still runs, so no later work can race the
 * in-flight transfer or sample a texture being evicted. */
static void drop_pending(void) {
    if (!s_pending.active) return;
    fence_pending();
    s_pending.active = false;
}

void PbW2_PicaInvalidate(void) {
    /* A pending async frame may still be sampling resident textures;
     * fence it before any eviction (the playdoh lesson). */
    drop_pending();
    for (unsigned i = 0; i < PICA_TEXTURE_SLOTS; ++i) evict(i);
    s_cacheBytes = 0;
    ++s_invalidations;
}

void PbW2_PicaExit(void) {
    if (!s_ready) return;
    drop_pending();
    for (unsigned i = 0; i < PICA_TEXTURE_SLOTS; ++i) evict(i);
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
    unsigned previous = __atomic_load_n(&s_options, __ATOMIC_RELAXED);
    unsigned options = (enabled ? PICA_ENABLED : 0) | (skipEdges ? PICA_SKIP_EDGES : 0)
                       | (previous & PICA_ASYNC);
    __atomic_store_n(&s_options, options, __ATOMIC_RELAXED);
    /* Turning the GPU off re-arms the one-shot setup: a failed first
     * attempt used to latch for the whole session, so the toggle
     * could never bring the GPU back after a transient setup miss. */
    if (!enabled) s_initTried = false;
}

void PbW2_PicaSetAsync(bool async) {
    unsigned previous = __atomic_load_n(&s_options, __ATOMIC_RELAXED);
    unsigned options = async ? (previous | PICA_ASYNC) : (previous & ~PICA_ASYNC);
    if (options == previous) return;
    __atomic_store_n(&s_options, options, __ATOMIC_RELAXED);
    Log_Line("GPU", "async readback %s (3D layer one publish late when on)", async ? "on" : "off");
}

bool PbW2_PicaSkipEdges(void) {
    return (__atomic_load_n(&s_options, __ATOMIC_RELAXED) & PICA_SKIP_EDGES) != 0;
}

bool PbW2_PicaEnabled(void) {
    if (!(__atomic_load_n(&s_options, __ATOMIC_RELAXED) & PICA_ENABLED)) return false;
    if (!s_ready && !s_initTried) {
        s_initTried = true;
        if (!PbW2_PicaInit()) Log_Error("GPU", "PICA200 setup failed, using the CPU rasterizer");
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
    unsigned oldest = PICA_TEXTURE_SLOTS;
    for (unsigned i = 0; i < PICA_TEXTURE_SLOTS; ++i) {
        if (s_texture[i].bytes && s_texture[i].frame != s_frame
            && (oldest == PICA_TEXTURE_SLOTS || s_texture[i].frame < s_texture[oldest].frame)) oldest = i;
    }
    return oldest;
}

int PbW2_PicaUpload(const PicaTexture *texture) {
    unsigned width = texture->width, height = texture->height;
    if (width > 1024 || height > 1024) return -1;
    for (unsigned i = 0; i < PICA_TEXTURE_SLOTS; ++i) {
        if (s_texture[i].bytes && !memcmp(&s_texture[i].key, &texture->key, sizeof texture->key)) {
            s_texture[i].frame = s_frame;
            ++s_hits;
            return i;
        }
    }
    unsigned bytes = width * height * 4;
    if (bytes > TEXTURE_CACHE_BYTES) return -1;
    /* Choose the slot and prove the byte budget before evicting anything.
     * A free slot is kept even when the cache is over budget -- the budget
     * is then met by evicting other unreferenced slots, not by throwing
     * the free slot away and evicting a live victim instead. And a failing
     * upload must leave the resident set untouched: the old code evicted
     * the chosen slot (then further LRU slots) before it could discover
     * every remaining slot was pinned this frame, permanently thinning
     * the cache on each failure -- a ratchet that guaranteed harder
     * eviction work on every later zone entry. */
    unsigned slot = 0;
    while (slot < PICA_TEXTURE_SLOTS && s_texture[slot].bytes) ++slot;
    if (slot == PICA_TEXTURE_SLOTS) {
        slot = oldest_unreferenced();
        if (slot == PICA_TEXTURE_SLOTS) return -1;
    }
    unsigned reclaimable = 0;
    for (unsigned i = 0; i < PICA_TEXTURE_SLOTS; ++i) {
        if (s_texture[i].bytes && (i == slot || s_texture[i].frame != s_frame))
            reclaimable += s_texture[i].bytes;
    }
    if (s_cacheBytes - reclaimable + bytes > TEXTURE_CACHE_BYTES) return -1;

    C3D_Tex *tex = &s_texture[slot].texture;
    /* A victim is always fully evicted and re-initialised, never overwritten
     * in place: the previous frame is not fenced on every publish path (the
     * frame-reuse early return submits no Pica work at all), so the previous
     * command list may still be sampling the victim's data. Overwriting it
     * in place rendered flat single-colour materials while every host-side
     * counter reported success (the "playdoh interiors" bug). Eviction is
     * already the slow path; an alloc/free per eviction is acceptable. */
    evict(slot);
    while (s_cacheBytes + bytes > TEXTURE_CACHE_BYTES) {
        unsigned oldest = oldest_unreferenced();
        if (oldest == PICA_TEXTURE_SLOTS) return -1; /* Unreachable: the budget was proven above. */
        evict(oldest);
    }
    if (!C3D_TexInit(tex, width, height, GPU_RGBA8)) return -1;
    s_cacheBytes += bytes;
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

/* Converts a fenced readback buffer into DS RGB5551 pixels. With a
 * native destination the centre image is resampled from each native
 * row in the same pass, while the row is still hot: the resample's
 * source-row map ((10y+5)/8) rises by 1 or 2 per result row, so each
 * native row feeds at most one centre row and no second pass over the
 * native buffer is needed. Pixel output is identical to the old
 * two-pass version. */
static void convert_frame(const u32 *pixels, unsigned stride, unsigned height,
                          unsigned outputWidth, unsigned renderWidth,
                          u16 *native, u16 *centre) {
    u16 *output = native ? native : centre;
    static int16_t rowMap[LCD_HEIGHT];
    static bool rowMapReady;
    if (native && !rowMapReady) {
        for (unsigned y = 0; y < LCD_HEIGHT; ++y) rowMap[y] = -1;
        for (unsigned y = 0; y < HEIGHT; ++y) rowMap[(10 * y + 5) / 8] = (int16_t)y;
        rowMapReady = true;
    }
    /* Prefetch the next row before converting the current one. The render
     * targets are exactly as tall as the longest viewport they serve. */
    for (unsigned y = 0; y < height; ++y) {
        if (y + 1 < height) __builtin_prefetch(pixels + (y + 1) * stride, 0, 0);
        const u32 *src = pixels + y * stride;
        u16 *dst = output + y * outputWidth;
        for (unsigned x = 0; x < outputWidth; x += 8) {
            for (unsigned j = 0; j < 8; ++j) {
                u32 rgba = src[x + j];
                dst[x + j] = ((rgba >> 27) & 31) | ((rgba >> 14) & 0x3e0) | ((rgba >> 1) & 0x7c00)
                             | ((rgba & 255) > 3 ? 0x8000 : 0);
            }
        }
        if (native) {
            int centreRow = rowMap[y];
            if (centreRow >= 0) {
                /* Map DS pixel centers to the nearest native sample for legacy capture. */
                u16 *dstRow = centre + centreRow * renderWidth;
                for (unsigned x = 0; x < renderWidth; ++x)
                    dstRow[x] = dst[(10 * x + 5) / 8];
            }
        }
    }
}

static void finish_stats(uint64_t ticks, uint64_t queueTicks, uint64_t resolveTicks) {
    static bool reported;
    static uint64_t totalTicks;
    static unsigned sampleCount;
    static uint64_t asyncQueueTicks, asyncResolveTicks;
    static unsigned asyncCount;
    totalTicks += ticks;
    ++sampleCount;
    if (queueTicks || resolveTicks) {
        asyncQueueTicks += queueTicks;
        asyncResolveTicks += resolveTicks;
        ++asyncCount;
    }
    if (!reported) {
        Log_Line("GPU", "first PICA200 frame (%u triangles, %u batches, %u uploads)", s_vertexCount / 3,
                 s_batchCount, s_uploads);
        reported = true;
    }
    if (s_frame == 2 || sampleCount == 120) {
        unsigned averageUs = totalTicks * 1000000ull / ((uint64_t)sampleCount * SYSCLOCK_ARM11);
        Log_Line("GPU", "texture cache: %u hits, %u uploads, %u invalidations, frame and readback avg %u us (%u frames)",
                 s_hits, s_uploads, s_invalidations, averageUs, sampleCount);
        if (asyncCount) {
            Log_Line("GPU", "async readback: queue avg %u us, resolve avg %u us (%u frames)",
                     (unsigned)(asyncQueueTicks * 1000000ull / ((uint64_t)asyncCount * SYSCLOCK_ARM11)),
                     (unsigned)(asyncResolveTicks * 1000000ull / ((uint64_t)asyncCount * SYSCLOCK_ARM11)),
                     asyncCount);
            asyncQueueTicks = asyncResolveTicks = 0;
            asyncCount = 0;
        }
        totalTicks = 0;
        sampleCount = 0;
    }
}

void PbW2_PicaSetForceSync(bool force) {
    s_forceSync = force;
}

bool PbW2_PicaAsyncPending(void) {
    return s_pending.active;
}

const uint16_t *PbW2_PicaResolvedCentre(void) {
    return s_stage;
}

bool PbW2_PicaResolvePending(void) {
    if (!s_pending.active) return false;
    uint64_t start = svcGetSystemTick();
    if (!fence_pending()) return false; /* Still in flight; retry next publish. */
    GSPGPU_InvalidateDataCache(s_pending.pixels, s_pending.stride * s_pending.height * sizeof(*s_pending.pixels));
    convert_frame(s_pending.pixels, s_pending.stride, s_pending.height, s_pending.outputWidth,
                  s_pending.renderWidth, s_pending.native, s_stage);
    uint64_t resolveTicks = svcGetSystemTick() - start;
    uint64_t queueTicks = s_pending.queueTicks;
    s_pending.active = false;
    finish_stats(queueTicks + resolveTicks, queueTicks, resolveTicks);
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
    bool async = pica_async() && !s_forceSync;
    s_forceSync = false;
    /* Never queue over an unresolved frame. The raster resolves at the
     * top of every publish, so this is only a safety net. */
    if (async && s_pending.active && !PbW2_PicaResolvePending()) return false;
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
    unsigned outputWidth = s_native ? LCD_WIDTH : s_renderWidth;
    if (async) {
        /* Queue only: the fence + convert run at the next publish's
         * resolve, after a full frame of game update has given the GPU
         * time to finish. The draw + transfer wait leaves the game
         * thread's serial path — that is the entire saving. */
        s_pending = (PicaPending){true, pixels, stride, height, outputWidth, s_renderWidth,
                                  s_native, svcGetSystemTick() - start};
        return true;
    }
    /* Beginning the next frame waits for its predecessor, including the transfer. */
    if (!C3D_FrameBegin(0)) return false;
    C3D_FrameEnd(GX_CMDLIST_FLUSH);
    GSPGPU_InvalidateDataCache(pixels, stride * height * sizeof(*pixels));
    convert_frame(pixels, stride, height, outputWidth, s_renderWidth, s_native, result);
    finish_stats(svcGetSystemTick() - start, 0, 0);
    return true;
}
