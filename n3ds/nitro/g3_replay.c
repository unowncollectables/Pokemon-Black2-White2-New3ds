/* SPDX-License-Identifier: GPL-3.0-or-later */
/* GX record/replay layer.
 *
 * WHY THIS EXISTS: the present layer only ever sees finished pictures
 * (g3_raster publishes a completed frame at swap time), so neither
 * true per-eye geometry stereo nor presentation interpolation can be
 * built on it. Both need one frame's GX command stream, retained so
 * it can be re-executed. This module is that retention, plus the one
 * consumer whose fidelity is provable today (per-eye geometry for
 * the stereo right eye, below). Frame interpolation is NOT enabled:
 * it would re-render at present time, on another thread, where the
 * quiescence replay relies on (below) does not hold.
 *
 * RECORD POINT: PbW2_G3Command, post-decoder. Every GX path funnels
 * through it — the FIFO packet decoder (PbW2_G3FifoWord, so packet
 * splitting/reassembly is already resolved and cannot diverge), the
 * display-list host (g3_gecom_host), direct register-style commands
 * (g3_gx_stub, the gx.h G3_WRITE_COMMAND inlines) and DMA-to-GXFIFO
 * (mi_host -> PbW2_G3RecordFifo). Recording (opcode, argc, args)
 * triples there captures exactly the stream the live pass executed.
 *
 * FIDELITY AUDIT — every command class the game can emit, by opcode:
 *   0x00 NOP (0 args)                    recorded; no-op on replay.
 *   0x10-0x1C matrix class (mode, push, pop, store, restore,
 *     identity, load/mult 4x4/4x3/3x3, scale, trans)
 *                                        recorded; replayed through
 *     the intercept's own matrix state, restored from the frame-start
 *     snapshot, so clip/texture/vector matrices evolve identically.
 *   0x20 COLOR, 0x21 NORMAL, 0x22 TEXCOORD, 0x23-0x28 VTX variants,
 *     0x29 POLYGON_ATTR, 0x2A TEXIMAGE_PARAM, 0x2B TEXPLTT_BASE,
 *     0x30 DIF_AMB, 0x31 SPE_EMI, 0x32 LIGHT_VECTOR, 0x33 LIGHT_COLOR,
 *     0x34 SHININESS (32 args), 0x40 BEGIN, 0x41 END, 0x60 VIEWPORT
 *                                        recorded; deterministic in
 *     the rasterizer given (a) the frame-start raster state
 *     (snapshotted in full — see PbW2_G3RasterStateSave), (b) the GX
 *     register snapshot start_frame takes (clear color/depth,
 *     DISP3DCNT, toon/edge/fog tables, alpha ref), and (c) texture
 *     texels in VRAM. (b) and (c) are NOT in the stream; replay is
 *     therefore only performed synchronously inside the frame's own
 *     swap processing, on the game thread, before any further game
 *     code runs — at that instant the registers and VRAM are exactly
 *     what the live pass just sampled (the CPU rasterizer's whole
 *     design already relies on this: it samples texels at submission
 *     time and PbW2_G3RasterBeforeLcdcWrite exists because uploads
 *     are synchronous calls, never asynchronous writes).
 *   0x50 SWAP_BUFFERS                    the frame delimiter; never
 *     recorded and never replayed (its publish side effects — display
 *     lock, generation bump, GPU reuse bookkeeping, rate logging —
 *     must not fire twice). The replay publish is a separate,
 *     side-effect-free copy of publish_swap's CPU tail.
 *   0x70 BOX_TEST, 0x71 POS_TEST, 0x72 VEC_TEST
 *                                        recorded but SUPPRESSED on
 *     replay (intercept replay mode): BOX_TEST writes GXSTAT bits
 *     the game reads back, and re-running it under a shifted
 *     projection could give a different answer than the live test
 *     the recorded geometry was submitted under. The tests do not
 *     affect rasterization (POS/VEC_TEST are already no-ops in the
 *     rasterizer), so suppressing them changes no pixel.
 *   any other opcode                     the decoder assigns it 0
 *     arguments and the rasterizer ignores it; it is recorded as
 *     executed (0 args) and replays as the same no-op.
 *
 * A frame is replayed ONLY when every condition holds; any failure
 * leaves the exact pre-existing path (depth-graded reprojection of
 * the published frame) in charge:
 *   - recording was armed (3D slider up) for the whole frame;
 *   - the stream fit the pool (overflow marks the frame invalid);
 *   - no zone invalidation and no deferred (post-VBlank) publish
 *     landed inside the frame (both mutate raster state outside the
 *     stream, so the frame-start snapshot would no longer describe
 *     the state the live pass actually started from);
 *   - the swap published synchronously (not afterVBlank), on the CPU
 *     rasterizer (GPU frames carry PICA texture-cache and
 *     previous-frame-reuse state that a second pass would corrupt),
 *     was not a reused frame, and was not a display-capture frame.
 *
 * STATE SNAPSHOTS: replay restores the frame-start intercept and
 * raster state, re-executes, publishes the right-eye frame, then
 * restores the frame-end state captured at the swap, so the live
 * pass continues bit-identically. The raster's drawing buffer is
 * deliberately not restored: after publish its contents are dead —
 * the next start_frame's clear fully rewrites colour, depth and the
 * id/mask planes, and the fog-alpha plane is rewritten whenever fog
 * is enabled and read only then.
 *
 * AUDIO: untouched by design. Replay runs inside the game's own
 * swap call; nothing here paces, waits on, or advances audio, which
 * continues to run on the DSP clock only.
 */
