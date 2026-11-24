/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "nnsys/g3d.h"

#if defined(PBW2_LINK_GAME)

#include "nitro/gx.h"
#include "stub_log.h"

/* The animation library references this state during drawing. The host
 * still stubs drawing, so there is no active render state. */
NNSG3dRS *NNS_G3dRS;

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

/* Material colors: real field writes into the host's NNS_G3dGlb, same as
 * the NitroSystem code (glbstate.c), so the state is there when the 3D
 * replay reads it. */
void NNS_G3dGlbMaterialColorDiffAmb(GXRgb diffuse, GXRgb ambient, BOOL isSetVtxColor) {
    NNS_G3dGlb.prmMatColor0 = GX_PACK_DIFFAMB_PARAM(diffuse, ambient, isSetVtxColor);
}

void NNS_G3dGlbMaterialColorSpecEmi(GXRgb specular, GXRgb emission, BOOL isShininess) {
    NNS_G3dGlb.prmMatColor1 = GX_PACK_SPECEMI_PARAM(specular, emission, isShininess);
}

/* Inverse view matrix: same lazy logic as glbstate.c. MTX_Inverse43 is
 * still a host stub (returns -1, writes nothing), so invCameraMtx stays
 * whatever the camera matrix holds — zero until the 3D replay tracks the
 * camera. Callers (fld_wipe_3dobj) only copy it into a transform. */
const MtxFx43 *NNS_G3dGlbGetInvV(void) {
    if (!(NNS_G3dGlb.flag & NNS_G3D_GLB_FLAG_INVCAMERA_UPTODATE)) {
        MTX_Inverse43(&NNS_G3dGlb.cameraMtx, &NNS_G3dGlb.invCameraMtx);
        NNS_G3dGlb.flag |= NNS_G3D_GLB_FLAG_INVCAMERA_UPTODATE;
    }
    return &NNS_G3dGlb.invCameraMtx;
}

/* Screen <-> world: the real math needs the tracked camera/projection
 * matrices, which only the 3D replay will provide. Until then, safe
 * defaults: WorldPosToScrPos reports screen (128, 0) — top-centre, so
 * talk windows land at the bottom (ActorMsgWin_CreateAtAutoPos branches
 * on y) — and ScrPosToWorldLine reports the origin line. Both log so the
 * gap is visible in the stub log. */
int NNS_G3dWorldPosToScrPos(const VecFx32 *world, int *x, int *y) {
    (void)world;
    if (x != NULL) {
        *x = 128;
    }
    if (y != NULL) {
        *y = 0;
    }
    PBW2_STUB_LOG("G3D", "WorldPosToScrPos: no tracked camera yet (3D replay)");
    return 0;
}

int NNS_G3dScrPosToWorldLine(int px, int py, VecFx32 *nearPos, VecFx32 *farPos) {
    (void)px;
    (void)py;
    if (nearPos != NULL) {
        nearPos->x = 0;
        nearPos->y = 0;
        nearPos->z = 0;
    }
    if (farPos != NULL) {
        farPos->x = 0;
        farPos->y = 0;
        farPos->z = 0;
    }
    PBW2_STUB_LOG("G3D", "ScrPosToWorldLine: no tracked camera yet (3D replay)");
    return 0;
}
