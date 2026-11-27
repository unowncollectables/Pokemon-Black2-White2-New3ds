/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "settings.h"

#include "config.h"
#include "log.h"
#include "save_backup.h"
#ifdef PBW2_GPU_GEOMETRY
#include "g3_pica.h"
#include "g3_raster.h"
#endif

#include <3ds.h>
#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Menu shell: SoulSilver's L+R+Y settings app layout, ported zone by  */
/* zone (menu-parity.md P1-P3). The menu is drawn on a 256x192         */
/* DS-space canvas and presented scaled 5/4 onto the 320x240 bottom    */
/* framebuffer, exactly like SS presents its canvas. Every zone        */
/* constant below is SS's (kSa* in ss3ds_settings_ui.h); only the      */
/* colours are B2W2's own greys/teals - the one difference Nate's      */
/* parity spec permits. Text still uses the libctru console font; the  */
/* game-font/sharp-text path is parity stage P5, not this one.         */
/*                                                                     */
/* Session semantics are SS's too: values are edited in a staged copy  */
/* and applied + written to config.txt only when the menu closes       */
/* (B, START, or the L+R+Y chord), with a release-wait so the closing  */
/* chord cannot leak into the game. Stage-on-close writes EVERY staged */
/* change through the config store before re-deriving the applied     */
/* state from it, so stored == applied after close (the REQ-4          */
/* caution in menu-parity.md SS5/P2).                                  */
/* ------------------------------------------------------------------ */

enum {
    /* SS layout constants, DS pixels. */
    TAB_H = 19,        /* kSaTabH: tab boxes y 2..19 */
    BAND_Y = 21,       /* kSaBandY */
    BAND_H = 13,       /* kSaBandH */
    LIST_Y = 37,       /* kSaListY */
    ROW_H = 17,        /* kSaRowH */
    ROW_PITCH = 19,    /* kSaRowPitch */
    ROWS_VISIBLE = 7,  /* kSaRows */
    FOOT_Y = 174,      /* kSaFootY */
    CANVAS_W = 256,
    CANVAS_H = 192,
};

/* B2W2 colours (RGB565), SS zone roles: mint backdrop -> B2W2's dark
 * teal field, green accents -> B2W2's bright teal selection tone. */
enum {
    COL_BG_TOP = 0x18c6,
    COL_BG_BOT = 0x0842,
    COL_TAB_OFF = 0x1063,
    COL_TAB_ON = 0x32ac,
    COL_TAB_ON_EDGE = 0x18c6,
    COL_TAB_TEXT = 0xffff,
    COL_BAND_A = 0x2108,
    COL_BAND_B = 0x1063,
    COL_BAND_TEXT = 0xffff,
    COL_CARD = 0x2108,
    COL_CARD_EDGE = 0x5aeb,
    COL_SEL_FILL = 0x2d6b,
    COL_SEL_EDGE = 0x7fff,
    COL_NAME = 0xffff,
    COL_PILL_ON = 0x32ac,
    COL_PILL_OFF = 0x7bef,
    COL_PILL_OTHER = 0x0842,
    COL_PILL_TEXT = 0xffff,
    COL_PILL_OFF_TEXT = 0x18c6,
    COL_ARROW = 0x7fff,
    COL_CHANGED = 0xf800,
    COL_SCROLL_RAIL = 0x1063,
    COL_SCROLL_THUMB = 0x32ac,
    COL_CHIP = 0xffff,
    COL_CHIP_TEXT = 0x0842,
    COL_FOOT_TEXT = 0xffff,
};

static bool s_open;
static unsigned s_row;
static unsigned s_top;
static unsigned s_tab;
static uint32_t s_display;
/* FPS-counter hotkey override: -1 = follow the config key, 0/1 =
 * forced by L+R+START. RAM-only on purpose - a diagnostic peek must
 * not rewrite the saved preference. Changing the DEBUG row clears
 * the override (the row wins from then on). */
static int s_fpsOverride = -1;
static char s_oldPath[160];
static char s_cfgPath[160];
static const char s_configHeader[] = "PBW2-DISPLAY 1\n";
static const char s_hex[] = "0123456789abcdef";

/* Hidden DEBUG tab (P3): session-only, exactly like SS - SELECT inside
 * the menu shows/hides it and the flag is never persisted. */
