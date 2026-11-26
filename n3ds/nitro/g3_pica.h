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

enum {
    PICA_TEX_ALPHA_OPAQUE = 1u << 0,
    PICA_TEX_ALPHA_FRACTIONAL = 1u << 1,
};

bool PbW2_PicaInit(void);
void PbW2_PicaExit(void);
void PbW2_PicaSetOptions(bool enabled, bool skipEdges);
bool PbW2_PicaSkipEdges(void);
bool PbW2_PicaEnabled(void);
bool PbW2_PicaBegin(unsigned alphaRef);
int PbW2_PicaUpload(const PicaTexture *texture);
unsigned PbW2_PicaTextureAlpha(int texture);
bool PbW2_PicaTriangle(const PicaVertex vertex[3], int texture, PicaAlphaTest test,
                      bool decal, unsigned translucentId, bool writeDepth);
void PbW2_PicaSetFrameWidth(unsigned width);
bool PbW2_PicaFinish(uint32_t clearColor, float clearDepth, uint16_t *result);

#endif