#include "g3_replay.h"

#if defined(PBW2_LINK_GAME)

#include "g3_intercept.h"
#include "g3_raster.h"
#include "log.h"

#include <string.h>

/* One recorded frame: a flat word pool of (header, args...) groups,
 * header = opcode | argc << 8. Vertex commands dominate (1-2 words
 * each); 256K words holds ~100K+ commands, far beyond a field frame.
 * Two pools retain the last completed frame beside the recording one
 * — the retained pair a future interpolation consumer needs. */
enum { POOL_WORDS = 262144, STATE_BYTES_INTERCEPT = 8192, STATE_BYTES_RASTER = 4096 };

static u32 s_pool[2][POOL_WORDS];
static u8 s_startIntercept[2][STATE_BYTES_INTERCEPT];
static u8 s_startRaster[2][STATE_BYTES_RASTER];
static u8 s_endIntercept[STATE_BYTES_INTERCEPT];
static u8 s_endRaster[STATE_BYTES_RASTER];

typedef struct {
    u32 words;
    u32 commands;
    bool valid;
    bool startValid;
} FrameRecord;

static FrameRecord s_frame[2];
static unsigned s_write;       /* pool being recorded */
static bool s_frameOpen;       /* recording into s_write */
static bool s_pendingEligible; /* the open frame's swap published replay-eligible */
static bool s_tainted;         /* the open frame saw an out-of-stream state change */
static bool s_disabled;        /* state-size check failed at init */
static bool s_loggedActive;

static unsigned char s_armed;  /* present thread writes, game thread reads */
static int s_shiftPx;          /* present thread writes, game thread reads */

void PbW2_G3ReplayInit(void) {
    if (PbW2_G3InterceptStateSize() > STATE_BYTES_INTERCEPT ||
        PbW2_G3RasterStateSize() > STATE_BYTES_RASTER) {
        s_disabled = true;
        Log_Line("GX", "record/replay disabled: state snapshot outgrew its buffer");
    }
}

void PbW2_G3ReplaySetArmed(bool armed) {
    __atomic_store_n(&s_armed, armed ? 1 : 0, __ATOMIC_RELAXED);
    if (!armed) {
        /* A frame retained from a previous stereo session must never
         * replay into a later one. (Game-thread state, written here
         * from the present thread only while stereo is off, i.e.
         * while no replay can be in flight; the game thread re-checks
         * s_armed at swap time before replaying.) */
        s_frame[0].valid = s_frame[1].valid = false;
        s_pendingEligible = false;
    }
}

void PbW2_G3ReplaySetShiftPx(int px) {
    __atomic_store_n(&s_shiftPx, px, __ATOMIC_RELAXED);
}

