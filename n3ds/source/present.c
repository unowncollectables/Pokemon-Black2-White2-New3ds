/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "present.h"

#include "log.h"
#include "pbw2_port.h"

static uint32_t s_flags;
static bool s_logged;

void Present_Commit(bool top, bool bottom) {
    s_flags = (top ? PBW2_SCREEN_TOP : 0u) | (bottom ? PBW2_SCREEN_BOTTOM : 0u);
}

uint32_t Present_Flags(void) {
    return s_flags;
}

void GxShim_Present(uint32_t flags) {
    if (s_logged) {
        return;
    }
    s_logged = true;
    Log_Line("GX", "present flags top=%d bottom=%d (sharp text uses these)",
             (flags & PBW2_SCREEN_TOP) != 0, (flags & PBW2_SCREEN_BOTTOM) != 0);
}
