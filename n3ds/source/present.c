/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "present.h"

#include <3ds.h>

void Pbw2_GspVblank(void) {
    gspWaitForVBlank();
}

#include "log.h"
#include "pbw2_port.h"
#include "settings.h"
#if defined(PBW2_LINK_GAME)
#include "ds2d_compose.h"
#include "game_present.h"
#include "g3_raster.h"
#include "g3_replay.h"
#include "game_text.h"
#include "vram_host.h"
#include "workers.h"
#endif

#include <3ds.h>
#include <3ds/services/gspgpu.h>
#include <string.h>

static uint32_t s_flags;
static bool s_logged;
/* Stereo: s_drawSide selects which eye landscape_fb returns for the
 * top screen (GFX_LEFT everywhere except the right-eye draw pass).
 * s_stereoOn mirrors the libctru 3D state so gfxSet3D is only called
 * when the 3D slider crosses zero. */
static gfx3dSide_t s_drawSide = GFX_LEFT;
static bool s_stereoOn;
enum { GAME_WIDTH = 256, GAME_HEIGHT = 192 };

typedef struct {
    unsigned x, y, width, height;
} GameRect;

static GameRect game_rect(gfxScreen_t screen, unsigned lcdWidth, unsigned lcdHeight, uint32_t options) {
    unsigned width = GAME_WIDTH;
    unsigned height = GAME_HEIGHT;
    if ((options & DISPLAY_LCD_FIT) || (screen == GFX_TOP && (options & (DISPLAY_TOP_FIT | DISPLAY_WIDE_FIELD)))) {
        width = lcdHeight * GAME_WIDTH / GAME_HEIGHT;
        height = lcdHeight;
    }
    return (GameRect){(lcdWidth - width) / 2, (lcdHeight - height) / 2, width, height};
}

void Present_Commit(bool top, bool bottom) {
    s_flags = (top ? PBW2_SCREEN_TOP : 0u) | (bottom ? PBW2_SCREEN_BOTTOM : 0u);
}

uint32_t Present_Flags(void) {
    return s_flags;
}

#if defined(PBW2_LINK_GAME)
static void draw_native_text(gfxScreen_t screen, const u16 *src, unsigned sourceWidth, const u32 *ink,
                             bool restoreLCD);
static void draw_scaled(gfxScreen_t screen, const u16 *src, unsigned sourceWidth, uint32_t options, const u32 *ink);
#endif

#if defined(PBW2_LINK_GAME)
static bool has_text_ink(const u32 *ink) {
    /* Glyph-free scenes can use the plain presenter and bottom-screen cache. */
    if (!ink) return false;
    for (unsigned i = 0; i < GAME_WIDTH * GAME_HEIGHT; i += 4) {
        if (ink[i] | ink[i + 1] | ink[i + 2] | ink[i + 3]) return true;
    }
    return false;
}
#endif

#if defined(PBW2_LINK_GAME)
/* Distinct-frame fingerprint for the present-rate stats (the window
 * below): the composed top picture's FNV-1a hash, refreshed by
 * GxShim_Present on every present. s_topFresh marks that the latest
 * composed picture differs from the previous present's, i.e. this
 * present carries a frame the display has not already shown. */
static uint64_t s_topHash;
static bool s_topHashValid;
static bool s_topFresh;

static uint64_t top_frame_hash(const u16 *pic, const u32 *ink) {
    uint64_t hash = 1469598103934665603ull;
    for (unsigned i = 0; i < GAME_WIDTH * GAME_HEIGHT; i += 2) {
        hash ^= (uint32_t)pic[i] | ((uint32_t)pic[i + 1] << 16);
        hash *= 1099511628211ull;
    }
    if (ink) {
        for (unsigned i = 0; i < GAME_WIDTH * GAME_HEIGHT; i++) {
            hash ^= ink[i];
            hash *= 1099511628211ull;
        }
    }
    return hash;
}
#endif

#if defined(PBW2_LINK_GAME)
/* Compose worker (Build 19, SoulSilver's threaded present): the sub
 * engine's compose — and, in stereo, the right eye's — runs on a
 * core-2 worker while the present thread composes the main engine /
 * draws, instead of every compose running serially on core 0. Core 2
 * already hosts the audio (prio 0x20) and IO (0x28) workers; the
 * compose worker runs at 0x30 so both preempt it and audio is never
 * starved by compose work.
 *
 * Synchronization: one job slot, submitted and joined by the present
 * thread only. A job is always joined inside the present that
 * submitted it, so the slot is never contended and the worker is
 * idle whenever the main loop is outside GxShim_Present (in
 * particular at shutdown, where Workers_Shutdown only has to see the
 * run flag fall — the idle wait below is timed for exactly that).
 * Job arguments are plain pointers to the present's own buffers,
 * written before the kick event is signaled and read by the worker
 * only after it observes the signal; the done event orders the
 * worker's writes back. The composed buffers are disjoint per engine
 * (compose scratch is per-engine since this build), and the two
 * joins are the only points where the threads exchange results.
 *
 * Fallback: if the worker cannot be created, s_composeWorker stays
 * NULL and every job runs inline on the present thread — the
 * pre-Build-19 serial path, unchanged. */