static bool s_debugShown;
/* Release-wait after close: swallow input until every button that was
 * held at close has been released, so the closing chord (or B/START)
 * cannot leak into the game. */
static bool s_waitRelease;
static uint32_t s_releaseMask;
/* Corner-hold open (SS's second open path): touch held in the
 * top-left corner for ~0.3 s (18 frames at the input scan rate). */
static unsigned s_cornerFrames;

static const struct {
    const char *label;
    const char *key;
    uint32_t flag;
} s_options[] = {
    {"LCD fit (4:3)", "lcd_fit", DISPLAY_LCD_FIT},
    {"Top screen fit (4:3)", "top_fit", DISPLAY_TOP_FIT},
    {"Sharp scaling", "sharp_pixels", DISPLAY_SHARP_PIXELS},
    {"Sharp game text", "sharp_text", DISPLAY_SHARP_TEXT},
#ifdef PBW2_GPU_GEOMETRY
    {"GPU geometry (exp.)", "gpu_geometry", DISPLAY_PICA_GEOMETRY},
    {"GPU 3D without outlines", "gpu_no_edges", DISPLAY_PICA_NO_EDGES},
    {"Widescreen 3D", "wide_field", DISPLAY_WIDE_FIELD},
#endif
};

enum {
    ROWS = sizeof s_options / sizeof s_options[0],
    DISPLAY_MASK = DISPLAY_SHARP_TEXT | DISPLAY_WIDE_FIELD | DISPLAY_LCD_FIT | DISPLAY_TOP_FIT | DISPLAY_SHARP_PIXELS | DISPLAY_PICA_GEOMETRY | DISPLAY_PICA_NO_EDGES,
};

/* ------------------------- row model ------------------------- */
/* SS's row shape: a label plus a labelled multi-state cycle. Rows
 * are staged: s_stage holds the value being edited, s_openVal the
 * value at open time; a row whose stage differs from its open value
 * gets the changed dot and is written on close. */

enum RowKind {
    ROW_VALUE,    /* config-backed labelled cycle, staged */
    ROW_ACTION,   /* staged arm/disarm; runs on close when armed */
    ROW_READONLY, /* diagnostic text, never steps, never writes */
    ROW_NOTE,     /* empty-state note (BATTLE / PLAY), no pill */
};

enum { ROW_SPECIAL_NONE = 0, ROW_SPECIAL_SCALE = 1 };

typedef struct {
    const char *label;
    const char *key; /* ROW_VALUE only; NULL for the scale row */
    const char *const *labels;
    int nLabels;
    int kind;
    int special;
} RowDef;

static const char *const kBoolLabels[] = { "OFF", "ON" };
static const char *const kScaleLabels[] = { "1:1", "FIT", "TOP FIT" };
static const char *const kExportLabels[] = { "A: EXPORT", "EXPORT ON CLOSE" };

enum {
    TAB_LOOK = 0, TAB_BATTLE = 1, TAB_PLAY = 2, TAB_SYSTEM = 3, TAB_DEBUG = 4,
    TAB_COUNT = 5,
    MAX_ROWS = 8,
};
static const char *const s_tabNames[TAB_COUNT] = { "LOOK", "BATTLE", "PLAY", "SYSTEM", "DEBUG" };

/* Rehousing map (menu-parity.md SS4.1): display rows -> LOOK, Export
 * save -> SYSTEM #1 (SS's SYSTEM export slot), FPS counter -> DEBUG
 * #1 and Save backups -> DEBUG #2 (their exact SS slots), the old
 * System diagnostics -> DEBUG as read-only rows. BATTLE ships with
 * Nate's note row only - no functional battle rows yet, and the
 * must-NOT-copy list binds (no SELECT=reset, no skip-intro, no shiny
 * odds, no no-wild-battles). PLAY's follower row lands with the
 * follower feature; until then it carries the same note treatment. */
