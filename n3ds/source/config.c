/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"

#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Built-in defaults for known keys. A key absent from the file and
 * from this table resolves to the caller's fallback. Display defaults
 * are all off, matching the historical fresh-boot behaviour exactly;
 * existing users keep their settings through the display.cfg
 * migration in settings.c. */
static const struct {
    const char *key;
    int value;
} kCfgDefaults[] = {
    { "lcd_fit", 0 },      { "top_fit", 0 },   { "sharp_pixels", 0 },
    { "gpu_geometry", 0 }, { "gpu_no_edges", 0 }, { "wide_field", 0 },
    { "sharp_text", 0 },
};

enum { MAX_LINES = 1024, CACHE_ENTRIES = 64 };

static char *s_lines[MAX_LINES];
static unsigned s_lineCount;
static char s_path[256];
static char s_tempPath[264];
static unsigned s_fileReads;
static bool s_ready;

struct CacheEntry {
    char key[48];
    int value;
    bool valid;
};
static struct CacheEntry s_cache[CACHE_ENTRIES];

static unsigned hash_key(const char *key) {
    /* FNV-1a */
    unsigned hash = 2166136261u;
    while (*key) hash = (hash ^ (unsigned char)*key++) * 16777619u;
    return hash;
}

/* Find "key=" at the start of a line; returns the value text or NULL. */
static const char *line_value(const char *line, const char *key) {
    size_t length = strlen(key);
    if (strncmp(line, key, length) != 0 || line[length] != '=') return NULL;
    return line + length + 1;
}

static const char *find_value(const char *key) {
    for (unsigned i = 0; i < s_lineCount; ++i) {
        const char *value = line_value(s_lines[i], key);
        if (value) return value;
    }
    return NULL;
}

static int parse_int(const char *text, int fallback) {
    char *end;
    long value = strtol(text, &end, 10);
    return end == text ? fallback : (int)value;
}

static int default_for(const char *key, int fallback) {
    for (unsigned i = 0; i < sizeof kCfgDefaults / sizeof kCfgDefaults[0]; ++i)
        if (strcmp(kCfgDefaults[i].key, key) == 0) return kCfgDefaults[i].value;
    return fallback;
}

static void load_file(void) {
    FILE *file = fopen(s_path, "rb");
    if (!file) return; /* Missing file is a normal first boot. */
    ++s_fileReads;
    char buffer[512];
    while (fgets(buffer, sizeof buffer, file)) {
        if (s_lineCount >= MAX_LINES) {
            Log_Line("CFG", "config.txt exceeds %d lines; tail ignored", MAX_LINES);
            break;
        }
        buffer[strcspn(buffer, "\r\n")] = '\0';
        s_lines[s_lineCount] = strdup(buffer);
        if (s_lines[s_lineCount]) ++s_lineCount;
    }
    fclose(file);
}

static bool save_file(void) {
    FILE *file = fopen(s_tempPath, "wb");
    if (!file) {
        Log_Line("CFG", "config.txt staged write failed");
        return false;
    }
    for (unsigned i = 0; i < s_lineCount; ++i) fprintf(file, "%s\n", s_lines[i]);
    if (fclose(file) != 0) {
        Log_Line("CFG", "config.txt staged close failed");
        return false;
    }
    remove(s_path);
    if (rename(s_tempPath, s_path) != 0) {
        Log_Line("CFG", "config.txt commit rename failed");
        return false;
    }
    return true;
}

void PbW2_ConfigInit(const char *path) {
    snprintf(s_path, sizeof s_path, "%s", path);
    snprintf(s_tempPath, sizeof s_tempPath, "%s.tmp", path);
    s_lineCount = 0;
    memset(s_cache, 0, sizeof s_cache);
    /* An interrupted rewrite leaves the staged file behind; it is the
     * newer copy, so prefer it (same recovery as display.cfg). */
    FILE *staged = fopen(s_tempPath, "rb");
    if (staged) {
        fclose(staged);
        remove(s_path);
        rename(s_tempPath, s_path);
    }
    load_file();
    s_ready = true;
    for (unsigned i = 0; i < sizeof kCfgDefaults / sizeof kCfgDefaults[0]; ++i) {
        const char *key = kCfgDefaults[i].key;
        Log_Line("CFG", "%s = %d", key, PbW2_ConfigGetInt(key, kCfgDefaults[i].value));
    }
    Log_Line("CFG", "config store ready: %u lines, %u file reads", s_lineCount, s_fileReads);
}

int PbW2_ConfigGetInt(const char *key, int fallback) {
    /* Writes memset the whole cache, so a valid entry is current.
     * Two keys sharing a slot just evict each other back to the
     * line scan - slower, never wrong. */
    unsigned slot = hash_key(key) % CACHE_ENTRIES;
    struct CacheEntry *entry = &s_cache[slot];
    if (entry->valid && strcmp(entry->key, key) == 0) return entry->value;
    const char *text = find_value(key);
    int value = text ? parse_int(text, default_for(key, fallback))
                     : default_for(key, fallback);
    snprintf(entry->key, sizeof entry->key, "%s", key);
    entry->value = value;
    entry->valid = true;
    return value;
}

bool PbW2_ConfigGetBool(const char *key, bool fallback) {
    return PbW2_ConfigGetInt(key, fallback ? 1 : 0) != 0;
}

bool PbW2_ConfigHas(const char *key) {
    return find_value(key) != NULL;
}

void PbW2_ConfigSetInt(const char *key, int value) {
    if (!s_ready) return;
    const char *existing = find_value(key);
    if (existing && parse_int(existing, 0x7fffffff) == value) return;
    char line[96];
    snprintf(line, sizeof line, "%s=%d", key, value);
    for (unsigned i = 0; i < s_lineCount; ++i) {
        if (line_value(s_lines[i], key)) {
            char *copy = strdup(line);
            if (!copy) return;
            free(s_lines[i]);
            s_lines[i] = copy;
            save_file();
            memset(s_cache, 0, sizeof s_cache);
            return;
        }
    }
    if (s_lineCount >= MAX_LINES) {
        Log_Line("CFG", "config.txt full; %s not stored", key);
        return;
    }
    s_lines[s_lineCount] = strdup(line);
    if (!s_lines[s_lineCount]) return;
    ++s_lineCount;
    save_file();
    memset(s_cache, 0, sizeof s_cache);
}

void PbW2_ConfigSetBool(const char *key, bool value) {
    PbW2_ConfigSetInt(key, value ? 1 : 0);
}

unsigned PbW2_ConfigFileReads(void) {
    return s_fileReads;
}
