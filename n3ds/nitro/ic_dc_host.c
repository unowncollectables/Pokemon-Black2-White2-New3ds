/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "nitro/os.h"

void IC_InvalidateRange(void *addr, u32 size) {
    (void)addr;
    (void)size;
}

void IC_InvalidateAll(void) {
}

void DC_WaitWriteBufferEmpty(void) {
}

void DC_FlushAll(void) {
}