/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "scene.h"

#include "log.h"

#include <stddef.h>

enum { SCENE_STACK_MAX = 32 };
static uint32_t s_stack[SCENE_STACK_MAX];
static unsigned s_depth;
static uint32_t s_current;

static void scene_changed(void) {
    uint32_t current = s_depth ? s_stack[s_depth - 1] : 0;
    if (current == s_current) return;
    s_current = current;
    Log_Line("WIDE", "scene overlay %u", (unsigned)current);
}

void PbW2_SceneOverlayLoaded(uint32_t id) {
    for (unsigned i = 0; i < s_depth; ++i)
        if (s_stack[i] == id) return;
    if (s_depth < SCENE_STACK_MAX) s_stack[s_depth++] = id;
    scene_changed();
}

void PbW2_SceneOverlayUnloaded(uint32_t id) {
    for (unsigned i = 0; i < s_depth; ++i) {
        if (s_stack[i] != id) continue;
        for (unsigned j = i + 1; j < s_depth; ++j) s_stack[j - 1] = s_stack[j];
        --s_depth;
        break;
    }
    scene_changed();
}

uint32_t PbW2_SceneCurrentOverlay(void) {
    return s_current;
}