static RowDef s_lookRows[ROWS + 1];
static const RowDef s_battleRows[] = {
    { "Battle settings come later", NULL, NULL, 0, ROW_NOTE, ROW_SPECIAL_NONE },
};
static const RowDef s_playRows[] = {
    { "Play settings come later", NULL, NULL, 0, ROW_NOTE, ROW_SPECIAL_NONE },
};
static const RowDef s_systemRows[] = {
    { "Export save", NULL, kExportLabels, 2, ROW_ACTION, ROW_SPECIAL_NONE },
};
static const RowDef s_debugRows[] = {
    { "FPS counter", "fps_counter", kBoolLabels, 2, ROW_VALUE, ROW_SPECIAL_NONE },
    { "Save backups", "save_backup", kBoolLabels, 2, ROW_VALUE, ROW_SPECIAL_NONE },
    { "Config store", NULL, NULL, 0, ROW_READONLY, ROW_SPECIAL_NONE },
    { "Config file reads", NULL, NULL, 0, ROW_READONLY, ROW_SPECIAL_NONE },
    { "Display flags", NULL, NULL, 0, ROW_READONLY, ROW_SPECIAL_NONE },
};

static const RowDef *tab_def(unsigned tab, unsigned *count) {
    switch (tab) {
    case TAB_LOOK: *count = ROWS + 1; return s_lookRows;
    case TAB_BATTLE: *count = sizeof s_battleRows / sizeof s_battleRows[0]; return s_battleRows;
    case TAB_PLAY: *count = sizeof s_playRows / sizeof s_playRows[0]; return s_playRows;
    case TAB_SYSTEM: *count = sizeof s_systemRows / sizeof s_systemRows[0]; return s_systemRows;
    default: *count = sizeof s_debugRows / sizeof s_debugRows[0]; return s_debugRows;
    }
}

static unsigned visible_tabs(void) {
    return s_debugShown ? TAB_COUNT : TAB_COUNT - 1;
}

/* Staged and open-time values, per tab/row. */
static int s_stage[TAB_COUNT][MAX_ROWS];
static int s_openVal[TAB_COUNT][MAX_ROWS];

static int scale_value(void) {
    if (PbW2_ConfigGetBool("lcd_fit", false)) return 1;
    if (PbW2_ConfigGetBool("top_fit", false)) return 2;
    return 0;
}

static int row_value(const RowDef *row) {
    if (row->kind == ROW_ACTION) return 0; /* actions always open disarmed */
    if (row->special == ROW_SPECIAL_SCALE) return scale_value();
    if (row->key) return PbW2_ConfigGetInt(row->key, 0);
    return 0;
}

static void snapshot_stage(void) {
    for (unsigned t = 0; t < TAB_COUNT; ++t) {
        unsigned n;
        const RowDef *rows = tab_def(t, &n);
        for (unsigned i = 0; i < n; ++i) s_stage[t][i] = s_openVal[t][i] = row_value(&rows[i]);
    }
}

/* The display bitmask is DERIVED from the config store, never stored
 * separately: the store is the single source of truth, so the menu,
 * the file and the applied state cannot disagree. */
static void derive_display(void) {
    s_display = 0;
    for (unsigned i = 0; i < ROWS; ++i)
        if (PbW2_ConfigGetBool(s_options[i].key, false)) s_display |= s_options[i].flag;
}

static void apply_display(void) {
#ifdef PBW2_GPU_GEOMETRY
    PbW2_G3SetWide((s_display & DISPLAY_WIDE_FIELD) != 0);
    PbW2_PicaSetOptions((s_display & DISPLAY_PICA_GEOMETRY) != 0,
                        (s_display & DISPLAY_PICA_NO_EDGES) != 0);
#endif
}

void Settings_ApplyDisplay(void) {
    apply_display();
}

static bool load_display(const char *path, uint32_t *flags) {
    FILE *file = fopen(path, "rb");
    if (!file) return false;
    char entry[sizeof s_configHeader + 2];
    bool complete = fread(entry, 1, sizeof entry, file) == sizeof entry && fgetc(file) == EOF && !ferror(file);
    fclose(file);
    const unsigned start = sizeof s_configHeader - 1;
    if (!complete || memcmp(entry, s_configHeader, start) || entry[start + 2] != '\n') return false;
    const char *high = entry[start] ? strchr(s_hex, entry[start]) : NULL;
    const char *low = entry[start + 1] ? strchr(s_hex, entry[start + 1]) : NULL;
    if (!high || !low) return false;
    *flags = ((high - s_hex) * 16 + (low - s_hex)) & DISPLAY_MASK;
    return true;
}

