/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef PBW2_TITLE_PIC_H
#define PBW2_TITLE_PIC_H

#include <stdbool.h>
#include <stdint.h>

/* Reads a/0/2/6 from the staged NitroFS. The top picture is the version logo
 * (members 0, 1, 2) over the title background (member 3, same tiles). The
 * bottom picture is the lower title screen (members 9, 10, 8). Both are
 * 256 by 192 RGB565. Returns false when the logo does not decode. */
bool TitlePic_Load(void);
bool TitlePic_FromArchive(const uint8_t *narc, uint32_t length);
bool TitlePic_Ready(void);
const char *TitlePic_Note(void);
const uint16_t *TitlePic_Top(void);
const uint16_t *TitlePic_Bottom(void);

#endif
