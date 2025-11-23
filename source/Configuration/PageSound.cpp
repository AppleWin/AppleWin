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
#include "../Mockingboard.h"
#include "../Registry.h"
#include "../Speaker.h"
#include "../resource/resource.h"
#include "../Uthernet2.h"
#include "../Tfe/PCapBackend.h"

CPageSound* CPageSound::ms_this = nullptr;	// reinit'd in ctor
UINT CPageSound::ms_slot = 0;

const char CPageSound::m_defaultDiskOptions[] =
				"Select Disk...\0"
				"Eject Disk\0";

const char CPageSound::m_defaultHDDOptions[] =
				"Select Hard Disk Image...\0"
				"Unplug Hard Disk Image\0";

const char CPageSound::m_auxChoices[] =	"80-column (1KB)\0"
										"Extended 80-column (64KB)\0"
										"RamWorks III (1MB)\0"
										"Empty\0";

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
			}
		}
		break;

	case WM_COMMAND:
		switch (LOWORD(wparam))
		{
#if 0
		case IDC_SPKR_VOLUME:
			break;
		case IDC_MB_VOLUME:
			break;
#endif
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
				if (newChoiceItem < choicesList[slot].size())
					newCard = choicesList[slot][newChoiceItem];
				else
					_ASSERT(0);

				m_PropertySheetHelper.GetConfigNew().m_Slot[slot] = newCard;

				if (newCard == CT_Disk2 || newCard == CT_GenericHDD)
					m_PropertySheetHelper.SetSlot(slot, m_PropertySheetHelper.GetConfigNew().m_Slot[slot]);

				InitOptions(hWnd);
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
			const UINT slot = (LOWORD(wparam) - IDC_SLOT0_OPTION) / 2;
			const SS_CARDTYPE cardInSlot = m_PropertySheetHelper.GetConfigNew().m_Slot[slot];
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
				DialogBox(GetFrame().g_hInstance, (LPCTSTR)IDD_TFE_SETTINGS_DIALOG, hWnd, CPageConfigTfe::DlgProc);
				m_PropertySheetHelper.GetConfigNew().m_Slot[slot] = m_PageConfigTfe.m_tfe_selected;
				m_PropertySheetHelper.GetConfigNew().m_tfeInterface = m_PageConfigTfe.m_tfe_interface_name;
				m_PropertySheetHelper.GetConfigNew().m_tfeVirtualDNS = m_PageConfigTfe.m_tfe_virtual_dns;
			}
			break;
		}
		break;

	case WM_INITDIALOG:
		InitOptions(hWnd);
		break;
	}

	return FALSE;
}

void CPageSound::DlgOK(HWND hWnd)
{
	uint32_t dwSoundType = REG_SOUNDTYPE_WAVE;
	REGSAVE(REGVALUE_SOUND_EMULATION, dwSoundType);

	// NB. Volume: 0=Loudest, VOLUME_MAX=Silence
	const uint32_t dwSpkrVolume = 0;
	SpkrSetVolume(dwSpkrVolume, VOLUME_MAX);
	const uint32_t dwMBVolume = 0;
	GetCardMgr().GetMockingboardCardMgr().SetVolume(dwMBVolume, VOLUME_MAX);

	REGSAVE(REGVALUE_SPKR_VOLUME, SpkrGetVolume());
	REGSAVE(REGVALUE_MB_VOLUME, GetCardMgr().GetMockingboardCardMgr().GetVolume());

	m_PropertySheetHelper.PostMsgAfterClose(hWnd, m_Page);
}

CPageSound::AUXCARDCHOICE CPageSound::AuxCardTypeToComboItem(SS_CARDTYPE card)
{
	switch (card)
	{
	case CT_80Col: return SC_80COL;
	case CT_Extended80Col: return SC_EXT80COL;
	case CT_RamWorksIII: return SC_RAMWORKS;
	case CT_Empty: return SC_AUX_EMPTY;
	default: _ASSERT(0); return SC_AUX_EMPTY;
	}
}