static bool replay_armed(void) {
    return __atomic_load_n(&s_armed, __ATOMIC_RELAXED) != 0;
}

bool PbW2_G3ReplayRecording(void) {
    return s_frameOpen && !s_disabled;
}

void PbW2_G3ReplayRecord(u32 opcode, const u32 *args, unsigned argc) {
    if (!s_frameOpen || s_disabled) return;
    FrameRecord *frame = &s_frame[s_write];
    if (!frame->valid) return;
    if (frame->words + 1 + argc > POOL_WORDS) {
        frame->valid = false;
        return;
    }
    u32 *pool = s_pool[s_write];
    pool[frame->words++] = opcode | (argc << 8);
    for (unsigned i = 0; i < argc; ++i) pool[frame->words++] = args[i];
    ++frame->commands;
}

void PbW2_G3ReplayNotePublish(bool eligible, bool afterVBlank) {
    if (afterVBlank) {
        /* A deferred publish re-snapshots registers into the raster
         * mid-frame; the open frame's start snapshot no longer
         * describes its live starting state. */
        s_tainted = true;
        return;
    }
    s_pendingEligible = eligible;
}

void PbW2_G3ReplayInvalidate(void) {
    s_tainted = true;
    s_frame[0].valid = s_frame[1].valid = false;
    s_pendingEligible = false;
}

/* Re-execute one recorded frame with the right-eye projection shift
 * and publish the result as the raster's right-eye frame. Runs on
 * the game thread inside the frame's own swap processing. */
static void replay_frame(unsigned index) {
    const FrameRecord *frame = &s_frame[index];
    const u32 *pool = s_pool[index];

    /* Preserve the live frame-end state; replay overwrites the
     * intercept and raster state wholesale. */
    PbW2_G3InterceptStateSave(s_endIntercept);
    PbW2_G3RasterStateSave(s_endRaster);
    PbW2_G3InterceptStateRestore(s_startIntercept[index]);
    PbW2_G3RasterStateRestore(s_startRaster[index]);

    float shift = (float)__atomic_load_n(&s_shiftPx, __ATOMIC_RELAXED) * (1.0f / 128.0f);
    PbW2_G3InterceptReplayBegin(shift);
    bool ok = true;
    u32 offset = 0;
    while (offset < frame->words) {
        u32 header = pool[offset++];
        unsigned argc = (header >> 8) & 0x3f;
        if (offset + argc > frame->words) {
            ok = false;
            break;
        }
        PbW2_G3Command(header & 0xff, pool + offset);
        offset += argc;
    }
    if (ok) PbW2_G3RasterPublishReplay();
    PbW2_G3InterceptReplayEnd();

    PbW2_G3InterceptStateRestore(s_endIntercept);
    PbW2_G3RasterStateRestore(s_endRaster);

    if (ok && !s_loggedActive) {
        s_loggedActive = true;
        Log_Line("GX", "record/replay active: right eye rendered from the recorded GX stream");
    }
}

void PbW2_G3ReplayFrameSwapped(void) {
    unsigned done = s_write;
    bool replay = s_pendingEligible && !s_tainted && s_frameOpen && !s_disabled &&
                  replay_armed() && s_frame[done].valid && s_frame[done].startValid &&
                  s_frame[done].commands > 0;
    if (replay) replay_frame(done);

    /* Open the next frame. Its start snapshot is the post-swap state,
     * which is exactly the state the next frame's live pass starts
     * from (publish_swap leaves the attribute state untouched). */
    s_pendingEligible = false;
    s_tainted = false;
    s_write ^= 1;
    FrameRecord *next = &s_frame[s_write];
    next->words = 0;
    next->commands = 0;
    next->valid = true;
    next->startValid = false;
    s_frameOpen = !s_disabled && replay_armed();
    if (s_frameOpen) {
        PbW2_G3InterceptStateSave(s_startIntercept[s_write]);
        PbW2_G3RasterStateSave(s_startRaster[s_write]);
        next->startValid = true;
    }
}

#endif /* PBW2_LINK_GAME */
