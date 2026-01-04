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

#include "PageSound.h"
#include "PropertySheet.h"

#include "../Common.h"
#include "../CardManager.h"
#include "../Disk.h"
#include "../Harddisk.h"
#include "../Interface.h"
#include "../Memory.h"
#include "../ParallelPrinter.h"
#include "../Registry.h"
#include "../SerialComms.h"
#include "../resource/resource.h"
#include "../Uthernet2.h"
#include "../Tfe/PCapBackend.h"
#include "../Windows/Win32Frame.h"

CPageSound* CPageSound::ms_this = nullptr;	// reinit'd in ctor
UINT CPageSound::ms_slot = 0;

const char CPageSound::m_defaultDiskOptions[] =
				"Select Disk...\0"
				"Eject Disk\0";

const char CPageSound::m_defaultHDDOptions[] =
				"Select Hard Disk Image...\0"
				"Unplug Hard Disk Image\0";

INT_PTR CALLBACK CPageSound::DlgProc(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	// Switch from static func to our instance
	return CPageSound::ms_this->DlgProcInternal(hWnd, message, wparam, lparam);
}

INT_PTR CPageSound::DlgProcInternal(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam)
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
			default:
				return FALSE;
			}
		}
		break;

	case WM_COMMAND:
		switch (LOWORD(wparam))
		{
		case IDC_SLOT0:
		case IDC_SLOT1:
		case IDC_SLOT2:
		case IDC_SLOT3:
		case IDC_SLOT4:
		case IDC_SLOT5:
		case IDC_SLOT6:
		case IDC_SLOT7:
			if (HIWORD(wparam) == CBN_SELCHANGE)
			{
				const UINT slot = (LOWORD(wparam) - IDC_SLOT0) / 2;
				const uint32_t newChoiceItem = (uint32_t)SendDlgItemMessage(hWnd, LOWORD(wparam), CB_GETCURSEL, 0, 0);

				SS_CARDTYPE newCard = CT_Empty;
				if (newChoiceItem < m_choicesList[slot].size())
					newCard = m_choicesList[slot][newChoiceItem];
				else
					_ASSERT(0);

				m_PropertySheetHelper.GetConfigNew().m_Slot[slot] = newCard;

				InitOptions(hWnd);
			}
			break;

		case IDC_SLOTAUX:
			if (HIWORD(wparam) == CBN_SELCHANGE)
			{
				const uint32_t newChoiceItem = (uint32_t)SendDlgItemMessage(hWnd, LOWORD(wparam), CB_GETCURSEL, 0, 0);

				SS_CARDTYPE newCard = CT_Empty;
				if (newChoiceItem < m_choicesListAux.size())
					newCard = m_choicesListAux[newChoiceItem];
				else
					_ASSERT(0);

				m_PropertySheetHelper.GetConfigNew().m_SlotAux = newCard;

				//InitOptions(hWnd);	// Not needed, since card in this slot don't affect other slots
			}
			break;

		case IDC_SLOT0_OPTION:
		case IDC_SLOT1_OPTION:
		case IDC_SLOT2_OPTION:
		case IDC_SLOT3_OPTION:
		case IDC_SLOT4_OPTION:
		case IDC_SLOT5_OPTION:
		case IDC_SLOT6_OPTION:
		case IDC_SLOT7_OPTION:
			{
				const UINT slot = (LOWORD(wparam) - IDC_SLOT0_OPTION) / 2;
				const SS_CARDTYPE cardInSlot = m_PropertySheetHelper.GetConfigNew().m_Slot[slot];

				if (cardInSlot == CT_Disk2 || cardInSlot == CT_GenericHDD)
				{
					if (GetCardMgr().GetRef(slot).QueryType() != cardInSlot)
					{
						// NB. Unusual as we create slot object when slot-option is clicked (instead of after OK)
						// Needed as we need a Disk2InterfaceCard or HarddiskInterfaceCard object so that images can be inserted/ejected
						m_PropertySheetHelper.SetSlot(slot, cardInSlot);
					}
				}

				if (cardInSlot == CT_Disk2)
				{
					ms_slot = slot;
					DialogBox(GetFrame().g_hInstance, (LPCTSTR)IDD_FLOPPY_DISK_DRIVES, hWnd, CPageSound::DlgProcDisk2);
				}
				else if (cardInSlot == CT_GenericHDD)
				{
					ms_slot = slot;
					DialogBox(GetFrame().g_hInstance, (LPCTSTR)IDD_HARD_DISK_DRIVES, hWnd, CPageSound::DlgProcHarddisk);
				}
				else if (cardInSlot == CT_Uthernet || cardInSlot == CT_Uthernet2)
				{
					m_PageConfigTfe.m_enableVirtualDnsCheckbox = (cardInSlot == CT_Uthernet2);
					DialogBox(GetFrame().g_hInstance, (LPCTSTR)IDD_TFE_SETTINGS_DIALOG, hWnd, CPageConfigTfe::DlgProc);
					m_PropertySheetHelper.GetConfigNew().m_tfeInterface = m_PageConfigTfe.m_tfe_interface_name;
					m_PropertySheetHelper.GetConfigNew().m_tfeVirtualDNS = m_PageConfigTfe.m_tfe_virtual_dns;
				}
				else if (cardInSlot == CT_SSC)
				{
					DialogBox(GetFrame().g_hInstance, (LPCTSTR)IDD_SSC, hWnd, CPageSound::DlgProcSSC);
				}
				else if (cardInSlot == CT_GenericPrinter)
				{
					DialogBox(GetFrame().g_hInstance, (LPCTSTR)IDD_PRINTER, hWnd, CPageSound::DlgProcPrinter);
				}
				else if (cardInSlot == CT_MouseInterface)
				{
					DialogBox(GetFrame().g_hInstance, (LPCTSTR)IDD_MOUSECARD, hWnd, CPageSound::DlgProcMouseCard);
				}
			}
			break;

		case IDC_SLOTAUX_OPTION:
			{
				const SS_CARDTYPE cardInSlot = m_PropertySheetHelper.GetConfigNew().m_SlotAux;
				if (cardInSlot == CT_RamWorksIII)
				{
					DialogBox(GetFrame().g_hInstance, (LPCTSTR)IDD_RAMWORKS3, hWnd, CPageSound::DlgProcRamWorks3);
				}
			}
			break;

		case IDC_SLOT_DEFAULT_CARDS:
			{
				for (UINT slot=SLOT0; slot<NUM_SLOTS; slot++)
					m_PropertySheetHelper.GetConfigNew().m_Slot[slot] = GetCardMgr().QueryDefaultCardForSlot(slot, m_PropertySheetHelper.GetConfigNew().m_Apple2Type);

				if (IsAppleIIe(m_PropertySheetHelper.GetConfigNew().m_Apple2Type))
					m_PropertySheetHelper.GetConfigNew().m_SlotAux = GetCardMgr().QueryDefaultCardForSlot(SLOT_AUX, m_PropertySheetHelper.GetConfigNew().m_Apple2Type);

				InitOptions(hWnd);
			}
			break;
		}
		break;

	case WM_INITDIALOG:
		InitOptions(hWnd);
		break;

	default:
		return FALSE;
	}

	return TRUE;
}

