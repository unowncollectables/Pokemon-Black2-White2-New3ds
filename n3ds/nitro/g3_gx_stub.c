/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "nitro/gx.h"
#include "stub_log.h"

void GX_SendFifo48B(const void *src, volatile void *dest) {
    (void)src;
    (void)dest;
}

void GX_SendFifo64B(const void *src, volatile void *dest) {
    (void)src;
    (void)dest;
}

void GX_BeginLoadTex(void) {
}

void GX_LoadTex(const void *src, u32 dest3dOffset, u32 size) {
    (void)src;
    (void)dest3dOffset;
    (void)size;
}

void GX_EndLoadTex(void) {
}

void GX_BeginLoadTexPltt(void) {
}

void GX_LoadTexPltt(const void *src, u32 dest3dOffset, u32 size) {
    (void)src;
    (void)dest3dOffset;
    (void)size;
}

void GX_EndLoadTexPltt(void) {
}

void GX_BeginLoadClearImage(void) {
}

void GX_LoadClearImageColor(const void *src, u32 size) {
    (void)src;
    (void)size;
}

void GX_LoadClearImageDepth(const void *src, u32 size) {
    (void)src;
    (void)size;
}

void GX_EndLoadClearImage(void) {
}

void G3X_Init(void) {
    PBW2_STUB_LOG("G3X", "Init");
}

void G3X_Reset(void) {
}

void G3X_InitMtxStack(void) {
}

void G3X_SetClearColor(GXRgb color, int alpha, int depth, int polygonId, BOOL fog) {
    (void)color;
    (void)alpha;
    (void)depth;
    (void)polygonId;
    (void)fog;
}

void G3X_SetDisp1DotDepth(fx32 depth) {
    (void)depth;
}

int G3X_GetBoxTestResult(s32 *result) {
    if (result) {
        *result = 0;
    }
    return 0;
}

int G3X_GetClipMtx(MtxFx44 *dest) {
    (void)dest;
    return 0;
}

int G3X_GetVectorMtx(MtxFx33 *dest) {
    (void)dest;
    return 0;
}