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

#include "PageSlots.h"
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

CPageSlots* CPageSlots::ms_this = nullptr;	// reinit'd in ctor
UINT CPageSlots::ms_slot = 0;

const char CPageSlots::m_defaultDiskOptions[] =
				"Select Disk...\0"
				"Eject Disk\0";

const char CPageSlots::m_defaultHDDOptions[] =
				"Select Hard Disk Image...\0"
				"Unplug Hard Disk Image\0";

INT_PTR CALLBACK CPageSlots::DlgProc(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	// Switch from static func to our instance
	return CPageSlots::ms_this->DlgProcInternal(hWnd, message, wparam, lparam);
}

INT_PTR CPageSlots::DlgProcInternal(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam)
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
				const UINT slot = LOWORD(wparam) - IDC_SLOT0;
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

				if (newCard == CT_RamWorksIII)
					m_PropertySheetHelper.GetConfigNew().m_RamWorksMemorySize = kDefaultExMemoryBanksRealRW3;
				else
					m_PropertySheetHelper.GetConfigNew().m_RamWorksMemorySize = 1;	// Must be 1 for all others (Empty, 80Col, Extended80Col)

				InitOptions(hWnd);	// Needed to enable/disable the options button (depending on card)
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
				const UINT slot = LOWORD(wparam) - IDC_SLOT0_OPTION;
				const SS_CARDTYPE cardInSlot = m_PropertySheetHelper.GetConfigNew().m_Slot[slot];

				if (!CardTypeHasOptions(cardInSlot))
					break;

				if (cardInSlot == CT_Disk2)
				{
					ms_slot = slot;
					DialogBox(GetFrame().g_hInstance, (LPCTSTR)IDD_FLOPPY_DISK_DRIVES, hWnd, CPageSlots::DlgProcDisk2);
				}
				else if (cardInSlot == CT_GenericHDD)
				{
					ms_slot = slot;
					DialogBox(GetFrame().g_hInstance, (LPCTSTR)IDD_HARD_DISK_DRIVES, hWnd, CPageSlots::DlgProcHarddisk);
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
					DialogBox(GetFrame().g_hInstance, (LPCTSTR)IDD_SSC, hWnd, CPageSlots::DlgProcSSC);
				}
				else if (cardInSlot == CT_GenericPrinter)
				{
					DialogBox(GetFrame().g_hInstance, (LPCTSTR)IDD_PRINTER, hWnd, CPageSlots::DlgProcPrinter);
				}
				else if (cardInSlot == CT_MouseInterface)
				{
					DialogBox(GetFrame().g_hInstance, (LPCTSTR)IDD_MOUSECARD, hWnd, CPageSlots::DlgProcMouseCard);
				}
			}
			break;

		case IDC_SLOTAUX_OPTION:
			{
				const SS_CARDTYPE cardInSlot = m_PropertySheetHelper.GetConfigNew().m_SlotAux;

				if (!CardTypeHasOptions(cardInSlot))
					break;

				if (cardInSlot == CT_RamWorksIII)
				{
					DialogBox(GetFrame().g_hInstance, (LPCTSTR)IDD_RAMWORKS3, hWnd, CPageSlots::DlgProcRamWorks3);
				}
			}
			break;

		case IDC_SLOT_DEFAULT_CARDS:
			if (!m_PropertySheetHelper.IsOkToResetConfig(hWnd))
				break;
			ResetToDefault();
			InitOptions(hWnd);
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

int CPageSlots::CardTypeToComboItem(UINT slot)
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

BOOL CPageSlots::CardTypeHasOptions(SS_CARDTYPE card)
{
	return (card == CT_Disk2 ||
		card == CT_GenericHDD ||
		card == CT_SSC ||
		card == CT_GenericPrinter ||
		card == CT_MouseInterface ||
		card == CT_Uthernet ||
		card == CT_Uthernet2 ||
		card == CT_RamWorksIII) ? TRUE : FALSE;
}

