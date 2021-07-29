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

#include "PageDisk.h"
#include "PropertySheet.h"

#include "../Windows/AppleWin.h"
#include "../CardManager.h"
#include "../Disk.h"	// Drive_e, Disk_Status_e
#include "../Registry.h"
#include "../resource/resource.h"

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

INT_PTR CALLBACK CPageDisk::DlgProc(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	// Switch from static func to our instance
	return CPageDisk::ms_this->DlgProcInternal(hWnd, message, wparam, lparam);
}

INT_PTR CPageDisk::DlgProcInternal(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam)
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
				SetWindowLongPtr(hWnd, DWLP_MSGRESULT, FALSE);			// Changes are valid
				break;
			case PSN_APPLY:
				DlgOK(hWnd);
				SetWindowLongPtr(hWnd, DWLP_MSGRESULT, PSNRET_NOERROR);	// Changes are valid
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
			if (HIWORD(wparam) == CBN_SELCHANGE)
			{
				HandleFloppyDriveCombo(hWnd, DRIVE_1, LOWORD(wparam), IDC_COMBO_DISK2, SLOT6);
				GetFrame().FrameRefreshStatus(DRAW_BUTTON_DRIVES | DRAW_DISK_STATUS);
			}
			break;
		case IDC_COMBO_DISK2:
			if (HIWORD(wparam) == CBN_SELCHANGE)
			{
				HandleFloppyDriveCombo(hWnd, DRIVE_2, LOWORD(wparam), IDC_COMBO_DISK1, SLOT6);
				GetFrame().FrameRefreshStatus(DRAW_BUTTON_DRIVES | DRAW_DISK_STATUS);
			}
			break;
		case IDC_COMBO_DISK1_SLOT5:
			if (HIWORD(wparam) == CBN_SELCHANGE)
			{
				HandleFloppyDriveCombo(hWnd, DRIVE_1, LOWORD(wparam), IDC_COMBO_DISK2_SLOT5, SLOT5);
				GetFrame().FrameRefreshStatus(DRAW_BUTTON_DRIVES | DRAW_DISK_STATUS);
			}
			break;
		case IDC_COMBO_DISK2_SLOT5:
			if (HIWORD(wparam) == CBN_SELCHANGE)
			{
				HandleFloppyDriveCombo(hWnd, DRIVE_2, LOWORD(wparam), IDC_COMBO_DISK1_SLOT5, SLOT5);
				GetFrame().FrameRefreshStatus(DRAW_BUTTON_DRIVES | DRAW_DISK_STATUS);
			}
			break;
		case IDC_COMBO_HDD1:
			if (HIWORD(wparam) == CBN_SELCHANGE)
			{
				HandleHDDCombo(hWnd, HARDDISK_1, LOWORD(wparam));
			}
			break;
		case IDC_COMBO_HDD2:
			if (HIWORD(wparam) == CBN_SELCHANGE)
			{
				HandleHDDCombo(hWnd, HARDDISK_2, LOWORD(wparam));
			}
			break;
		case IDC_HDD_ENABLE:
			EnableHDD(hWnd, IsDlgButtonChecked(hWnd, IDC_HDD_ENABLE));
			break;
		case IDC_HDD_SWAP:
			HandleHDDSwap(hWnd);
			break;
		case IDC_CIDERPRESS_BROWSE:
			{
				std::string CiderPressLoc = m_PropertySheetHelper.BrowseToFile(hWnd, TEXT("Select path to CiderPress"), REGVALUE_CIDERPRESSLOC, TEXT("Applications (*.exe)\0*.exe\0") TEXT("All Files\0*.*\0") );
				SendDlgItemMessage(hWnd, IDC_CIDERPRESS_FILENAME, WM_SETTEXT, 0, (LPARAM) CiderPressLoc.c_str());
			}
			break;
		}
		break;

	case WM_INITDIALOG:
		{
			CheckDlgButton(hWnd, IDC_ENHANCE_DISK_ENABLE, GetCardMgr().GetDisk2CardMgr().GetEnhanceDisk() ? BST_CHECKED : BST_UNCHECKED);

			UINT slot = SLOT6;
			if (GetCardMgr().QuerySlot(slot) == CT_Disk2)
				InitComboFloppyDrive(hWnd, slot);
			else
				EnableFloppyDrive(hWnd, FALSE, slot);	// disable if slot6 is empty (or has some other card in it)

			//

			slot = SLOT5;
			if (GetCardMgr().QuerySlot(slot) == CT_Disk2)
				InitComboFloppyDrive(hWnd, slot);
			else if (GetCardMgr().QuerySlot(slot) != CT_Empty)
				EnableFloppyDrive(hWnd, FALSE, slot);	// disable if slot5 has some other card in it

			CheckDlgButton(hWnd, IDC_DISKII_SLOT5_ENABLE, (GetCardMgr().QuerySlot(slot) == CT_Disk2) ? BST_CHECKED : BST_UNCHECKED);

			//

			InitComboHDD(hWnd, SLOT7);

			TCHAR PathToCiderPress[MAX_PATH];
			RegLoadString(TEXT(REG_CONFIG), REGVALUE_CIDERPRESSLOC, 1, PathToCiderPress, MAX_PATH, TEXT(""));
			SendDlgItemMessage(hWnd, IDC_CIDERPRESS_FILENAME ,WM_SETTEXT, 0, (LPARAM)PathToCiderPress);

			CheckDlgButton(hWnd, IDC_HDD_ENABLE, HD_CardIsEnabled() ? BST_CHECKED : BST_UNCHECKED);

			EnableHDD(hWnd, IsDlgButtonChecked(hWnd, IDC_HDD_ENABLE));

			InitOptions(hWnd);

			break;
		}

	}

	return FALSE;
}