typedef enum {
    CJ_NONE,
    CJ_SUB, /* PbW2_ComposeSubScreen on the job buffers */
    CJ_FULL /* PbW2_ComposeScreensWithText on the job buffers */
} ComposeJobKind;

static Worker *s_composeWorker;
static bool s_composeTried;
static Handle s_jobKick, s_jobDone;
static volatile ComposeJobKind s_jobKind = CJ_NONE;
static u16 *s_jobTop, *s_jobBottom;
static u32 *s_jobTopInk, *s_jobBottomInk;

static void Compose_WorkerMain(void *arg) {
    Worker *worker = arg;
    while (Worker_Run(worker)) {
        /* Timed wait: Workers_Shutdown clears the run flag and joins
         * without signaling, so the idle wait must expire on its own.
         * 50 ms of shutdown latency; kicks wake it immediately. */
        if (R_FAILED(svcWaitSynchronization(s_jobKick, 50000000LL))) continue;
        ComposeJobKind kind = s_jobKind;
        __atomic_thread_fence(__ATOMIC_ACQUIRE);
        if (kind == CJ_SUB) {
            PbW2_ComposeSubScreen(s_jobTop, s_jobBottom, s_jobTopInk, s_jobBottomInk);
        } else if (kind == CJ_FULL) {
            PbW2_ComposeScreensWithText(s_jobTop, s_jobBottom, s_jobTopInk, s_jobBottomInk);
        }
        s_jobKind = CJ_NONE;
        __atomic_thread_fence(__ATOMIC_RELEASE);
        svcSignalEvent(s_jobDone);
    }
}

static bool ComposeWorker_Ensure(void) {
    if (s_composeWorker) return true;
    if (s_composeTried) return false;
    s_composeTried = true;
    if (R_FAILED(svcCreateEvent(&s_jobKick, RESET_ONESHOT)) ||
        R_FAILED(svcCreateEvent(&s_jobDone, RESET_ONESHOT))) {
        Log_Line("GX", "compose worker: event create failed, serial compose");
        return false;
    }
    if (!Workers_Start("compose", Compose_WorkerMain, 0x30, 2, &s_composeWorker)) {
        Log_Line("GX", "compose worker: start failed, serial compose");
        return false;
    }
    return true;
}

/* Submits a job to the compose worker. Returns false (job NOT
 * submitted; caller runs it inline) when no worker is available. */
static bool ComposeJob_Kick(ComposeJobKind kind, u16 *top, u16 *bottom, u32 *topInk, u32 *bottomInk) {
    if (!ComposeWorker_Ensure()) return false;
    s_jobTop = top;
    s_jobBottom = bottom;
    s_jobTopInk = topInk;
    s_jobBottomInk = bottomInk;
    s_jobKind = kind;
    __atomic_thread_fence(__ATOMIC_RELEASE);
    svcClearEvent(s_jobDone);
    svcSignalEvent(s_jobKick);
    return true;
}

static void ComposeJob_Join(void) {
    svcWaitSynchronization(s_jobDone, U64_MAX);
    __atomic_thread_fence(__ATOMIC_ACQUIRE);
}
#endif

#if defined(PBW2_LINK_GAME)
/* Draws the top screen through the configured presentation chain —
 * the first variant the frame offers, in the flat path's order: the
 * wide 2D layout, the GPU native frame, the wide sides assembly, the
 * plain composed centre. Used for the flat draw and for each stereo
 * eye: an eye pass composes its own variants (its FinishWide and its
 * sides/native outputs replace the previous pass's), so the caller
 * draws each eye while that eye's pass is the one the accessors
 * describe — the left eye before the right compose is kicked, the
 * right eye after it joins. Stereo adds per-eye separation and
 * nothing else: the wide layout and the wide field stay exactly as
 * configured (Build 22's stereo draw bypassed every variant and
 * showed the bare centre — Run 15: engaging the slider "kills wide
 * and the graphics setting").
 *
 * The one variant stereo may not use is the native frame: it is a
 * single-viewpoint GPU render (the compositor's native handoff
 * serves the live frame only, and only once per publish), so neither
 * eye can be given a native frame of its own viewpoint; both eyes
 * take the wide sides assembly of the GPU-published frame instead.
 * The GPU renderer itself is untouched by stereo — every frame is
 * still GPU-rendered, with its texture cache and edge settings; only
 * the native-resolution upscale is flat-only. */
