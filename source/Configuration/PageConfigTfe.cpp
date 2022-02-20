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

#include "PageConfigTfe.h"

#include "../Common.h"
#include "../Registry.h"
#include "../resource/resource.h"
#include "../Tfe/PCapBackend.h"
#include "../Tfe/tfesupp.h"

CPageConfigTfe* CPageConfigTfe::ms_this = 0;	// reinit'd in ctor

uilib_localize_dialog_param CPageConfigTfe::ms_dialog[] =
{
	{0, IDS_TFE_CAPTION, -1},
	{IDC_TFE_SETTINGS_ENABLE_T, IDS_TFE_ETHERNET, 0},
	{IDC_TFE_SETTINGS_INTERFACE_T, IDS_TFE_INTERFACE, 0},
	{IDOK, IDS_OK, 0},
	{IDCANCEL, IDS_CANCEL, 0},
	{0, 0, 0}
};

uilib_dialog_group CPageConfigTfe::ms_leftgroup[] =
{
	{IDC_TFE_SETTINGS_ENABLE_T, 0},
	{IDC_TFE_SETTINGS_INTERFACE_T, 0},
	{0, 0}
};

uilib_dialog_group CPageConfigTfe::ms_rightgroup[] =
{
	{IDC_TFE_SETTINGS_ENABLE, 0},
	{IDC_TFE_SETTINGS_INTERFACE, 0},
	{0, 0}
};

INT_PTR CALLBACK CPageConfigTfe::DlgProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	// Switch from static func to our instance
	return CPageConfigTfe::ms_this->DlgProcInternal(hwnd, msg, wparam, lparam);
}

INT_PTR CPageConfigTfe::DlgProcInternal(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	switch (msg)
	{
	case WM_COMMAND:
		switch (LOWORD(wparam))
		{
		case IDOK:
			DlgOK(hwnd);
			/* FALL THROUGH */

		case IDCANCEL:
			DlgCANCEL(hwnd);
			return TRUE;

		case IDC_TFE_SETTINGS_INTERFACE:
			/* FALL THROUGH */

		case IDC_TFE_SETTINGS_ENABLE:
			gray_ungray_items(hwnd);
			break;
		}
		return FALSE;

	case WM_CLOSE:
		EndDialog(hwnd,0);
		return TRUE;

	case WM_INITDIALOG:
		init_tfe_dialog(hwnd);
		return TRUE;
	}

	return FALSE;
}

void CPageConfigTfe::DlgOK(HWND window)
{
	save_tfe_dialog(window);
}

void CPageConfigTfe::DlgCANCEL(HWND window)
{
	EndDialog(window, 0);
}

BOOL CPageConfigTfe::get_tfename(int number, char **ppname, char **ppdescription)
{
	if (PCapBackend::tfe_enumadapter_open())
	{
		char *pname = NULL;
		char *pdescription = NULL;

		while (number--)
		{
			if (!PCapBackend::tfe_enumadapter(&pname, &pdescription))
				break;

			lib_free(pname);
			lib_free(pdescription);
		}

		if (PCapBackend::tfe_enumadapter(&pname, &pdescription))
		{
			*ppname = pname;
			*ppdescription = pdescription;
			PCapBackend::tfe_enumadapter_close();
			return TRUE;
		}

		PCapBackend::tfe_enumadapter_close();
	}

	return FALSE;
}

int CPageConfigTfe::gray_ungray_items(HWND hwnd)
{
	int enable;
	int number;

	int disabled = 0;
	PCapBackend::get_disabled_state(&disabled);

	if (disabled)
	{
		EnableWindow(GetDlgItem(hwnd, IDC_TFE_SETTINGS_ENABLE_T), 0);
		EnableWindow(GetDlgItem(hwnd, IDC_TFE_SETTINGS_ENABLE), 0);
		EnableWindow(GetDlgItem(hwnd, IDOK), 0);
		SetWindowText(GetDlgItem(hwnd,IDC_TFE_SETTINGS_INTERFACE_NAME), "");
		SetWindowText(GetDlgItem(hwnd,IDC_TFE_SETTINGS_INTERFACE_DESC), "");
		enable = 0;
	}
	else
	{
		enable = SendMessage(GetDlgItem(hwnd, IDC_TFE_SETTINGS_ENABLE), CB_GETCURSEL, 0, 0) ? 1 : 0;
	}

	EnableWindow(GetDlgItem(hwnd, IDC_TFE_SETTINGS_INTERFACE_T), enable);
	EnableWindow(GetDlgItem(hwnd, IDC_TFE_SETTINGS_INTERFACE), enable);

	if (enable)
	{
		char *pname = NULL;
		char *pdescription = NULL;

		number = SendMessage(GetDlgItem(hwnd, IDC_TFE_SETTINGS_INTERFACE), CB_GETCURSEL, 0, 0);

		if (get_tfename(number, &pname, &pdescription))
		{
			SetWindowText(GetDlgItem(hwnd, IDC_TFE_SETTINGS_INTERFACE_NAME), pname);
			SetWindowText(GetDlgItem(hwnd, IDC_TFE_SETTINGS_INTERFACE_DESC), pdescription);
			lib_free(pname);
			lib_free(pdescription);
		}
	}
	else
	{
		SetWindowText(GetDlgItem(hwnd, IDC_TFE_SETTINGS_INTERFACE_NAME), "");
		SetWindowText(GetDlgItem(hwnd, IDC_TFE_SETTINGS_INTERFACE_DESC), "");
	}

	return disabled ? 1 : 0;
}

