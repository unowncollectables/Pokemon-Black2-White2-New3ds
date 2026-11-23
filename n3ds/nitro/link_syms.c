/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "nitro/fs.h"
#include "nitro/os.h"

u32 __OVERLAY_COUNT[1] = {344u};

FSOverlayDigest SDK_OVERLAY_DIGEST[1];
FSOverlayDigest SDK_OVERLAY_DIGEST_END[1];

u8 SDK_IRQ_STACKSIZE[0x800];

u8 LTD_AUTOLOAD_0_TEXT_START[1];
u8 LTD_AUTOLOAD_0_BSS_END[1];

void sha1_block(void) {
}