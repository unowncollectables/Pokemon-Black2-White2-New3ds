/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "nitro/fx.h"
#include "stub_log.h"

#define PBW2_MTX_NOOP(name, ...) \
    void name(__VA_ARGS__) { PBW2_STUB_LOG("MTX", #name); }

PBW2_MTX_NOOP(MTX_Identity33_, MtxFx33 *mtx)
PBW2_MTX_NOOP(MTX_Identity43_, MtxFx43 *mtx)
PBW2_MTX_NOOP(MTX_Identity44_, MtxFx44 *mtx)
PBW2_MTX_NOOP(MTX_Identity22_, MtxFx22 *mtx)
PBW2_MTX_NOOP(MTX_RotX33_, MtxFx33 *mtx, fx32 sin, fx32 cos)
PBW2_MTX_NOOP(MTX_RotY33_, MtxFx33 *mtx, fx32 sin, fx32 cos)
PBW2_MTX_NOOP(MTX_RotZ33_, MtxFx33 *mtx, fx32 sin, fx32 cos)
PBW2_MTX_NOOP(MTX_RotY43_, MtxFx43 *mtx, fx32 sin, fx32 cos)
PBW2_MTX_NOOP(MTX_RotAxis43, MtxFx43 *mtx, const VecFx32 *axis, fx32 sin, fx32 cos)
PBW2_MTX_NOOP(MTX_Rot22_, MtxFx22 *mtx, fx32 sin, fx32 cos)
PBW2_MTX_NOOP(MTX_Scale22_, MtxFx22 *mtx, fx32 x, fx32 y)
PBW2_MTX_NOOP(MTX_Scale33_, MtxFx33 *mtx, fx32 x, fx32 y, fx32 z)
PBW2_MTX_NOOP(MTX_Scale43_, MtxFx43 *mtx, fx32 x, fx32 y, fx32 z)
PBW2_MTX_NOOP(MTX_ScaleApply22, const MtxFx22 *src, MtxFx22 *dest, fx32 x, fx32 y)
PBW2_MTX_NOOP(MTX_ScaleApply43, const MtxFx43 *src, MtxFx43 *dest, fx32 x, fx32 y, fx32 z)
PBW2_MTX_NOOP(MTX_Concat33, const MtxFx33 *a, const MtxFx33 *b, MtxFx33 *ab)
PBW2_MTX_NOOP(MTX_Concat43, const MtxFx43 *a, const MtxFx43 *b, MtxFx43 *ab)
PBW2_MTX_NOOP(MTX_Concat44, const MtxFx44 *a, const MtxFx44 *b, MtxFx44 *ab)
PBW2_MTX_NOOP(MTX_MultVec33, const VecFx32 *vec, const MtxFx33 *mtx, VecFx32 *dest)
PBW2_MTX_NOOP(MTX_MultVec43, const VecFx32 *vec, const MtxFx43 *mtx, VecFx32 *dest)

int MTX_Inverse33(const MtxFx33 *mtx, MtxFx33 *inv) {
    (void)mtx;
    (void)inv;
    return -1;
}

int MTX_Inverse43(const MtxFx43 *mtx, MtxFx43 *inv) {
    (void)mtx;
    (void)inv;
    return -1;
}

PBW2_MTX_NOOP(MTX_LookAt, const VecFx32 *camPos, const VecFx32 *camUp, const VecFx32 *target, MtxFx43 *mtx)
PBW2_MTX_NOOP(MTX_PerspectiveW, fx32 fovySin, fx32 fovyCos, fx32 aspect, fx32 n, fx32 f, fx32 scaleW, MtxFx44 *mtx)
PBW2_MTX_NOOP(MTX_OrthoW, fx32 t, fx32 b, fx32 l, fx32 r, fx32 n, fx32 f, fx32 scaleW, MtxFx44 *mtx)
PBW2_MTX_NOOP(MTX_FrustumW, fx32 t, fx32 b, fx32 l, fx32 r, fx32 n, fx32 f, fx32 scaleW, MtxFx44 *mtx)
PBW2_MTX_NOOP(MTX_Copy33To44_, const MtxFx33 *src, MtxFx44 *dst)
PBW2_MTX_NOOP(MTX_Copy43To44_, const MtxFx43 *src, MtxFx44 *dst)
PBW2_MTX_NOOP(MTX_Copy44To43_, const MtxFx44 *src, MtxFx43 *dst)

fx32 FX_Div(fx32 numer, fx32 denom) {
    return denom ? (numer / denom) : 0;
}

fx32 FX_Inv(fx32 denom) {
    return denom ? (1 << 12) / denom : 0;
}

fx32 FX_Sqrt(fx32 x) {
    (void)x;
    return 0;
}

u16 FX_Atan2Idx(fx32 y, fx32 x) {
    (void)y;
    (void)x;
    return 0;
}