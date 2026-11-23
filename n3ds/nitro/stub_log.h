/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include <stdio.h>

#define PBW2_STUB_LOG(tag, fmt, ...) \
    do { static int s_logged; if (!s_logged) { s_logged = 1; printf("[stub:%s] first call " fmt "\n", (tag), ##__VA_ARGS__); fflush(stdout); } } while (0)