static void draw_top_eye(const u16 *centre, uint32_t options, const u32 *ink, bool stereo) {
    const u16 *sides = PbW2_TopSides();
    const u16 *native = stereo ? NULL : PbW2_NativeTop();
    const u16 *wideTop = PbW2_WideTop();
    if (wideTop && (options & DISPLAY_WIDE_FIELD)) {
        draw_scaled(GFX_TOP, wideTop, 320, options, ink);
    } else if (native && (options & DISPLAY_WIDE_FIELD)) {
        Present_BlitRGB565(GFX_TOP, native, PBW2_NATIVE_TOP_WIDTH, PBW2_NATIVE_TOP_HEIGHT);
        if (ink) draw_native_text(GFX_TOP, centre, GAME_WIDTH, ink, true);
    } else if (sides && (options & DISPLAY_WIDE_FIELD)) {
        static u16 wide[320 * GAME_HEIGHT];
        for (unsigned y = 0; y < GAME_HEIGHT; ++y) {
            memcpy(wide + y * 320, sides + y * 64, 32 * sizeof(u16));
            memcpy(wide + y * 320 + 32, centre + y * GAME_WIDTH, GAME_WIDTH * sizeof(u16));
            memcpy(wide + y * 320 + 288, sides + y * 64 + 32, 32 * sizeof(u16));
        }
        draw_scaled(GFX_TOP, wide, 320, options, ink);
    } else {
        Present_DrawGameWithText(GFX_TOP, centre, options, ink);
    }
}
#endif