int CPageSound::CardTypeToComboItem(UINT slot)
{
	int currentChoice = 0;
	for (UINT i = 0; i < choicesList[slot].size(); i++)
		if (m_PropertySheetHelper.GetConfigNew().m_Slot[slot] == choicesList[slot][i])
			currentChoice = i;

	return currentChoice;
}

void CPageSound::InitOptions(HWND hWnd)
{
	SS_CARDTYPE currConfig[NUM_SLOTS];
	for (int i = SLOT0; i < NUM_SLOTS; i++)
		currConfig[i] = m_PropertySheetHelper.GetConfigNew().m_Slot[i];

	if (IsApple2PlusOrClone(GetApple2Type()))
	{
		std::string choices;
		GetCardMgr().GetCardChoicesForSlot(SLOT0, currConfig, choices, choicesList[SLOT0]);
		int currentChoice = CardTypeToComboItem(SLOT0);
		m_PropertySheetHelper.FillComboBox(hWnd, IDC_SLOT0, choices.c_str(), currentChoice);
	}
	else
	{
		EnableWindow(GetDlgItem(hWnd, IDC_SLOT0), FALSE);
		EnableWindow(GetDlgItem(hWnd, IDC_SLOT0_OPTION), FALSE);
	}

	for (int slot = SLOT1; slot < NUM_SLOTS; slot++)
	{
		std::string choices;
		GetCardMgr().GetCardChoicesForSlot(slot, currConfig, choices, choicesList[slot]);
		int currentChoice = CardTypeToComboItem(slot);
		m_PropertySheetHelper.FillComboBox(hWnd, IDC_SLOT0 + slot * 2, choices.c_str(), currentChoice);
	}

	if (IsAppleIIe(GetApple2Type()))
	{
		const SS_CARDTYPE slotAux = m_PropertySheetHelper.GetConfigNew().m_SlotAux;
		m_PropertySheetHelper.FillComboBox(hWnd, IDC_SLOTAUX, m_auxChoices, (int)AuxCardTypeToComboItem(slotAux));
	}
	else
	{
		EnableWindow(GetDlgItem(hWnd, IDC_SLOTAUX), FALSE);
		EnableWindow(GetDlgItem(hWnd, IDC_SLOTAUX_OPTION), FALSE);
	}

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

	m_PageConfigTfe.m_tfe_selected = card;
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
		case IDOK:
			EndDialog(hWnd, 0);
			return TRUE;

		case IDCANCEL:
			EndDialog(hWnd, 0);
			return TRUE;
		}
		return FALSE;

	case WM_CLOSE:
		EndDialog(hWnd, 0);
		return TRUE;

	case WM_INITDIALOG:
		InitComboFloppyDrive(hWnd, ms_slot);
		return TRUE;
	}

	return FALSE;
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
		case IDOK:
			EndDialog(hWnd, 0);
			return TRUE;

		case IDCANCEL:
			EndDialog(hWnd, 0);
			return TRUE;
		}
		return FALSE;

	case WM_CLOSE:
		EndDialog(hWnd, 0);
		return TRUE;

	case WM_INITDIALOG:
		InitComboHDD(hWnd, ms_slot);
		return TRUE;
	}

	return FALSE;
}

void CPageSound::InitComboHDD(HWND hWnd, UINT slot)
{
	m_PropertySheetHelper.FillComboBox(hWnd, IDC_SLOT_OPT_COMBO_HDD1, m_defaultHDDOptions, -1);
	m_PropertySheetHelper.FillComboBox(hWnd, IDC_SLOT_OPT_COMBO_HDD2, m_defaultHDDOptions, -1);

	HarddiskInterfaceCard& card = dynamic_cast<HarddiskInterfaceCard&>(GetCardMgr().GetRef(SLOT7));

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
	HarddiskInterfaceCard& card = dynamic_cast<HarddiskInterfaceCard&>(GetCardMgr().GetRef(SLOT7));

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
	else if (command == IDC_SLOT_OPT_COMBO_HDD1 || command == IDC_SLOT_OPT_COMBO_HDD2)
		strText = StrFormat("Do you really want to unplug harddisk-%c ?", '1' + command - IDC_SLOT_OPT_COMBO_HDD1);
	else if (command == IDC_HDD_SWAP)
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
