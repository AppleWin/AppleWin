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
	const int number = (int)SendMessage(GetDlgItem(hwnd, IDC_TFE_SETTINGS_INTERFACE), CB_GETCURSEL, 0, 0);
	std::string name;
	std::string description;

	if (get_tfename(number, name, description))
	{
		SetWindowText(GetDlgItem(hwnd, IDC_TFE_SETTINGS_INTERFACE_NAME), name.c_str());
		SetWindowText(GetDlgItem(hwnd, IDC_TFE_SETTINGS_INTERFACE_DESC), description.c_str());
	}

	EnableWindow(GetDlgItem(hwnd, IDC_CHECK_TFE_VIRTUAL_DNS), m_enableVirtualDnsCheckbox ? TRUE : FALSE);
}

void CPageConfigTfe::init_tfe_dialog(HWND hwnd)
{
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

	CheckDlgButton(hwnd, IDC_CHECK_TFE_VIRTUAL_DNS, m_tfe_virtual_dns ? BST_CHECKED : BST_UNCHECKED);

	if (PCapBackend::tfe_enumadapter_open())
	{
		int cnt = 0;

		std::string name;
		std::string description;

		HWND temp_hwnd = GetDlgItem(hwnd, IDC_TFE_SETTINGS_INTERFACE);

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
	char buffer[256] = {};
	GetDlgItemText(hwnd, IDC_TFE_SETTINGS_INTERFACE, buffer, sizeof(buffer) - 1);

	m_tfe_interface_name = buffer;
	m_tfe_virtual_dns = IsDlgButtonChecked(hwnd, IDC_CHECK_TFE_VIRTUAL_DNS) ? true : false;
}