void GxShim_Present(uint32_t flags) {
#if defined(PBW2_LINK_GAME)
    static u16 s_top[256 * 192];
    static u16 s_bottom[256 * 192];
    static u32 s_topInk[256 * 192], s_bottomInk[256 * 192];
    static u16 s_topRight[256 * 192];
    static u32 s_topInkRight[256 * 192];
    if (PbW2_VramHostReady()) {
        PbW2_GxDisplayTrace();
        /* Readiness gates the first game frame. Later frames can intentionally blank the LCDs. */
        if (PbW2_GameHasPresented() || PbW2_GameDisplayReady()) {
            u16 *top = (flags & PBW2_SCREEN_TOP) ? s_top : NULL;
            u16 *bottom = (flags & PBW2_SCREEN_BOTTOM) && !Settings_Open() ? s_bottom : NULL;
            uint32_t options = Settings_DisplayFlags();
            bool sharpText = (options & DISPLAY_SHARP_TEXT) != 0;
            u32 *topInk = top && sharpText && (options & (DISPLAY_TOP_FIT | DISPLAY_LCD_FIT | DISPLAY_WIDE_FIELD))
                             ? s_topInk : NULL;
            u32 *bottomInk = bottom && sharpText && (options & DISPLAY_LCD_FIT) ? s_bottomInk : NULL;
            /* Stereo: the 3D slider gates everything. While it is up,
             * the top screen is composed and drawn per eye — the 3D
             * layer carries the disparity (ds2d_compose / g3_raster),
             * 2D layers stay at screen depth. While it is down, the
             * flat path runs exactly as before. Per-eye disparity
             * tops out at 6 DS pixels at full slider, SoulSilver's
             * scale. */
            float slider = top ? osGet3DSliderState() : 0.0f;
            bool wantStereo = slider > 0.0f;
            if (wantStereo != s_stereoOn) {
                gfxSet3D(wantStereo);
                s_stereoOn = wantStereo;
                PbW2_G3RasterSetDepthPublish(wantStereo);
                /* Arm the GX recorder with the same gate: while the
                 * slider is up it retains each frame's command stream
                 * so the rasterizer can replay it into a true
                 * right-eye geometry frame (g3_replay.c). Slider down
                 * disarms it and the flat path is exactly as before. */
                PbW2_G3ReplaySetArmed(wantStereo);
                Log_Line("GX", "stereo %s", wantStereo ? "on" : "off");
            }
            int eyeShift = s_stereoOn ? (int)(6.0f * slider + 0.5f) : 0;
            if (s_stereoOn) PbW2_G3ReplaySetShiftPx(2 * eyeShift);
            if (s_stereoOn) PbW2_ComposeSetEyeShift(-eyeShift);
            /* Compose in phases (Build 19): Begin samples the frame's
             * LCD routing; the sub engine then composes on the worker
             * while the main engine composes here, and FinishWide
             * applies the wide layouts after the join — the same
             * order the monolithic compose ran, with the two engine
             * composes overlapped. If no worker is available the sub
             * engine composes inline after the main engine, as
             * before. */
            PbW2_ComposeBegin(topInk, bottomInk);
            bool subKicked = false;
            if (PbW2_ComposeMainUpper() ? bottom : top)
                subKicked = ComposeJob_Kick(CJ_SUB, top, bottom, topInk, bottomInk);
            PbW2_ComposeMainScreen(top, bottom, topInk, bottomInk);
            if (subKicked) ComposeJob_Join();
            else PbW2_ComposeSubScreen(top, bottom, topInk, bottomInk);
            PbW2_ComposeFinishWide(top, bottom);
            if (!has_text_ink(topInk)) topInk = NULL;
            if (!has_text_ink(bottomInk)) bottomInk = NULL;
            if (top) {
                /* Fingerprint the composed top picture for the FPS
                 * stats: a present whose picture matches the previous
                 * present's is a re-present, not a new frame. */
                uint64_t hash = top_frame_hash(top, topInk);
                s_topFresh = !s_topHashValid || hash != s_topHash;
                s_topHash = hash;
                s_topHashValid = true;
            }
            if (top) {
                /* Through the configured presentation chain (wide
                 * layout / native / wide sides / centre), for the
                 * flat draw and the stereo left eye alike: this eye
                 * pass's variants are the ones the accessors describe
                 * until the right compose below replaces them. */
                draw_top_eye(top, options, topInk, s_stereoOn);
            }
            /* Stereo right eye: kicked to the compose worker BEFORE
             * the bottom draw so the compose overlaps it, and joined
             * after. The wide-bottom pointer is fetched first: the
             * worker's compose begins a new frame (resetting the
             * have-flags), and the bottom draw must use this frame's
             * result. The right compose writes only the right-eye
             * buffers and its own wide-top output, neither of which
             * the bottom draw reads; the top draw above has already
             * consumed this frame's sides/native/wide-top outputs. */
            const u16 *wideBottom = PbW2_WideBottom();
            u32 *rightInk = NULL;
            bool rightKicked = false;
            if (s_stereoOn && top) {
                rightInk = sharpText && (options & (DISPLAY_TOP_FIT | DISPLAY_LCD_FIT | DISPLAY_WIDE_FIELD))
                               ? s_topInkRight : NULL;
                PbW2_ComposeSetEyeShift(eyeShift);
                rightKicked = ComposeJob_Kick(CJ_FULL, s_topRight, NULL, rightInk, NULL);
                if (!rightKicked)
                    PbW2_ComposeScreensWithText(s_topRight, NULL, rightInk, NULL);
            }
            {
                if (bottom && wideBottom && (options & DISPLAY_WIDE_FIELD)) {
                    Present_DrawBottomCached(NULL, 0, NULL);
                    Present_BlitRGB565(GFX_BOTTOM, wideBottom, 320, GAME_HEIGHT);
                } else {
                    Present_DrawBottomCached(bottom, options, bottomInk);
                }
            }
            if (s_stereoOn && top) {
                /* The bottom screen has no second eye and was already
                 * drawn from the left pass; draw the right eye the
                 * worker composed (or the inline compose above),
                 * through the same presentation chain — the accessors
                 * now describe the right pass's variants. */
                if (rightKicked) ComposeJob_Join();
                if (!has_text_ink(rightInk)) rightInk = NULL;
                s_drawSide = GFX_RIGHT;
                draw_top_eye(s_topRight, options, rightInk, true);
                s_drawSide = GFX_LEFT;
            }
            if (s_stereoOn) PbW2_ComposeSetEyeShift(0);
            pbw2_game_notify_present();
        }
    }
#endif
    if (s_logged) {
        return;
    }
    s_logged = true;
    Log_Line("GX", "present flags top=%d bottom=%d",
             (flags & PBW2_SCREEN_TOP) != 0, (flags & PBW2_SCREEN_BOTTOM) != 0);
}

/* libctru names the GSP buffer width 240 and height 400 (top). On the card
 * that is a column-major landscape picture: landscape sx maps to column, sy to
 * row within the column flipped like consoleDrawChar (sx*stride+(stride-1-sy)). */