void Settings_Init(const char *dir) {
    /* Build the LOOK tab: the display rows in their historical order,
     * then the scale row - the L+R+SELECT hotkey cycle finally gets
     * its menu home as a labelled row (menu-parity.md SS4.1). */
    for (unsigned i = 0; i < ROWS; ++i) {
        s_lookRows[i].label = s_options[i].label;
        s_lookRows[i].key = s_options[i].key;
        s_lookRows[i].labels = kBoolLabels;
        s_lookRows[i].nLabels = 2;
        s_lookRows[i].kind = ROW_VALUE;
        s_lookRows[i].special = ROW_SPECIAL_NONE;
    }
    s_lookRows[ROWS].label = "Scale";
    s_lookRows[ROWS].key = NULL;
    s_lookRows[ROWS].labels = kScaleLabels;
    s_lookRows[ROWS].nLabels = 3;
    s_lookRows[ROWS].kind = ROW_VALUE;
    s_lookRows[ROWS].special = ROW_SPECIAL_SCALE;

    snprintf(s_cfgPath, sizeof s_cfgPath, "%s/config.txt", dir);
    snprintf(s_oldPath, sizeof s_oldPath, "%s/display.cfg", dir);
    PbW2_ConfigInit(s_cfgPath);
    /* One-way migration: if the config store has none of the display
     * keys, carry the old display.cfg value across once. Afterwards
     * the keys exist and this never runs again; display.cfg itself is
     * left on the card, retired. */
    bool anyKey = false;
    for (unsigned i = 0; i < ROWS; ++i) anyKey = anyKey || PbW2_ConfigHas(s_options[i].key);
    if (!anyKey) {
        uint32_t old = 0;
        char tempPath[168];
        snprintf(tempPath, sizeof tempPath, "%s.tmp", s_oldPath);
        bool loaded = load_display(tempPath, &old) || load_display(s_oldPath, &old);
        if (!loaded) {
            /* A present but unparsable display.cfg silently loading
             * as all-off looks exactly like the settings resetting
             * themselves, so say it out loud. */
            FILE *probe = fopen(s_oldPath, "rb");
            if (probe) {
                fclose(probe);
                Log_Line("CFG", "display.cfg present but unreadable; using defaults");
            }
        }
        if (loaded) {
            for (unsigned i = 0; i < ROWS; ++i)
                PbW2_ConfigSetBool(s_options[i].key, (old & s_options[i].flag) != 0);
            Log_Line("CFG", "migrated display.cfg (%02lx) to config.txt", (unsigned long)old);
        }
    }
    derive_display();
    Log_Line("CFG", "display flags loaded: %02lx", (unsigned long)s_display);
    apply_display();
}

bool Settings_FpsCounterOn(void) {
    if (s_fpsOverride >= 0) return s_fpsOverride != 0;
    return PbW2_ConfigGetBool("fps_counter", false);
}

/* Cycle the fit flags 1:1 -> Fit -> Top fit. A display preference,
 * so unlike the FPS hotkey it persists through the store. Used by
 * the L+R+SELECT hotkey; the LOOK scale row stages the same cycle
 * and commits it on close, so the two always agree afterwards. */
static void cycle_scale(void) {
    static const char *const names[3] = { "1:1", "fit", "top fit" };
    unsigned state = (s_display & DISPLAY_LCD_FIT) ? 1 : (s_display & DISPLAY_TOP_FIT) ? 2 : 0;
    state = (state + 1) % 3;
    PbW2_ConfigSetBool("lcd_fit", state == 1);
    PbW2_ConfigSetBool("top_fit", state == 2);
    derive_display();
    apply_display();
    Log_Line("CFG", "scale cycle -> %s", names[state]);
}

bool Settings_Open(void) {
    return s_open;
}

static void open_menu(void) {
    snapshot_stage();
    s_open = true;
    s_row = 0;
    s_top = 0;
    Log_Line("CFG", "settings open");
}

/* Commit every staged change through the config store, then derive
 * the applied state back out of the store: stored == applied by
 * construction, checked and logged (REQ-4 caution). */