void CPageDisk::InitComboFloppyDrive(HWND hWnd, UINT slot)
{
	Disk2InterfaceCard& disk2Card = dynamic_cast<Disk2InterfaceCard&>(GetCardMgr().GetRef(slot));

	const UINT idcComboDisk1 = (slot == SLOT6) ? IDC_COMBO_DISK1 : IDC_COMBO_DISK1_SLOT5;
	const UINT idcComboDisk2 = (slot == SLOT6) ? IDC_COMBO_DISK2 : IDC_COMBO_DISK2_SLOT5;

	m_PropertySheetHelper.FillComboBox(hWnd, idcComboDisk1, m_defaultDiskOptions, -1);
	m_PropertySheetHelper.FillComboBox(hWnd, idcComboDisk2, m_defaultDiskOptions, -1);

	if (!disk2Card.GetFullName(DRIVE_1).empty())
	{
		SendDlgItemMessage(hWnd, idcComboDisk1, CB_INSERTSTRING, 0, (LPARAM)disk2Card.GetFullName(DRIVE_1).c_str());
		SendDlgItemMessage(hWnd, idcComboDisk1, CB_SETCURSEL, 0, 0);
	}

	if (!disk2Card.GetFullName(DRIVE_2).empty())
	{ 
		SendDlgItemMessage(hWnd, idcComboDisk2, CB_INSERTSTRING, 0, (LPARAM)disk2Card.GetFullName(DRIVE_2).c_str());
		SendDlgItemMessage(hWnd, idcComboDisk2, CB_SETCURSEL, 0, 0);
	}
}

void CPageDisk::InitComboHDD(HWND hWnd, UINT /*slot*/)
{
	m_PropertySheetHelper.FillComboBox(hWnd, IDC_COMBO_HDD1, m_defaultHDDOptions, -1);
	m_PropertySheetHelper.FillComboBox(hWnd, IDC_COMBO_HDD2, m_defaultHDDOptions, -1);

	if (!HD_GetFullName(HARDDISK_1).empty())
	{
		SendDlgItemMessage(hWnd, IDC_COMBO_HDD1, CB_INSERTSTRING, 0, (LPARAM)HD_GetFullName(HARDDISK_1).c_str());
		SendDlgItemMessage(hWnd, IDC_COMBO_HDD1, CB_SETCURSEL, 0, 0);
	}

	if (!HD_GetFullName(HARDDISK_2).empty())
	{
		SendDlgItemMessage(hWnd, IDC_COMBO_HDD2, CB_INSERTSTRING, 0, (LPARAM)HD_GetFullName(HARDDISK_2).c_str());
		SendDlgItemMessage(hWnd, IDC_COMBO_HDD2, CB_SETCURSEL, 0, 0);
	}
}

