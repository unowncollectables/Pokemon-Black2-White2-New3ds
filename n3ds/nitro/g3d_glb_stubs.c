/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "nnsys/g3d.h"
#include "stub_log.h"

void NNS_G3dGlbInit(void) {
    PBW2_STUB_LOG("G3D", "GlbInit");
}

void NNS_G3dGlbFlushP(void) {
}

void NNS_G3dGlbFlushVP(void) {
}

void NNS_G3dGlbFlushWVP(void) {
}

void NNS_G3dGlbLightColor(GXLightId lightID, GXRgb rgb) {
    (void)lightID;
    (void)rgb;
}

void NNS_G3dGlbLightVector(GXLightId lightID, fx16 x, fx16 y, fx16 z) {
    (void)lightID;
    (void)x;
    (void)y;
    (void)z;
}

void NNS_G3dGlbSetBaseScale(const VecFx32 *scale) {
    (void)scale;
}

void NNS_G3dGlbSetBaseTrans(const VecFx32 *trans) {
    (void)trans;
}