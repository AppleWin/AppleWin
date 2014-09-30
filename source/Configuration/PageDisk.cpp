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

#include "..\AppleWin.h"
#include "..\Frame.h"
#include "..\Registry.h"
#include "..\resource\resource.h"
#include "PageDisk.h"
#include "PropertySheetHelper.h"

CPageDisk* CPageDisk::ms_this = 0;	// reinit'd in ctor

const TCHAR CPageDisk::m_discchoices[] =
				TEXT("Authentic Speed\0")
				TEXT("Enhanced Speed\0");

const TCHAR CPageDisk::m_defaultDiskOptions[] =
				TEXT("Select Disk...\0")
				TEXT("Eject Disk\0");

const TCHAR CPageDisk::m_defaultHDDOptions[] =
				TEXT("Select Hard Disk Image...\0")
				TEXT("Unplug Hard Disk Image\0");

BOOL CALLBACK CPageDisk::DlgProc(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	// Switch from static func to our instance
	return CPageDisk::ms_this->DlgProcInternal(hWnd, message, wparam, lparam);
}

BOOL CPageDisk::DlgProcInternal(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	switch (message)
	{
	case WM_NOTIFY:
		{
			// Property Sheet notifications

			switch (((LPPSHNOTIFY)lparam)->hdr.code)
			{
			case PSN_SETACTIVE:
				// About to become the active page
				m_PropertySheetHelper.SetLastPage(m_Page);
				InitOptions(hWnd);
				break;
			case PSN_KILLACTIVE:
				SetWindowLong(hWnd, DWL_MSGRESULT, FALSE);			// Changes are valid
				break;
			case PSN_APPLY:
				DlgOK(hWnd);
				SetWindowLong(hWnd, DWL_MSGRESULT, PSNRET_NOERROR);	// Changes are valid
				break;
			case PSN_QUERYCANCEL:
				// Can use this to ask user to confirm cancel
				break;
			case PSN_RESET:
				DlgCANCEL(hWnd);
				break;
			}
		}
		break;

	case WM_COMMAND:
		switch (LOWORD(wparam))
		{
		case IDC_COMBO_DISK1:
			if (HIWORD(wparam) == CBN_SELCHANGE) {
				handleDiskCombo(hWnd, DRIVE_1, LOWORD(wparam));
				FrameRefreshStatus(DRAW_BUTTON_DRIVES);
			}
			break;
		case IDC_COMBO_DISK2:
			if (HIWORD(wparam) == CBN_SELCHANGE) {
				handleDiskCombo(hWnd, DRIVE_2, LOWORD(wparam));
				FrameRefreshStatus(DRAW_BUTTON_DRIVES);
			}
			break;
		case IDC_COMBO_HDD1:
			if (HIWORD(wparam) == CBN_SELCHANGE) {
				handleHDDCombo(hWnd, HARDDISK_1, LOWORD(wparam));
			}
			break;
		case IDC_COMBO_HDD2:
			if (HIWORD(wparam) == CBN_SELCHANGE) {
				handleHDDCombo(hWnd, HARDDISK_2, LOWORD(wparam));
			}
			break;
		case IDC_HDD_ENABLE:
			EnableHDD(hWnd, IsDlgButtonChecked(hWnd, IDC_HDD_ENABLE));
			break;

		case IDC_CIDERPRESS_BROWSE:
			{
				std::string CiderPressLoc = m_PropertySheetHelper.BrowseToFile(hWnd, TEXT("Select path to CiderPress"), REGVALUE_CIDERPRESSLOC, TEXT("Applications (*.exe)\0*.exe\0") TEXT("All Files\0*.*\0") );
				RegSaveString(TEXT(REG_CONFIG), REGVALUE_CIDERPRESSLOC, 1, CiderPressLoc.c_str());
				SendDlgItemMessage(hWnd, IDC_CIDERPRESS_FILENAME, WM_SETTEXT, 0, (LPARAM) CiderPressLoc.c_str());
			}
			break;
		}
		break;

	case WM_INITDIALOG:
		{
			m_PropertySheetHelper.FillComboBox(hWnd,IDC_DISKTYPE,m_discchoices,enhancedisk);
			m_PropertySheetHelper.FillComboBox(hWnd, IDC_COMBO_DISK1, m_defaultDiskOptions, -1);
			m_PropertySheetHelper.FillComboBox(hWnd, IDC_COMBO_DISK2, m_defaultDiskOptions, -1);
			m_PropertySheetHelper.FillComboBox(hWnd, IDC_COMBO_HDD1, m_defaultHDDOptions, hdd1Selection);
			m_PropertySheetHelper.FillComboBox(hWnd, IDC_COMBO_HDD2, m_defaultHDDOptions, hdd1Selection);

			if (strlen(DiskGetFullName(DRIVE_1)) > 0) {
				SendDlgItemMessage(hWnd, IDC_COMBO_DISK1, CB_INSERTSTRING, 0, (LPARAM)DiskGetFullName(DRIVE_1));
				SendDlgItemMessage(hWnd, IDC_COMBO_DISK1, CB_SETCURSEL, 0, 0);
			}
			if (strlen(DiskGetFullName(DRIVE_2)) > 0) { 
				SendDlgItemMessage(hWnd, IDC_COMBO_DISK2, CB_INSERTSTRING, 0, (LPARAM)DiskGetFullName(DRIVE_2));
				SendDlgItemMessage(hWnd, IDC_COMBO_DISK2, CB_SETCURSEL, 0, 0);
			}

			LPCTSTR hd = HD_GetFullName(HARDDISK_1);
			if (strlen(hd)>0) {
				SendDlgItemMessage(hWnd, IDC_COMBO_HDD1, CB_INSERTSTRING, 0, (LPARAM)HD_GetFullName(HARDDISK_1));
				(DWORD)SendDlgItemMessage(hWnd, IDC_COMBO_HDD1, CB_SETCURSEL, 0, 0);
			}

			hd = HD_GetFullName(HARDDISK_2);
			if (strlen(hd)>0) {
				SendDlgItemMessage(hWnd, IDC_COMBO_HDD2, CB_INSERTSTRING, 0, (LPARAM)HD_GetFullName(HARDDISK_2));
				(DWORD)SendDlgItemMessage(hWnd, IDC_COMBO_HDD2, CB_SETCURSEL, 0, 0);
			}

			TCHAR PathToCiderPress[MAX_PATH] = "";
			RegLoadString(TEXT("Configuration"), REGVALUE_CIDERPRESSLOC, 1, PathToCiderPress,MAX_PATH);
			SendDlgItemMessage(hWnd, IDC_CIDERPRESS_FILENAME ,WM_SETTEXT, 0, (LPARAM)PathToCiderPress);

			CheckDlgButton(hWnd, IDC_HDD_ENABLE, HD_CardIsEnabled() ? BST_CHECKED : BST_UNCHECKED);

			EnableHDD(hWnd, IsDlgButtonChecked(hWnd, IDC_HDD_ENABLE));

			InitOptions(hWnd);

			break;
		}

	}

	return FALSE;
}