void CPageDisk::DlgOK(HWND hWnd)
{
	// Update CiderPress pathname
	{
		char szFilename[MAX_PATH];
		memset(szFilename, 0, sizeof(szFilename));
		* (USHORT*) szFilename = sizeof(szFilename);

		UINT nLineLength = SendDlgItemMessage(hWnd, IDC_CIDERPRESS_FILENAME, EM_LINELENGTH, 0, 0);

		SendDlgItemMessage(hWnd, IDC_CIDERPRESS_FILENAME, EM_GETLINE, 0, (LPARAM)szFilename);

		nLineLength = nLineLength > sizeof(szFilename)-1 ? sizeof(szFilename)-1 : nLineLength;
		szFilename[nLineLength] = 0x00;

		RegSaveString(TEXT(REG_CONFIG), REGVALUE_CIDERPRESSLOC, 1, szFilename);
	}

	const bool bNewEnhanceDisk = IsDlgButtonChecked(hWnd, IDC_ENHANCE_DISK_ENABLE) ? true : false;
	if (bNewEnhanceDisk != GetCardMgr().GetDisk2CardMgr().GetEnhanceDisk())
	{
		GetCardMgr().GetDisk2CardMgr().SetEnhanceDisk(bNewEnhanceDisk);
		REGSAVE(TEXT(REGVALUE_ENHANCE_DISK_SPEED), (DWORD)bNewEnhanceDisk);
	}

	if (GetCardMgr().QuerySlot(SLOT5) == CT_Disk2 || GetCardMgr().QuerySlot(SLOT5) == CT_Empty)
	{
		const SS_CARDTYPE newSlot5Card = IsDlgButtonChecked(hWnd, IDC_DISKII_SLOT5_ENABLE) ? CT_Disk2 : CT_Empty;
		if (newSlot5Card != GetCardMgr().QuerySlot(SLOT5))
		{
			GetCardMgr().Insert(SLOT5, newSlot5Card);
		}
	}

	const bool bNewHDDIsEnabled = IsDlgButtonChecked(hWnd, IDC_HDD_ENABLE) ? true : false;
	if (bNewHDDIsEnabled != HD_CardIsEnabled())
	{
		m_PropertySheetHelper.GetConfigNew().m_bEnableHDD = bNewHDDIsEnabled;
	}

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
	EnableWindow(GetDlgItem(hWnd, IDC_HDD_SWAP), bEnable);
}

void CPageDisk::EnableFloppyDrive(HWND hWnd, BOOL bEnable, UINT slot)
{
	_ASSERT(slot == SLOT6 || slot == SLOT5);

	if (slot == SLOT6)
	{
		EnableWindow(GetDlgItem(hWnd, IDC_COMBO_DISK1), bEnable);
		EnableWindow(GetDlgItem(hWnd, IDC_COMBO_DISK2), bEnable);
	}
	else if (slot == SLOT5)
	{
		EnableWindow(GetDlgItem(hWnd, IDC_COMBO_DISK1_SLOT5), bEnable);
		EnableWindow(GetDlgItem(hWnd, IDC_COMBO_DISK2_SLOT5), bEnable);
	}
}

