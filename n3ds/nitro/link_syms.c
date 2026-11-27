/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "nitro/fs.h"
#include "nitro/math.h"
#include "nitro/os.h"

u32 __OVERLAY_COUNT[1] = {344u};

FSOverlayDigest SDK_OVERLAY_DIGEST[1];
FSOverlayDigest SDK_OVERLAY_DIGEST_END[1];

u8 SDK_IRQ_STACKSIZE[0x800];

u8 LTD_AUTOLOAD_0_TEXT_START[1];
u8 LTD_AUTOLOAD_0_BSS_END[1];

static u32 rotate_left(u32 word, unsigned bits) {
    return (word << bits) | (word >> (32 - bits));
}

void sha1_block(MATHSHA1Context *context) {
    static const u32 constants[] = {0x5a827999, 0x6ed9eba1, 0x8f1bbcdc, 0xca62c1d6};
    u32 schedule[16];
    for (unsigned i = 0; i < 16; ++i) schedule[i] = __builtin_bswap32(context->block.words[i]);

    u32 a = context->state[0], b = context->state[1], c = context->state[2];
    u32 d = context->state[3], e = context->state[4];
    for (unsigned i = 0; i < 80; ++i) {
        unsigned slot = i & 15;
        if (i >= 16) {
            schedule[slot] = rotate_left(schedule[(i - 3) & 15] ^ schedule[(i - 8) & 15] ^
                                         schedule[(i - 14) & 15] ^ schedule[slot], 1);
        }
        u32 combine;
        if (i < 20) combine = (b & c) | (~b & d);
        else if (i < 40 || i >= 60) combine = b ^ c ^ d;
        else combine = (b & c) | (b & d) | (c & d);

        u32 next = rotate_left(a, 5) + combine + e + constants[i / 20] + schedule[slot];
        e = d;
        d = c;
        c = rotate_left(b, 30);
        b = a;
        a = next;
    }
    context->state[0] += a;
    context->state[1] += b;
    context->state[2] += c;
    context->state[3] += d;
    context->state[4] += e;
}