static void commit_stage(void) {
    bool exportArmed = false;
    for (unsigned t = 0; t < TAB_COUNT; ++t) {
        unsigned n;
        const RowDef *rows = tab_def(t, &n);
        for (unsigned i = 0; i < n; ++i) {
            const RowDef *row = &rows[i];
            if (row->kind == ROW_ACTION) {
                if (s_stage[t][i] != 0) exportArmed = true;
                continue;
            }
            if (row->kind != ROW_VALUE || s_stage[t][i] == s_openVal[t][i]) continue;
            if (row->special == ROW_SPECIAL_SCALE) {
                PbW2_ConfigSetBool("lcd_fit", s_stage[t][i] == 1);
                PbW2_ConfigSetBool("top_fit", s_stage[t][i] == 2);
            } else if (row->key) {
                PbW2_ConfigSetInt(row->key, s_stage[t][i]);
            }
            Log_Line("CFG", "%s -> %s", row->label, row->labels[s_stage[t][i]]);
            if (row->key && strcmp(row->key, "fps_counter") == 0) s_fpsOverride = -1;
        }
    }
    derive_display();
    apply_display();
    Log_Line("CFG", "settings applied on close: display flags %02lx", (unsigned long)s_display);
    if (exportArmed) {
        /* Export save: hand the request to the IO worker; the
         * settings screen does no file IO itself. */
        SaveBackup_RequestExport();
        Log_Line("CFG", "export save requested");
    }
}

static void close_menu(uint32_t held) {
    commit_stage();
    s_open = false;
    s_waitRelease = true;
    s_releaseMask = held;
    Log_Line("CFG", "settings close");
}

static void step_row(int dir) {
    unsigned n;
    const RowDef *rows = tab_def(s_tab, &n);
    if (s_row >= n) return;
    const RowDef *row = &rows[s_row];
    if (row->kind != ROW_VALUE && row->kind != ROW_ACTION) return;
    int v = s_stage[s_tab][s_row] + dir;
    v %= row->nLabels;
    if (v < 0) v += row->nLabels;
    s_stage[s_tab][s_row] = v;
}

static void clamp_scroll(void) {
    if (s_row < s_top) s_top = s_row;
    if (s_row >= s_top + ROWS_VISIBLE) s_top = s_row - ROWS_VISIBLE + 1;
}

static void switch_tab(unsigned tab) {
    s_tab = tab;
    s_row = 0;
    s_top = 0;
}