void CPageDisk::HandleHDDCombo(HWND hWnd, UINT driveSelected, UINT comboSelected)
{
	if (!IsDlgButtonChecked(hWnd, IDC_HDD_ENABLE))
		return;

	// Search from "select hard drive"
	DWORD dwOpenDialogIndex = (DWORD)SendDlgItemMessage(hWnd, comboSelected, CB_FINDSTRINGEXACT, -1, (LPARAM)&m_defaultHDDOptions[0]);
	DWORD dwComboSelection = (DWORD)SendDlgItemMessage(hWnd, comboSelected, CB_GETCURSEL, 0, 0);

	SendDlgItemMessage(hWnd, comboSelected, CB_SETCURSEL, -1, 0);	// Set to "empty" item

	if (dwComboSelection == dwOpenDialogIndex)
	{
		EnableHDD(hWnd, FALSE);	// Prevent multiple Selection dialogs to be triggered
		bool bRes = HD_Select(driveSelected);
		EnableHDD(hWnd, TRUE);

		if (!bRes)
		{
			if (SendDlgItemMessage(hWnd, comboSelected, CB_GETCOUNT, 0, 0) == 3)	// If there's already a HDD...
				SendDlgItemMessage(hWnd, comboSelected, CB_SETCURSEL, 0, 0);		// then reselect it in the ComboBox
			return;
		}

		// Add hard drive name as item 0 and select it
		if (dwOpenDialogIndex > 0)
		{
			// Remove old item first
			SendDlgItemMessage(hWnd, comboSelected, CB_DELETESTRING, 0, 0);
		}

		SendDlgItemMessage(hWnd, comboSelected, CB_INSERTSTRING, 0, (LPARAM)HD_GetFullName(driveSelected).c_str());
		SendDlgItemMessage(hWnd, comboSelected, CB_SETCURSEL, 0, 0);

		// If the HD was in the other combo, remove now
		DWORD comboOther = (comboSelected == IDC_COMBO_HDD1) ? IDC_COMBO_HDD2 : IDC_COMBO_HDD1;

		DWORD duplicated = (DWORD)SendDlgItemMessage(hWnd, comboOther, CB_FINDSTRINGEXACT, -1, (LPARAM)HD_GetFullName(driveSelected).c_str());
		if (duplicated != CB_ERR)
		{
			SendDlgItemMessage(hWnd, comboOther, CB_DELETESTRING, duplicated, 0);
			SendDlgItemMessage(hWnd, comboOther, CB_SETCURSEL, -1, 0);
		}
	}
	else if (dwComboSelection == (dwOpenDialogIndex+1))
	{
		if (dwComboSelection > 1)
		{
			if (RemovalConfirmation(comboSelected))
			{
				// Unplug selected disk
				HD_Unplug(driveSelected);
				// Remove drive from list
				SendDlgItemMessage(hWnd, comboSelected, CB_DELETESTRING, 0, 0);
			}
			else
			{
				SendDlgItemMessage(hWnd, comboSelected, CB_SETCURSEL, 0, 0);
			}
		}
	}
}

