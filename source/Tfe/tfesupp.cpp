/*
 * This file is a consolidation of functions required for tfe
 * emulation taken from the following files
 *
 * lib.c   - Library functions.
 * util.c  - Miscellaneous utility functions.
 * crc32.c
 *
 * Written by
 *  Andreas Boose <viceteam@t-online.de>
 *  Ettore Perazzoli <ettore@comm2000.it>
 *  Andreas Matthies <andreas.matthies@gmx.net>
 *  Tibor Biczo <crown@mail.matav.hu>
 *  Spiro Trikaliotis <Spiro.Trikaliotis@gmx.de>*
 *
 * This file is part of VICE, the Versatile Commodore Emulator.
 * See README for copyright notice.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 *  02111-1307  USA.
 *
 */

#include "tfesupp.h"

#define CRC32_POLY  0xedb88320
static unsigned long crc32_table[256];
static int crc32_is_initialized = 0;

// crc32 Stuff

unsigned long crc32_buf(const char *buffer, unsigned int len)
{
    int i, j;
    unsigned long crc, c;
    const char *p;

    if (!crc32_is_initialized) {
        for (i = 0; i < 256; i++) {
            c = (unsigned long) i;
            for (j = 0; j < 8; j++)
                c = c & 1 ? CRC32_POLY ^ (c >> 1) : c >> 1;
            crc32_table[i] = c;
        }
        crc32_is_initialized = 1;
    }

    crc = 0xffffffff;
    for (p = buffer; len > 0; ++p, --len)
        crc = (crc >> 8) ^ crc32_table[(crc ^ *p) & 0xff];
    
    return ~crc;
}
