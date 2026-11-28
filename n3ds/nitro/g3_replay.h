/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include "types.h"

#include <stdbool.h>

/* GX record/replay layer (see g3_replay.c for the full design and the
 * fidelity audit). The recorder captures the post-decoder G3 command
 * stream (opcode + arguments, exactly as PbW2_G3Command receives them)
 * for one swap-delimited frame at a time, retaining the last completed
 * frame beside the one being recorded. When a frame's swap publishes
 * synchronously on the CPU rasterizer path, the layer can re-execute
 * the recorded stream with a shifted projection to produce a true
 * per-eye geometry frame for the stereo right eye.
 *
 * Everything here is driven from the game thread's command flow
 * (PbW2_G3Command / publish_swap), except the two setters, which the
 * present thread writes and the game thread reads atomically.
 * Recording is armed only while the 3D slider is up; while disarmed
 * the per-command cost is one predictable branch in the intercept. */

/* Size-check the intercept/raster state snapshots against this
 * module's fixed buffers; disables the layer permanently (with a log
 * line) if a state struct ever outgrows them. Called from
 * PbW2_G3InterceptInit. */
void PbW2_G3ReplayInit(void);

/* Present-thread gates. Armed follows the hardware 3D slider (the
 * existing stereo gate); shift is the right eye's total projection
 * shift in DS pixels (twice the per-eye disparity present composes
 * with, because the live frame serves as the left eye). */
void PbW2_G3ReplaySetArmed(bool armed);
void PbW2_G3ReplaySetShiftPx(int px);

/* Game-thread hooks. Record is called by the intercept for every
 * command while a frame is being recorded; FrameSwapped is called
 * just after a SWAP_BUFFERS command has been fully processed;
 * NotePublish is called at the tail of every publish_swap with
 * whether that publish is replay-eligible (synchronous, CPU-rendered,
 * not reused, not a display capture) and whether it was a deferred
 * (post-VBlank) publish, which taints the frame being recorded.
 * Invalidate drops all retained state (zone change / renderer reset):
 * a frame spanning one is never replayed. */
bool PbW2_G3ReplayRecording(void);
void PbW2_G3ReplayRecord(u32 opcode, const u32 *args, unsigned argc);
void PbW2_G3ReplayFrameSwapped(void);
void PbW2_G3ReplayNotePublish(bool eligible, bool afterVBlank);
void PbW2_G3ReplayInvalidate(void);
