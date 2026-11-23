/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef PBW2_LOG_H
#define PBW2_LOG_H

#include <stdbool.h>

/* True when log.txt was opened on the card. */
bool Log_Init(void);
void Log_Close(void);
void Log_Line(const char *tag, const char *fmt, ...);

#endif
