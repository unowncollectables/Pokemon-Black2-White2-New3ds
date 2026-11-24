/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "nitro/gx.h"
#include "stub_log.h"

#if defined(PBW2_LINK_GAME)
#include "g3_intercept.h"
#endif

void GX_SendFifo48B(const void *src, volatile void *dest) {
    (void)dest;
#if defined(PBW2_LINK_GAME)
    PbW2_G3RecordFifo(src, 12u);
#endif
    (void)src;
}

void GX_SendFifo64B(const void *src, volatile void *dest) {
    (void)dest;
#if defined(PBW2_LINK_GAME)
    PbW2_G3RecordFifo(src, 16u);
#endif
    (void)src;
}

/* GXx_GetMasterBrightness_: pure function of the master-brightness
 * register, same as gx.c (gx.c itself is excluded: the host owns GX).
 * The register is the host's shadowed 0x0400006C. */
int GXx_GetMasterBrightness_(vu16 *reg) {
    u16 mode = *reg & REG_GX_MASTER_BRIGHT_E_MOD_MASK;
    if (mode == 0) {
        return 0;
    } else if (mode == REG_GX_MASTER_BRIGHT_MODE_UP) {
        return *reg & REG_GX_MASTER_BRIGHT_E_VALUE_MASK;
    } else if (mode == REG_GX_MASTER_BRIGHT_MODE_DOWN) {
        return -(*reg & REG_GX_MASTER_BRIGHT_E_VALUE_MASK);
    }
    return 0;
}
