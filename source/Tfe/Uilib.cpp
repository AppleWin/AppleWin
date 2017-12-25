/*
 * uilib.c - Common UI elements for the Windows user interface.
 *
 * Written by
 *  Ettore Perazzoli <ettore@comm2000.it>
 *  Andreas Boose <viceteam@t-online.de>
 *  Manfred Spraul <manfreds@colorfullife.com>
 *  Andreas Matthies <andreas.matthies@gmx.net>
 *  Tibor Biczo <crown@mail.matav.hu>
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

#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <windowsx.h>
#include <tchar.h>
#include <commctrl.h>
#include <commdlg.h>
#include "uilib.h"




/* Mingw & pre VC 6 headers doesn't have this definition */
#ifndef OFN_ENABLESIZING
#define OFN_ENABLESIZING    0x00800000
#endif

void uilib_get_general_window_extents(HWND hwnd, int *xsize, int *ysize)
{
    HDC hdc;
    HFONT hFont;
    HFONT hOldFont;
    int strlen;
    char *buffer;
    SIZE  size;

    hFont = (HFONT)SendMessage(hwnd, WM_GETFONT, 0, 0);
    strlen = SendMessage(hwnd, WM_GETTEXTLENGTH, 0, 0);
	/* RGJ added cast for AppleWin */
    buffer = (char *) malloc(strlen + 1);
	if (buffer == NULL)	// TC: add null check for AppleWin
	{
		*xsize = 0;
		*ysize = 0;
		return;
	}
    GetWindowText(hwnd, buffer, strlen + 1);

    hdc = GetDC(hwnd);
    hOldFont = (HFONT)SelectObject(hdc, hFont);

    GetTextExtentPoint32(hdc, buffer, strlen, &size);

    free(buffer);

    SelectObject(hdc, hOldFont);
    ReleaseDC(hwnd, hdc);

    *xsize = size.cx;
    *ysize = size.cy;
}

void uilib_get_group_extent(HWND hwnd, uilib_dialog_group *group, int *xsize, int *ysize)
{
HWND element;
int x;
int y;

    if (xsize && ysize) {
        *xsize = 0;
        *ysize = 0;
        while (group->idc) {
            element = GetDlgItem(hwnd, group->idc);
            uilib_get_general_window_extents(element, &x, &y);
            if (group->element_type == 1) x += 20;
            if (*xsize < x) *xsize = x;
            *ysize += y;
            group++;
        }
    }
}


void uilib_move_group(HWND hwnd, uilib_dialog_group *group, int xpos)
{
HWND element;
RECT element_rect;

    while (group->idc) {
        element = GetDlgItem(hwnd, group->idc);
        GetClientRect(element, &element_rect);
        MapWindowPoints(element, hwnd, (POINT*)&element_rect, 2);
        MoveWindow(element, xpos, element_rect.top, element_rect.right - element_rect.left, element_rect.bottom - element_rect.top, TRUE);
        group++;
    }
}

void uilib_adjust_group_width(HWND hwnd, uilib_dialog_group *group)
{
HWND element;
RECT element_rect;
int xsize;
int ysize;

    while (group->idc) {
        element = GetDlgItem(hwnd, group->idc);
        GetClientRect(element, &element_rect);
        MapWindowPoints(element, hwnd, (POINT*)&element_rect, 2);
        uilib_get_general_window_extents(element, &xsize, &ysize);
        if (group->element_type == 1) xsize += 20;
        MoveWindow(element, element_rect.left, element_rect.top, xsize, element_rect.bottom - element_rect.top, TRUE);
        group++;
    }
}
