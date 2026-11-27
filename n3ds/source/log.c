/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "log.h"

#include "pbw2_port.h"

#include <3ds.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* The old logger formatted every line twice (vsnprintf, then fprintf
 * re-formatting), mirrored it to printf, and fflush'd the SD file on
 * EVERY line. Log call sites sit on the game thread, the audio thread
 * and the present path, so a hardware run paid an SD write + flush per
 * line — the 49,887-line hardware log in docs/HANDOFF.md is 49,887 SD
 * flushes, most of them mid-gameplay. This logger keeps the exact line
 * format and call sites but changes the transport:
 *
 * - one vsnprintf into a 1 MB ring buffer in RAM (LightLock-guarded,
 *   records are u16 length + payload, never split across the wrap);
 * - no printf except in verbose mode (the emulator flow reads stdout);
 * - no SD I/O at all during gameplay. The ring reaches log.txt only at
 *   flush points: Log_Close, Log_Error (which also marks the run
 *   failed), Log_SafePoint at >= 50% full (called from overlay loads —
 *   a moment the game is already reading the card), and a forced flush
 *   at >= 7/8 full so a verbose firehose loses a bounded tail instead
 *   of wrapping silently. Overflow past that drops the oldest records
 *   and counts them; the count is written into the flushed log.
 *
 * Failure preservation: Log_Error rewrites log_failed.flag with the
 * run's error count. At boot, the previous log.txt is classified —
 * failed (flag present), crashed (no "[LOG] close" tail marker), or
 * clean — and renamed to log_crash.txt (failed/crashed) or
 * log_prev.txt (clean) BEFORE anything can overwrite it. log_health.txt
 * is append-only: one boot line and one exit line per run. */

#define LOG_RING_BYTES (1024u * 1024u)
#define LOG_LINE_MAX 512u

static u8 s_ring[LOG_RING_BYTES];
static LightLock s_lock;
static bool s_ready;
static bool s_verbose =
#if defined(PBW2_NO_SPEEDUP)
    true; /* emulator test builds: tester flow unchanged */
#else
    false; /* hardware builds default to quiet */
#endif
static size_t s_head;      /* write position */
static size_t s_tail;      /* oldest record position */
static size_t s_used;      /* bytes held, including wrap padding */
static size_t s_wrap_dead; /* dead bytes at the ring end before the wrap */
static unsigned s_kept;
static unsigned s_suppressed;
static unsigned s_dropped;
static unsigned s_errors;

static void path_of(char *buf, size_t size, const char *name) {
    snprintf(buf, size, "%s/%s", PBW2_SD_DIR, name);
}

/* ---- ring primitives (lock held) ---- */

/* Advance the tail past the oldest record. Returns false when empty. */
static bool ring_drop_oldest(void) {
    if (!s_used) {
        return false;
    }
    if (s_wrap_dead && s_tail == LOG_RING_BYTES - s_wrap_dead) {
        s_used -= s_wrap_dead;
        s_tail = 0;
        s_wrap_dead = 0;
        if (!s_used) {
            return false;
        }
    }
    u16 len;
    memcpy(&len, s_ring + s_tail, sizeof len);
    size_t rec = sizeof len + len;
    s_tail += rec;
    if (s_tail >= LOG_RING_BYTES) {
        s_tail = 0;
    }
    s_used -= rec;
    ++s_dropped;
    return true;
}

static void ring_store(const char *line, size_t len) {
    size_t need = sizeof(u16) + len;
    for (;;) {
        size_t tailspace = LOG_RING_BYTES - s_head;
        size_t dead = tailspace < need ? tailspace : 0;
        if (s_used + dead + need <= LOG_RING_BYTES) {
            if (dead) {
                s_wrap_dead = dead;
                s_used += dead;
                s_head = 0;
            }
            break;
        }
        if (!ring_drop_oldest()) {
            return; /* record larger than the ring; cannot happen (<= 514) */
        }
    }
    u16 stored = (u16)len;
    memcpy(s_ring + s_head, &stored, sizeof stored);
    memcpy(s_ring + s_head + sizeof stored, line, len);
    s_head += sizeof stored + len;
    if (s_head >= LOG_RING_BYTES) {
        s_head = 0;
    }
    s_used += sizeof stored + len;
    ++s_kept;
}

