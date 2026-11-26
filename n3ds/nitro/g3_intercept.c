/* SPDX-License-Identifier: GPL-3.0-or-later */
/* DS GXFIFO decoder and geometry engine matrix state.
 * Packet shortcut layout adapted from unowncollectables/Pokemon-HeartGold-SoulSilver-New3ds
 * (soulsilver3ds/platform/g3packet.c, GPL-3.0). */
#include "g3_intercept.h"

#if defined(PBW2_LINK_GAME)

#include "g3_raster.h"
#include "nitro/gx.h"
#include "types.h"

#include <string.h>

/* Packed commands are consumed in FIFO order, including commands split across writes. */
static const u8 s_arguments[0x73] = {
    [0x10] = 1, [0x12] = 1, [0x13] = 1, [0x14] = 1,
    [0x16] = 16, [0x17] = 12, [0x18] = 16, [0x19] = 12, [0x1a] = 9, [0x1b] = 3, [0x1c] = 3,
    [0x20] = 1, [0x21] = 1, [0x22] = 1, [0x23] = 2, [0x24] = 1, [0x25] = 1,
    [0x26] = 1, [0x27] = 1, [0x28] = 1, [0x29] = 1, [0x2a] = 1, [0x2b] = 1,
    [0x30] = 1, [0x31] = 1, [0x32] = 1, [0x33] = 1, [0x34] = 32, [0x40] = 1,
    [0x50] = 1, [0x60] = 1, [0x70] = 3, [0x71] = 2, [0x72] = 1,
};

static struct {
    u32 opcodes;
    unsigned remaining;
    unsigned count;
    u32 args[32];
} s_packet;
static MtxFx44 s_matrix[4];
static MtxFx44 s_positionStack[32], s_vectorStack[32];
static MtxFx44 s_projectionStack, s_textureStack;
static unsigned s_mode, s_depth;
static MtxFx44 s_clip;
static int s_clipDirty;

void PbW2_G3InterceptInit(void) {
    memset(&s_packet, 0, sizeof s_packet);
    for (unsigned i = 0; i < 4; ++i) {
        MTX_Identity44_(&s_matrix[i]);
    }
    for (unsigned i = 0; i < 32; ++i) {
        s_positionStack[i] = s_matrix[GX_MTXMODE_POSITION];
        s_vectorStack[i] = s_matrix[GX_MTXMODE_POSITION_VECTOR];
    }
    s_projectionStack = s_matrix[GX_MTXMODE_PROJECTION];
    s_textureStack = s_matrix[GX_MTXMODE_TEXTURE];
    s_mode = GX_MTXMODE_PROJECTION;
    s_depth = 0;
    s_clipDirty = 1;
    PbW2_G3RasterReset();
}

static void store_matrix(unsigned index, int restore) {
    unsigned mode = s_mode;
    if (restore) s_clipDirty = 1;
    if (mode == GX_MTXMODE_PROJECTION || mode == GX_MTXMODE_TEXTURE) {
        MtxFx44 *saved = mode == GX_MTXMODE_PROJECTION ? &s_projectionStack : &s_textureStack;
        if (restore) s_matrix[mode] = *saved;
        else *saved = s_matrix[mode];
    } else if (restore) {
        s_matrix[GX_MTXMODE_POSITION] = s_positionStack[index];
        s_matrix[GX_MTXMODE_POSITION_VECTOR] = s_vectorStack[index];
    } else {
        s_positionStack[index] = s_matrix[GX_MTXMODE_POSITION];
        s_vectorStack[index] = s_matrix[GX_MTXMODE_POSITION_VECTOR];
    }
}

static void update_clip_matrix(void) {
    if (s_clipDirty) {
        G3X_GetClipMtx(&s_clip);
        s_clipDirty = 0;
    }
}

static void box_test(const u32 *params) {
    update_clip_matrix();
    s32 origin[3] = {(s16)params[0], (s16)(params[0] >> 16), (s16)params[1]};
    s32 extent[3] = {(s16)(params[1] >> 16), (s16)params[2], (s16)(params[2] >> 16)};
    unsigned outside = 0x3f;
    bool wide = PbW2_G3WideCamera();
    for (unsigned corner = 0; corner < 8 && outside; ++corner) {
        s64 clip[4];
        for (unsigned column = 0; column < 4; ++column) {
            clip[column] = (s64)FX32_ONE * s_clip.m[3][column];
            for (unsigned axis = 0; axis < 3; ++axis) {
                s32 coordinate = origin[axis];
                if (corner & (1u << axis)) coordinate += extent[axis];
                clip[column] += (s64)coordinate * s_clip.m[axis][column];
            }
        }
        for (unsigned axis = 0; axis < 3; ++axis) {
            s64 limit = wide && axis == 0 ? clip[3] * 5 / 4 : clip[3];
            if (clip[axis] >= -limit) outside &= ~(1u << (2 * axis));
            if (clip[axis] <= limit) outside &= ~(2u << (2 * axis));
        }
    }
    /* Keep intersecting boxes for the rasterizer to clip per primitive. */
    reg_G3X_GXSTAT &= ~(REG_G3X_GXSTAT_TB_MASK | REG_G3X_GXSTAT_TR_MASK);
    if (!outside) reg_G3X_GXSTAT |= REG_G3X_GXSTAT_TR_MASK;
}

