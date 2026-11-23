/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "nitro/mi.h"

#include <string.h>

void *__memcpy(void *dst, const void *src, size_t n) {
    return memcpy(dst, src, n);
}