/* ---- file side (cold paths only) ---- */

void Log_Flush(void) {
    if (!s_ready) {
        return;
    }
    LightLock_Lock(&s_lock);
    if (!s_used) {
        LightLock_Unlock(&s_lock);
        return;
    }
    char path[128];
    path_of(path, sizeof path, "log.txt");
    FILE *fp = fopen(path, "a");
    if (!fp) {
        LightLock_Unlock(&s_lock);
        return; /* keep the ring; a later flush can retry */
    }
    if (s_dropped) {
        fprintf(fp, "[LOG] %u earlier line(s) dropped (ring overflow)\n", s_dropped);
    }
    size_t tail = s_tail;
    size_t left = s_used;
    while (left) {
        if (s_wrap_dead && tail == LOG_RING_BYTES - s_wrap_dead) {
            left -= s_wrap_dead;
            tail = 0;
            if (!left) {
                break;
            }
        }
        u16 len;
        memcpy(&len, s_ring + tail, sizeof len);
        fwrite(s_ring + tail + sizeof len, 1, len, fp);
        fputc('\n', fp);
        tail += sizeof len + len;
        if (tail >= LOG_RING_BYTES) {
            tail = 0;
        }
        left -= sizeof len + len;
    }
    fclose(fp);
    s_head = s_tail = s_used = s_wrap_dead = 0;
    s_dropped = 0;
    LightLock_Unlock(&s_lock);
}

void Log_SafePoint(void) {
    if (s_ready && s_used * 2 >= LOG_RING_BYTES) {
        Log_Flush();
    }
}

bool Log_Verbose(void) {
    return s_verbose;
}

/* ---- line intake ---- */

enum LogLevel { LOG_INFO, LOG_ERROR };

static void log_store_line(enum LogLevel level, const char *tag, const char *fmt, va_list ap) {
    char line[LOG_LINE_MAX];
    int n = snprintf(line, sizeof line, "[%s] ", tag);
    if (n < 0) {
        n = 0;
    }
    if ((size_t)n >= sizeof line) {
        n = (int)sizeof line - 1;
    }
    vsnprintf(line + n, sizeof line - (size_t)n, fmt, ap);
    size_t len = strlen(line);

    if (s_verbose || level == LOG_ERROR || !strcmp(tag, "3DS")) {
        printf("%s\n", line);
    }
    if (!s_ready) {
        return;
    }
    LightLock_Lock(&s_lock);
    ring_store(line, len);
    bool near_full = s_used * 8 >= LOG_RING_BYTES * 7;
    LightLock_Unlock(&s_lock);
    if (near_full) {
        Log_Flush();
    }
}

static bool keep_line(enum LogLevel level, const char *tag) {
    if (s_verbose || level == LOG_ERROR) {
        return true;
    }
    /* Quiet mode: errors, boot/exit markers and the PERF summaries. */
    return !strcmp(tag, "PERF") || !strcmp(tag, "3DS");
}

void Log_Line(const char *tag, const char *fmt, ...) {
    if (!keep_line(LOG_INFO, tag)) {
        if (s_ready) {
            ++s_suppressed;
        }
        return;
    }
    va_list ap;
    va_start(ap, fmt);
    log_store_line(LOG_INFO, tag, fmt, ap);
    va_end(ap);
}

void Log_Error(const char *tag, const char *fmt, ...) {
    ++s_errors;
    va_list ap;
    va_start(ap, fmt);
    log_store_line(LOG_ERROR, tag, fmt, ap);
    va_end(ap);
    if (!s_ready) {
        return;
    }
    /* Mark the run failed on the card, then drain the ring so a hard
     * crash after this point cannot take the log with it. */
    char path[128];
    path_of(path, sizeof path, "log_failed.flag");
    FILE *fp = fopen(path, "w");
    if (fp) {
        fprintf(fp, "%u\n", s_errors);
        fclose(fp);
    }
    Log_Flush();
}

/* ---- boot / shutdown ---- */