static u16 *landscape_fb(gfxScreen_t screen, u16 *stride, u16 *land_w) {
    u16 land_h = 0;
    u16 *fb = (u16 *)gfxGetFramebuffer(screen, screen == GFX_TOP ? s_drawSide : GFX_LEFT, stride, &land_h);
    if (!fb || *stride == 0 || land_h == 0) {
        return NULL;
    }
    *land_w = land_h;
    return fb;
}

static void clear_side_margins(u16 *fb, unsigned stride, unsigned width, unsigned left, unsigned right) {
    if (left) memset(fb, 0, (size_t)stride * left * sizeof(*fb));
    if (right < width) memset(fb + right * stride, 0, (size_t)stride * (width - right) * sizeof(*fb));
}

#if defined(PBW2_LINK_GAME)
bool Present_TouchToGame(uint16_t px, uint16_t py, uint16_t *x, uint16_t *y) {
    uint32_t options = Settings_DisplayFlags();
    /* The 320x192 wide bottom image centres its 256x192 touch area
     * at native size. Ordinary gameplay follows the LCD fit setting. */
    if ((options & DISPLAY_WIDE_FIELD) && PbW2_WideBottom()) options &= ~DISPLAY_LCD_FIT;
    GameRect rect = game_rect(GFX_BOTTOM, 320, 240, options);
    if (px < rect.x || px >= rect.x + rect.width || py < rect.y || py >= rect.y + rect.height) return false;
    *x = (2 * (px - rect.x) + 1) * GAME_WIDTH / (2 * rect.width);
    *y = (2 * (py - rect.y) + 1) * GAME_HEIGHT / (2 * rect.height);
    return true;
}

static u16 mix565(u16 a, u16 b, unsigned weight) {
    if (!weight) return a;
    /* 5-bit fixed point for each packed color lane. */
    unsigned other = 32 - weight;
    u32 rb = ((a & 0xf81f) * other + (b & 0xf81f) * weight) >> 5;
    u32 g = ((a & 0x07e0) * other + (b & 0x07e0) * weight) >> 5;
    return (rb & 0xf81f) | (g & 0x07e0);
}

static unsigned sample_axis(unsigned pixel, unsigned output, unsigned input, bool sharp) {
    int fixed = ((2 * pixel + 1) * input * 16) / output - 16;
    /* Linear filtering of a virtual nearest-neighbor 2x image. Only the
     * middle half of each source-pixel interval blends into its neighbor. */
    if (sharp && fixed >= 0) {
        int fraction = (fixed & 31) * 2 - 16;
        if (fraction < 0) fraction = 0;
        if (fraction > 32) fraction = 32;
        fixed = (fixed & ~31) + fraction;
    }
    if (fixed < 0) return 0;
    if (fixed > (int)(input - 1) * 32) return (input - 1) * 32;
    return fixed;
}

