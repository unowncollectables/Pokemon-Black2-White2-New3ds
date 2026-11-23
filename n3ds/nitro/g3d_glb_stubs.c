/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "nnsys/g3d.h"

#if defined(PBW2_LINK_GAME)

#include "nitro/gx.h"
#include "stub_log.h"

void NNS_G3dGlbInit(void) {
}

void NNS_G3dGlbFlushP(void) {
    u32 mode = GX_MTXMODE_PROJECTION;
    NNS_G3dGeBufferOP_N(G3OP_MTX_MODE, &mode, 1);
}

void NNS_G3dGlbFlushVP(void) {
    u32 mode = GX_MTXMODE_POSITION;
    NNS_G3dGeBufferOP_N(G3OP_MTX_MODE, &mode, 1);
}

void NNS_G3dGlbFlushWVP(void) {
    NNS_G3dGlbFlushVP();
}

#else

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

#endif

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