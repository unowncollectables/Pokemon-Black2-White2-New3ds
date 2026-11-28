/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef PBW2_G3_PICA_H
#define PBW2_G3_PICA_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    float x, y, z, w;
    float s, t;
    float r, g, b, a;
} PicaVertex;

typedef uint32_t (*PicaTexel)(const void *source, unsigned x, unsigned y);

typedef struct {
    uint32_t param, palette, banks, revision[7];
} PicaTextureKey;

typedef struct {
    unsigned width, height, mode;
    const void *source;
    PicaTexel texel;
    PicaTextureKey key;
} PicaTexture;

typedef enum {
    PICA_ALPHA_VISIBLE,
    PICA_ALPHA_OPAQUE,
} PicaAlphaTest;

enum { PICA_TEXTURE_SLOTS = 256 };

enum {
    PICA_TEX_ALPHA_OPAQUE = 1u << 0,
    PICA_TEX_ALPHA_FRACTIONAL = 1u << 1,
};

bool PbW2_PicaInit(void);
void PbW2_PicaExit(void);
void PbW2_PicaSetOptions(bool enabled, bool skipEdges);
/* Sets the async-readback toggle (Build 24) independently, so the
 * existing two-argument callers (settings, fidelity tests) are
 * untouched. Logged on change. */
void PbW2_PicaSetAsync(bool async);
bool PbW2_PicaSkipEdges(void);
bool PbW2_PicaEnabled(void);
bool PbW2_PicaBegin(unsigned alphaRef);
void PbW2_PicaInvalidate(void);
int PbW2_PicaUpload(const PicaTexture *texture);
unsigned PbW2_PicaTextureAlpha(int texture);
bool PbW2_PicaTriangle(const PicaVertex vertex[3], int texture, PicaAlphaTest test,
                      bool decal, unsigned translucentId, bool writeDepth);
void PbW2_PicaSetFrameWidth(unsigned width, uint16_t *native);
bool PbW2_PicaFinish(uint32_t clearColor, float clearDepth, uint16_t *result);
/* Async readback (Build 24, SoulSilver's gpu3d_async pattern): with the
 * gpu_async toggle on, PbW2_PicaFinish only queues the draw + display
 * transfer and returns; the fence, cache invalidate and CPU convert for
 * that frame run in PbW2_PicaResolvePending, which the raster calls at
 * the top of the NEXT publish — before any texture upload or eviction
 * that could disturb the in-flight frame. The converted centre image
 * is then available via PbW2_PicaResolvedCentre (the native image, if
 * any, was converted into the native buffer captured at queue time).
 * The 3D layer therefore shows one publish late. In sync mode no frame
 * is ever pending and both functions are inert. */
bool PbW2_PicaResolvePending(void);
const uint16_t *PbW2_PicaResolvedCentre(void);
bool PbW2_PicaAsyncPending(void);
/* Forces the next PbW2_PicaFinish to run synchronously (DS capture
 * frames must read back the frame they capture, not the previous one).
 * Consumed by that Finish call. */
void PbW2_PicaSetForceSync(bool force);

#endif
