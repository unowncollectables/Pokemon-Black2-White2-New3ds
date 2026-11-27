/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include <stdint.h>

/* Host-side scene identity, the seam the wide layout work keys off:
 * the most recently loaded still-resident ARM9 overlay, maintained
 * from overlay_host's load/unload path. 0 means none tracked.
 * Scenes are refined later with a DISPCNT signature; an unknown
 * scene simply gets no layout, which is today's behaviour. */
void PbW2_SceneOverlayLoaded(uint32_t id);
void PbW2_SceneOverlayUnloaded(uint32_t id);
uint32_t PbW2_SceneCurrentOverlay(void);
