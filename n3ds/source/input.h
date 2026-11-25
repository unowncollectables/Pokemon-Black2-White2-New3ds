/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef PBW2_INPUT_H
#define PBW2_INPUT_H

#include <stdbool.h>
#include <stdint.h>

bool Input_Init(void);
void Input_Exit(void);
void Input_Scan(void);
uint32_t Input_GameHeld(void);
void Input_Draw(void);
/* START leaves the shell. It does not reset or relaunch the game. */
bool Input_Quit(void);

#endif