bool Settings_Handle(uint32_t down, uint32_t held) {
    /* Release-wait: after a close, swallow everything until the
     * buttons held at close have all been released. */
    if (s_waitRelease) {
        if (held & s_releaseMask) return true;
        s_waitRelease = false;
    }
    if ((held & KEY_L) && (held & KEY_R) && (down & KEY_Y)) {
        if (s_open)
            close_menu(held);
        else
            open_menu();
        return true;
    }
    /* Hotkeys: L+R held + an edge press, only while Settings is
     * closed. Returning true swallows the whole held set, so the
     * game never sees START/SELECT leak through (input.c's blocked
     * mask is sticky while held). Soft reset is deliberately NOT
     * here - the game never resets itself. */
    if (!s_open && (held & KEY_L) && (held & KEY_R)) {
        if (down & KEY_START) {
            s_fpsOverride = Settings_FpsCounterOn() ? 0 : 1;
            Log_Line("CFG", "fps counter %s (hotkey)", s_fpsOverride ? "on" : "off");
            return true;
        }
        if (down & KEY_SELECT) {
            cycle_scale();
            return true;
        }
    }
    if (!s_open) {
        /* Corner-hold open: touch held in the top-left corner of the
         * bottom screen (DS coords x<32, y<24) for ~0.3 s. */
        if (held & KEY_TOUCH) {
            touchPosition touch;
            hidTouchRead(&touch);
            if (touch.px * 4 / 5 < 32 && touch.py * 4 / 5 < 24) {
                if (++s_cornerFrames >= 18) {
                    s_cornerFrames = 0;
                    open_menu();
                    return true;
                }
            } else {
                s_cornerFrames = 0;
            }
        } else {
            s_cornerFrames = 0;
        }
        return false;
    }

    /* ---- menu open ---- */
    if ((down & KEY_B) || (down & KEY_START)) {
        close_menu(held);
        return true;
    }
    if (down & KEY_SELECT) {
        s_debugShown = !s_debugShown;
        if (!s_debugShown && s_tab == TAB_DEBUG) switch_tab(TAB_SYSTEM);
        Log_Line("APP", "debug tab %s", s_debugShown ? "shown" : "hidden");
        return true;
    }
    if (down & KEY_L) {
        switch_tab((s_tab + visible_tabs() - 1) % visible_tabs());
        return true;
    }
    if (down & KEY_R) {
        switch_tab((s_tab + 1) % visible_tabs());
        return true;
    }
    unsigned n;
    tab_def(s_tab, &n);
    if (n > 0) {
        if (down & KEY_DUP) s_row = (s_row + n - 1) % n;
        if (down & KEY_DDOWN) s_row = (s_row + 1) % n;
    }
    if ((down & KEY_A) || (down & KEY_DRIGHT)) step_row(1);
    if ((down & KEY_X) || (down & KEY_DLEFT)) step_row(-1);
    if (down & KEY_TOUCH) {
        touchPosition touch;
        hidTouchRead(&touch);
        /* Touch arrives in 320x240 screen px; the menu lives in
         * 256x192 DS px. */
        unsigned tx = touch.px * 4 / 5, ty = touch.py * 4 / 5;
        if (ty < TAB_H) {
            switch_tab(tx * visible_tabs() / CANVAS_W);
        } else if (ty >= LIST_Y && n > 0) {
            unsigned rel = ty - LIST_Y;
            unsigned k = rel / ROW_PITCH;
            if (rel % ROW_PITCH < ROW_H && s_top + k < n) {
                unsigned row = s_top + k;
                unsigned n2;
                const RowDef *rows = tab_def(s_tab, &n2);
                const RowDef *def = &rows[row];
                /* SS touch semantics: tap the value pill's left/right
                 * half to step back/forward; tap the name to select,
                 * tap it again to step forward. */
                bool onPill = false;
                if (def->kind == ROW_VALUE || def->kind == ROW_ACTION) {
                    const char *val = def->labels[s_stage[s_tab][row]];
                    unsigned pw = strlen(val) * 8 + 10;
                    if (pw < 30) pw = 30;
                    unsigned x1 = (n > ROWS_VISIBLE) ? 246 : 253;
                    unsigned xr = x1 - (row == s_row ? 11 : 4);
                    if (tx >= xr - pw && tx < xr) {
                        onPill = true;
                        s_row = row;
                        step_row(tx < xr - pw / 2 ? -1 : 1);
                    }
                }
                if (!onPill) {
                    if (row == s_row)
                        step_row(1);
                    else
                        s_row = row;
                }
            }
        }
    }
    clamp_scroll();
    return true;
}

uint32_t Settings_DisplayFlags(void) {
    return s_display;
}

/* ------------------------- drawing ------------------------- */
#if defined(PBW2_LINK_GAME)
static u16 s_canvas[CANVAS_W * CANVAS_H];

static u16 mix565(u16 a, u16 c, int t, int n) {
    int ar = (a >> 11) & 31, ag = (a >> 5) & 63, ab = a & 31;
    int cr = (c >> 11) & 31, cg = (c >> 5) & 63, cb = c & 31;
    int r = ar + (cr - ar) * t / n, g = ag + (cg - ag) * t / n, b = ab + (cb - ab) * t / n;
    return (u16)((r << 11) | (g << 5) | b);
}

static void cv_px(int x, int y, u16 color) {
    if ((unsigned)x < CANVAS_W && (unsigned)y < CANVAS_H) s_canvas[y * CANVAS_W + x] = color;
}

static void cv_fill(int x0, int y0, int x1, int y1, u16 color) {
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > CANVAS_W) x1 = CANVAS_W;
    if (y1 > CANVAS_H) y1 = CANVAS_H;
    for (int y = y0; y < y1; ++y)
        for (int x = x0; x < x1; ++x) s_canvas[y * CANVAS_W + x] = color;
}

static void cv_grad(int x0, int y0, int x1, int y1, u16 a, u16 c) {
    for (int y = y0; y < y1; ++y) cv_fill(x0, y, x1, y + 1, mix565(a, c, y - y0, y1 - y0 > 1 ? y1 - y0 - 1 : 1));
}

