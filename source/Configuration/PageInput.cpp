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

#include "PageInput.h"
#include "PropertySheet.h"

#include "../Common.h"
#include "../Keyboard.h"
#include "../Registry.h"
#include "../resource/resource.h"

CPageInput* CPageInput::ms_this = 0;	// reinit'd in ctor

// Joystick option choices - NOTE maximum text length is MaxMenuChoiceLen = 40
const char CPageInput::m_szJoyChoice0[] = "Disabled\0";
const char CPageInput::m_szJoyChoice1[] = "PC Joystick #1\0";
const char CPageInput::m_szJoyChoice2[] = "PC Joystick #2\0";
const char CPageInput::m_szJoyChoice3[] = "Keyboard (cursors)\0";
const char CPageInput::m_szJoyChoice4[] = "Keyboard (numpad)\0";
const char CPageInput::m_szJoyChoice5[] = "Mouse\0";
const char CPageInput::m_szJoyChoice6[] = "PC Joystick #1 Thumbstick 2\0";

const char* const CPageInput::m_pszJoy0Choices[J0C_MAX] = {
									CPageInput::m_szJoyChoice0,
									CPageInput::m_szJoyChoice1,	// PC Joystick #1
									CPageInput::m_szJoyChoice3,
									CPageInput::m_szJoyChoice4,
									CPageInput::m_szJoyChoice5 };

const char* const CPageInput::m_pszJoy1Choices[J1C_MAX] = {
									CPageInput::m_szJoyChoice0,
									CPageInput::m_szJoyChoice2,	// PC Joystick #2
									CPageInput::m_szJoyChoice3,
									CPageInput::m_szJoyChoice4,
									CPageInput::m_szJoyChoice5,
									CPageInput::m_szJoyChoice6 };

INT_PTR CALLBACK CPageInput::DlgProc(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	// Switch from static func to our instance
	return CPageInput::ms_this->DlgProcInternal(hWnd, message, wparam, lparam);
}

INT_PTR CPageInput::DlgProcInternal(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam)
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

			/*		// Could use this to display PDL() value
			case UDN_DELTAPOS:
				LPNMUPDOWN lpnmud = (LPNMUPDOWN) lparam;
				if (lpnmud->hdr.idFrom == IDC_SPIN_XTRIM)
				{
					static int x = 0;
					x = lpnmud->iPos + lpnmud->iDelta;
					x = SendDlgItemMessage(hWnd, IDC_SPIN_XTRIM, UDM_GETPOS, 0, 0);
				}
				else if (lpnmud->hdr.idFrom == IDC_SPIN_YTRIM)
				{
					static int y = 0;
					y = lpnmud->iPos + lpnmud->iDelta;
					y = SendDlgItemMessage(hWnd, IDC_SPIN_YTRIM, UDM_GETPOS, 0, 0);
				}
				break;
			*/
			}
		}
		break;

	case WM_COMMAND:
		switch (LOWORD(wparam))
		{
		case IDC_JOYSTICK0:
			if (HIWORD(wparam) == CBN_SELCHANGE)
			{
				uint32_t newJoyType = (uint32_t)SendDlgItemMessage(hWnd, IDC_JOYSTICK0, CB_GETCURSEL, 0, 0);
				m_PropertySheetHelper.GetConfigNew().m_joystickType[JN_JOYSTICK0] = m_nJoy0ChoiceTranlationTbl[newJoyType];
				InitOptions(hWnd);
			}
			break;

		case IDC_JOYSTICK1:
			if (HIWORD(wparam) == CBN_SELCHANGE)
			{
				uint32_t newJoyType = (uint32_t)SendDlgItemMessage(hWnd, IDC_JOYSTICK1, CB_GETCURSEL, 0, 0);
				m_PropertySheetHelper.GetConfigNew().m_joystickType[JN_JOYSTICK1] = m_nJoy0ChoiceTranlationTbl[newJoyType];
				InitOptions(hWnd);
			}
			break;

		case IDC_PASTE_FROM_CLIPBOARD:
			ClipboardInitiatePaste();
			break;
		}
		break;

	case WM_INITDIALOG:
		InitOptions(hWnd);
		break;
	}

	return FALSE;
}