static void read_mode_config(void) {
    char path[128];
    path_of(path, sizeof path, "logger.cfg");
    FILE *fp = fopen(path, "r");
    if (!fp) {
        return;
    }
    char buf[256];
    size_t got = fread(buf, 1, sizeof buf - 1, fp);
    fclose(fp);
    buf[got] = '\0';
    char *mode = strstr(buf, "mode");
    if (!mode) {
        return;
    }
    if (strstr(mode, "verbose")) {
        s_verbose = true;
    } else if (strstr(mode, "quiet")) {
        s_verbose = false;
    }
}

/* Classify and rotate the previous run's log.txt. Returns the fate
 * string for the health record; *prev_errors gets its error count. */
static const char *rotate_previous(unsigned *prev_errors) {
    *prev_errors = 0;
    char log_path[128], dest_path[128], flag_path[128];
    path_of(log_path, sizeof log_path, "log.txt");
    path_of(flag_path, sizeof flag_path, "log_failed.flag");

    bool flagged = false;
    FILE *fp = fopen(flag_path, "r");
    if (fp) {
        flagged = true;
        if (fscanf(fp, "%u", prev_errors) != 1) {
            *prev_errors = 0;
        }
        fclose(fp);
    }

    fp = fopen(log_path, "r");
    if (!fp) {
        remove(flag_path);
        return flagged ? "failed (no log)" : "none";
    }
    bool closed = false;
    if (fseek(fp, 0, SEEK_END) == 0) {
        long size = ftell(fp);
        long window = size < 512 ? size : 512;
        if (window > 0 && fseek(fp, size - window, SEEK_SET) == 0) {
            char tail[513];
            size_t got = fread(tail, 1, (size_t)window, fp);
            tail[got] = '\0';
            closed = strstr(tail, "[LOG] close") != NULL;
        }
    }
    fclose(fp);

    const char *fate = flagged ? "failed" : (closed ? "clean" : "crash");
    path_of(dest_path, sizeof dest_path, closed && !flagged ? "log_prev.txt" : "log_crash.txt");
    remove(dest_path);
    rename(log_path, dest_path);
    remove(flag_path);
    return fate;
}

static void health_append(const char *fmt, ...) {
    char path[128];
    path_of(path, sizeof path, "log_health.txt");
    FILE *fp = fopen(path, "a");
    if (!fp) {
        return;
    }
    va_list ap;
    va_start(ap, fmt);
    vfprintf(fp, fmt, ap);
    va_end(ap);
    fclose(fp);
}

bool Log_Init(void) {
    LightLock_Init(&s_lock);
    mkdir("sdmc:/3ds", 0777);
    mkdir(PBW2_SD_DIR, 0777);
    read_mode_config();
    unsigned prev_errors = 0;
    const char *fate = rotate_previous(&prev_errors);
    s_ready = true;
    health_append("[boot] prev=%s prev_errors=%u mode=%s\n", fate, prev_errors,
                  s_verbose ? "verbose" : "quiet");
    Log_Line("3DS", "%s port log", PBW2_GAME_NAME);
    Log_Line("3DS", "logger: %s mode, %u KiB RAM ring, no SD writes during play",
             s_verbose ? "verbose" : "quiet", LOG_RING_BYTES / 1024u);
    /* SD is usable when the health record accepted the boot line. */
    char path[128];
    path_of(path, sizeof path, "log_health.txt");
    FILE *probe = fopen(path, "a");
    if (probe) {
        fclose(probe);
        return true;
    }
    return false;
}

void Log_Close(void) {
    if (!s_ready) {
        return;
    }
    /* Summary and close marker bypass the quiet filter. */
    char line[LOG_LINE_MAX];
    snprintf(line, sizeof line, "[LOG] run summary: kept=%u suppressed=%u errors=%u dropped=%u",
             s_kept, s_suppressed, s_errors, s_dropped);
    LightLock_Lock(&s_lock);
    ring_store(line, strlen(line));
    ring_store("[LOG] close", strlen("[LOG] close"));
    LightLock_Unlock(&s_lock);
    unsigned dropped = s_dropped;
    Log_Flush();
    health_append("[exit] result=%s errors=%u dropped=%u\n", s_errors ? "failed" : "clean",
                  s_errors, dropped);
    s_ready = false;
}