void CPageDisk::HandleFloppyDriveCombo(HWND hWnd, UINT driveSelected, UINT comboSelected, UINT comboOther, UINT slot)
{
	_ASSERT(slot == SLOT6 || slot == SLOT5);

	if (GetCardMgr().QuerySlot(slot) != CT_Disk2)
	{
		_ASSERT(0);	// Shouldn't come here, as the combo is disabled
		return;
	}

	Disk2InterfaceCard& disk2Card = dynamic_cast<Disk2InterfaceCard&>(GetCardMgr().GetRef(slot));

	// Search from "select floppy drive"
	DWORD dwOpenDialogIndex = (DWORD)SendDlgItemMessage(hWnd, comboSelected, CB_FINDSTRINGEXACT, -1, (LPARAM)&m_defaultDiskOptions[0]);
	DWORD dwComboSelection = (DWORD)SendDlgItemMessage(hWnd, comboSelected, CB_GETCURSEL, 0, 0);

	SendDlgItemMessage(hWnd, comboSelected, CB_SETCURSEL, -1, 0);	// Set to "empty" item

	if (dwComboSelection == dwOpenDialogIndex)
	{
		EnableFloppyDrive(hWnd, FALSE, slot);	// Prevent multiple Selection dialogs to be triggered
		bool bRes = disk2Card.UserSelectNewDiskImage(driveSelected);
		EnableFloppyDrive(hWnd, TRUE, slot);

		if (!bRes)
		{
			if (SendDlgItemMessage(hWnd, comboSelected, CB_GETCOUNT, 0, 0) == 3)	// If there's already a disk...
				SendDlgItemMessage(hWnd, comboSelected, CB_SETCURSEL, 0, 0);		// then reselect it in the ComboBox
			return;
		}

		// Add floppy drive name as item 0 and select it
		if (dwOpenDialogIndex > 0)
		{
			//Remove old item first
			SendDlgItemMessage(hWnd, comboSelected, CB_DELETESTRING, 0, 0);
		}

		std::string fullname = disk2Card.GetFullName(driveSelected);
		SendDlgItemMessage(hWnd, comboSelected, CB_INSERTSTRING, 0, (LPARAM)fullname.c_str());
		SendDlgItemMessage(hWnd, comboSelected, CB_SETCURSEL, 0, 0);

		// If the FD was in the other combo, remove now
		DWORD duplicated = (DWORD)SendDlgItemMessage(hWnd, comboOther, CB_FINDSTRINGEXACT, -1, (LPARAM)fullname.c_str());
		if (duplicated != CB_ERR)
		{
			SendDlgItemMessage(hWnd, comboOther, CB_DELETESTRING, duplicated, 0);
			SendDlgItemMessage(hWnd, comboOther, CB_SETCURSEL, -1, 0);
		}
	}
	else if (dwComboSelection == (dwOpenDialogIndex + 1))
	{
		if (dwComboSelection > 1)
		{
			if (RemovalConfirmation(comboSelected))
			{
				// Eject selected disk
				disk2Card.EjectDisk(driveSelected);
				// Remove drive from list
				SendDlgItemMessage(hWnd, comboSelected, CB_DELETESTRING, 0, 0);
			}
			else
			{
				SendDlgItemMessage(hWnd, comboSelected, CB_SETCURSEL, 0, 0);
			}
		}
	}
}

void CPageDisk::HandleHDDSwap(HWND hWnd)
{
	if (!RemovalConfirmation(IDC_HDD_SWAP))
		return;

	if (!HD_ImageSwap())
		return;

	InitComboHDD(hWnd, SLOT7);
}

UINT CPageDisk::RemovalConfirmation(UINT uCommand)
{
	TCHAR szText[100];
	bool bMsgBox = true;

	bool isDisk = false;
	UINT drive = 0;
	if (uCommand == IDC_COMBO_DISK1 || uCommand == IDC_COMBO_DISK2)
	{
		isDisk = true;
		drive = uCommand - IDC_COMBO_DISK1;
	}
	else if (uCommand == IDC_COMBO_DISK1_SLOT5 || uCommand == IDC_COMBO_DISK2_SLOT5)
	{
		isDisk = true;
		drive = uCommand - IDC_COMBO_DISK1_SLOT5;
	}

	if (isDisk)
		StringCbPrintf(szText, sizeof(szText), "Do you really want to eject the disk in drive-%c ?", '1' + drive);
	else if (uCommand == IDC_COMBO_HDD1 || uCommand == IDC_COMBO_HDD2)
		StringCbPrintf(szText, sizeof(szText), "Do you really want to unplug harddisk-%c ?", '1' + uCommand - IDC_COMBO_HDD1);
	else if (uCommand == IDC_HDD_SWAP)
		StringCbPrintf(szText, sizeof(szText), "Do you really want to swap the harddisk images?");
	else
		bMsgBox = false;

	if (bMsgBox)
	{
		int nRes = GetFrame().FrameMessageBox(szText, TEXT("Eject/Unplug Warning"), MB_ICONWARNING | MB_YESNO | MB_SETFOREGROUND);
		if (nRes == IDNO)
			uCommand = 0;
	}

	return uCommand;
}