void CPageInput::InitOptions(HWND hWnd)
{
	InitJoystickChoices(hWnd, JN_JOYSTICK0);
	InitJoystickChoices(hWnd, JN_JOYSTICK1);

	SendDlgItemMessage(hWnd, IDC_SPIN_XTRIM, UDM_SETRANGE, 0, MAKELONG(128, -127));
	SendDlgItemMessage(hWnd, IDC_SPIN_YTRIM, UDM_SETRANGE, 0, MAKELONG(128, -127));

	SendDlgItemMessage(hWnd, IDC_SPIN_XTRIM, UDM_SETPOS, 0, MAKELONG(m_PropertySheetHelper.GetConfigNew().m_pdlXTrim, 0));
	SendDlgItemMessage(hWnd, IDC_SPIN_YTRIM, UDM_SETPOS, 0, MAKELONG(m_PropertySheetHelper.GetConfigNew().m_pdlYTrim, 0));

	CheckDlgButton(hWnd, IDC_AUTOFIRE, m_PropertySheetHelper.GetConfigNew().m_autofire ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(hWnd, IDC_CENTERINGCONTROL, m_PropertySheetHelper.GetConfigNew().m_centeringControl == JOYSTICK_MODE_CENTERING ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(hWnd, IDC_CURSORCONTROL, m_PropertySheetHelper.GetConfigNew().m_cursorControl ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(hWnd, IDC_SWAPBUTTONS0AND1, m_PropertySheetHelper.GetConfigNew().m_swapButtons0and1 ? BST_CHECKED : BST_UNCHECKED);

	EnableWindow(GetDlgItem(hWnd, IDC_CURSORCONTROL), JoyUsingKeyboardCursors() ? TRUE : FALSE);
	EnableWindow(GetDlgItem(hWnd, IDC_CENTERINGCONTROL), JoyUsingKeyboard() ? TRUE : FALSE);
}

void CPageInput::DlgOK(HWND hWnd)
{
	uint32_t newJoyType0 = (uint32_t)SendDlgItemMessage(hWnd, IDC_JOYSTICK0, CB_GETCURSEL, 0, 0);
	if (newJoyType0 >= J0C_MAX) newJoyType0 = J0C_DISABLED;	// GH#434
	if (JoySetEmulationType(hWnd, m_nJoy0ChoiceTranlationTbl[newJoyType0], JN_JOYSTICK0, IsMouseCardInAnySlot()))
		REGSAVE(REGVALUE_JOYSTICK0_EMU_TYPE, JoyGetJoyType(0));

	uint32_t newJoyType1 = (uint32_t)SendDlgItemMessage(hWnd, IDC_JOYSTICK1, CB_GETCURSEL, 0, 0);
	if (newJoyType1 >= J1C_MAX) newJoyType1 = J1C_DISABLED;	// GH#434
	if (JoySetEmulationType(hWnd, m_nJoy1ChoiceTranlationTbl[newJoyType1], JN_JOYSTICK1, IsMouseCardInAnySlot()))
		REGSAVE(REGVALUE_JOYSTICK1_EMU_TYPE, JoyGetJoyType(1));

	JoySetTrim((short)SendDlgItemMessage(hWnd, IDC_SPIN_XTRIM, UDM_GETPOS, 0, 0), true);
	JoySetTrim((short)SendDlgItemMessage(hWnd, IDC_SPIN_YTRIM, UDM_GETPOS, 0, 0), false);

	m_uCursorControl = IsDlgButtonChecked(hWnd, IDC_CURSORCONTROL) ? 1 : 0;
	m_bmAutofire = IsDlgButtonChecked(hWnd, IDC_AUTOFIRE) ? 7 : 0;	// bitmap of 3 bits
	m_bSwapButtons0and1 = IsDlgButtonChecked(hWnd, IDC_SWAPBUTTONS0AND1) ? true : false;
	m_uCenteringControl = IsDlgButtonChecked(hWnd, IDC_CENTERINGCONTROL) ? 1 : 0;

	REGSAVE(REGVALUE_PDL_XTRIM, JoyGetTrim(true));
	REGSAVE(REGVALUE_PDL_YTRIM, JoyGetTrim(false));

	REGSAVE(REGVALUE_AUTOFIRE, m_bmAutofire);
	REGSAVE(REGVALUE_CENTERING_CONTROL, m_uCenteringControl);
	REGSAVE(REGVALUE_CURSOR_CONTROL, m_uCursorControl);
	REGSAVE(REGVALUE_SWAP_BUTTONS_0_AND_1, m_bSwapButtons0and1);

	m_PropertySheetHelper.PostMsgAfterClose(hWnd, m_Page);
}

void CPageInput::InitJoystickChoices(HWND hWnd, const int joyNum)
{
	char* pnzJoystickChoices;
	int *pnJoyTranslationTbl;
	int nJoyTranslationTblSize;
	unsigned short nJC_DISABLED, nJC_JOYSTICK, nJC_KEYBD_CURSORS, nJC_KEYBD_NUMPAD, nJC_MOUSE, nJC_MAX;
	char** ppszJoyChoices;

	if (joyNum == JN_JOYSTICK0)
	{
		pnzJoystickChoices = m_joystick0choices;
		pnJoyTranslationTbl = m_nJoy0ChoiceTranlationTbl;
		nJoyTranslationTblSize = sizeof(m_nJoy0ChoiceTranlationTbl);
		nJC_DISABLED = J0C_DISABLED;
		nJC_JOYSTICK = J0C_JOYSTICK1;
		nJC_KEYBD_CURSORS = J0C_KEYBD_CURSORS;
		nJC_KEYBD_NUMPAD = J0C_KEYBD_NUMPAD;
		nJC_MOUSE = J0C_MOUSE;
		nJC_MAX = J0C_MAX;
		ppszJoyChoices = (char**) m_pszJoy0Choices;
	}
	else
	{
		pnzJoystickChoices = m_joystick1choices;
		pnJoyTranslationTbl = m_nJoy1ChoiceTranlationTbl;
		nJoyTranslationTblSize = sizeof(m_nJoy1ChoiceTranlationTbl);
		nJC_DISABLED = J1C_DISABLED;
		nJC_JOYSTICK = J1C_JOYSTICK2;
		nJC_KEYBD_CURSORS = J1C_KEYBD_CURSORS;
		nJC_KEYBD_NUMPAD = J1C_KEYBD_NUMPAD;
		nJC_MOUSE = J1C_MOUSE;
		nJC_MAX = J1C_MAX;
		ppszJoyChoices = (char**) m_pszJoy1Choices;
	}

	char* pszMem = pnzJoystickChoices;
	int nIdx = 0;
	memset(pnJoyTranslationTbl, -1, nJoyTranslationTblSize);

	// Build the Joystick choices string list. These first 2 are always selectable.
	memcpy(pszMem, ppszJoyChoices[nJC_DISABLED], strlen(ppszJoyChoices[nJC_DISABLED])+1);
	pszMem += strlen(ppszJoyChoices[nJC_DISABLED])+1;
	pnJoyTranslationTbl[nIdx++] = nJC_DISABLED;

	memcpy(pszMem, ppszJoyChoices[nJC_JOYSTICK], strlen(ppszJoyChoices[nJC_JOYSTICK])+1);
	pszMem += strlen(ppszJoyChoices[nJC_JOYSTICK])+1;
	pnJoyTranslationTbl[nIdx++] = nJC_JOYSTICK;

	const bool bHasMouseCard = IsMouseCardInAnySlot();

	// Now exclude:
	// . the other Joystick type (if it exists) from this new list
	// . the mouse if the mousecard is plugged in
	const uint32_t joyType = m_PropertySheetHelper.GetConfigNew().m_joystickType[joyNum];
	const uint32_t otherJoyType = m_PropertySheetHelper.GetConfigNew().m_joystickType[joyNum == JN_JOYSTICK0 ? JN_JOYSTICK1 : JN_JOYSTICK0];

	int removedItemCompensation = 0;
	for (UINT i=nJC_KEYBD_CURSORS; i<nJC_MAX; i++)
	{
		if ( ( (i == nJC_KEYBD_CURSORS) || (i == nJC_KEYBD_NUMPAD) ) &&
			( (otherJoyType == nJC_KEYBD_CURSORS) || (otherJoyType == nJC_KEYBD_NUMPAD) )
		  )
		{
			if (i <= joyType)
				removedItemCompensation++;
			continue;
		}

		if (i == nJC_MOUSE && bHasMouseCard)
		{
			if (i <= joyType)
				removedItemCompensation++;
			continue;
		}

		if (otherJoyType != i)
		{
			memcpy(pszMem, ppszJoyChoices[i], strlen(ppszJoyChoices[i])+1);
			pszMem += strlen(ppszJoyChoices[i])+1;
			pnJoyTranslationTbl[nIdx++] = i;
		}
	}

	*pszMem = 0x00;	// Doubly null terminated

	const int nIdcValue = joyNum == JN_JOYSTICK0 ? IDC_JOYSTICK0 : IDC_JOYSTICK1;
	m_PropertySheetHelper.FillComboBox(hWnd, nIdcValue, pnzJoystickChoices, joyType - removedItemCompensation);
}

bool CPageInput::IsMouseCardInAnySlot()
{
	for (UINT i = SLOT1; i < NUM_SLOTS; i++)
	{
		if (m_PropertySheetHelper.GetConfigNew().m_Slot[i] == CT_MouseInterface)
		{
			return true;
		}
	}

	return false;
}

void CPageInput::ResetToDefault()
{
	m_PropertySheetHelper.GetConfigNew().m_joystickType[JN_JOYSTICK0] = kJoystick_Default[JN_JOYSTICK0];
	m_PropertySheetHelper.GetConfigNew().m_joystickType[JN_JOYSTICK1] = kJoystick_Default[JN_JOYSTICK1];
	m_PropertySheetHelper.GetConfigNew().m_pdlXTrim = kPdlXTrim_Default;
	m_PropertySheetHelper.GetConfigNew().m_pdlYTrim = kPdlYTrim_Default;

	m_PropertySheetHelper.GetConfigNew().m_autofire = CPageInput::kAutofire_Default;
	m_PropertySheetHelper.GetConfigNew().m_centeringControl = CPageInput::kCenteringControl_Default;
	m_PropertySheetHelper.GetConfigNew().m_cursorControl = CPageInput::kCursorControl_Default;
	m_PropertySheetHelper.GetConfigNew().m_swapButtons0and1 = CPageInput::kSwapButtons0and1_Default;
}
