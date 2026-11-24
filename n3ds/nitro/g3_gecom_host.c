/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Host NNS geometry buffer for packed G3 commands. */
#include "nnsys/g3d.h"

#if defined(PBW2_LINK_GAME)

#include "g3_intercept.h"
#include "nitro/gx.h"
#include "nitro/mi.h"

/* Only buffers supplied by the game are returned to its DTCM allocator. */
static NNSG3dGeBuffer *s_ge_active;

BOOL NNS_G3dGeIsBufferExist(void) {
    return s_ge_active != NULL;
}

void NNS_G3dGeSetBuffer(NNSG3dGeBuffer *buf) {
    if (s_ge_active == NULL && buf != NULL) {
        buf->idx = 0;
        s_ge_active = buf;
    }
}

NNSG3dGeBuffer *NNS_G3dGeReleaseBuffer(void) {
    NNSG3dGeBuffer *buf = s_ge_active;

    NNS_G3dGeFlushBuffer();
    s_ge_active = NULL;
    return buf;
}

void NNS_G3dGeFlushBuffer(void) {
    if (s_ge_active != NULL && s_ge_active->idx != 0) {
        PbW2_G3RecordFifo(s_ge_active->data, s_ge_active->idx);
        s_ge_active->idx = 0;
    }
}

void NNS_G3dGeBufferOP_N(u32 op, const u32 *args, u32 num) {
    if (s_ge_active != NULL) {
        if (s_ge_active->idx + 1u + num <= NNS_G3D_SIZE_COMBUFFER) {
            s_ge_active->data[s_ge_active->idx++] = op;
            if (num != 0 && args != NULL) {
                MIi_CpuCopyFast(args, &s_ge_active->data[s_ge_active->idx], num * sizeof(u32));
                s_ge_active->idx += num;
            }
            return;
        }
        NNS_G3dGeFlushBuffer();
    }
    PbW2_G3RecordFifo(&op, 1);
    PbW2_G3RecordFifo(args, num);
}

void NNS_G3dGeSendDL(const void *list, u32 size) {
    const u32 *dl = (const u32 *)list;
    u32 words;

    if (list == NULL || size < 4u) {
        return;
    }
    words = size / 4u;
    if (words < 64u) {
        NNS_G3dGeBufferOP_N(dl[0], dl + 1, words - 1u);
        return;
    }
    NNS_G3dGeFlushBuffer();
    PbW2_G3RecordFifo(dl, words);
}

void NNS_G3dInit(void) {
    G3X_Init();
    NNS_G3dGlbInit();
    G3X_SetFifoIntrCond(GX_FIFOINTR_COND_EMPTY);
}

#endif