void CPageSound::DlgOK(HWND hWnd)
{
	m_PropertySheetHelper.PostMsgAfterClose(hWnd, m_Page);
}

int CPageSound::CardTypeToComboItem(UINT slot)
{
	int currentChoice = 0;

	if (slot < NUM_SLOTS)
	{
		for (UINT i = 0; i < m_choicesList[slot].size(); i++)
			if (m_PropertySheetHelper.GetConfigNew().m_Slot[slot] == m_choicesList[slot][i])
				currentChoice = i;
	}
	else
	{
		_ASSERT(slot == SLOT_AUX);
		for (UINT i = 0; i < m_choicesListAux.size(); i++)
			if (m_PropertySheetHelper.GetConfigNew().m_SlotAux == m_choicesListAux[i])
				currentChoice = i;
	}

	return currentChoice;
}

void CPageSound::InitOptions(HWND hWnd)
{
	BOOL enable = FALSE;

	SS_CARDTYPE currConfig[NUM_SLOTS];
	for (int i = SLOT0; i < NUM_SLOTS; i++)
		currConfig[i] = m_PropertySheetHelper.GetConfigNew().m_Slot[i];

	if (IsApple2PlusOrClone(m_PropertySheetHelper.GetConfigNew().m_Apple2Type))
	{
		std::string choices;
		GetCardMgr().GetCardChoicesForSlot(SLOT0, currConfig, choices, m_choicesList[SLOT0]);
		int currentChoice = CardTypeToComboItem(SLOT0);
		m_PropertySheetHelper.FillComboBox(hWnd, IDC_SLOT0, choices.c_str(), currentChoice);
		enable = TRUE;
	}
	else
	{
		enable = FALSE;
	}
	EnableWindow(GetDlgItem(hWnd, IDC_SLOT0), enable);
	EnableWindow(GetDlgItem(hWnd, IDC_SLOT0_OPTION), enable);

	for (int slot = SLOT1; slot < NUM_SLOTS; slot++)
	{
		std::string choices;
		GetCardMgr().GetCardChoicesForSlot(slot, currConfig, choices, m_choicesList[slot]);
		int currentChoice = CardTypeToComboItem(slot);
		m_PropertySheetHelper.FillComboBox(hWnd, IDC_SLOT0 + slot * 2, choices.c_str(), currentChoice);
	}

	if (IsAppleIIe(m_PropertySheetHelper.GetConfigNew().m_Apple2Type))
	{
		std::string choices;
		GetCardMgr().GetCardChoicesForAuxSlot(choices, m_choicesListAux);
		int currentChoice = CardTypeToComboItem(SLOT_AUX);
		m_PropertySheetHelper.FillComboBox(hWnd, IDC_SLOTAUX, choices.c_str(), currentChoice);
		enable = TRUE;
	}
	else
	{
		enable = FALSE;
	}
	EnableWindow(GetDlgItem(hWnd, IDC_SLOTAUX), enable);
	EnableWindow(GetDlgItem(hWnd, IDC_SLOTAUX_OPTION), enable);

	//

	// Find any Uthernet/Uthernet2 card and get its config
	// NB. Assume we don't have both cards inserted - just use the 1st one we find

	SS_CARDTYPE card = CT_Empty;

	for (int slot = SLOT1; slot < NUM_SLOTS; slot++)
	{
		card = GetCardMgr().QuerySlot(slot);
		if (card == CT_Uthernet || card == CT_Uthernet2)
		{
			m_PageConfigTfe.m_tfe_interface_name = PCapBackend::GetRegistryInterface(slot);
			m_PageConfigTfe.m_tfe_virtual_dns = Uthernet2::GetRegistryVirtualDNS(slot);
			break;
		}
	}
}

