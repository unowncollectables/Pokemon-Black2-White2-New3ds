/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "nitro/wm.h"
#include "stub_log.h"

/* WM_GetLinkLevel: signal strength, 0 (none) to 3. No wireless link exists
 * on this platform, so the level is 0. It lives here (not in link_stubs.c)
 * because link_stubs.c cannot include nitro/wm.h: the header declares the
 * real WM_* signatures, which conflict with the void stubs there.
 * Typed, not void: callers index signal bars by the level. */
WMLinkLevel WM_GetLinkLevel(void) {
    PBW2_STUB_LOG("link", "WM_GetLinkLevel (0, no link)");
    return WM_LINK_LEVEL_0;
}
