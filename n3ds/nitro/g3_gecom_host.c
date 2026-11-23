/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Host NNS geometry buffer; records G3 packets for Citro3D replay (HGSS g3packet pattern). */
#include "nnsys/g3d.h"

#if defined(PBW2_LINK_GAME)

#include "g3_intercept.h"
#include "nitro/gx.h"
#include "nitro/mi.h"

#include <string.h>

#define PBW2_G3_COMBUF_WORDS 512u

static NNSG3dGeBuffer s_ge_buf;
static NNSG3dGeBuffer *s_ge_active;
static volatile u8 s_dma_async;

static void ge_record_words(const u32 *words, u32 count) {
    if (words != NULL && count != 0) {
        PbW2_G3RecordFifo(words, count);
    }
}

static void ge_flush_to_fifo(const u32 *data, u32 words) {
    ge_record_words(data, words);
}

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
        ge_record_words(s_ge_active->data, s_ge_active->idx);
        ge_flush_to_fifo(s_ge_active->data, s_ge_active->idx);
        s_ge_active->idx = 0;
    }
    s_dma_async = 0;
}

void NNS_G3dGeBufferOP_N(u32 op, const u32 *args, u32 num) {
    u32 packet[66];
    u32 n;

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
    n = 1u + num;
    if (n <= 66u) {
        packet[0] = op;
        if (num != 0 && args != NULL) {
            memcpy(packet + 1, args, num * sizeof(u32));
        }
        ge_record_words(packet, n);
    }
    if (num != 0 && args != NULL) {
        ge_record_words(args, num);
    }
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
    ge_record_words(dl, words);
    ge_flush_to_fifo(dl, words);
}

void PbW2_G3GeProvideDefaultBuffer(void) {
    if (s_ge_active == NULL) {
        NNS_G3dGeSetBuffer(&s_ge_buf);
    }
}

void NNS_G3dInit(void) {
    PbW2_G3GeProvideDefaultBuffer();
}

#endif