//===========================================================================

INT_PTR CALLBACK CPageSound::DlgProcDisk2(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	// Switch from static func to our instance
	return CPageSound::ms_this->DlgProcDisk2Internal(hWnd, message, wparam, lparam);
}

INT_PTR CPageSound::DlgProcDisk2Internal(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	switch (message)
	{
	case WM_COMMAND:
		switch (LOWORD(wparam))
		{
		case IDC_SLOT_OPT_COMBO_DISK1:
			if (HIWORD(wparam) == CBN_SELCHANGE)
			{
				HandleFloppyDriveCombo(hWnd, DRIVE_1, LOWORD(wparam), ms_slot);
				GetFrame().FrameRefreshStatus(DRAW_BUTTON_DRIVES | DRAW_DISK_STATUS);
			}
			break;

		case IDC_SLOT_OPT_COMBO_DISK2:
			if (HIWORD(wparam) == CBN_SELCHANGE)
			{
				HandleFloppyDriveCombo(hWnd, DRIVE_2, LOWORD(wparam), ms_slot);
				GetFrame().FrameRefreshStatus(DRAW_BUTTON_DRIVES | DRAW_DISK_STATUS);
			}
			break;

		case IDC_SLOT_OPT_DISK_SWAP:
			HandleFloppyDriveSwap(hWnd, ms_slot);
			break;

		case IDOK:
			DlgDisk2OK(hWnd);
			EndDialog(hWnd, 0);
			break;

		case IDCANCEL:
			EndDialog(hWnd, 0);
			break;

		default:
			return FALSE;
		}
		break;

	case WM_CLOSE:
		EndDialog(hWnd, 0);
		break;

	case WM_INITDIALOG:
		if (ms_slot == SLOT5 || ms_slot == SLOT6)
			CheckDlgButton(hWnd, IDC_DISKII_STATUS_ENABLE, Win32Frame::GetWin32Frame().GetWindowedModeShowDiskiiStatus() ? BST_CHECKED : BST_UNCHECKED);
		else
			EnableWindow(GetDlgItem(hWnd, IDC_DISKII_STATUS_ENABLE), FALSE);
		InitComboFloppyDrive(hWnd, ms_slot);
		break;

	default:
		return FALSE;
	}

	return TRUE;
}