/* Cut-corner box, SS's saBox shape: `rim` px edge, rest fill. */
static void cv_box(int x0, int y0, int x1, int y1, u16 fill, u16 edge, int rim) {
    for (int y = y0; y < y1; ++y) {
        int dy = y - y0 < y1 - 1 - y ? y - y0 : y1 - 1 - y;
        int cut = 2 - dy > 0 ? 2 - dy : 0;
        for (int x = x0 + cut; x < x1 - cut; ++x) {
            int dx = x - x0 < x1 - 1 - x ? x - x0 : x1 - 1 - x;
            cv_px(x, y, (dy < rim || dx < rim || dx + dy < 2 + rim) ? edge : fill);
        }
    }
}

static void cv_text(int x, int y, const char *text, u16 color) {
    const ConsoleFont *font = &consoleGetDefault()->font;
    for (; *text; ++text, x += 8) {
        const u8 *glyph = font->gfx + 8 * ((unsigned char)*text - font->asciiOffset);
        for (unsigned dx = 0; dx < 8; ++dx)
            for (unsigned dy = 0; dy < 8; ++dy)
                if (glyph[dy] & (0x80u >> dx)) cv_px(x + (int)dx, y + (int)dy, color);
    }
}

static int text_w(const char *text) {
    return (int)strlen(text) * 8;
}

static void cv_arrow(int x, int y, int dir, u16 color) {
    for (int k = 0; k < 4; k++)
        for (int yy = k; yy < 7 - k; yy++) cv_px(dir > 0 ? x + k : x + 3 - k, y + yy, color);
}

/* A right-aligned value pill; returns its left x. */
static int cv_pill(int xr, int y0, const char *text, u16 fill, u16 ink) {
    int w = text_w(text) + 10;
    if (w < 30) w = 30;
    int x0 = xr - w;
    cv_box(x0, y0, xr, y0 + 12, fill, fill, 1);
    cv_text(x0 + (w - text_w(text)) / 2, y0 + 2, text, ink);
    return x0;
}

static const char *readonly_value(unsigned tab, unsigned row, char *buf, size_t size) {
    (void)tab;
    /* DEBUG read-only rows, in s_debugRows order after the two
     * config-backed rows. */
    if (row == 2) return "config.txt";
    if (row == 3) {
        snprintf(buf, size, "%u", PbW2_ConfigFileReads());
        return buf;
    }
    snprintf(buf, size, "%02lx", (unsigned long)s_display);
    return buf;
}