// For InitOptions(), DlgOK() and ApplyConfigAfterClose(), see comment in PageConfig.cpp about "Property Sheet Page flow"
void CPageSlots::InitOptions(HWND hWnd)
{
	BOOL enable = FALSE, enableOpt = FALSE;

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
		enableOpt = CardTypeHasOptions(m_PropertySheetHelper.GetConfigNew().m_Slot[SLOT0]);
	}
	else
	{
		enable = FALSE;
		enableOpt = FALSE;
	}
	EnableWindow(GetDlgItem(hWnd, IDC_SLOT0), enable);
	EnableWindow(GetDlgItem(hWnd, IDC_SLOT0_OPTION), enableOpt);

	for (int slot = SLOT1; slot < NUM_SLOTS; slot++)
	{
		std::string choices;
		GetCardMgr().GetCardChoicesForSlot(slot, currConfig, choices, m_choicesList[slot]);
		int currentChoice = CardTypeToComboItem(slot);
		m_PropertySheetHelper.FillComboBox(hWnd, IDC_SLOT0 + slot, choices.c_str(), currentChoice);

		enableOpt = CardTypeHasOptions(m_PropertySheetHelper.GetConfigNew().m_Slot[slot]);
		EnableWindow(GetDlgItem(hWnd, IDC_SLOT0_OPTION + slot), enableOpt);
	}

	if (IsAppleIIe(m_PropertySheetHelper.GetConfigNew().m_Apple2Type))
	{
		std::string choices;
		GetCardMgr().GetCardChoicesForAuxSlot(choices, m_choicesListAux);
		int currentChoice = CardTypeToComboItem(SLOT_AUX);
		m_PropertySheetHelper.FillComboBox(hWnd, IDC_SLOTAUX, choices.c_str(), currentChoice);

		enable = TRUE;
		enableOpt = CardTypeHasOptions(m_PropertySheetHelper.GetConfigNew().m_SlotAux);
	}
	else
	{
		enable = FALSE;
		enableOpt = FALSE;
	}
	EnableWindow(GetDlgItem(hWnd, IDC_SLOTAUX), enable);
	EnableWindow(GetDlgItem(hWnd, IDC_SLOTAUX_OPTION), enableOpt);

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

void CPageSlots::DiskCardCleanup()
{
	for (UINT i = DRIVE_1; i < NUM_DRIVES; i++)
		m_PropertySheetHelper.GetConfigNew().m_disk2Card.EjectDisk(i);	// close any open file handles

	for (UINT i = HARDDISK_1; i < NUM_HARDDISKS; i++)
		m_PropertySheetHelper.GetConfigNew().m_harddiskCard.Unplug(i);	// close any open file handles
}

void CPageSlots::DlgCANCEL(HWND hWnd)
{
	DiskCardCleanup();
}

void CPageSlots::DlgOK(HWND hWnd)
{
	DiskCardCleanup();
	m_PropertySheetHelper.PostMsgAfterClose(hWnd, m_Page);
}