void CPageSound::InitComboFloppyDrive(HWND hWnd, UINT slot)
{
	m_PropertySheetHelper.FillComboBox(hWnd, IDC_SLOT_OPT_COMBO_DISK1, m_defaultDiskOptions, -1);
	m_PropertySheetHelper.FillComboBox(hWnd, IDC_SLOT_OPT_COMBO_DISK2, m_defaultDiskOptions, -1);

	Disk2InterfaceCard& card = dynamic_cast<Disk2InterfaceCard&>(GetCardMgr().GetRef(slot));

	if (!card.GetFullName(DRIVE_1).empty())
	{
		SendDlgItemMessage(hWnd, IDC_SLOT_OPT_COMBO_DISK1, CB_INSERTSTRING, 0, (LPARAM)card.GetFullName(DRIVE_1).c_str());
		SendDlgItemMessage(hWnd, IDC_SLOT_OPT_COMBO_DISK1, CB_SETCURSEL, 0, 0);
	}

	if (!card.GetFullName(DRIVE_2).empty())
	{
		SendDlgItemMessage(hWnd, IDC_SLOT_OPT_COMBO_DISK2, CB_INSERTSTRING, 0, (LPARAM)card.GetFullName(DRIVE_2).c_str());
		SendDlgItemMessage(hWnd, IDC_SLOT_OPT_COMBO_DISK2, CB_SETCURSEL, 0, 0);
	}
}

