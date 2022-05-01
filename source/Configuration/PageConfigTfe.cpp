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

BOOL CPageConfigTfe::get_tfename(int number, std::string & name, std::string & description)
{
	if (PCapBackend::tfe_enumadapter_open())
	{
		std::string adapterName;
		std::string adapterDescription;

		while (number--)
		{
			if (!PCapBackend::tfe_enumadapter(adapterName, adapterDescription))
				break;
		}

		if (PCapBackend::tfe_enumadapter(adapterName, adapterDescription))
		{
			name = adapterName;
			description = adapterDescription;
			PCapBackend::tfe_enumadapter_close();
			return TRUE;
		}

		PCapBackend::tfe_enumadapter_close();
	}

	return FALSE;
}

void CPageConfigTfe::gray_ungray_items(HWND hwnd)
{
	const int enable = SendMessage(GetDlgItem(hwnd, IDC_TFE_SETTINGS_ENABLE), CB_GETCURSEL, 0, 0);

	if (enable)
	{
		std::string name;
		std::string description;

		const int number = SendMessage(GetDlgItem(hwnd, IDC_TFE_SETTINGS_INTERFACE), CB_GETCURSEL, 0, 0);

		if (get_tfename(number, name, description))
		{
			SetWindowText(GetDlgItem(hwnd, IDC_TFE_SETTINGS_INTERFACE_NAME), name.c_str());
			SetWindowText(GetDlgItem(hwnd, IDC_TFE_SETTINGS_INTERFACE_DESC), description.c_str());
		}
	}
	else
	{
		SetWindowText(GetDlgItem(hwnd, IDC_TFE_SETTINGS_INTERFACE_NAME), "");
		SetWindowText(GetDlgItem(hwnd, IDC_TFE_SETTINGS_INTERFACE_DESC), "");
	}

	EnableWindow(GetDlgItem(hwnd, IDC_CHECK_TFE_VIRTUAL_DNS), enable == 2);
}

void CPageConfigTfe::init_tfe_dialog(HWND hwnd)
{
	HWND temp_hwnd;
	int active_value;

	int xsize, ysize;

	uilib_get_group_extent(hwnd, ms_leftgroup, &xsize, &ysize);
	uilib_adjust_group_width(hwnd, ms_leftgroup);
	uilib_move_group(hwnd, ms_rightgroup, xsize + 30);

	if (PCapBackend::tfe_is_npcap_loaded())
	{
		const char * version = PCapBackend::tfe_lib_version();
		SetWindowText(GetDlgItem(hwnd, IDC_TFE_NPCAP_INFO), version);
	}
	else
	{
		EnableWindow(GetDlgItem(hwnd, IDC_TFE_SETTINGS_INTERFACE), 0);
		EnableWindow(GetDlgItem(hwnd, IDC_TFE_SETTINGS_INTERFACE_NAME), 0);
		EnableWindow(GetDlgItem(hwnd, IDC_TFE_SETTINGS_INTERFACE_DESC), 0);

		SetWindowText(GetDlgItem(hwnd, IDC_TFE_NPCAP_INFO),
			"Limited Uthernet support is available on your system.\n\n"
			"Install Npcap from https://npcap.com\n"
			"or select Uthernet II with Virtual DNS.");
	}

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

	CheckDlgButton(hwnd, IDC_CHECK_TFE_VIRTUAL_DNS, m_tfe_virtual_dns ? BST_CHECKED : BST_UNCHECKED);

	temp_hwnd=GetDlgItem(hwnd,IDC_TFE_SETTINGS_ENABLE);
	SendMessage(temp_hwnd, CB_ADDSTRING, 0, (LPARAM)"Disabled");
	SendMessage(temp_hwnd, CB_ADDSTRING, 0, (LPARAM)"Uthernet");
	SendMessage(temp_hwnd, CB_ADDSTRING, 0, (LPARAM)"Uthernet II");
	SendMessage(temp_hwnd, CB_SETCURSEL, (WPARAM)active_value, 0);

	if (PCapBackend::tfe_enumadapter_open())
	{
		int cnt = 0;

		std::string name;
		std::string description;

		temp_hwnd=GetDlgItem(hwnd,IDC_TFE_SETTINGS_INTERFACE);

		for (cnt = 0; PCapBackend::tfe_enumadapter(name, description); cnt++)
		{
			BOOL this_entry = FALSE;

			if (name == m_tfe_interface_name)
			{
				this_entry = TRUE;
			}

			SetWindowText(GetDlgItem(hwnd, IDC_TFE_SETTINGS_INTERFACE_NAME), name.c_str());
			SetWindowText(GetDlgItem(hwnd, IDC_TFE_SETTINGS_INTERFACE_DESC), description.c_str());
			SendMessage(temp_hwnd, CB_ADDSTRING, 0, (LPARAM)name.c_str());

			if (this_entry)
			{
				SendMessage(GetDlgItem(hwnd, IDC_TFE_SETTINGS_INTERFACE),
					CB_SETCURSEL, (WPARAM)cnt, 0);
			}
		}

		PCapBackend::tfe_enumadapter_close();
	}

	gray_ungray_items(hwnd);
}

void CPageConfigTfe::save_tfe_dialog(HWND hwnd)
{
	int active_value;
	char buffer[256];

	buffer[255] = 0;
	GetDlgItemText(hwnd, IDC_TFE_SETTINGS_INTERFACE, buffer, sizeof(buffer)-1);

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

	m_tfe_virtual_dns = IsDlgButtonChecked(hwnd, IDC_CHECK_TFE_VIRTUAL_DNS) ? 1 : 0;
}