static void draw_scaled(gfxScreen_t screen, const u16 *src, unsigned sourceWidth, uint32_t options, const u32 *ink) {
    u16 stride, width;
    u16 *fb = landscape_fb(screen, &stride, &width);
    if (!fb || stride != 240 || width < 320) return;

    GameRect rect = game_rect(screen, width, stride, options);
    if (sourceWidth == 320) rect = (GameRect){0, 0, width, stride};
    /* Scaled modes fill the LCD vertically. Only the side margins need clearing. */
    clear_side_margins(fb, stride, width, rect.x, rect.x + rect.width);
    const u16 *glyphColors = src;
    if (ink) {
        static u16 background[320 * GAME_HEIGHT];
        unsigned left = (sourceWidth - GAME_WIDTH) / 2;
        memcpy(background, src, sourceWidth * GAME_HEIGHT * sizeof(*src));
        for (unsigned y = 0; y < GAME_HEIGHT; ++y) {
            for (unsigned x = 0; x < GAME_WIDTH; ++x) {
                u32 pixel = ink[y * GAME_WIDTH + x];
                if (pixel & PBW2_TEXT_NATIVE) background[y * sourceWidth + left + x] = (u16)pixel;
            }
        }
        src = background;
    }
    bool sharp = (options & DISPLAY_SHARP_PIXELS) != 0;
    /* Both screens reuse the same sampling coordinates until their layout
     * or filter changes. Keep separate tables so switching screens is free. */
    static struct ScaleMap {
        unsigned width, height, sourceWidth;
        bool sharp;
        u16 sampleX[400], sampleY[240], nearestX[400];
        unsigned nearestRow[240];
    } maps[2];
    struct ScaleMap *map = &maps[screen == GFX_TOP];
    if (rect.width > 400 || rect.height > 240) return;
    if (map->width != rect.width || map->height != rect.height ||
        map->sourceWidth != sourceWidth || map->sharp != sharp) {
        map->width = rect.width;
        map->height = rect.height;
        map->sourceWidth = sourceWidth;
        map->sharp = sharp;
        for (unsigned y = 0; y < rect.height; ++y) {
            map->sampleY[y] = sample_axis(y, rect.height, GAME_HEIGHT, sharp);
            map->nearestRow[y] = ((2 * y + 1) * GAME_HEIGHT / (2 * rect.height)) * sourceWidth;
        }
        for (unsigned x = 0; x < rect.width; ++x) {
            map->sampleX[x] = sample_axis(x, rect.width, sourceWidth, sharp);
            map->nearestX[x] = (2 * x + 1) * sourceWidth / (2 * rect.width);
        }
    }
    unsigned offset = (sourceWidth - GAME_WIDTH) / 2;
    for (unsigned x = 0; x < rect.width; ++x) {
        unsigned sx = map->sampleX[x];
        u16 *out = fb + (rect.x + x) * stride + stride - 1 - rect.y;
        unsigned xi = sx >> 5;
        unsigned xn = xi + (xi < sourceWidth - 1);
        bool canSampleInk = ink && xi >= offset && xn < offset + GAME_WIDTH;
        unsigned nearX = map->nearestX[x];
        for (unsigned y = 0; y < rect.height; ++y) {
            unsigned sy = map->sampleY[y];
            unsigned yi = sy >> 5;
            unsigned yn = yi + (yi < GAME_HEIGHT - 1);
            if (canSampleInk &&
                (ink[yi * GAME_WIDTH + xi - offset] == PBW2_TEXT_INK || ink[yi * GAME_WIDTH + xn - offset] == PBW2_TEXT_INK ||
                 ink[yn * GAME_WIDTH + xi - offset] == PBW2_TEXT_INK || ink[yn * GAME_WIDTH + xn - offset] == PBW2_TEXT_INK)) {
                out[-(int)y] = src[map->nearestRow[y] + nearX];
                continue;
            }
            u16 a = mix565(src[yi * sourceWidth + xi], src[yi * sourceWidth + xn], sx & 31);
            u16 b = mix565(src[yn * sourceWidth + xi], src[yn * sourceWidth + xn], sx & 31);
            out[-(int)y] = mix565(a, b, sy & 31);
        }
    }
    if (ink) draw_native_text(screen, glyphColors, sourceWidth, ink, false);
}

static void draw_native_text(gfxScreen_t screen, const u16 *src, unsigned sourceWidth, const u32 *ink,
                             bool restoreLCD) {
    u16 stride, width;
    u16 *fb = landscape_fb(screen, &stride, &width);
    if (!fb || stride != 240 || width < 320) return;
    unsigned left = (width - 320) / 2;
    unsigned offset = (sourceWidth - GAME_WIDTH) / 2;
    if (restoreLCD) {
        /* Restore the background at each LCD sample copied from a glyph pixel. */
        for (unsigned y = 0; y < GAME_HEIGHT; ++y) {
            for (unsigned x = 0; x < GAME_WIDTH; ++x) {
                u32 pixel = ink[y * GAME_WIDTH + x];
                if (!(pixel & PBW2_TEXT_NATIVE)) continue;
                for (unsigned ny = (5 * y + 1) / 4; ny < (5 * y + 6) / 4; ++ny) {
                    for (unsigned nx = (5 * x + 1) / 4; nx < (5 * x + 6) / 4; ++nx)
                        fb[(left + nx) * stride + stride - 1 - ny] = (u16)pixel;
                }
            }
        }
    }
    for (unsigned y = 0; y < GAME_HEIGHT; ++y) {
        for (unsigned x = 0; x < GAME_WIDTH; ++x) {
            u32 pixel = ink[y * GAME_WIDTH + x];
            if (!(pixel & PBW2_TEXT_NATIVE)) continue;
            unsigned nx = PbW2_TextNativeCoordinate(x, (pixel >> 16) & 31);
            unsigned ny = PbW2_TextNativeCoordinate(y, (pixel >> 21) & 31);
            fb[(left + nx) * stride + stride - 1 - ny] = src[y * sourceWidth + offset + x];
        }
    }
}

void Present_DrawGameWithText(gfxScreen_t screen, const u16 *src, uint32_t options, const u32 *ink) {
    if (!(options & (DISPLAY_LCD_FIT | DISPLAY_TOP_FIT | DISPLAY_WIDE_FIELD)) ||
        (screen == GFX_BOTTOM && !(options & DISPLAY_LCD_FIT))) {
        Present_BlitRGB565(screen, src, GAME_WIDTH, GAME_HEIGHT);
        return;
    }
    draw_scaled(screen, src, GAME_WIDTH, options, ink);
}

