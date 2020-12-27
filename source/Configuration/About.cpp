/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 1994-1996, Michael O'Brien
Copyright (C) 1999-2001, Oliver Schmidt
Copyright (C) 2002-2005, Tom Charlesworth
Copyright (C) 2006-2014, Tom Charlesworth, Michael Pohoreski, Nick Westgate

AppleWin is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

AppleWin is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with AppleWin; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "StdAfx.h"

#include "About.h"
#include "../Core.h"
#include "../Interface.h"
#include "../Windows/AppleWin.h"
#include "../resource/resource.h"

static const TCHAR g_szGPL[] = 
"This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation; either version 2 of the License, or (at your option) any later version.\r\n\
\r\n\
This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.\r\n\
\r\n\
You should have received a copy of the GNU General Public License along with this program; if not, write to the Free Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.";


static BOOL CALLBACK DlgProcAbout(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	switch (message)
	{
	case WM_NOTIFY:
		{
		}
		break;

	case WM_COMMAND:
		switch (LOWORD(wparam))
		{
		case IDOK:
			EndDialog(hWnd, 1);
			return TRUE;
		case IDCANCEL:
			EndDialog(hWnd, 0);
			return TRUE;
		}
		break;

	case WM_CLOSE:
		//EndDialog(hWnd, 0);
		return TRUE;

	case WM_INITDIALOG:
		{
			HICON hIcon = LoadIcon(GetFrame().g_hInstance, TEXT("APPLEWIN_ICON"));
			SendDlgItemMessage(hWnd, IDC_APPLEWIN_ICON, STM_SETIMAGE, IMAGE_ICON, (LPARAM)hIcon);

			TCHAR szAppleWinVersion[50];
			StringCbPrintf(szAppleWinVersion, 50, "AppleWin v%s", VERSIONSTRING);
			SendDlgItemMessage(hWnd, IDC_APPLEWIN_VERSION, WM_SETTEXT, 0, (LPARAM)szAppleWinVersion);

			SendDlgItemMessage(hWnd, IDC_GPL_TEXT, WM_SETTEXT, 0, (LPARAM)g_szGPL);
		}
		break;
	}

	return FALSE;
}

bool AboutDlg(void)
{
	return DialogBox(GetFrame().g_hInstance, (LPCTSTR)IDD_ABOUT, GetFrame().g_hFrameWindow, DlgProcAbout) ? true : false;
}
