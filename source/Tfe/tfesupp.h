/*
 * This file is a consolidation of functions required for tfe
 * emulation taken from the following files
 *
 * lib.h   - Library functions.
 * util.h  - Miscellaneous utility functions.
 * crc32.h
 *
 * Written by
 *  Andreas Boose <viceteam@t-online.de>
 *  Ettore Perazzoli <ettore@comm2000.it>
 *  Manfred Spraul <manfreds@colorfullife.com>
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



#ifndef _TFESUPP_H
#define _TFESUPP_H

extern unsigned long crc32_buf(const char *buffer, unsigned int len);

#endif