static void draw_canvas(void) {
    /* Backdrop: vertical gradient in B2W2's teals. */
    cv_grad(0, 0, CANVAS_W, CANVAS_H, COL_BG_TOP, COL_BG_BOT);

    /* Tab strip: equal-width boxes across the full width; the
     * selected tab is filled with a darker underline strip. */
    unsigned nt = visible_tabs();
    for (unsigned t = 0; t < nt; ++t) {
        int x0 = (int)(t * CANVAS_W / nt) + 2, x1 = (int)((t + 1) * CANVAS_W / nt) - 2;
        bool on = t == s_tab;
        cv_box(x0, 2, x1, TAB_H, on ? COL_TAB_ON : COL_TAB_OFF, COL_BG_BOT, 1);
        if (on) cv_fill(x0 + 2, TAB_H - 3, x1 - 2, TAB_H - 1, COL_TAB_ON_EDGE);
        cv_text((x0 + x1) / 2 - text_w(s_tabNames[t]) / 2, 6, s_tabNames[t], COL_TAB_TEXT);
    }

    /* Band: current tab name at left, "N settings" at right. Note
     * rows are not settings, so BATTLE honestly reads 0. */
    unsigned n;
    const RowDef *rows = tab_def(s_tab, &n);
    unsigned nSettings = 0;
    for (unsigned i = 0; i < n; ++i)
        if (rows[i].kind != ROW_NOTE) ++nSettings;
    cv_grad(2, BAND_Y, 254, BAND_Y + BAND_H, COL_BAND_A, COL_BAND_B);
    cv_text(7, BAND_Y + 2, s_tabNames[s_tab], COL_BAND_TEXT);
    char count[24];
    snprintf(count, sizeof count, nSettings == 1 ? "1 setting" : "%u settings", nSettings);
    cv_text(249 - text_w(count), BAND_Y + 2, count, COL_BAND_TEXT);

    /* List: full-width row cards, value as a right-aligned pill,
     * selected row gets a thicker bright rim and < > arrows flanking
     * the pill, a changed row a red dot at its top-left corner. */
    bool scroll = n > ROWS_VISIBLE;
    for (unsigned k = 0; k < ROWS_VISIBLE && s_top + k < n; ++k) {
        unsigned i = s_top + k;
        const RowDef *row = &rows[i];
        int x0 = 3, x1 = scroll ? 246 : 253;
        int y0 = LIST_Y + (int)k * ROW_PITCH, y1 = y0 + ROW_H;
        bool sel = i == s_row;
        cv_box(x0, y0, x1, y1, sel ? COL_SEL_FILL : COL_CARD, sel ? COL_SEL_EDGE : COL_CARD_EDGE, sel ? 2 : 1);
        cv_text(x0 + 6, y0 + 4, row->label, COL_NAME);
        if (row->kind == ROW_VALUE || row->kind == ROW_ACTION) {
            const char *val = row->labels[s_stage[s_tab][i]];
            bool isOn = strcmp(val, "ON") == 0, isOff = strcmp(val, "OFF") == 0;
            int xr = x1 - (sel ? 11 : 4);
            int px0 = cv_pill(xr, y0 + 3, val, isOn ? COL_PILL_ON : isOff ? COL_PILL_OFF : COL_PILL_OTHER,
                              isOff ? COL_PILL_OFF_TEXT : COL_PILL_TEXT);
            if (sel) {
                cv_arrow(px0 - 6, y0 + 5, -1, COL_ARROW);
                cv_arrow(xr + 3, y0 + 5, 1, COL_ARROW);
            }
            if (s_stage[s_tab][i] != s_openVal[s_tab][i])
                cv_fill(x0 + 2, y0 + 2, x0 + 5, y0 + 5, COL_CHANGED);
        } else if (row->kind == ROW_READONLY) {
            char buf[24];
            const char *val = readonly_value(s_tab, i, buf, sizeof buf);
            int xr = x1 - 4;
            cv_pill(xr, y0 + 3, val, COL_PILL_OTHER, COL_PILL_TEXT);
        }
    }
    if (scroll) {
        int h = ROWS_VISIBLE * ROW_PITCH - 2;
        int th = h * ROWS_VISIBLE / (int)n;
        if (th < 10) th = 10;
        int ty = LIST_Y + (h - th) * (int)s_top / ((int)n - ROWS_VISIBLE);
        cv_box(248, LIST_Y, 254, LIST_Y + h, COL_SCROLL_RAIL, COL_SCROLL_RAIL, 1);
        cv_box(248, ty, 254, ty + th, COL_SCROLL_THUMB, COL_SCROLL_THUMB, 1);
    }

    /* Footer: gradient band with key chips [L R] tab [A] change
     * [B] close. */
    cv_grad(0, FOOT_Y, CANVAS_W, CANVAS_H, COL_BAND_B, COL_BG_BOT);
    static const char *const keys[3][2] = { { "L R", "tab" }, { "A", "change" }, { "B", "close" } };
    int x = 5;
    for (int i = 0; i < 3; ++i) {
        int w = text_w(keys[i][0]) + 10;
        cv_box(x, FOOT_Y + 3, x + w, 189, COL_CHIP, COL_CHIP, 1);
        cv_text(x + 5, FOOT_Y + 5, keys[i][0], COL_CHIP_TEXT);
        x += w + 3;
        cv_text(x, FOOT_Y + 5, keys[i][1], COL_FOOT_TEXT);
        x += text_w(keys[i][1]) + 9;
    }
}
#endif

void Settings_Draw(void) {
#if defined(PBW2_LINK_GAME)
    u16 stride, width;
    u16 *fb = (u16 *)gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, &stride, &width);
    if (!fb || stride != 240 || width != 320) return;
    draw_canvas();
    /* Present the 256x192 canvas at 320x240 (uniform 5/4), in the
     * framebuffer's column-major addressing. */
    for (unsigned x = 0; x < width; ++x) {
        unsigned sx = x * 4 / 5;
        for (unsigned y = 0; y < stride; ++y) {
            fb[x * stride + stride - 1 - y] = s_canvas[(y * 4 / 5) * CANVAS_W + sx];
        }
    }
#else
    printf("\x1b[1;0HSETTINGS");
    printf("\x1b[3;0HDisplay settings in linked builds.");
    printf("\x1b[5;0HB closes.");
#endif
}
