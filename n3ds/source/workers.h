/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef PBW2_WORKERS_H
#define PBW2_WORKERS_H

#include <3ds.h>
#include <stdbool.h>

typedef struct Worker Worker;

/* Starts a worker. On New 3DS, audio and IO use core 2. If that core is
 * refused, the worker is created on core 0 and the log says so. */
bool Workers_Start(const char *name, ThreadFunc fn, int prio, int core, Worker **out);
bool Worker_Run(Worker *worker);
void Workers_Shutdown(void);
int Workers_Count(void);

#endif