void CPageConfigTfe::init_tfe_dialog(HWND hwnd)
{
	HWND temp_hwnd;
	int active_value;

	int xsize, ysize;

	uilib_get_group_extent(hwnd, ms_leftgroup, &xsize, &ysize);
	uilib_adjust_group_width(hwnd, ms_leftgroup);
	uilib_move_group(hwnd, ms_rightgroup, xsize + 30);

	switch (m_tfe_selected)
	{
	case CT_Uthernet:
		active_value = 1;
		break;
	case CT_Uthernet2:
		active_value = 2;
		break;
	default:
		active_value = 0;
		break;
	}

	temp_hwnd=GetDlgItem(hwnd,IDC_TFE_SETTINGS_ENABLE);
	SendMessage(temp_hwnd, CB_ADDSTRING, 0, (LPARAM)"Disabled");
	SendMessage(temp_hwnd, CB_ADDSTRING, 0, (LPARAM)"Uthernet");
	SendMessage(temp_hwnd, CB_ADDSTRING, 0, (LPARAM)"Uthernet II");
	SendMessage(temp_hwnd, CB_SETCURSEL, (WPARAM)active_value, 0);

	if (PCapBackend::tfe_enumadapter_open())
	{
		int cnt = 0;

		char *pname;
		char *pdescription;

		temp_hwnd=GetDlgItem(hwnd,IDC_TFE_SETTINGS_INTERFACE);

		for (cnt = 0; PCapBackend::tfe_enumadapter(&pname, &pdescription); cnt++)
		{
			BOOL this_entry = FALSE;

			if (strcmp(pname, m_tfe_interface_name.c_str()) == 0)
			{
				this_entry = TRUE;
			}

			SetWindowText(GetDlgItem(hwnd, IDC_TFE_SETTINGS_INTERFACE_NAME), pname);
			SetWindowText(GetDlgItem(hwnd, IDC_TFE_SETTINGS_INTERFACE_DESC), pdescription);
			SendMessage(temp_hwnd, CB_ADDSTRING, 0, (LPARAM)pname);
			lib_free(pname);
			lib_free(pdescription);

			if (this_entry)
			{
				SendMessage(GetDlgItem(hwnd, IDC_TFE_SETTINGS_INTERFACE),
					CB_SETCURSEL, (WPARAM)cnt, 0);
			}
		}

		PCapBackend::tfe_enumadapter_close();
	}

	if (gray_ungray_items(hwnd))
	{
		/* we have a problem: TFE is disabled. Give a message to the user */
		// TC (18 Dec 2017) this vicekb URL is a broken link now, so I copied it to the AppleWin repo, here:
		// . https://github.com/AppleWin/AppleWin/blob/master/docs/VICE%20Knowledge%20Base%20-%20Article%2013-005.htm
		MessageBox( hwnd,
			"Uthernet support is not available on your system,\n"
			"WPCAP.DLL cannot be loaded.\n\n"
			"Install Npcap from\n\n"
			"      https://npcap.com\n\n"
			"to activate networking with AppleWin.",
			"Uthernet support", MB_ICONINFORMATION|MB_OK);

		/* just quit the dialog before it is open */
		SendMessage( hwnd, WM_COMMAND, IDCANCEL, 0);
	}
}

void CPageConfigTfe::save_tfe_dialog(HWND hwnd)
{
	int active_value;
	char buffer[256];

	buffer[255] = 0;
	GetDlgItemText(hwnd, IDC_TFE_SETTINGS_INTERFACE, buffer, sizeof(buffer)-1);

	// RGJ - Added check for NULL interface so we don't set it active without a valid interface selected
	if (strlen(buffer) > 0)
	{
		m_tfe_interface_name = buffer;
		active_value = SendMessage(GetDlgItem(hwnd, IDC_TFE_SETTINGS_ENABLE), CB_GETCURSEL, 0, 0);
		switch (active_value)
		{
		case 1:
			m_tfe_selected = CT_Uthernet;
			break;
		case 2:
			m_tfe_selected = CT_Uthernet2;
			break;
		default:
			m_tfe_selected = CT_Empty;
			break;
		}
	}
	else
	{
		m_tfe_selected = CT_Empty;
		m_tfe_interface_name.clear();
	}
}