void CPageSlots::ApplyConfigAfterClose()
{
	// Disk II card
	for (UINT slot = SLOT0; slot < NUM_SLOTS; slot++)
	{
		if (GetCardMgr().QuerySlot(slot) != CT_Disk2)
			continue;

		Disk2InterfaceCard& card = dynamic_cast<Disk2InterfaceCard&>(GetCardMgr().GetRef(slot));
		for (UINT i = DRIVE_1; i < NUM_DRIVES; i++)
		{
			std::string pathname = m_PropertySheetHelper.GetConfigNew().m_slotInfoForFDC[slot].pathname[i];

			if (card.DiskGetFullPathName(i) == pathname)
				continue;

			ImageError_e error = card.InsertDisk(i, pathname, false, false);
			_ASSERT(error == eIMAGE_ERROR_NONE);	// Should've already been rejected in HandleFloppyDriveCombo()
			if (error != eIMAGE_ERROR_NONE)
				card.NotifyInvalidImage(i, pathname, error);
		}
	}

	GetFrame().FrameRefreshStatus(DRAW_BUTTON_DRIVES | DRAW_DISK_STATUS);

	// Hard disk card
	for (UINT slot = SLOT0; slot < NUM_SLOTS; slot++)
	{
		if (GetCardMgr().QuerySlot(slot) != CT_GenericHDD)
			continue;

		HarddiskInterfaceCard& card = dynamic_cast<HarddiskInterfaceCard&>(GetCardMgr().GetRef(slot));
		for (UINT i = HARDDISK_1; i < NUM_HARDDISKS; i++)
		{
			std::string pathname = m_PropertySheetHelper.GetConfigNew().m_slotInfoForHDC[slot].pathname[i];

			if (card.HarddiskGetFullPathName(i) == pathname)
				continue;

			bool error = card.Insert(i, pathname);
			_ASSERT(error == true);	// Should've already been rejected in HandleFloppyDriveCombo()
			if (error != true)
				card.NotifyInvalidImage(pathname);
		}
	}

	// Parallel Printer card
	CConfigNeedingRestart& config = const_cast<CConfigNeedingRestart&>(m_PropertySheetHelper.GetConfigNew());
	config.m_parallelPrinterCard.SetRegistryConfig();

	if (GetCardMgr().GetParallelPrinterCard())
		*(GetCardMgr().GetParallelPrinterCard()) = config.m_parallelPrinterCard;	// copy object

	// Mouse card
	m_mouseShowCrosshair = m_PropertySheetHelper.GetConfigNew().m_mouseShowCrosshair;
	m_mouseRestrictToWindow = m_PropertySheetHelper.GetConfigNew().m_mouseRestrictToWindow;
	REGSAVE(REGVALUE_MOUSE_CROSSHAIR, m_mouseShowCrosshair);
	REGSAVE(REGVALUE_MOUSE_RESTRICT_TO_WINDOW, m_mouseRestrictToWindow);
}

void CPageSlots::ResetToDefault()
{
	// TODO: each card needs setting to its default config too

	CConfigNeedingRestart& configNew = m_PropertySheetHelper.GetConfigNew();

	for (UINT slot = SLOT0; slot < NUM_SLOTS; slot++)
		configNew.m_Slot[slot] = GetCardMgr().QueryDefaultCardForSlot(slot, configNew.m_Apple2Type);

	if (IsAppleIIe(m_PropertySheetHelper.GetConfigNew().m_Apple2Type))
		configNew.m_SlotAux = GetCardMgr().QueryDefaultCardForSlot(SLOT_AUX, configNew.m_Apple2Type);
}

//===========================================================================

INT_PTR CALLBACK CPageSlots::DlgProcDisk2(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	// Switch from static func to our instance
	return CPageSlots::ms_this->DlgProcDisk2Internal(hWnd, message, wparam, lparam);
}

