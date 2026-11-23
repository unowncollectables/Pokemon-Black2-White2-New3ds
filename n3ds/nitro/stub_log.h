/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include "log.h"

#define PBW2_STUB_LOG(tag, fmt, ...) \
    do { \
        static int s_logged; \
        if (!s_logged) { \
            s_logged = 1; \
            Log_Line(tag, "[stub] first call " fmt, ##__VA_ARGS__); \
        } \
    } while (0)
