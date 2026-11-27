/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef PBW2_LOG_H
#define PBW2_LOG_H

#include <stdbool.h>

/* RAM-buffered logger. Log_Line/Log_Error never touch the SD card: lines
 * are formatted once into a 1 MB ring buffer in memory and reach log.txt
 * only at flush points (clean exit, an error line, an overlay-load safe
 * point when the ring is half full, or the ring approaching full). A
 * hardware test run therefore pays zero SD I/O and zero per-line stdio
 * cost during gameplay.
 *
 * Mode comes from logger.cfg in the game dir ("mode=quiet" or
 * "mode=verbose"). Default is quiet on hardware builds and verbose on
 * emulator test builds (PBW2_NO_SPEEDUP), so the tester's flow is
 * unchanged. Quiet keeps errors, boot/exit markers and the PERF summary
 * lines; everything else is counted and reported in one summary line at
 * close instead of being written. */

/* True when the SD log target is usable. Rotates the previous run's
 * log.txt (to log_crash.txt when that run failed or never closed, else
 * log_prev.txt) and appends this boot's line to log_health.txt. */
bool Log_Init(void);
/* Flush, write the close marker and the run's health record. */
void Log_Close(void);
/* Drain the ring buffer to log.txt now. Cold paths only. */
void Log_Flush(void);
/* Flush only when the ring is at least half full. Cheap to call at
 * loading moments (overlay loads) so a long session's log reaches the
 * card in bounded pieces without I/O during gameplay. */
void Log_SafePoint(void);
bool Log_Verbose(void);

void Log_Line(const char *tag, const char *fmt, ...);
/* An error line: always kept in both modes, marks the run failed (so
 * its log is preserved as log_crash.txt at the next boot even when the
 * run exits clean), and flushes the ring immediately so a following
 * hard crash cannot take the log with it. Cold paths only. */
void Log_Error(const char *tag, const char *fmt, ...);

#endif