INT_PTR CPageSlots::DlgProcDisk2Internal(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	switch (message)
	{
	case WM_COMMAND:
		switch (LOWORD(wparam))
		{
		case IDC_SLOT_OPT_COMBO_DISK1:
		case IDC_SLOT_OPT_COMBO_DISK2:
			if (HIWORD(wparam) == CBN_SELCHANGE)
			{
				UINT driveSelected = LOWORD(wparam) - IDC_SLOT_OPT_COMBO_DISK1;
				HandleFloppyDriveCombo(hWnd, driveSelected, LOWORD(wparam), ms_slot);
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

void CPageSlots::InitComboFloppyDrive(HWND hWnd, UINT slot)
{
	for (UINT i = DRIVE_1; i < NUM_DRIVES; i++)
	{
		m_PropertySheetHelper.FillComboBox(hWnd, IDC_SLOT_OPT_COMBO_DISK1 + i, m_defaultDiskOptions, -1);

		std::string pathname = m_PropertySheetHelper.GetConfigNew().m_slotInfoForFDC[slot].pathname[i];
		std::string imagename, fullname;
		GetImageTitle(pathname.c_str(), imagename, fullname);
		if (!pathname.empty())
		{
			SendDlgItemMessage(hWnd, IDC_SLOT_OPT_COMBO_DISK1 + i, CB_INSERTSTRING, 0, (LPARAM)fullname.c_str());
			SendDlgItemMessage(hWnd, IDC_SLOT_OPT_COMBO_DISK1 + i, CB_SETCURSEL, 0, 0);
		}
	}
}

bool CPageSlots::CheckFloppyPathnameInUse(const std::string& pathname, BYTE& inUseSlot, BYTE& inUseDrive)
{
	for (UINT slot = SLOT0; slot < NUM_SLOTS; slot++)
	{
		if (GetCardMgr().QuerySlot(slot) == CT_Disk2)
		{
			for (UINT i = DRIVE_1; i < NUM_DRIVES; i++)
			{
				if (dynamic_cast<Disk2InterfaceCard&>(GetCardMgr().GetRef(slot)).DiskGetFullPathName(i) == pathname)
				{
					inUseSlot = slot;
					inUseDrive = i + 1;
					return true;
				}
			}
		}
	}

	return false;
}

void CPageSlots::HandleFloppyDriveCombo(HWND hWnd, UINT driveSelected, UINT comboSelected, UINT slot)
{
	Disk2InterfaceCard& card = m_PropertySheetHelper.GetConfigNew().m_disk2Card;

	// Search from "select floppy drive"
	uint32_t dwOpenDialogIndex = (uint32_t)SendDlgItemMessage(hWnd, comboSelected, CB_FINDSTRINGEXACT, -1, (LPARAM)&m_defaultDiskOptions[0]);
	uint32_t dwComboSelection = (uint32_t)SendDlgItemMessage(hWnd, comboSelected, CB_GETCURSEL, 0, 0);

	SendDlgItemMessage(hWnd, comboSelected, CB_SETCURSEL, -1, 0);	// Set to "empty" item

	if (dwComboSelection == dwOpenDialogIndex)
	{
		EnableFloppyDrive(hWnd, FALSE);	// Prevent multiple Selection dialogs to be triggered
		std::string pathname;
		DWORD flags = 0;
		bool bRes = card.UserSelectNewDiskImageOnly(driveSelected, "", pathname, flags);
		EnableFloppyDrive(hWnd, TRUE);

		if (!bRes)
		{
			if (SendDlgItemMessage(hWnd, comboSelected, CB_GETCOUNT, 0, 0) == 3)	// If there's already a disk...
				SendDlgItemMessage(hWnd, comboSelected, CB_SETCURSEL, 0, 0);		// then reselect it in the ComboBox
			return;
		}

		// Check if pathname is in use by any of emulator's disk cards
		BYTE inUseSlot = 0, inUseDrive = 0;
		if (CheckFloppyPathnameInUse(pathname, inUseSlot, inUseDrive))
		{
			m_PropertySheetHelper.GetConfigNew().m_slotInfoForFDC[ms_slot].pathname[driveSelected] = "";

			std::string strText = StrFormat("%s already mounted in slot %d, drive %d.", pathname.c_str(), inUseSlot, inUseDrive);
			GetFrame().FrameMessageBox(strText.c_str(), g_pAppTitle.c_str(), MB_ICONEXCLAMATION | MB_SETFOREGROUND);
			return;
		}
		else
		{
			// Not in use: insert image to validate it
			ImageError_e error = card.InsertDisk(driveSelected, pathname, false, false);
			if (error != eIMAGE_ERROR_NONE)
			{
				card.NotifyInvalidImage(driveSelected, pathname, error);
				m_PropertySheetHelper.GetConfigNew().m_slotInfoForFDC[ms_slot].pathname[driveSelected] = "";
				return;
			}

			m_PropertySheetHelper.GetConfigNew().m_slotInfoForFDC[ms_slot].pathname[driveSelected] = pathname;
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
				m_PropertySheetHelper.GetConfigNew().m_slotInfoForFDC[ms_slot].pathname[driveSelected] = "";
				// Remove drive from list
				SendDlgItemMessage(hWnd, comboSelected, CB_DELETESTRING, 0, 0);
			}
			else
			{
				// Eject was cancelled: reselect the disk in the ComboBox
				SendDlgItemMessage(hWnd, comboSelected, CB_SETCURSEL, 0, 0);
			}
		}
	}
}

void CPageSlots::EnableFloppyDrive(HWND hWnd, BOOL enable)
{
	EnableWindow(GetDlgItem(hWnd, IDC_SLOT_OPT_COMBO_DISK1), enable);
	EnableWindow(GetDlgItem(hWnd, IDC_SLOT_OPT_COMBO_DISK2), enable);
	EnableWindow(GetDlgItem(hWnd, IDC_SLOT_OPT_DISK_SWAP), enable);
}

void CPageSlots::HandleFloppyDriveSwap(HWND hWnd, UINT slot)
{
	if (!RemovalConfirmation(IDC_SLOT_OPT_DISK_SWAP))
		return;

	std::string temp = m_PropertySheetHelper.GetConfigNew().m_slotInfoForFDC[ms_slot].pathname[DRIVE_1];
	m_PropertySheetHelper.GetConfigNew().m_slotInfoForFDC[ms_slot].pathname[DRIVE_1] = m_PropertySheetHelper.GetConfigNew().m_slotInfoForFDC[ms_slot].pathname[DRIVE_2];
	m_PropertySheetHelper.GetConfigNew().m_slotInfoForFDC[ms_slot].pathname[DRIVE_2] = temp;

	InitComboFloppyDrive(hWnd, slot);
}

void CPageSlots::DlgDisk2OK(HWND hWnd)
{
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

INT_PTR CALLBACK CPageSlots::DlgProcHarddisk(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	// Switch from static func to our instance
	return CPageSlots::ms_this->DlgProcHarddiskInternal(hWnd, message, wparam, lparam);
}

INT_PTR CPageSlots::DlgProcHarddiskInternal(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	switch (message)
	{
	case WM_COMMAND:
		switch (LOWORD(wparam))
		{
		case IDC_SLOT_OPT_COMBO_HDD1:
		case IDC_SLOT_OPT_COMBO_HDD2:
		case IDC_SLOT_OPT_COMBO_HDD3:
		case IDC_SLOT_OPT_COMBO_HDD4:
		case IDC_SLOT_OPT_COMBO_HDD5:
		case IDC_SLOT_OPT_COMBO_HDD6:
		case IDC_SLOT_OPT_COMBO_HDD7:
		case IDC_SLOT_OPT_COMBO_HDD8:
			if (HIWORD(wparam) == CBN_SELCHANGE)
			{
				UINT driveSelected = LOWORD(wparam) - IDC_SLOT_OPT_COMBO_HDD1;
				HandleHDDCombo(hWnd, driveSelected, LOWORD(wparam), ms_slot);
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

void CPageSlots::InitComboHDD(HWND hWnd, UINT slot)
{
	for (UINT i = HARDDISK_1; i < NUM_HARDDISKS; i++)
	{
		m_PropertySheetHelper.FillComboBox(hWnd, IDC_SLOT_OPT_COMBO_HDD1 + i, m_defaultHDDOptions, -1);

		std::string pathname = m_PropertySheetHelper.GetConfigNew().m_slotInfoForHDC[slot].pathname[i];
		std::string imagename, fullname;
		GetImageTitle(pathname.c_str(), imagename, fullname);
		if (!pathname.empty())
		{
			SendDlgItemMessage(hWnd, IDC_SLOT_OPT_COMBO_HDD1 + i, CB_INSERTSTRING, 0, (LPARAM)fullname.c_str());
			SendDlgItemMessage(hWnd, IDC_SLOT_OPT_COMBO_HDD1 + i, CB_SETCURSEL, 0, 0);
		}
	}
}

bool CPageSlots::CheckHDDPathnameInUse(const std::string& pathname, BYTE& inUseSlot, BYTE& inUseDrive)
{
	for (UINT slot = SLOT0; slot < NUM_SLOTS; slot++)
	{
		if (GetCardMgr().QuerySlot(slot) == CT_GenericHDD)
		{
			for (UINT i = HARDDISK_1; i < NUM_HARDDISKS; i++)
			{
				if (dynamic_cast<HarddiskInterfaceCard&>(GetCardMgr().GetRef(slot)).HarddiskGetFullPathName(i) == pathname)
				{
					inUseSlot = slot;
					inUseDrive = i + 1;
					return true;
				}
			}
		}
	}

	return false;
}

void CPageSlots::HandleHDDCombo(HWND hWnd, UINT driveSelected, UINT comboSelected, UINT slot)
{
	HarddiskInterfaceCard& card = m_PropertySheetHelper.GetConfigNew().m_harddiskCard;

	// Search from "select hard drive"
	uint32_t dwOpenDialogIndex = (uint32_t)SendDlgItemMessage(hWnd, comboSelected, CB_FINDSTRINGEXACT, -1, (LPARAM)&m_defaultHDDOptions[0]);
	uint32_t dwComboSelection = (uint32_t)SendDlgItemMessage(hWnd, comboSelected, CB_GETCURSEL, 0, 0);

	SendDlgItemMessage(hWnd, comboSelected, CB_SETCURSEL, -1, 0);	// Set to "empty" item

	if (dwComboSelection == dwOpenDialogIndex)
	{
		EnableHDD(hWnd, FALSE);	// Prevent multiple Selection dialogs to be triggered
		std::string pathname;
		DWORD flags = 0;
		bool bRes = card.UserSelectNewDiskImageOnly(driveSelected, "", pathname, flags);
		EnableHDD(hWnd, TRUE);

		if (!bRes)
		{
			if (SendDlgItemMessage(hWnd, comboSelected, CB_GETCOUNT, 0, 0) == 3)	// If there's already a HDD...
				SendDlgItemMessage(hWnd, comboSelected, CB_SETCURSEL, 0, 0);		// then reselect it in the ComboBox
			return;
		}

		// Check if pathname is in use by any of emulator's disk cards
		BYTE inUseSlot = 0, inUseDrive = 0;
		if (CheckHDDPathnameInUse(pathname, inUseSlot, inUseDrive))
		{
			m_PropertySheetHelper.GetConfigNew().m_slotInfoForHDC[ms_slot].pathname[driveSelected] = "";

			std::string strText = StrFormat("%s already mounted in slot %d, drive %d.", pathname.c_str(), inUseSlot, inUseDrive);
			GetFrame().FrameMessageBox(strText.c_str(), g_pAppTitle.c_str(), MB_ICONEXCLAMATION | MB_SETFOREGROUND);
			return;
		}
		else
		{
			// Not in use: insert image to validate it
			bool error = card.Insert(driveSelected, pathname);
			if (error != true)
			{
				card.NotifyInvalidImage(pathname);
				m_PropertySheetHelper.GetConfigNew().m_slotInfoForHDC[ms_slot].pathname[driveSelected] = "";
				return;
			}

			m_PropertySheetHelper.GetConfigNew().m_slotInfoForHDC[ms_slot].pathname[driveSelected] = pathname;
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
				m_PropertySheetHelper.GetConfigNew().m_slotInfoForHDC[ms_slot].pathname[driveSelected] = "";
				// Remove drive from list
				SendDlgItemMessage(hWnd, comboSelected, CB_DELETESTRING, 0, 0);
			}
			else
			{
				// Cancel the Unplug, and set the menu back to the image name
				SendDlgItemMessage(hWnd, comboSelected, CB_SETCURSEL, 0, 0);
			}
		}
	}
}

void CPageSlots::EnableHDD(HWND hWnd, BOOL enable)
{
	EnableWindow(GetDlgItem(hWnd, IDC_SLOT_OPT_COMBO_HDD1), enable);
	EnableWindow(GetDlgItem(hWnd, IDC_SLOT_OPT_COMBO_HDD2), enable);
	EnableWindow(GetDlgItem(hWnd, IDC_SLOT_OPT_HDD_SWAP), enable);
}

void CPageSlots::HandleHDDSwap(HWND hWnd, UINT slot)
{
	if (!RemovalConfirmation(IDC_SLOT_OPT_HDD_SWAP))
		return;

	std::string temp = m_PropertySheetHelper.GetConfigNew().m_slotInfoForHDC[ms_slot].pathname[HARDDISK_1];
	m_PropertySheetHelper.GetConfigNew().m_slotInfoForHDC[ms_slot].pathname[HARDDISK_1] = m_PropertySheetHelper.GetConfigNew().m_slotInfoForHDC[ms_slot].pathname[HARDDISK_2];
	m_PropertySheetHelper.GetConfigNew().m_slotInfoForHDC[ms_slot].pathname[HARDDISK_2] = temp;

	InitComboHDD(hWnd, slot);
}

void CPageSlots::DlgHarddiskOK(HWND hWnd)
{
}

//===========================================================================

UINT CPageSlots::RemovalConfirmation(UINT command)
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

INT_PTR CALLBACK CPageSlots::DlgProcSSC(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	// Switch from static func to our instance
	return CPageSlots::ms_this->DlgProcSSCInternal(hWnd, message, wparam, lparam);
}

INT_PTR CPageSlots::DlgProcSSCInternal(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam)
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

INT_PTR CALLBACK CPageSlots::DlgProcPrinter(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	// Switch from static func to our instance
	return CPageSlots::ms_this->DlgProcPrinterInternal(hWnd, message, wparam, lparam);
}

INT_PTR CPageSlots::DlgProcPrinterInternal(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam)
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

void CPageSlots::DlgPrinterOK(HWND hWnd)
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

INT_PTR CALLBACK CPageSlots::DlgProcMouseCard(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	// Switch from static func to our instance
	return CPageSlots::ms_this->DlgProcMouseCardInternal(hWnd, message, wparam, lparam);
}

INT_PTR CPageSlots::DlgProcMouseCardInternal(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam)
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

void CPageSlots::DlgMouseCardOK(HWND hWnd)
{
	m_PropertySheetHelper.GetConfigNew().m_mouseShowCrosshair = IsDlgButtonChecked(hWnd, IDC_MOUSE_CROSSHAIR) ? 1 : 0;
	m_PropertySheetHelper.GetConfigNew().m_mouseRestrictToWindow = IsDlgButtonChecked(hWnd, IDC_MOUSE_RESTRICT_TO_WINDOW) ? 1 : 0;
}

//===========================================================================

INT_PTR CALLBACK CPageSlots::DlgProcRamWorks3(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	// Switch from static func to our instance
	return CPageSlots::ms_this->DlgProcRamWorks3Internal(hWnd, message, wparam, lparam);
}

INT_PTR CPageSlots::DlgProcRamWorks3Internal(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam)
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
		SendDlgItemMessage(hWnd, IDC_SLIDER_RW3_SIZE, TBM_SETRANGE, TRUE, MAKELONG(1, 16));
		SendDlgItemMessage(hWnd, IDC_SLIDER_RW3_SIZE, TBM_SETPAGESIZE, 0, 4);
		SendDlgItemMessage(hWnd, IDC_SLIDER_RW3_SIZE, TBM_SETTIC, 0, 1);
		SendDlgItemMessage(hWnd, IDC_SLIDER_RW3_SIZE, TBM_SETTIC, 0, 4);
		SendDlgItemMessage(hWnd, IDC_SLIDER_RW3_SIZE, TBM_SETTIC, 0, 8);
		SendDlgItemMessage(hWnd, IDC_SLIDER_RW3_SIZE, TBM_SETTIC, 0, 12);
		SendDlgItemMessage(hWnd, IDC_SLIDER_RW3_SIZE, TBM_SETTIC, 0, 16);

		const uint32_t size = m_PropertySheetHelper.GetConfigNew().m_RamWorksMemorySize / 16;	// Convert from 64K banks to MB
		SendDlgItemMessage(hWnd, IDC_SLIDER_RW3_SIZE, TBM_SETPOS, TRUE, size);
	}
	break;

	default:
		return FALSE;
	}

	return TRUE;
}

void CPageSlots::DlgRamWorks3OK(HWND hWnd)
{
	const uint32_t size = (uint32_t)SendDlgItemMessage(hWnd, IDC_SLIDER_RW3_SIZE, TBM_GETPOS, 0, 0);
	m_PropertySheetHelper.GetConfigNew().m_RamWorksMemorySize = size * 16;	// Convert from MB to 64K banks
}