void PbW2_G3Command(u32 opcode, const u32 *p) {
    MtxFx44 matrix;
    unsigned mode = s_mode == GX_MTXMODE_POSITION_VECTOR ? GX_MTXMODE_POSITION : s_mode;
    int load = opcode <= G3OP_MTX_LOAD_4x3;
    switch (opcode) {
    case G3OP_MTX_MODE:
        s_mode = p[0] & 3;
        return;
    case G3OP_MTX_PUSH:
        store_matrix(s_depth, 0);
        if (mode == GX_MTXMODE_POSITION) s_depth = (s_depth + 1) & 31;
        return;
    case G3OP_MTX_POP:
        if (mode == GX_MTXMODE_POSITION) {
            int offset = (int)(p[0] & 63);
            if (offset >= 32) offset -= 64;
            s_depth = (s_depth - offset) & 31;
        }
        store_matrix(s_depth, 1);
        return;
    case G3OP_MTX_STORE:
    case G3OP_MTX_RESTORE:
        store_matrix(p[0] & 31, opcode == G3OP_MTX_RESTORE);
        return;
    case G3OP_MTX_IDENTITY:
        MTX_Identity44_(&matrix);
        break;
    case G3OP_MTX_LOAD_4x4:
    case G3OP_MTX_MULT_4x4:
        memcpy(&matrix, p, sizeof matrix);
        break;
    case G3OP_MTX_LOAD_4x3:
    case G3OP_MTX_MULT_4x3:
    case G3OP_MTX_MULT_3x3: {
        unsigned rows = opcode == G3OP_MTX_MULT_3x3 ? 3 : 4;
        MTX_Identity44_(&matrix);
        for (unsigned i = 0; i < rows; ++i) {
            memcpy(matrix.m[i], p + 3 * i, 3 * sizeof(u32));
        }
        break;
    }
    case G3OP_MTX_SCALE:
    case G3OP_MTX_TRANS:
        MTX_Identity44_(&matrix);
        for (unsigned i = 0; i < 3; ++i) {
            matrix.m[opcode == G3OP_MTX_SCALE ? i : 3][i] = (fx32)p[i];
        }
        break;
    case G3OP_BOX_TEST:
        box_test(p);
        return;
    default:
        if (opcode >= G3OP_VTX_16 && opcode <= 0x28) update_clip_matrix();
        PbW2_G3RasterCommand(opcode, p, &s_clip, &s_matrix[GX_MTXMODE_TEXTURE],
                            &s_matrix[GX_MTXMODE_POSITION_VECTOR]);
        return;
    }
    if (mode != GX_MTXMODE_TEXTURE) s_clipDirty = 1;
    if (load) s_matrix[mode] = matrix;
    else MTX_Concat44(&matrix, &s_matrix[mode], &s_matrix[mode]);

    if (s_mode == GX_MTXMODE_POSITION_VECTOR && opcode != G3OP_MTX_SCALE && opcode != G3OP_MTX_TRANS) {
        for (unsigned i = 0; i < 3; ++i) {
            matrix.m[3][i] = matrix.m[i][3] = 0;
        }
        matrix.m[3][3] = FX32_ONE;
        MtxFx44 *vector = &s_matrix[GX_MTXMODE_POSITION_VECTOR];
        if (load) *vector = matrix;
        else MTX_Concat44(&matrix, vector, vector);
    }
}

void PbW2_G3FifoWord(u32 word) {
    if (s_packet.remaining == 0) {
        s_packet.opcodes = word;
        s_packet.remaining = 4;
    } else {
        s_packet.args[s_packet.count++] = word;
    }

    while (s_packet.remaining) {
        u32 op = s_packet.opcodes & 255;
        unsigned needed = op < sizeof s_arguments ? s_arguments[op] : 0;
        if (s_packet.count < needed) return;
        PbW2_G3Command(op, s_packet.args);
        s_packet.opcodes >>= 8;
        --s_packet.remaining;
        s_packet.count = 0;
    }
}

void PbW2_G3RecordFifo(const void *src, u32 words) {
    const u32 *p = src;
    for (u32 i = 0; p && i < words; ++i) {
        PbW2_G3FifoWord(p[i]);
    }
}

int G3X_GetClipMtx(MtxFx44 *dest) {
    MTX_Concat44(&s_matrix[GX_MTXMODE_POSITION], &s_matrix[GX_MTXMODE_PROJECTION], dest);
    return 0;
}

int G3X_GetVectorMtx(MtxFx33 *dest) {
    for (unsigned i = 0; i < 3; ++i) {
        memcpy(dest->m[i], s_matrix[GX_MTXMODE_POSITION_VECTOR].m[i], sizeof dest->m[i]);
    }
    return 0;
}

void PbW2_G3Compose(int is_main, u16 *fb_rgb565) {
    if (is_main && fb_rgb565 != NULL) PbW2_G3RasterComposite(fb_rgb565, NULL, NULL, NULL, NULL, 0);
}

#endif