void Present_DrawGame(gfxScreen_t screen, const u16 *src, uint32_t options) {
    Present_DrawGameWithText(screen, src, options, NULL);
}

/* Keep track of which of libctru's two LCD buffers contains the latest bottom image. */
static struct {
    u16 image[GAME_WIDTH * GAME_HEIGHT];
    u32 ink[GAME_WIDTH * GAME_HEIGHT];
    u16 *buffers[2];
    uint32_t options;
    bool hasInk, reused;
} s_bottomCache;

void Present_DrawBottomCached(const u16 *src, uint32_t options, const u32 *ink) {
    s_bottomCache.reused = false;
    u16 stride, width;
    u16 *fb = src ? landscape_fb(GFX_BOTTOM, &stride, &width) : NULL;
    if (!fb || stride != 240 || width != 320) {
        s_bottomCache.buffers[0] = s_bottomCache.buffers[1] = NULL;
        if (src) Present_DrawGameWithText(GFX_BOTTOM, src, options, ink);
        return;
    }
    options &= DISPLAY_LCD_FIT | DISPLAY_SHARP_PIXELS;
    bool same = s_bottomCache.buffers[0] && options == s_bottomCache.options &&
                (ink != NULL) == s_bottomCache.hasInk &&
                memcmp(src, s_bottomCache.image, sizeof s_bottomCache.image) == 0 &&
                (!ink || memcmp(ink, s_bottomCache.ink, sizeof s_bottomCache.ink) == 0);
    if (same && (fb == s_bottomCache.buffers[0] || fb == s_bottomCache.buffers[1])) {
        s_bottomCache.reused = true;
        return;
    }
    if (same) {
        s_bottomCache.buffers[1] = fb;
    } else {
        memcpy(s_bottomCache.image, src, sizeof s_bottomCache.image);
        if (ink) memcpy(s_bottomCache.ink, ink, sizeof s_bottomCache.ink);
        s_bottomCache.hasInk = ink != NULL;
        s_bottomCache.options = options;
        s_bottomCache.buffers[0] = fb;
        s_bottomCache.buffers[1] = NULL;
    }
    Present_DrawGameWithText(GFX_BOTTOM, src, options, ink);
}

/* Present-rate window: fed once per main-loop present, closes about
 * every 500 ms so the FPS overlay refreshes ~2x/s, and emits the PERF
 * line about every 8th window in its long-standing format.
 *
 * The FPS figure counts DISTINCT presented frames: a present counts
 * only when GxShim_Present's fingerprint shows the composed top
 * picture changed since the previous present. The pre-Build-18 window
 * counted every host present in a 120-frame window, so when the game
 * produced ~20 new frames/s and the host loop re-presented each one
 * about twice, the counter (and the PERF line) read ~40 - the host
 * loop's present rate, not the rate of new frames reaching the
 * display. (It was never a both-screens sum: one loop iteration
 * commits both screens, and this was called once per iteration.)
 *
 * The frame-work figures (average / worst) deliberately cover ALL
 * presents in the window, re-presents included: they price the host's
 * per-present work, which is what presentation cost cutting moves. */
static PresentStats s_stats;
static bool s_statsValid;

void Present_NoteFrame(uint64_t start, uint64_t flushStart, uint64_t finish) {
    static uint64_t windowStart;
    static uint64_t drawTicks, flushTicks, workTicks, worstTicks;
    static unsigned presents, freshFrames, windows;
    if (!presents) windowStart = start;
    uint64_t work = finish - start;
    drawTicks += flushStart - start;
    flushTicks += finish - flushStart;
    workTicks += work;
    if (work > worstTicks) worstTicks = work;
#if defined(PBW2_LINK_GAME)
    /* The fingerprint only exists in the game build; the shell build
     * never calls this function. */
    if (s_topFresh) {
        s_topFresh = false;
        freshFrames++;
    }
#endif
    presents++;
    uint64_t elapsed = finish - windowStart;
    if (elapsed < (uint64_t)SYSCLOCK_ARM11 / 2) return;
    PresentStats next;
    next.fps = (unsigned)(freshFrames * (uint64_t)SYSCLOCK_ARM11 / elapsed);
    next.avgFrameMs10 = (unsigned)(workTicks * 10000ull / ((uint64_t)presents * SYSCLOCK_ARM11));
    next.worstFrameMs10 = (unsigned)(worstTicks * 10000ull / (uint64_t)SYSCLOCK_ARM11);
    s_stats = next;
    s_statsValid = true;
    if ((++windows & 7) == 0) {
        unsigned drawUs = drawTicks * 1000000ull / ((uint64_t)presents * SYSCLOCK_ARM11);
        unsigned flushUs = flushTicks * 1000000ull / ((uint64_t)presents * SYSCLOCK_ARM11);
        Log_Line("PERF", "LCD %u fps, update and compose avg %u us, flush and swap avg %u us", next.fps, drawUs, flushUs);
    }
    presents = freshFrames = 0;
    drawTicks = flushTicks = workTicks = worstTicks = 0;
}