void CPageDisk::DlgOK(HWND hWnd)
{
	const BOOL bNewEnhanceDisk = (BOOL) SendDlgItemMessage(hWnd, IDC_DISKTYPE,CB_GETCURSEL, 0, 0);
	if (bNewEnhanceDisk != enhancedisk)
	{
		m_PropertySheetHelper.GetConfigNew().m_bEnhanceDisk = bNewEnhanceDisk;
	}

	const bool bNewHDDIsEnabled = IsDlgButtonChecked(hWnd, IDC_HDD_ENABLE) ? true : false;
	if (bNewHDDIsEnabled != HD_CardIsEnabled())
	{
		m_PropertySheetHelper.GetConfigNew().m_bEnableHDD = bNewHDDIsEnabled;
	}

	RegSaveString(TEXT(REG_PREFS), TEXT(REGVALUE_PREF_LAST_HARDDISK_1), 1, HD_GetFullPathName(HARDDISK_1));
	RegSaveString(TEXT(REG_PREFS), TEXT(REGVALUE_PREF_LAST_HARDDISK_2), 1, HD_GetFullPathName(HARDDISK_2));

	m_PropertySheetHelper.PostMsgAfterClose(hWnd, m_Page);
}

void CPageDisk::InitOptions(HWND hWnd)
{
	// Nothing to do:
	// - no changes made on any other pages affect this page
}

void CPageDisk::EnableHDD(HWND hWnd, BOOL bEnable)
{
	EnableWindow(GetDlgItem(hWnd, IDC_COMBO_HDD1), bEnable);
	EnableWindow(GetDlgItem(hWnd, IDC_COMBO_HDD2), bEnable);
}

void CPageDisk::handleHDDCombo(HWND hWnd, UINT16 driveSelected, UINT16 comboSelected)
{
	// Search from "select hard drive"
	DWORD dwOpenDialogIndex = (DWORD)SendDlgItemMessage(hWnd, comboSelected, CB_FINDSTRINGEXACT, -1, (LPARAM)&m_defaultHDDOptions[0]);
	DWORD dwComboSelection = (DWORD)SendDlgItemMessage(hWnd, comboSelected, CB_GETCURSEL, 0, 0);
	if (IsDlgButtonChecked(hWnd, IDC_HDD_ENABLE)) {
		if (dwComboSelection == dwOpenDialogIndex) {
			(DWORD)SendDlgItemMessage(hWnd, comboSelected, CB_SETCURSEL, -1, 0);
			HD_Select(driveSelected);
			// Add hard drive name as item 0 and select it
			if (dwOpenDialogIndex > 0) {
				//Remove old item first
				SendDlgItemMessage(hWnd, comboSelected, CB_DELETESTRING, 0, 0);
			}
			SendDlgItemMessage(hWnd, comboSelected, CB_INSERTSTRING, 0, (LPARAM)HD_GetFullName(driveSelected));
			SendDlgItemMessage(hWnd, comboSelected, CB_SETCURSEL, 0, 0);

			// If the HD was in the other combo, remove now
			DWORD comboOther = (comboSelected == IDC_COMBO_HDD1) ? IDC_COMBO_HDD2 : IDC_COMBO_HDD1;

			DWORD duplicated = (DWORD)SendDlgItemMessage(hWnd, comboOther, CB_FINDSTRINGEXACT, -1, (LPARAM)HD_GetFullName(driveSelected));
			if (duplicated != CB_ERR) {
				SendDlgItemMessage(hWnd, comboOther, CB_DELETESTRING, duplicated, 0);
				SendDlgItemMessage(hWnd, comboOther, CB_SETCURSEL, -1, 0);
			}
		}
		else if (dwComboSelection == (dwOpenDialogIndex+1)){
			if (dwComboSelection > 1) {
				SendDlgItemMessage(hWnd, comboSelected, CB_SETCURSEL, -1, 0);
				int iCommand = (driveSelected == 0) ? ID_DISKMENU_UNPLUG_HARDDISK1 : ID_DISKMENU_UNPLUG_HARDDISK2;
				if (removalConfirmation(iCommand)) {
					// unplug selected disk
					HD_Unplug(driveSelected);
					//Remove drive from list
					SendDlgItemMessage(hWnd, comboSelected, CB_DELETESTRING, 0, 0);
				} else {
					SendDlgItemMessage(hWnd, comboSelected, CB_SETCURSEL, 0, 0);
				}
			}
		}
	}
}

