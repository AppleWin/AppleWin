/*
 * uilib.h - Common UI elements for the Windows user interface.
 *
 * Written by
 *  Ettore Perazzoli <ettore@comm2000.it>
 *  Andreas Boose <viceteam@t-online.de>
 *  Manfred Spraul <manfreds@colorfullife.com>
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

#ifndef _UILIB_H
#define _UILIB_H

typedef struct {
    unsigned int idc;
    int element_type;
} uilib_dialog_group;

extern void uilib_get_group_extent(HWND hwnd, uilib_dialog_group *group, int *xsize, int *ysize);
extern void uilib_move_group(HWND hwnd, uilib_dialog_group *group, int xpos);
extern void uilib_adjust_group_width(HWND hwnd, uilib_dialog_group *group);

typedef struct {
    unsigned int idc;
    unsigned int ids;
    int element_type;
} uilib_localize_dialog_param;

#endif