unsigned Present_Fps(void) {
    return s_stats.fps;
}

bool Present_ReadStats(PresentStats *out) {
    if (!s_statsValid) return false;
    *out = s_stats;
    return true;
}

void Present_Flush(void) {
    if (s_stereoOn) {
        /* The retained-bottom shortcut below flushes only the
         * top-left buffer by hand; in stereo both eyes need the
         * full flush. */
        gfxFlushBuffers();
        return;
    }
    if (s_bottomCache.reused) {
        /* The game uses a single RGB565 image at the top. The retained bottom
         * buffer was already flushed when it was last written. */
        u16 stride, width;
        u16 *top = landscape_fb(GFX_TOP, &stride, &width);
        if (top && stride == 240 && width == 400 &&
            R_SUCCEEDED(GSPGPU_FlushDataCache(top, stride * width * sizeof(*top)))) return;
    }
    gfxFlushBuffers();
}

static void clear_screen(gfxScreen_t screen) {
    u16 stride;
    u16 width;
    u16 *fb = landscape_fb(screen, &stride, &width);
    if (fb != NULL) {
        memset(fb, 0, (size_t)stride * width * sizeof(*fb));
    }
    if (screen == GFX_TOP && s_stereoOn) {
        s_drawSide = GFX_RIGHT;
        fb = landscape_fb(screen, &stride, &width);
        if (fb != NULL) {
            memset(fb, 0, (size_t)stride * width * sizeof(*fb));
        }
        s_drawSide = GFX_LEFT;
    }
}

void Present_Clear(uint32_t flags) {
    if (flags & PBW2_SCREEN_TOP) {
        clear_screen(GFX_TOP);
    }
    if (flags & PBW2_SCREEN_BOTTOM) {
        Present_DrawBottomCached(NULL, 0, NULL);
        clear_screen(GFX_BOTTOM);
    }
}
#endif

void Present_BlitRGB565(gfxScreen_t screen, const uint16_t *pic, int pic_w, int pic_h) {
    u16 stride = 0;
    u16 land_w = 0;
    u16 *fb;
    int ox;
    int oy;
    int y;
    int x;

    if (!pic || pic_w <= 0 || pic_h <= 0) {
        return;
    }
    fb = landscape_fb(screen, &stride, &land_w);
    if (!fb) {
        return;
    }
    /* Landscape height is 240; landscape width is 400 (top) or 320 (bottom). */
    if (stride != GSP_SCREEN_WIDTH || pic_w > land_w || pic_h > stride) {
        return;
    }
    ox = ((int)land_w - pic_w) / 2;
    oy = ((int)stride - pic_h) / 2;
    clear_side_margins(fb, stride, land_w, ox, ox + pic_w);
    unsigned bottom = stride - oy - pic_h;

    for (x = 0; x < pic_w; ++x) {
        u16 *column = fb + (ox + x) * stride;
        if (bottom) memset(column, 0, bottom * sizeof(*column));
        if (oy) memset(column + stride - oy, 0, oy * sizeof(*column));
    }
    /* Work on a short band so neighbouring columns reuse the same source
     * cache lines during the row-major to column-major transpose. */
    for (int row = 0; row < pic_h; row += 16) {
        int end = row + 16 < pic_h ? row + 16 : pic_h;
        for (x = 0; x < pic_w; ++x) {
            u16 *out = fb + (ox + x) * stride + stride - 1 - oy;
            for (y = row; y < end; ++y) out[-y] = pic[y * pic_w + x];
        }
    }
}

void Present_DrawTitle(uint32_t flags, const uint16_t *top, const uint16_t *bottom) {
    if ((flags & PBW2_SCREEN_TOP) != 0 && top != NULL) {
        Present_BlitRGB565(GFX_TOP, top, 256, 192);
    }
    if ((flags & PBW2_SCREEN_BOTTOM) != 0 && bottom != NULL) {
        Present_BlitRGB565(GFX_BOTTOM, bottom, 256, 192);
    }
}