void CPageSound::HandleFloppyDriveCombo(HWND hWnd, UINT driveSelected, UINT comboSelected, UINT slot)
{
	Disk2InterfaceCard& card = dynamic_cast<Disk2InterfaceCard&>(GetCardMgr().GetRef(slot));

	// Search from "select floppy drive"
	uint32_t dwOpenDialogIndex = (uint32_t)SendDlgItemMessage(hWnd, comboSelected, CB_FINDSTRINGEXACT, -1, (LPARAM)&m_defaultDiskOptions[0]);
	uint32_t dwComboSelection = (uint32_t)SendDlgItemMessage(hWnd, comboSelected, CB_GETCURSEL, 0, 0);

	SendDlgItemMessage(hWnd, comboSelected, CB_SETCURSEL, -1, 0);	// Set to "empty" item

	if (dwComboSelection == dwOpenDialogIndex)
	{
		EnableFloppyDrive(hWnd, FALSE);	// Prevent multiple Selection dialogs to be triggered
		bool bRes = card.UserSelectNewDiskImage(driveSelected);
		EnableFloppyDrive(hWnd, TRUE);

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

		std::string fullname = card.GetFullName(driveSelected);
		SendDlgItemMessage(hWnd, comboSelected, CB_INSERTSTRING, 0, (LPARAM)fullname.c_str());
		SendDlgItemMessage(hWnd, comboSelected, CB_SETCURSEL, 0, 0);

		// If the FD was in the other combo, remove now
		const uint32_t comboOther = (comboSelected == IDC_SLOT_OPT_COMBO_DISK1) ? IDC_SLOT_OPT_COMBO_DISK2 : IDC_SLOT_OPT_COMBO_DISK1;

		const uint32_t duplicated = (uint32_t)SendDlgItemMessage(hWnd, comboOther, CB_FINDSTRINGEXACT, -1, (LPARAM)fullname.c_str());
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
				card.EjectDisk(driveSelected);
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

void CPageSound::EnableFloppyDrive(HWND hWnd, BOOL enable)
{
	EnableWindow(GetDlgItem(hWnd, IDC_SLOT_OPT_COMBO_DISK1), enable);
	EnableWindow(GetDlgItem(hWnd, IDC_SLOT_OPT_COMBO_DISK2), enable);
	EnableWindow(GetDlgItem(hWnd, IDC_SLOT_OPT_DISK_SWAP), enable);
}

void CPageSound::HandleFloppyDriveSwap(HWND hWnd, UINT slot)
{
	if (!RemovalConfirmation(IDC_SLOT_OPT_DISK_SWAP))
		return;

	if (!dynamic_cast<Disk2InterfaceCard&>(GetCardMgr().GetRef(slot)).DriveSwap())
		return;

	InitComboFloppyDrive(hWnd, slot);
}

void CPageSound::DlgDisk2OK(HWND hWnd)
{
	Disk2InterfaceCard& card = dynamic_cast<Disk2InterfaceCard&>(GetCardMgr().GetRef(ms_slot));

	for (UINT i = DRIVE_1; i < NUM_DRIVES; i++)
		m_PropertySheetHelper.GetConfigNew().m_slotInfoForFDC[ms_slot].pathname[i] = card.DiskGetFullPathName(i);

	//

	if (ms_slot == SLOT5 || ms_slot == SLOT6)
	{
		Win32Frame& win32Frame = Win32Frame::GetWin32Frame();
		const bool bNewDiskiiStatus = IsDlgButtonChecked(hWnd, IDC_DISKII_STATUS_ENABLE) ? true : false;

		if (win32Frame.GetWindowedModeShowDiskiiStatus() != bNewDiskiiStatus)
		{
			REGSAVE(REGVALUE_SHOW_DISKII_STATUS, bNewDiskiiStatus ? 1 : 0);
			win32Frame.SetWindowedModeShowDiskiiStatus(bNewDiskiiStatus);

			if (!win32Frame.IsFullScreen())
				win32Frame.FrameRefreshStatus(DRAW_BACKGROUND | DRAW_LEDS | DRAW_DISK_STATUS);
		}
	}
}

//===========================================================================

INT_PTR CALLBACK CPageSound::DlgProcHarddisk(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	// Switch from static func to our instance
	return CPageSound::ms_this->DlgProcHarddiskInternal(hWnd, message, wparam, lparam);
}

INT_PTR CPageSound::DlgProcHarddiskInternal(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	switch (message)
	{
	case WM_COMMAND:
		switch (LOWORD(wparam))
		{
		case IDC_SLOT_OPT_COMBO_HDD1:
			if (HIWORD(wparam) == CBN_SELCHANGE)
			{
				HandleHDDCombo(hWnd, HARDDISK_1, LOWORD(wparam), ms_slot);
			}
			break;

		case IDC_SLOT_OPT_COMBO_HDD2:
			if (HIWORD(wparam) == CBN_SELCHANGE)
			{
				HandleHDDCombo(hWnd, HARDDISK_2, LOWORD(wparam), ms_slot);
			}
			break;

		case IDC_SLOT_OPT_HDD_SWAP:
			HandleHDDSwap(hWnd, ms_slot);
			break;

		case IDOK:
			DlgHarddiskOK(hWnd);
			EndDialog(hWnd, 0);
			break;

		case IDCANCEL:
			EndDialog(hWnd, 0);
			break;

		default:
			return FALSE;
		}
		break;

	case WM_CLOSE:
		EndDialog(hWnd, 0);
		break;

	case WM_INITDIALOG:
		InitComboHDD(hWnd, ms_slot);
		break;

	default:
		return FALSE;
	}

	return TRUE;
}

void CPageSound::InitComboHDD(HWND hWnd, UINT slot)
{
	m_PropertySheetHelper.FillComboBox(hWnd, IDC_SLOT_OPT_COMBO_HDD1, m_defaultHDDOptions, -1);
	m_PropertySheetHelper.FillComboBox(hWnd, IDC_SLOT_OPT_COMBO_HDD2, m_defaultHDDOptions, -1);

	HarddiskInterfaceCard& card = dynamic_cast<HarddiskInterfaceCard&>(GetCardMgr().GetRef(slot));

	if (!card.GetFullName(HARDDISK_1).empty())
	{
		SendDlgItemMessage(hWnd, IDC_SLOT_OPT_COMBO_HDD1, CB_INSERTSTRING, 0, (LPARAM)card.GetFullName(HARDDISK_1).c_str());
		SendDlgItemMessage(hWnd, IDC_SLOT_OPT_COMBO_HDD1, CB_SETCURSEL, 0, 0);
	}

	if (!card.GetFullName(HARDDISK_2).empty())
	{
		SendDlgItemMessage(hWnd, IDC_SLOT_OPT_COMBO_HDD2, CB_INSERTSTRING, 0, (LPARAM)card.GetFullName(HARDDISK_2).c_str());
		SendDlgItemMessage(hWnd, IDC_SLOT_OPT_COMBO_HDD2, CB_SETCURSEL, 0, 0);
	}
}

void CPageSound::HandleHDDCombo(HWND hWnd, UINT driveSelected, UINT comboSelected, UINT slot)
{
	HarddiskInterfaceCard& card = dynamic_cast<HarddiskInterfaceCard&>(GetCardMgr().GetRef(slot));

	// Search from "select hard drive"
	uint32_t dwOpenDialogIndex = (uint32_t)SendDlgItemMessage(hWnd, comboSelected, CB_FINDSTRINGEXACT, -1, (LPARAM)&m_defaultHDDOptions[0]);
	uint32_t dwComboSelection = (uint32_t)SendDlgItemMessage(hWnd, comboSelected, CB_GETCURSEL, 0, 0);

	SendDlgItemMessage(hWnd, comboSelected, CB_SETCURSEL, -1, 0);	// Set to "empty" item

	if (dwComboSelection == dwOpenDialogIndex)
	{
		EnableHDD(hWnd, FALSE);	// Prevent multiple Selection dialogs to be triggered
		bool bRes = card.Select(driveSelected);
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

		SendDlgItemMessage(hWnd, comboSelected, CB_INSERTSTRING, 0, (LPARAM)card.GetFullName(driveSelected).c_str());
		SendDlgItemMessage(hWnd, comboSelected, CB_SETCURSEL, 0, 0);

		// If the HD was in the other combo, remove now
		const uint32_t comboOther = (comboSelected == IDC_SLOT_OPT_COMBO_HDD1) ? IDC_SLOT_OPT_COMBO_HDD2 : IDC_SLOT_OPT_COMBO_HDD1;

		const uint32_t duplicated = (uint32_t)SendDlgItemMessage(hWnd, comboOther, CB_FINDSTRINGEXACT, -1, (LPARAM)card.GetFullName(driveSelected).c_str());
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
				// Unplug selected disk
				card.Unplug(driveSelected);
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

void CPageSound::EnableHDD(HWND hWnd, BOOL enable)
{
	EnableWindow(GetDlgItem(hWnd, IDC_SLOT_OPT_COMBO_HDD1), enable);
	EnableWindow(GetDlgItem(hWnd, IDC_SLOT_OPT_COMBO_HDD2), enable);
	EnableWindow(GetDlgItem(hWnd, IDC_SLOT_OPT_HDD_SWAP), enable);
}

void CPageSound::HandleHDDSwap(HWND hWnd, UINT slot)
{
	if (!RemovalConfirmation(IDC_SLOT_OPT_HDD_SWAP))
		return;

	if (!dynamic_cast<HarddiskInterfaceCard&>(GetCardMgr().GetRef(slot)).ImageSwap())
		return;

	InitComboHDD(hWnd, slot);
}

void CPageSound::DlgHarddiskOK(HWND hWnd)
{
	HarddiskInterfaceCard& card = dynamic_cast<HarddiskInterfaceCard&>(GetCardMgr().GetRef(ms_slot));

	for (UINT i = HARDDISK_1; i < NUM_HARDDISKS; i++)
		m_PropertySheetHelper.GetConfigNew().m_slotInfoForHDC[ms_slot].pathname[i] = card.HarddiskGetFullPathName(i);
}

//===========================================================================

UINT CPageSound::RemovalConfirmation(UINT command)
{
	bool bMsgBox = true;

	bool isFloppyDisk = false;
	UINT drive = 0;
	if (command == IDC_SLOT_OPT_COMBO_DISK1 || command == IDC_SLOT_OPT_COMBO_DISK2)
	{
		isFloppyDisk = true;
		drive = command - IDC_SLOT_OPT_COMBO_DISK1;
	}

	std::string strText;
	if (isFloppyDisk)
		strText = StrFormat("Do you really want to eject the disk in drive-%c ?", '1' + drive);
	else if (command == IDC_SLOT_OPT_DISK_SWAP)
		strText = "Do you really want to swap the floppy disks?";
	else if (command == IDC_SLOT_OPT_COMBO_HDD1 || command == IDC_SLOT_OPT_COMBO_HDD2)
		strText = StrFormat("Do you really want to unplug harddisk-%c ?", '1' + command - IDC_SLOT_OPT_COMBO_HDD1);
	else if (command == IDC_SLOT_OPT_HDD_SWAP)
		strText = "Do you really want to swap the harddisk images?";
	else
		bMsgBox = false;

	if (bMsgBox)
	{
		int nRes = GetFrame().FrameMessageBox(strText.c_str(), "Eject/Unplug Warning", MB_ICONWARNING | MB_YESNO | MB_SETFOREGROUND);
		if (nRes == IDNO)
			command = 0;
	}

	return command;
}

//===========================================================================

INT_PTR CALLBACK CPageSound::DlgProcSSC(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	// Switch from static func to our instance
	return CPageSound::ms_this->DlgProcSSCInternal(hWnd, message, wparam, lparam);
}

INT_PTR CPageSound::DlgProcSSCInternal(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	switch (message)
	{
	case WM_COMMAND:
		switch (LOWORD(wparam))
		{
		case IDC_SERIALPORT:
			if (HIWORD(wparam) == CBN_SELCHANGE)
			{
				const UINT serialPortItem = (UINT)SendDlgItemMessage(hWnd, IDC_SERIALPORT, CB_GETCURSEL, 0, 0);
				m_PropertySheetHelper.GetConfigNew().m_serialPortItem = serialPortItem;
			}
			break;

		case IDOK:
			EndDialog(hWnd, 0);
			break;

		case IDCANCEL:
			EndDialog(hWnd, 0);
			break;

		default:
			return FALSE;
		}
		break;

	case WM_CLOSE:
		EndDialog(hWnd, 0);
		break;

	case WM_INITDIALOG:
		{
			CSuperSerialCard* card = GetCardMgr().GetSSC();
			_ASSERT(card);
			if (!card) return TRUE;
			const UINT serialPortItem = m_PropertySheetHelper.GetConfigNew().m_serialPortItem;
			m_PropertySheetHelper.FillComboBox(hWnd, IDC_SERIALPORT, card->GetSerialPortChoices().c_str(), serialPortItem);
			EnableWindow(GetDlgItem(hWnd, IDC_SERIALPORT), !card->IsActive() ? TRUE : FALSE);
			break;
		}

	default:
		return FALSE;
	}

	return TRUE;
}

//===========================================================================

INT_PTR CALLBACK CPageSound::DlgProcPrinter(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	// Switch from static func to our instance
	return CPageSound::ms_this->DlgProcPrinterInternal(hWnd, message, wparam, lparam);
}

INT_PTR CPageSound::DlgProcPrinterInternal(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	switch (message)
	{
	case WM_COMMAND:
		switch (LOWORD(wparam))
		{
		case IDC_PRINTER_DUMP_FILENAME_BROWSE:
			{
				std::string strPrinterDumpLoc = m_PropertySheetHelper.BrowseToFile(hWnd, "Select printer dump file", REGVALUE_PRINTER_FILENAME, "Text files (*.txt)\0*.txt\0" "All Files\0*.*\0");
				SendDlgItemMessage(hWnd, IDC_PRINTER_DUMP_FILENAME, WM_SETTEXT, 0, (LPARAM)strPrinterDumpLoc.c_str());
			}
			break;

		case IDOK:
			DlgPrinterOK(hWnd);
			EndDialog(hWnd, 0);
			break;

		case IDCANCEL:
			EndDialog(hWnd, 0);
			break;

		default:
			return FALSE;
		}
		break;

	case WM_CLOSE:
		EndDialog(hWnd, 0);
		break;

	case WM_INITDIALOG:
		{
			ParallelPrinterCard& card = m_PropertySheetHelper.GetConfigNew().m_parallelPrinterCard;

			CheckDlgButton(hWnd, IDC_DUMPTOPRINTER, card.GetDumpToPrinter() ? BST_CHECKED : BST_UNCHECKED);
			CheckDlgButton(hWnd, IDC_PRINTER_CONVERT_ENCODING, card.GetConvertEncoding() ? BST_CHECKED : BST_UNCHECKED);
			CheckDlgButton(hWnd, IDC_PRINTER_FILTER_UNPRINTABLE, card.GetFilterUnprintable() ? BST_CHECKED : BST_UNCHECKED);
			CheckDlgButton(hWnd, IDC_PRINTER_APPEND, card.GetPrinterAppend() ? BST_CHECKED : BST_UNCHECKED);
			SendDlgItemMessage(hWnd, IDC_SPIN_PRINTER_IDLE, UDM_SETRANGE, 0, MAKELONG(999, 0));
			SendDlgItemMessage(hWnd, IDC_SPIN_PRINTER_IDLE, UDM_SETPOS, 0, MAKELONG(card.GetIdleLimit(), 0));
			SendDlgItemMessage(hWnd, IDC_PRINTER_DUMP_FILENAME, WM_SETTEXT, 0, (LPARAM)card.GetFilename().c_str());

			// Need to specify cmd-line switch: -printer-real to enable this control
			EnableWindow(GetDlgItem(hWnd, IDC_DUMPTOPRINTER), card.GetEnableDumpToRealPrinter() ? TRUE : FALSE);
		}
		break;

	default:
		return FALSE;
	}

	return TRUE;
}

void CPageSound::DlgPrinterOK(HWND hWnd)
{
	ParallelPrinterCard& card = m_PropertySheetHelper.GetConfigNew().m_parallelPrinterCard;

	// Update printer dump filename
	{
		char szFilename[MAX_PATH];
		memset(szFilename, 0, sizeof(szFilename));
		*(USHORT*)szFilename = sizeof(szFilename);

		LRESULT nLineLength = SendDlgItemMessage(hWnd, IDC_PRINTER_DUMP_FILENAME, EM_LINELENGTH, 0, 0);

		SendDlgItemMessage(hWnd, IDC_PRINTER_DUMP_FILENAME, EM_GETLINE, 0, (LPARAM)szFilename);

		nLineLength = nLineLength > sizeof(szFilename) - 1 ? sizeof(szFilename) - 1 : nLineLength;
		szFilename[nLineLength] = 0x00;

		card.SetFilename(szFilename);
	}

	card.SetDumpToPrinter(IsDlgButtonChecked(hWnd, IDC_DUMPTOPRINTER) ? true : false);
	card.SetConvertEncoding(IsDlgButtonChecked(hWnd, IDC_PRINTER_CONVERT_ENCODING) ? true : false);
	card.SetFilterUnprintable(IsDlgButtonChecked(hWnd, IDC_PRINTER_FILTER_UNPRINTABLE) ? true : false);
	card.SetPrinterAppend(IsDlgButtonChecked(hWnd, IDC_PRINTER_APPEND) ? true : false);

	card.SetIdleLimit((short)SendDlgItemMessage(hWnd, IDC_SPIN_PRINTER_IDLE, UDM_GETPOS, 0, 0));
}

//===========================================================================

INT_PTR CALLBACK CPageSound::DlgProcMouseCard(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	// Switch from static func to our instance
	return CPageSound::ms_this->DlgProcMouseCardInternal(hWnd, message, wparam, lparam);
}

INT_PTR CPageSound::DlgProcMouseCardInternal(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	switch (message)
	{
	case WM_COMMAND:
		switch (LOWORD(wparam))
		{
		case IDOK:
			DlgMouseCardOK(hWnd);
			EndDialog(hWnd, 0);
			break;

		case IDCANCEL:
			EndDialog(hWnd, 0);
			break;

		default:
			return FALSE;
		}
		break;

	case WM_CLOSE:
		EndDialog(hWnd, 0);
		break;

	case WM_INITDIALOG:
	{
		CheckDlgButton(hWnd, IDC_MOUSE_CROSSHAIR, m_mouseShowCrosshair ? BST_CHECKED : BST_UNCHECKED);
		CheckDlgButton(hWnd, IDC_MOUSE_RESTRICT_TO_WINDOW, m_mouseRestrictToWindow ? BST_CHECKED : BST_UNCHECKED);
	}
	break;

	default:
		return FALSE;
	}

	return TRUE;
}

void CPageSound::DlgMouseCardOK(HWND hWnd)
{
	m_mouseShowCrosshair = IsDlgButtonChecked(hWnd, IDC_MOUSE_CROSSHAIR) ? 1 : 0;
	m_mouseRestrictToWindow = IsDlgButtonChecked(hWnd, IDC_MOUSE_RESTRICT_TO_WINDOW) ? 1 : 0;

	REGSAVE(REGVALUE_MOUSE_CROSSHAIR, m_mouseShowCrosshair);
	REGSAVE(REGVALUE_MOUSE_RESTRICT_TO_WINDOW, m_mouseRestrictToWindow);
}

//===========================================================================

INT_PTR CALLBACK CPageSound::DlgProcRamWorks3(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	// Switch from static func to our instance
	return CPageSound::ms_this->DlgProcRamWorks3Internal(hWnd, message, wparam, lparam);
}

INT_PTR CPageSound::DlgProcRamWorks3Internal(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	switch (message)
	{
	case WM_COMMAND:
		switch (LOWORD(wparam))
		{
		case IDOK:
			DlgRamWorks3OK(hWnd);
			EndDialog(hWnd, 0);
			break;

		case IDCANCEL:
			EndDialog(hWnd, 0);
			break;

		default:
			return FALSE;
		}
		break;

	case WM_CLOSE:
		EndDialog(hWnd, 0);
		break;

	case WM_INITDIALOG:
	{
		SendDlgItemMessage(hWnd, IDC_SLIDER_RW3_SIZE, TBM_SETRANGE, TRUE, MAKELONG(1, 32));
		SendDlgItemMessage(hWnd, IDC_SLIDER_RW3_SIZE, TBM_SETPAGESIZE, 0, 4);
		SendDlgItemMessage(hWnd, IDC_SLIDER_RW3_SIZE, TBM_SETTIC, 0, 1);
		SendDlgItemMessage(hWnd, IDC_SLIDER_RW3_SIZE, TBM_SETTIC, 0, 8);
		SendDlgItemMessage(hWnd, IDC_SLIDER_RW3_SIZE, TBM_SETTIC, 0, 16);
		SendDlgItemMessage(hWnd, IDC_SLIDER_RW3_SIZE, TBM_SETTIC, 0, 24);
		SendDlgItemMessage(hWnd, IDC_SLIDER_RW3_SIZE, TBM_SETTIC, 0, 32);

		const uint32_t size = m_PropertySheetHelper.GetConfigNew().m_RamWorksMemorySize;
		SendDlgItemMessage(hWnd, IDC_SLIDER_RW3_SIZE, TBM_SETPOS, TRUE, size);
	}
	break;

	default:
		return FALSE;
	}

	return TRUE;
}

void CPageSound::DlgRamWorks3OK(HWND hWnd)
{
	const uint32_t size = (uint32_t)SendDlgItemMessage(hWnd, IDC_SLIDER_RW3_SIZE, TBM_GETPOS, 0, 0);
	m_PropertySheetHelper.GetConfigNew().m_RamWorksMemorySize = size;
}