void CPageDisk::handleDiskCombo(HWND hWnd, UINT16 driveSelected, UINT16 comboSelected)
{
	// Search from "select hard drive"
	DWORD dwOpenDialogIndex = (DWORD)SendDlgItemMessage(hWnd, comboSelected, CB_FINDSTRINGEXACT, -1, (LPARAM)&m_defaultDiskOptions[0]);
	DWORD dwComboSelection = (DWORD)SendDlgItemMessage(hWnd, comboSelected, CB_GETCURSEL, 0, 0);
	if (dwComboSelection == dwOpenDialogIndex) {
		(DWORD)SendDlgItemMessage(hWnd, comboSelected, CB_SETCURSEL, -1, 0);
		DiskSelect(driveSelected);
		// Add hard drive name as item 0 and select it
		if (dwOpenDialogIndex > 0) {
			//Remove old item first
			SendDlgItemMessage(hWnd, comboSelected, CB_DELETESTRING, 0, 0);
		}
		SendDlgItemMessage(hWnd, comboSelected, CB_INSERTSTRING, 0, (LPARAM)DiskGetFullName(driveSelected));
		SendDlgItemMessage(hWnd, comboSelected, CB_SETCURSEL, 0, 0);

		// If the HD was in the other combo, remove now
		DWORD comboOther = (comboSelected == IDC_COMBO_DISK1) ? IDC_COMBO_DISK2 : IDC_COMBO_DISK1;

		DWORD duplicated = (DWORD)SendDlgItemMessage(hWnd, comboOther, CB_FINDSTRINGEXACT, -1, (LPARAM)DiskGetFullName(driveSelected));
		if (duplicated != CB_ERR) {
			SendDlgItemMessage(hWnd, comboOther, CB_DELETESTRING, duplicated, 0);
			SendDlgItemMessage(hWnd, comboOther, CB_SETCURSEL, -1, 0);
		}
	}
	else if (dwComboSelection == (dwOpenDialogIndex + 1)){
		if (dwComboSelection > 1) {
			SendDlgItemMessage(hWnd, comboSelected, CB_SETCURSEL, -1, 0);
			int iCommand = (driveSelected == 0) ? ID_DISKMENU_EJECT_DISK1 : ID_DISKMENU_EJECT_DISK2;
			if (removalConfirmation(iCommand)) {
				// unplug selected disk
				DiskEject(driveSelected);
				//Remove drive from list
				SendDlgItemMessage(hWnd, comboSelected, CB_DELETESTRING, 0, 0);
			}
			else {
				SendDlgItemMessage(hWnd, comboSelected, CB_SETCURSEL, 0, 0);
			}
		}
	}
}


int CPageDisk::removalConfirmation(int iCommand)
{
	TCHAR szText[100];
	bool bMsgBox = true;
	if (iCommand == ID_DISKMENU_EJECT_DISK1 || iCommand == ID_DISKMENU_EJECT_DISK2)
		wsprintf(szText, "Do you really want to eject the disk in drive-%c ?", '1' + iCommand - ID_DISKMENU_EJECT_DISK1);
	else if (iCommand == ID_DISKMENU_UNPLUG_HARDDISK1 || iCommand == ID_DISKMENU_UNPLUG_HARDDISK2)
		wsprintf(szText, "Do you really want to unplug harddisk-%c ?", '1' + iCommand - ID_DISKMENU_UNPLUG_HARDDISK1);
	else
		bMsgBox = false;

	if (bMsgBox)
	{
		int nRes = MessageBox(g_hFrameWindow, szText, TEXT("Eject/Unplug Warning"), MB_ICONWARNING | MB_YESNO | MB_SETFOREGROUND);
		if (nRes == IDNO)
			iCommand = 0;
	}
	return iCommand;
}