/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 1994-1996, Michael O'Brien
Copyright (C) 1999-2001, Oliver Schmidt
Copyright (C) 2002-2005, Tom Charlesworth
Copyright (C) 2006-2009, Tom Charlesworth, Michael Pohoreski

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

/* Description: Property Sheet Pages
 *
 * Author: Tom Charlesworth
 *         Spiro Trikaliotis <Spiro.Trikaliotis@gmx.de>
 */

#include "StdAfx.h"
#include "Harddisk.h"
#include "..\resource\resource.h"
#include "Tfe\Tfesupp.h"
#include "Tfe\Uilib.h"

TCHAR   computerchoices[] =
				TEXT("Apple ][ (Original)\0")
				TEXT("Apple ][+\0")
				TEXT("Apple //e\0")
				TEXT("Enhanced Apple //e\0")
				TEXT("Clone\0");

TCHAR* szJoyChoice0 = TEXT("Disabled\0");
TCHAR* szJoyChoice1 = TEXT("PC Joystick #1\0");
TCHAR* szJoyChoice2 = TEXT("PC Joystick #2\0");
TCHAR* szJoyChoice3 = TEXT("Keyboard (standard)\0");
TCHAR* szJoyChoice4 = TEXT("Keyboard (centering)\0");
TCHAR* szJoyChoice5 = TEXT("Mouse\0");

static const int g_nMaxJoyChoiceLen = 40;
//eApple2Type NewApple2Type = 0;
static DWORD NewApple2Type = 0;		// TC-FIXME: Also a local NewApple2Type!
static DWORD NewCloneType = 0;		// TC-FIXME: Also a local NewCloneType!

enum JOY0CHOICE {J0C_DISABLED=0, J0C_JOYSTICK1, J0C_KEYBD_STANDARD, J0C_KEYBD_CENTERING, J0C_MOUSE, J0C_MAX};
TCHAR* pszJoy0Choices[J0C_MAX] = {	szJoyChoice0,
									szJoyChoice1,	// PC Joystick #1
									szJoyChoice3,
									szJoyChoice4,
									szJoyChoice5 };
int g_nJoy0ChoiceTranlationTbl[J0C_MAX];
TCHAR joystick0choices[J0C_MAX*g_nMaxJoyChoiceLen];

enum JOY1CHOICE {J1C_DISABLED=0, J1C_JOYSTICK2, J1C_KEYBD_STANDARD, J1C_KEYBD_CENTERING, J1C_MOUSE, J1C_MAX};
TCHAR* pszJoy1Choices[J1C_MAX] = {	szJoyChoice0,
									szJoyChoice2,	// PC Joystick #2
									szJoyChoice3,
									szJoyChoice4,
									szJoyChoice5 };
int g_nJoy1ChoiceTranlationTbl[J1C_MAX];
TCHAR joystick1choices[J1C_MAX*g_nMaxJoyChoiceLen];

TCHAR   soundchoices[]    =  TEXT("Disabled\0")
                             TEXT("PC Speaker (direct)\0")
                             TEXT("PC Speaker (translated)\0")
                             TEXT("Sound Card\0");

TCHAR   discchoices[]     =  TEXT("Authentic Speed\0")
                             TEXT("Enhanced Speed\0");

const UINT VOLUME_MIN = 0;
const UINT VOLUME_MAX = 59;

enum {PG_CONFIG=0, PG_INPUT, PG_SOUND, PG_DISK, PG_ADVANCED, PG_NUM_SHEETS};

UINT g_nLastPage = PG_CONFIG;

// TODO: CLEANUP! Move to peripherals.cpp !!!
// g_nConfig_
UINT g_uScrollLockToggle = 0;
UINT g_uMouseInSlot4 = 0;
UINT g_uMouseShowCrosshair = 0;
UINT g_uMouseRestrictToWindow = 0;

UINT g_uZ80InSlot5 = 0;

//

UINT g_uTheFreezesF8Rom = 0;

#define UNDEFINED ((UINT)-1)
static UINT g_bEnableFreezeDlgButton = UNDEFINED;

//

enum
{
	CLONETYPE_PRAVETS82=0,
	CLONETYPE_PRAVETS8M,
	CLONETYPE_PRAVETS8A,
	CLONETYPE_NUM
};

DWORD g_uCloneType = CLONETYPE_PRAVETS82;

static TCHAR g_CloneChoices[]	=	TEXT("Pravets 82\0")	// Bulgarian
									TEXT("Pravets 8M\0")    // Bulgarian
									TEXT("Pravets 8A\0");	// Bulgarian

//===========================================================================

static void FillComboBox(HWND window, int controlid, LPCTSTR choices, int currentchoice)
{
	_ASSERT(choices);
	HWND combowindow = GetDlgItem(window, controlid);
	SendMessage(combowindow, CB_RESETCONTENT, 0, 0);
	while (choices && *choices)
	{
		SendMessage(combowindow, CB_ADDSTRING, 0, (LPARAM)(LPCTSTR)choices);
		choices += _tcslen(choices)+1;
	}
	SendMessage(combowindow, CB_SETCURSEL, currentchoice, 0);
}

//===========================================================================

static void EnableTrackbar(HWND window, BOOL enable)
{
	EnableWindow(GetDlgItem(window,IDC_SLIDER_CPU_SPEED),enable);
	EnableWindow(GetDlgItem(window,IDC_0_5_MHz),enable);
	EnableWindow(GetDlgItem(window,IDC_1_0_MHz),enable);
	EnableWindow(GetDlgItem(window,IDC_2_0_MHz),enable);
	EnableWindow(GetDlgItem(window,IDC_MAX_MHz),enable);
}

//===========================================================================

static void InitJoystickChoices(HWND window, int nJoyNum, int nIdcValue)
{
	TCHAR *pszMem;
	int nIdx;
	unsigned long i;

	TCHAR* pnzJoystickChoices;
	int *pnJoyTranslationTbl;
	int nJoyTranslationTblSize;
	unsigned short nJC_DISABLED,nJC_JOYSTICK,nJC_KEYBD_STANDARD,nJC_KEYBD_CENTERING,nJC_MAX;
	TCHAR** ppszJoyChoices;
	int nOtherJoyNum = nJoyNum == JN_JOYSTICK0 ? JN_JOYSTICK1 : JN_JOYSTICK0;

	if(nJoyNum == JN_JOYSTICK0)
	{
		pnzJoystickChoices = joystick0choices;
		pnJoyTranslationTbl = g_nJoy0ChoiceTranlationTbl;
		nJoyTranslationTblSize = sizeof(g_nJoy0ChoiceTranlationTbl);
		nJC_DISABLED = J0C_DISABLED;
		nJC_JOYSTICK = J0C_JOYSTICK1;
		nJC_KEYBD_STANDARD = J0C_KEYBD_STANDARD;
		nJC_KEYBD_CENTERING = J0C_KEYBD_CENTERING;
		nJC_MAX = J0C_MAX;
		ppszJoyChoices = pszJoy0Choices;
	}
	else
	{
		pnzJoystickChoices = joystick1choices;
		pnJoyTranslationTbl = g_nJoy1ChoiceTranlationTbl;
		nJoyTranslationTblSize = sizeof(g_nJoy1ChoiceTranlationTbl);
		nJC_DISABLED = J1C_DISABLED;
		nJC_JOYSTICK = J1C_JOYSTICK2;
		nJC_KEYBD_STANDARD = J1C_KEYBD_STANDARD;
		nJC_KEYBD_CENTERING = J1C_KEYBD_CENTERING;
		nJC_MAX = J1C_MAX;
		ppszJoyChoices = pszJoy1Choices;
	}

	pszMem = pnzJoystickChoices;
	nIdx = 0;
	memset(pnJoyTranslationTbl, -1, nJoyTranslationTblSize);

	// Build the Joystick choices string list. These first 2 are always selectable.
	memcpy(pszMem, ppszJoyChoices[nJC_DISABLED], strlen(ppszJoyChoices[nJC_DISABLED])+1);
	pszMem += strlen(ppszJoyChoices[nJC_DISABLED])+1;
	pnJoyTranslationTbl[nIdx++] = nJC_DISABLED;

	memcpy(pszMem, ppszJoyChoices[nJC_JOYSTICK], strlen(ppszJoyChoices[nJC_JOYSTICK])+1);
	pszMem += strlen(ppszJoyChoices[nJC_JOYSTICK])+1;
	pnJoyTranslationTbl[nIdx++] = nJC_JOYSTICK;

	// Now exclude the other Joystick type (if it exists) from this new list
	for(i=nJC_KEYBD_STANDARD; i<nJC_MAX; i++)
	{
		if( ( (i == nJC_KEYBD_STANDARD) || (i == nJC_KEYBD_CENTERING) ) &&
			( (joytype[nOtherJoyNum] == nJC_KEYBD_STANDARD) || (joytype[nOtherJoyNum] == nJC_KEYBD_CENTERING) )
		  )
		{
			continue;
		}

		if(joytype[nOtherJoyNum] != i)
		{
			memcpy(pszMem, ppszJoyChoices[i], strlen(ppszJoyChoices[i])+1);
			pszMem += strlen(ppszJoyChoices[i])+1;
			pnJoyTranslationTbl[nIdx++] = i;
		}
	}

	*pszMem = 0x00;	// Doubly null terminated

	FillComboBox(window, nIdcValue, pnzJoystickChoices, joytype[nJoyNum]);
}

//===========================================================================

static eApple2Type GetApple2Type(DWORD NewCompType, DWORD NewCloneType)
{
	switch (NewCompType)
	{
		case 0:		return A2TYPE_APPLE2;
		case 1:		return A2TYPE_APPLE2PLUS;
		case 2:		return A2TYPE_APPLE2E;
		case 3:		return A2TYPE_APPLE2EEHANCED;
		case 4:		// Clone
			switch (NewCloneType)
			{
			case 0: return A2TYPE_PRAVETS82; break;
			case 1: return A2TYPE_PRAVETS8M; break;
			case 2: return A2TYPE_PRAVETS8A; break;
			}			
		default:	return A2TYPE_APPLE2EEHANCED;
	}
}

// ====================================================================

void Config_Save_Video()
{
	REGSAVE(TEXT(REGVALUE_VIDEO_MODE           ),g_eVideoType);
	REGSAVE(TEXT(REGVALUE_VIDEO_HALF_SCAN_LINES),g_uHalfScanLines);
	REGSAVE(TEXT(REGVALUE_VIDEO_MONO_COLOR     ),monochrome);
}

// ====================================================================

void Config_Load_Video()
{
	REGLOAD(TEXT(REGVALUE_VIDEO_MODE           ),&g_eVideoType);
	REGLOAD(TEXT(REGVALUE_VIDEO_HALF_SCAN_LINES),&g_uHalfScanLines);
	REGLOAD(TEXT(REGVALUE_VIDEO_MONO_COLOR     ),&monochrome);

	if (g_eVideoType >= NUM_VIDEO_MODES)
		g_eVideoType = VT_COLOR_STANDARD; // Old default: VT_COLOR_TVEMU
}


static void ConfigDlg_OK(HWND window, UINT afterclose)
{
	DWORD NewCompType = (DWORD) SendDlgItemMessage(window,IDC_COMPUTER,CB_GETCURSEL,0,0);
	DWORD OldApple2Type = g_Apple2Type;
	eApple2Type NewApple2Type = GetApple2Type(NewCompType, 0);

	DWORD newvidtype    = (DWORD)SendDlgItemMessage(window,IDC_VIDEOTYPE,CB_GETCURSEL,0,0);
	DWORD newserialport = (DWORD)SendDlgItemMessage(window,IDC_SERIALPORT,CB_GETCURSEL,0,0);

	if (OldApple2Type > (APPLECLONE_MASK|APPLE2E_MASK))
		OldApple2Type = (APPLECLONE_MASK|APPLE2E_MASK);

	if (NewApple2Type != OldApple2Type)
	{
		if ((afterclose == WM_USER_RESTART) ||	// Eg. Changing 'Freeze ROM' & user has already OK'd the restart for this
			MessageBox(window,
						TEXT(
						"You have changed the emulated computer "
						"type.  This change will not take effect "
						"until the next time you restart the "
						"emulator.\n\n"
						"Would you like to restart the emulator now?"),
						TEXT("Configuration"),
						MB_ICONQUESTION | MB_YESNO | MB_SETFOREGROUND) == IDYES)
		{
			afterclose = WM_USER_RESTART;
		}
	}

	if (g_eVideoType != newvidtype)
	{
		g_eVideoType = newvidtype;
		VideoReinitialize();
		if ((g_nAppMode != MODE_LOGO) && (g_nAppMode != MODE_DEBUG))
		{
			VideoRedrawScreen();
		}
	}

	sg_SSC.CommSetSerialPort(window,newserialport);
	
	if (IsDlgButtonChecked(window,IDC_AUTHENTIC_SPEED))
		g_dwSpeed = SPEED_NORMAL;
	else
		g_dwSpeed = SendDlgItemMessage(window,IDC_SLIDER_CPU_SPEED,TBM_GETPOS,0,0);

	SetCurrentCLK6502();
	
	if (NewApple2Type > A2TYPE_CLONE) 
		NewCloneType = NewApple2Type - A2TYPE_CLONE;	// TC-FIXME: Must be global scope (ie. g_NewCloneType)

	if ((NewApple2Type == A2TYPE_PRAVETS82) || (NewApple2Type == A2TYPE_PRAVETS8A) || (NewApple2Type == A2TYPE_PRAVETS8M))
		REGSAVE(TEXT(REGVALUE_APPLE2_TYPE),A2TYPE_CLONE );
	else
		REGSAVE(TEXT(REGVALUE_APPLE2_TYPE),NewApple2Type );

	RegSaveString(	TEXT("Configuration"),
					TEXT(REGVALUE_SERIAL_PORT_NAME),
					TRUE,
					sg_SSC.GetSerialPortName() );

	REGSAVE(TEXT("Custom Speed")      ,IsDlgButtonChecked(window,IDC_CUSTOM_SPEED));
	REGSAVE(TEXT("Emulation Speed")   ,g_dwSpeed);

	Config_Save_Video();

	//

	if (afterclose)
		PostMessage(g_hFrameWindow,afterclose,0,0);
}

static void ConfigDlg_CANCEL(HWND window)
{
}

//---------------------------------------------------------------------------

static BOOL CALLBACK ConfigDlgProc( HWND   window,
									UINT   message,
									WPARAM wparam,
									LPARAM lparam)
{
	static UINT afterclose = 0;

	switch (message)
	{
	case WM_NOTIFY:
		{
			// Property Sheet notifications

			switch (((LPPSHNOTIFY)lparam)->hdr.code)
			{
			case PSN_KILLACTIVE:
				// About to stop being active page
				{
					DWORD NewCompType = (DWORD) SendDlgItemMessage(window, IDC_COMPUTER, CB_GETCURSEL, 0, 0);
					g_bEnableFreezeDlgButton = GetApple2Type(NewCompType,0)<=A2TYPE_APPLE2PLUS ? TRUE : FALSE;
					SetWindowLong(window, DWL_MSGRESULT, FALSE);		// Changes are valid
				}
				break;
			case PSN_APPLY:
				SetWindowLong(window, DWL_MSGRESULT, PSNRET_NOERROR);	// Changes are valid
				ConfigDlg_OK(window, afterclose);
				break;
			case PSN_QUERYCANCEL:
				// Can use this to ask user to confirm cancel
				break;
			case PSN_RESET:
				ConfigDlg_CANCEL(window);
				break;
			}
		}
		break;

	case WM_COMMAND:
		switch (LOWORD(wparam))
		{
		case IDC_AUTHENTIC_SPEED:	// Authentic Machine Speed
			SendDlgItemMessage(window,IDC_SLIDER_CPU_SPEED,TBM_SETPOS,1,SPEED_NORMAL);
			EnableTrackbar(window,0);
			break;

		case IDC_CUSTOM_SPEED:		// Select Custom Speed
			SetFocus(GetDlgItem(window,IDC_SLIDER_CPU_SPEED));
			EnableTrackbar(window,1);
			break;

		case IDC_SLIDER_CPU_SPEED:	// CPU speed slider
			CheckRadioButton(window,IDC_AUTHENTIC_SPEED,IDC_CUSTOM_SPEED,IDC_CUSTOM_SPEED);
			EnableTrackbar(window,1);
			break;

		case IDC_BENCHMARK:
			afterclose = WM_USER_BENCHMARK;
			PropSheet_PressButton(GetParent(window), PSBTN_OK);
			break;

		case IDC_ETHERNET:
			ui_tfe_settings_dialog(window);
			break;

		case IDC_MONOCOLOR:
			VideoChooseColor();
			break;

		case IDC_CHECK_HALF_SCAN_LINES:
			g_uHalfScanLines = IsDlgButtonChecked(window, IDC_CHECK_HALF_SCAN_LINES) ? 1 : 0;

#if 0
		case IDC_RECALIBRATE:
			RegSaveValue(TEXT(""),TEXT("RunningOnOS"),0,0);
			if (MessageBox(window,
				TEXT("The emulator has been set to recalibrate ")
				TEXT("itself the next time it is started.\n\n")
				TEXT("Would you like to restart the emulator now?"),
				TEXT("Configuration"),
				MB_ICONQUESTION | MB_YESNO | MB_SETFOREGROUND) == IDYES)
			{
					afterclose = WM_USER_RESTART;
					PropSheet_PressButton(GetParent(window), PSBTN_OK);
			}
			break;
#endif
		} // switch( (LOWORD(wparam))
		break; // WM_COMMAND

	case WM_HSCROLL:
		CheckRadioButton(window, IDC_AUTHENTIC_SPEED, IDC_CUSTOM_SPEED, IDC_CUSTOM_SPEED);	// FirstButton, LastButton, CheckButton
		break;

	case WM_INITDIALOG: //Init general settings dialog
		{
			g_nLastPage = PG_CONFIG;

			UINT iApple2String = 0;
			switch (g_Apple2Type)
			{
			default:
			case A2TYPE_APPLE2:			iApple2String = 0; break;
			case A2TYPE_APPLE2PLUS:		iApple2String = 1; break;
			case A2TYPE_APPLE2E:		iApple2String = 2; break;
			case A2TYPE_APPLE2EEHANCED:	iApple2String = 3; break;
			case A2TYPE_PRAVETS82:		iApple2String = 4; break;
			case A2TYPE_PRAVETS8M:		iApple2String = 5; break;
			case A2TYPE_PRAVETS8A:		iApple2String = 6; break;
			}

			if (iApple2String > 3) 
				FillComboBox(window,IDC_COMPUTER,computerchoices,4);
			else
				FillComboBox(window,IDC_COMPUTER,computerchoices,iApple2String);

			FillComboBox(window,IDC_VIDEOTYPE,g_aVideoChoices,g_eVideoType);
			CheckDlgButton(window, IDC_CHECK_HALF_SCAN_LINES, g_uHalfScanLines ? BST_CHECKED : BST_UNCHECKED);

			FillComboBox(window,IDC_SERIALPORT, sg_SSC.GetSerialPortChoices(), sg_SSC.GetSerialPort());
			EnableWindow(GetDlgItem(window, IDC_SERIALPORT), !sg_SSC.IsActive() ? TRUE : FALSE);

			SendDlgItemMessage(window,IDC_SLIDER_CPU_SPEED,TBM_SETRANGE,1,MAKELONG(0,40));
			SendDlgItemMessage(window,IDC_SLIDER_CPU_SPEED,TBM_SETPAGESIZE,0,5);
			SendDlgItemMessage(window,IDC_SLIDER_CPU_SPEED,TBM_SETTICFREQ,10,0);
			SendDlgItemMessage(window,IDC_SLIDER_CPU_SPEED,TBM_SETPOS,1,g_dwSpeed);

			{
				BOOL custom = 1;
				if (g_dwSpeed == SPEED_NORMAL)
				{
					custom = 0;
					REGLOAD(TEXT("Custom Speed"),(DWORD *)&custom);
				}
				CheckRadioButton(window, IDC_AUTHENTIC_SPEED, IDC_CUSTOM_SPEED, custom ? IDC_CUSTOM_SPEED : IDC_AUTHENTIC_SPEED);
				SetFocus(GetDlgItem(window, custom ? IDC_SLIDER_CPU_SPEED : IDC_AUTHENTIC_SPEED));
				EnableTrackbar(window, custom);
			}

			afterclose = 0;
			break;
		}

	case WM_LBUTTONDOWN:
		{
			POINT pt = { LOWORD(lparam), HIWORD(lparam) };
			ClientToScreen(window,&pt);
			RECT rect;
			GetWindowRect(GetDlgItem(window,IDC_SLIDER_CPU_SPEED),&rect);
			if ((pt.x >= rect.left) && (pt.x <= rect.right) &&
				(pt.y >= rect.top) && (pt.y <= rect.bottom))
			{
				CheckRadioButton(window, IDC_AUTHENTIC_SPEED, IDC_CUSTOM_SPEED, IDC_CUSTOM_SPEED);
				EnableTrackbar(window,1);
				SetFocus(GetDlgItem(window,IDC_SLIDER_CPU_SPEED));
				ScreenToClient(GetDlgItem(window,IDC_SLIDER_CPU_SPEED),&pt);
				PostMessage(GetDlgItem(window,IDC_SLIDER_CPU_SPEED),WM_LBUTTONDOWN,wparam,MAKELONG(pt.x,pt.y));
			}
			break;
		}

	case WM_SYSCOLORCHANGE:
		SendDlgItemMessage(window,IDC_SLIDER_CPU_SPEED,WM_SYSCOLORCHANGE,0,0);
		break;
	}

	return 0;
}

//===========================================================================

static void InputDlg_OK(HWND window, UINT afterclose)
{
	UINT uNewJoyType0 = SendDlgItemMessage(window,IDC_JOYSTICK0,CB_GETCURSEL,0,0);
	UINT uNewJoyType1 = SendDlgItemMessage(window,IDC_JOYSTICK1,CB_GETCURSEL,0,0);

	if (!JoySetEmulationType(window, g_nJoy0ChoiceTranlationTbl[uNewJoyType0], JN_JOYSTICK0))
	{
		afterclose = 0;
		return;
	}
	
	if (!JoySetEmulationType(window, g_nJoy1ChoiceTranlationTbl[uNewJoyType1], JN_JOYSTICK1))
	{
		afterclose = 0;
		return;
	}

	JoySetTrim((short)SendDlgItemMessage(window, IDC_SPIN_XTRIM, UDM_GETPOS, 0, 0), true);
	JoySetTrim((short)SendDlgItemMessage(window, IDC_SPIN_YTRIM, UDM_GETPOS, 0, 0), false);

	g_uMouseShowCrosshair = IsDlgButtonChecked(window, IDC_MOUSE_CROSSHAIR) ? 1 : 0;
	g_uMouseRestrictToWindow = IsDlgButtonChecked(window, IDC_MOUSE_RESTRICT_TO_WINDOW) ? 1 : 0;

	REGSAVE(TEXT("Joystick 0 Emulation"),joytype[0]);
	REGSAVE(TEXT("Joystick 1 Emulation"),joytype[1]);
	REGSAVE(TEXT(REGVALUE_PDL_XTRIM),JoyGetTrim(true));
	REGSAVE(TEXT(REGVALUE_PDL_YTRIM),JoyGetTrim(false));
	REGSAVE(TEXT(REGVALUE_SCROLLLOCK_TOGGLE),g_uScrollLockToggle);
	REGSAVE(TEXT(REGVALUE_MOUSE_IN_SLOT4),g_uMouseInSlot4);
	REGSAVE(TEXT(REGVALUE_MOUSE_CROSSHAIR),g_uMouseShowCrosshair);
	REGSAVE(TEXT(REGVALUE_MOUSE_RESTRICT_TO_WINDOW),g_uMouseRestrictToWindow);
	REGSAVE(TEXT(REGVALUE_Z80_IN_SLOT5),g_uZ80InSlot5);

	//

	if (afterclose)
		PostMessage(g_hFrameWindow,afterclose,0,0);
}

static void InputDlg_CANCEL(HWND window)
{
}

//---------------------------------------------------------------------------

static BOOL CALLBACK InputDlgProc(HWND   window,
								  UINT   message,
								  WPARAM wparam,
								  LPARAM lparam)
{
	static UINT afterclose = 0;

	switch (message)
	{
	case WM_NOTIFY:
		{
			// Property Sheet notifications

			switch (((LPPSHNOTIFY)lparam)->hdr.code)
			{
			case PSN_KILLACTIVE:
				SetWindowLong(window, DWL_MSGRESULT, FALSE);			// Changes are valid
				break;
			case PSN_APPLY:
				SetWindowLong(window, DWL_MSGRESULT, PSNRET_NOERROR);	// Changes are valid
				InputDlg_OK(window, afterclose);
				break;
			case PSN_QUERYCANCEL:
				// Can use this to ask user to confirm cancel
				break;
			case PSN_RESET:
				InputDlg_CANCEL(window);
				break;

			/*		// Could use this to display PDL() value
			case UDN_DELTAPOS:
				LPNMUPDOWN lpnmud = (LPNMUPDOWN) lparam;
				if (lpnmud->hdr.idFrom == IDC_SPIN_XTRIM)
				{
					static int x = 0;
					x = lpnmud->iPos + lpnmud->iDelta;
					x = SendDlgItemMessage(window, IDC_SPIN_XTRIM, UDM_GETPOS, 0, 0);
				}
				else if (lpnmud->hdr.idFrom == IDC_SPIN_YTRIM)
				{
					static int y = 0;
					y = lpnmud->iPos + lpnmud->iDelta;
					y = SendDlgItemMessage(window, IDC_SPIN_YTRIM, UDM_GETPOS, 0, 0);
				}
				break;
			*/
			}
		}
		break;

	case WM_COMMAND:
		switch (LOWORD(wparam))
		{
		case IDC_JOYSTICK0: // joystick0
			if(HIWORD(wparam) == CBN_SELCHANGE)
			{
				DWORD newjoytype = (DWORD)SendDlgItemMessage(window,IDC_JOYSTICK0,CB_GETCURSEL,0,0);
				JoySetEmulationType(window,g_nJoy0ChoiceTranlationTbl[newjoytype],JN_JOYSTICK0);
				InitJoystickChoices(window, JN_JOYSTICK1, IDC_JOYSTICK1);	// Re-init joy1 list
			}
			break;

		case IDC_JOYSTICK1: // joystick1
			if(HIWORD(wparam) == CBN_SELCHANGE)
			{
				DWORD newjoytype = (DWORD)SendDlgItemMessage(window,IDC_JOYSTICK1,CB_GETCURSEL,0,0);
				JoySetEmulationType(window,g_nJoy1ChoiceTranlationTbl[newjoytype],JN_JOYSTICK1);
				InitJoystickChoices(window, JN_JOYSTICK0, IDC_JOYSTICK0);	// Re-init joy0 list
			}
			break;

		case IDC_SCROLLLOCK_TOGGLE:
			g_uScrollLockToggle = IsDlgButtonChecked(window, IDC_SCROLLLOCK_TOGGLE) ? 1 : 0;
			break;

		case IDC_MOUSE_IN_SLOT4:
			{
				UINT uNewState = IsDlgButtonChecked(window, IDC_MOUSE_IN_SLOT4) ? 1 : 0;
				LPCSTR pMsg = uNewState ?
					TEXT("The emulator needs to restart as the slot configuration has changed.\n\n")
					TEXT("Also Mockingboard/Phasor cards won't be available in slot 4\n")
					TEXT("and the mouse can't be used for joystick emulation.\n\n")
					TEXT("Would you like to restart the emulator now?")
					:
				TEXT("The emulator needs to restart as the slot configuration has changed.\n\n")
					TEXT("(Mockingboard/Phasor cards will now be available in slot 4\n")
					TEXT("and the mouse can be used for joystick emulation)\n\n")
					TEXT("Would you like to restart the emulator now?");
				if (MessageBox(window,
					pMsg,
					TEXT("Configuration"),
					MB_ICONQUESTION | MB_YESNO | MB_SETFOREGROUND) == IDYES)
				{
					g_uMouseInSlot4 = uNewState;

					if (uNewState)
					{
						JoyDisableUsingMouse();
						InitJoystickChoices(window, JN_JOYSTICK0, IDC_JOYSTICK0);
						InitJoystickChoices(window, JN_JOYSTICK1, IDC_JOYSTICK1);
					}

					afterclose = WM_USER_RESTART;
					PropSheet_PressButton(GetParent(window), PSBTN_OK);
				}
				else
				{
					CheckDlgButton(window, IDC_MOUSE_IN_SLOT4, g_uMouseInSlot4 ? BST_CHECKED : BST_UNCHECKED);
				}
			}
			break;

		case IDC_Z80_IN_SLOT5:
			{
				UINT uNewState = IsDlgButtonChecked(window, IDC_Z80_IN_SLOT5) ? 1 : 0;
				LPCSTR pMsg = uNewState ?
					TEXT("The emulator needs to restart as the slot configuration has changed.\n")
					TEXT("Microsoft CP/M SoftCard will be placed in slot 5.\n\n")
					TEXT("Would you like to restart the emulator now?")
					:
				TEXT("The emulator needs to restart as the slot configuration has changed.\n")
					TEXT("Microsoft CP/M SoftCard will be removed from slot 5\n\n")
					TEXT("Would you like to restart the emulator now?");
				if (MessageBox(window,
					pMsg,
					TEXT("Configuration"),
					MB_ICONQUESTION | MB_YESNO | MB_SETFOREGROUND) == IDYES)
				{
					g_uZ80InSlot5 = uNewState;
					afterclose = WM_USER_RESTART;
					PropSheet_PressButton(GetParent(window), PSBTN_OK);
				}
				else
				{
					CheckDlgButton(window, IDC_Z80_IN_SLOT5, g_uZ80InSlot5 ? BST_CHECKED : BST_UNCHECKED);
				}
			}
			break;

		case IDC_PASTE_FROM_CLIPBOARD:
			ClipboardInitiatePaste();
			break;
		}
		break;

	case WM_INITDIALOG: //Init input settings dialog
		{
			g_nLastPage = PG_INPUT;

			InitJoystickChoices(window, JN_JOYSTICK0, IDC_JOYSTICK0);
			InitJoystickChoices(window, JN_JOYSTICK1, IDC_JOYSTICK1);

			SendDlgItemMessage(window, IDC_SPIN_XTRIM, UDM_SETRANGE, 0, MAKELONG(128,-127));
			SendDlgItemMessage(window, IDC_SPIN_YTRIM, UDM_SETRANGE, 0, MAKELONG(128,-127));

			SendDlgItemMessage(window, IDC_SPIN_XTRIM, UDM_SETPOS, 0, MAKELONG(JoyGetTrim(true),0));
			SendDlgItemMessage(window, IDC_SPIN_YTRIM, UDM_SETPOS, 0, MAKELONG(JoyGetTrim(false),0));

			CheckDlgButton(window, IDC_SCROLLLOCK_TOGGLE, g_uScrollLockToggle ? BST_CHECKED : BST_UNCHECKED);
			CheckDlgButton(window, IDC_MOUSE_IN_SLOT4, g_uMouseInSlot4 ? BST_CHECKED : BST_UNCHECKED);
			CheckDlgButton(window, IDC_MOUSE_CROSSHAIR, g_uMouseShowCrosshair ? BST_CHECKED : BST_UNCHECKED);
			CheckDlgButton(window, IDC_MOUSE_RESTRICT_TO_WINDOW, g_uMouseRestrictToWindow ? BST_CHECKED : BST_UNCHECKED);
			EnableWindow(GetDlgItem(window, IDC_MOUSE_CROSSHAIR), g_uMouseInSlot4 ? TRUE : FALSE);
			EnableWindow(GetDlgItem(window, IDC_MOUSE_RESTRICT_TO_WINDOW), g_uMouseInSlot4 ? TRUE : FALSE);
			CheckDlgButton(window, IDC_Z80_IN_SLOT5, g_uZ80InSlot5 ? BST_CHECKED : BST_UNCHECKED);

			afterclose = 0;
			break;
		}
	}

	return 0;
}

//===========================================================================

static void SoundDlg_OK(HWND window, UINT afterclose, UINT uNewSoundcardType)
{
	DWORD newsoundtype  = (DWORD)SendDlgItemMessage(window,IDC_SOUNDTYPE,CB_GETCURSEL,0,0);

	DWORD dwSpkrVolume = SendDlgItemMessage(window,IDC_SPKR_VOLUME,TBM_GETPOS,0,0);
	DWORD dwMBVolume   = SendDlgItemMessage(window,IDC_MB_VOLUME,TBM_GETPOS,0,0);

	if (!SpkrSetEmulationType(window,newsoundtype))
	{
		afterclose = 0;
		return;
	}

	// NB. Volume: 0=Loudest, VOLUME_MAX=Silence
	SpkrSetVolume(dwSpkrVolume, VOLUME_MAX);
	MB_SetVolume(dwMBVolume, VOLUME_MAX);

	MB_SetSoundcardType((eSOUNDCARDTYPE)uNewSoundcardType);

	REGSAVE(TEXT("Sound Emulation")   ,soundtype);
	REGSAVE(TEXT(REGVALUE_SPKR_VOLUME),SpkrGetVolume());
	REGSAVE(TEXT(REGVALUE_MB_VOLUME),MB_GetVolume());
	REGSAVE(TEXT(REGVALUE_SOUNDCARD_TYPE),(DWORD)MB_GetSoundcardType());

	//

	if (afterclose)
		PostMessage(g_hFrameWindow,afterclose,0,0);
}

static void SoundDlg_CANCEL(HWND window)
{
}

//---------------------------------------------------------------------------

static BOOL CALLBACK SoundDlgProc (HWND   window,
								   UINT   message,
								   WPARAM wparam,
								   LPARAM lparam)
{
	static UINT afterclose = 0;
	static UINT uNewSoundcardType = SC_UNINIT;

	switch (message)
	{
	case WM_NOTIFY:
		{
			// Property Sheet notifications

			switch (((LPPSHNOTIFY)lparam)->hdr.code)
			{
			case PSN_KILLACTIVE:
				SetWindowLong(window, DWL_MSGRESULT, FALSE);			// Changes are valid
				break;
			case PSN_APPLY:
				SetWindowLong(window, DWL_MSGRESULT, PSNRET_NOERROR);	// Changes are valid
				SoundDlg_OK(window, afterclose, uNewSoundcardType);
				break;
			case PSN_QUERYCANCEL:
				// Can use this to ask user to confirm cancel
				break;
			case PSN_RESET:
				SoundDlg_CANCEL(window);
				break;
			}
		}
		break;

	case WM_COMMAND:
		switch (LOWORD(wparam))
		{
		case IDC_SPKR_VOLUME:
			break;
		case IDC_MB_VOLUME:
			break;
		case IDC_MB_ENABLE:
			uNewSoundcardType = SC_MOCKINGBOARD;
			EnableWindow(GetDlgItem(window, IDC_MB_VOLUME), TRUE);
			break;
		case IDC_PHASOR_ENABLE:
			uNewSoundcardType = SC_PHASOR;
			EnableWindow(GetDlgItem(window, IDC_MB_VOLUME), TRUE);
			break;
		case IDC_SOUNDCARD_DISABLE:
			uNewSoundcardType = SC_NONE;
			EnableWindow(GetDlgItem(window, IDC_MB_VOLUME), FALSE);
			break;
		}
		break;

	case WM_INITDIALOG:
		{
			g_nLastPage = PG_SOUND;

			FillComboBox(window,IDC_SOUNDTYPE,soundchoices,soundtype);

			SendDlgItemMessage(window,IDC_SPKR_VOLUME,TBM_SETRANGE,1,MAKELONG(VOLUME_MIN,VOLUME_MAX));
			SendDlgItemMessage(window,IDC_SPKR_VOLUME,TBM_SETPAGESIZE,0,10);
			SendDlgItemMessage(window,IDC_SPKR_VOLUME,TBM_SETTICFREQ,10,0);
			SendDlgItemMessage(window,IDC_SPKR_VOLUME,TBM_SETPOS,1,SpkrGetVolume());

			SendDlgItemMessage(window,IDC_MB_VOLUME,TBM_SETRANGE,1,MAKELONG(VOLUME_MIN,VOLUME_MAX));
			SendDlgItemMessage(window,IDC_MB_VOLUME,TBM_SETPAGESIZE,0,10);
			SendDlgItemMessage(window,IDC_MB_VOLUME,TBM_SETTICFREQ,10,0);
			SendDlgItemMessage(window,IDC_MB_VOLUME,TBM_SETPOS,1,MB_GetVolume());

			int nID;
			eSOUNDCARDTYPE SoundcardType = MB_GetSoundcardType();
			if(SoundcardType == SC_MOCKINGBOARD)
				nID = IDC_MB_ENABLE;
			else if(SoundcardType == SC_PHASOR)
				nID = IDC_PHASOR_ENABLE;
			else
				nID = IDC_SOUNDCARD_DISABLE;

			CheckRadioButton(window, IDC_MB_ENABLE, IDC_SOUNDCARD_DISABLE, nID);

			if (g_uMouseInSlot4)
			{
				EnableWindow(GetDlgItem(window, IDC_PHASOR_ENABLE), FALSE);
			}

			if (g_uZ80InSlot5)
			{
				EnableWindow(GetDlgItem(window, IDC_MB_ENABLE), FALSE);
			}

			afterclose = 0;
			break;
		}
	}

	return 0;
}

//===========================================================================

static void EnableHDD(HWND window, BOOL bEnable)
{
	EnableWindow(GetDlgItem(window, IDC_HDD1), bEnable);
	EnableWindow(GetDlgItem(window, IDC_EDIT_HDD1), bEnable);

	EnableWindow(GetDlgItem(window, IDC_HDD2), bEnable);
	EnableWindow(GetDlgItem(window, IDC_EDIT_HDD2), bEnable);
}

//---------------------------------------------------------------------------

static void DiskDlg_OK(HWND window, UINT afterclose)
{
	BOOL newdisktype = (BOOL) SendDlgItemMessage(window,IDC_DISKTYPE,CB_GETCURSEL,0,0);

	if (newdisktype != enhancedisk)
	{
		if (MessageBox(window,
			TEXT("You have changed the disk speed setting.  ")
			TEXT("This change will not take effect ")
			TEXT("until the next time you restart the ")
			TEXT("emulator.\n\n")
			TEXT("Would you like to restart the emulator now?"),
			TEXT("Configuration"),
			MB_ICONQUESTION | MB_YESNO | MB_SETFOREGROUND) == IDYES)
			afterclose = WM_USER_RESTART;
	}

	bool bHDDIsEnabled = IsDlgButtonChecked(window, IDC_HDD_ENABLE) ? true : false;
	HD_SetEnabled(bHDDIsEnabled);

	REGSAVE(TEXT(REGVALUE_ENHANCE_DISK_SPEED),newdisktype);
	REGSAVE(TEXT(REGVALUE_HDD_ENABLED), bHDDIsEnabled ? 1 : 0);

	RegSaveString(TEXT(REG_PREFS), TEXT(REGVALUE_PREF_LAST_HARDDISK_1), 1, HD_GetFullPathName(HARDDISK_1));
	RegSaveString(TEXT(REG_PREFS), TEXT(REGVALUE_PREF_LAST_HARDDISK_2), 1, HD_GetFullPathName(HARDDISK_2));

	//

	if (afterclose)
		PostMessage(g_hFrameWindow,afterclose,0,0);
}

static void DiskDlg_CANCEL(HWND window)
{
}

//---------------------------------------------------------------------------

static BOOL CALLBACK DiskDlgProc (HWND   window,
								  UINT   message,
								  WPARAM wparam,
								  LPARAM lparam)
{
	static UINT afterclose = 0;

	switch (message)
	{
	case WM_NOTIFY:
		{
			// Property Sheet notifications

			switch (((LPPSHNOTIFY)lparam)->hdr.code)
			{
			case PSN_KILLACTIVE:
				SetWindowLong(window, DWL_MSGRESULT, FALSE);			// Changes are valid
				break;
			case PSN_APPLY:
				SetWindowLong(window, DWL_MSGRESULT, PSNRET_NOERROR);	// Changes are valid
				DiskDlg_OK(window, afterclose);
				break;
			case PSN_QUERYCANCEL:
				// Can use this to ask user to confirm cancel
				break;
			case PSN_RESET:
				DiskDlg_CANCEL(window);
				break;
			}
		}
		break;

	case WM_COMMAND:
		switch (LOWORD(wparam))
		{
		case IDC_DISK1:
			DiskSelect(DRIVE_1);
			SendDlgItemMessage(window, IDC_EDIT_DISK1, WM_SETTEXT, 0, (LPARAM)DiskGetFullName(DRIVE_1));
			FrameRefreshStatus(DRAW_BUTTON_DRIVES);
			break;

		case IDC_DISK2:
			DiskSelect(DRIVE_2);
			SendDlgItemMessage(window, IDC_EDIT_DISK2, WM_SETTEXT, 0, (LPARAM)DiskGetFullName(DRIVE_2));
			FrameRefreshStatus(DRAW_BUTTON_DRIVES);
			break;

		case IDC_HDD1:
			if(IsDlgButtonChecked(window, IDC_HDD_ENABLE))
			{
				HD_Select(HARDDISK_1);
				SendDlgItemMessage(window, IDC_EDIT_HDD1, WM_SETTEXT, 0, (LPARAM)HD_GetFullName(HARDDISK_1));
			}
			break;

		case IDC_HDD2:
			if(IsDlgButtonChecked(window, IDC_HDD_ENABLE))
			{
				HD_Select(HARDDISK_2);
				SendDlgItemMessage(window, IDC_EDIT_HDD2, WM_SETTEXT, 0, (LPARAM)HD_GetFullName(HARDDISK_2));
			}
			break;

		case IDC_HDD_ENABLE:
			EnableHDD(window, IsDlgButtonChecked(window, IDC_HDD_ENABLE));
			break;

		case IDC_CIDERPRESS_BROWSE:
			{
				string CiderPressLoc = BrowseToFile(window, TEXT("Select path to CiderPress"), REGVALUE_CIDERPRESSLOC, TEXT("Applications (*.exe)\0*.exe\0") TEXT("All Files\0*.*\0") );
				RegSaveString(TEXT("Configuration"),REGVALUE_CIDERPRESSLOC,1,CiderPressLoc.c_str());
				SendDlgItemMessage(window, IDC_CIDERPRESS_FILENAME, WM_SETTEXT, 0, (LPARAM) CiderPressLoc.c_str());
			}
			break;
		}
		break;

	case WM_INITDIALOG: //Init disk settings dialog
		{
			g_nLastPage = PG_DISK;

			FillComboBox(window,IDC_DISKTYPE,discchoices,enhancedisk);

			SendDlgItemMessage(window,IDC_EDIT_DISK1,WM_SETTEXT,0,(LPARAM)DiskGetFullName(DRIVE_1));
			SendDlgItemMessage(window,IDC_EDIT_DISK2,WM_SETTEXT,0,(LPARAM)DiskGetFullName(DRIVE_2));

			SendDlgItemMessage(window,IDC_EDIT_HDD1,WM_SETTEXT,0,(LPARAM)HD_GetFullName(HARDDISK_1));
			SendDlgItemMessage(window,IDC_EDIT_HDD2,WM_SETTEXT,0,(LPARAM)HD_GetFullName(HARDDISK_2));

			//

			TCHAR PathToCiderPress[MAX_PATH] = "";
			RegLoadString(TEXT("Configuration"), REGVALUE_CIDERPRESSLOC, 1, PathToCiderPress,MAX_PATH);
			SendDlgItemMessage(window,IDC_CIDERPRESS_FILENAME ,WM_SETTEXT,0,(LPARAM)PathToCiderPress);

			//

			CheckDlgButton(window, IDC_HDD_ENABLE, HD_CardIsEnabled() ? BST_CHECKED : BST_UNCHECKED);

			EnableHDD(window, IsDlgButtonChecked(window, IDC_HDD_ENABLE));

			afterclose = 0;
			break;
		}

	case WM_RBUTTONUP:
		{
			RECT  rect;		// client area
			POINT pt;		// location of mouse click

			// Get the bounding rectangle of the client area.
			GetClientRect(window, (LPRECT) &rect);

			// Get the client coordinates for the mouse click.
			pt.x = GET_X_LPARAM(lparam);
			pt.y = GET_Y_LPARAM(lparam);

			// If the mouse click took place inside the client
			// area, execute the application-defined function
			// that displays the shortcut menu.
			if (PtInRect((LPRECT) &rect, pt))
			{
				//  Load the menu template containing the shortcut menu from the application's resources.
				HMENU hMenu = LoadMenu(g_hInstance, MAKEINTRESOURCE(IDR_MENU_DISK_CFG_POPUP));	// menu template
				_ASSERT(hMenu);
				if (!hMenu)
					break;

				// Get the first shortcut menu in the menu template.
				// This is the menu that TrackPopupMenu displays.
				HMENU hMenuTrackPopup = GetSubMenu(hMenu, 0);	// shortcut menu

				// TrackPopup uses screen coordinates, so convert the coordinates of the mouse click to screen coordinates.
				ClientToScreen(window, (LPPOINT) &pt);

				if (Disk_IsDriveEmpty(DRIVE_1))
					EnableMenuItem(hMenu, ID_DISKMENU_EJECT_DISK1, MF_GRAYED);
				if (Disk_IsDriveEmpty(DRIVE_2))
					EnableMenuItem(hMenu, ID_DISKMENU_EJECT_DISK2, MF_GRAYED);
				if (HD_IsDriveUnplugged(HARDDISK_1))
					EnableMenuItem(hMenu, ID_DISKMENU_UNPLUG_HARDDISK1, MF_GRAYED);
				if (HD_IsDriveUnplugged(HARDDISK_2))
					EnableMenuItem(hMenu, ID_DISKMENU_UNPLUG_HARDDISK2, MF_GRAYED);

				// Draw and track the shortcut menu.
				int iCommand = TrackPopupMenu(
					hMenuTrackPopup
					, TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_RETURNCMD
					, pt.x, pt.y
					, 0
					, window, NULL );

				if (iCommand)
				{
					TCHAR szText[100];
					bool bMsgBox = true;
					if (iCommand == ID_DISKMENU_EJECT_DISK1 || iCommand == ID_DISKMENU_EJECT_DISK2)
						wsprintf(szText, "Do you really want to eject the disk in drive-%c ?", '1'+iCommand-ID_DISKMENU_EJECT_DISK1);
					else if (iCommand == ID_DISKMENU_UNPLUG_HARDDISK1 || iCommand == ID_DISKMENU_UNPLUG_HARDDISK2)
						wsprintf(szText, "Do you really want to unplug harddisk-%c ?", '1'+iCommand-ID_DISKMENU_UNPLUG_HARDDISK1);
					else
						bMsgBox = false;

					if (bMsgBox)
					{
						int nRes = MessageBox(g_hFrameWindow, szText, TEXT("Eject/Unplug Warning"), MB_ICONWARNING | MB_YESNO | MB_SETFOREGROUND);
						if (nRes == IDNO)
							iCommand = 0;
					}
				}

				switch (iCommand)
				{
				case ID_DISKMENU_EJECT_DISK1:
					DiskEject(DRIVE_1);
					SendDlgItemMessage(window, IDC_EDIT_DISK1, WM_SETTEXT, 0, (LPARAM)DiskGetFullName(DRIVE_1));
					break;
				case ID_DISKMENU_EJECT_DISK2:
					DiskEject(DRIVE_2);
					SendDlgItemMessage(window, IDC_EDIT_DISK2, WM_SETTEXT, 0, (LPARAM)DiskGetFullName(DRIVE_2));
					break;
				case ID_DISKMENU_UNPLUG_HARDDISK1:
					HD_Unplug(HARDDISK_1);
					SendDlgItemMessage(window, IDC_EDIT_HDD1, WM_SETTEXT, 0, (LPARAM)HD_GetFullName(HARDDISK_1));
					break;
				case ID_DISKMENU_UNPLUG_HARDDISK2:
					HD_Unplug(HARDDISK_2);
					SendDlgItemMessage(window, IDC_EDIT_HDD2, WM_SETTEXT, 0, (LPARAM)HD_GetFullName(HARDDISK_2));
					break;
				}

				if (iCommand != 0)
					FrameRefreshStatus(DRAW_BUTTON_DRIVES);
			}
			break;
		}

	}

	return 0;
}

//===========================================================================

static bool g_bSSNewFilename = false;
static char g_szNewFilename[MAX_PATH];
static char g_szSSNewDirectory[MAX_PATH];
static char g_szSSNewFilename[MAX_PATH];

static void SaveStateUpdate()
{
	if (g_bSSNewFilename)
	{
		Snapshot_SetFilename(g_szSSNewFilename);

		RegSaveString(TEXT(REG_CONFIG), REGVALUE_SAVESTATE_FILENAME, 1, Snapshot_GetFilename());

		if(g_szSSNewDirectory[0])
			RegSaveString(TEXT(REG_PREFS), REGVALUE_PREF_START_DIR, 1 ,g_szSSNewDirectory);
	}
}

static void GetDiskBaseNameWithAWS(TCHAR* pszFilename)
{
	LPCTSTR pDiskName = DiskGetBaseName(DRIVE_1);
	if (pDiskName && pDiskName[0])
	{
		strcpy(pszFilename, pDiskName);
		strcpy(&pszFilename[strlen(pDiskName)], ".aws");
	}
}

// NB. OK'ing this property sheet will call Snapshot_SetFilename() with this new filename
static int SaveStateSelectImage(HWND hWindow, TCHAR* pszTitle, bool bSave)
{
	TCHAR szDirectory[MAX_PATH] = TEXT("");
	TCHAR szFilename[MAX_PATH] = {0};

	if (bSave)
	{
		// Attempt to use drive1's image name as the name for the .aws file
		// Else Attempt to use the Prop Sheet's filename
		GetDiskBaseNameWithAWS(szFilename);
		if (szFilename[0] == 0)
		{
			strcpy(szFilename, Snapshot_GetFilename());
		}
	}
	else	// Load
	{
		// Attempt to use the Prop Sheet's filename first
		// Else attempt to use drive1's image name as the name for the .aws file
		strcpy(szFilename, Snapshot_GetFilename());
		if (szFilename[0] == 0)
		{
			GetDiskBaseNameWithAWS(szFilename);
		}
	}
	
	RegLoadString(TEXT(REG_PREFS),REGVALUE_PREF_START_DIR,1,szDirectory,MAX_PATH);

	//
	
	OPENFILENAME ofn;
	ZeroMemory(&ofn,sizeof(OPENFILENAME));
	
	ofn.lStructSize     = sizeof(OPENFILENAME);
	ofn.hwndOwner       = hWindow;
	ofn.hInstance       = g_hInstance;
	ofn.lpstrFilter     =	TEXT("Save State files (*.aws)\0*.aws\0")
							TEXT("All Files\0*.*\0");
	ofn.lpstrFile       = szFilename;
	ofn.nMaxFile        = MAX_PATH;
	ofn.lpstrInitialDir = szDirectory;
	ofn.Flags           = OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
	ofn.lpstrTitle      = pszTitle;
	
	int nRes = bSave ? GetSaveFileName(&ofn) : GetOpenFileName(&ofn);

	if(nRes)
	{
		strcpy(g_szSSNewFilename, &szFilename[ofn.nFileOffset]);

		if (bSave)	// Only for saving (allow loading of any file for backwards compatibility)
		{
			// Append .aws if it's not there
			const char szAWS_EXT[] = ".aws";
			const UINT uStrLenFile = strlen(g_szSSNewFilename);
			const UINT uStrLenExt  = strlen(szAWS_EXT);
			if ((uStrLenFile <= uStrLenExt) || (strcmp(&g_szSSNewFilename[uStrLenFile-uStrLenExt], szAWS_EXT) != 0))
				strcpy(&g_szSSNewFilename[uStrLenFile], szAWS_EXT);
		}

		szFilename[ofn.nFileOffset] = 0;
		if (_tcsicmp(szDirectory, szFilename))
			strcpy(g_szSSNewDirectory, szFilename);
	}

	g_bSSNewFilename = nRes ? true : false;
	return nRes;
}

static void InitFreezeDlgButton(HWND window)
{
	if (g_bEnableFreezeDlgButton == UNDEFINED)
		EnableWindow(GetDlgItem(window, IDC_THE_FREEZES_F8_ROM_FW), IS_APPLE2 ? TRUE : FALSE);
	else
		EnableWindow(GetDlgItem(window, IDC_THE_FREEZES_F8_ROM_FW), g_bEnableFreezeDlgButton ? TRUE : FALSE);

	CheckDlgButton(window, IDC_THE_FREEZES_F8_ROM_FW, g_uTheFreezesF8Rom ? BST_CHECKED : BST_UNCHECKED);
}

//---------------------------------------------------------------------------

static void AdvancedDlg_OK(HWND window, UINT afterclose)
{
	// Update save-state filename
	{
		char szFilename[MAX_PATH];
		memset(szFilename, 0, sizeof(szFilename));
		* (USHORT*) szFilename = sizeof(szFilename);

		UINT nLineLength = SendDlgItemMessage(window, IDC_SAVESTATE_FILENAME, EM_LINELENGTH, 0, 0);

		SendDlgItemMessage(window, IDC_SAVESTATE_FILENAME, EM_GETLINE, 0, (LPARAM)szFilename);

		nLineLength = nLineLength > sizeof(szFilename)-1 ? sizeof(szFilename)-1 : nLineLength;
		szFilename[nLineLength] = 0x00;

		SaveStateUpdate();
	}

	// Update printer dump filename
	{
		char szFilename[MAX_PATH];
		memset(szFilename, 0, sizeof(szFilename));
		* (USHORT*) szFilename = sizeof(szFilename);

		UINT nLineLength = SendDlgItemMessage(window, IDC_PRINTER_DUMP_FILENAME, EM_LINELENGTH, 0, 0);

		SendDlgItemMessage(window, IDC_PRINTER_DUMP_FILENAME, EM_GETLINE, 0, (LPARAM)szFilename);

		nLineLength = nLineLength > sizeof(szFilename)-1 ? sizeof(szFilename)-1 : nLineLength;
		szFilename[nLineLength] = 0x00;

		Printer_SetFilename(szFilename);
		RegSaveString(TEXT(REG_CONFIG), REGVALUE_PRINTER_FILENAME, 1, Printer_GetFilename());
	}

	g_bSaveStateOnExit = IsDlgButtonChecked(window, IDC_SAVESTATE_ON_EXIT) ? true : false;
	REGSAVE(TEXT(REGVALUE_SAVE_STATE_ON_EXIT), g_bSaveStateOnExit ? 1 : 0);

	g_bDumpToPrinter = IsDlgButtonChecked(window, IDC_DUMPTOPRINTER ) ? true : false;
	REGSAVE(TEXT(REGVALUE_DUMP_TO_PRINTER), g_bDumpToPrinter ? 1 : 0);

	g_bConvertEncoding = IsDlgButtonChecked(window, IDC_PRINTER_CONVERT_ENCODING ) ? true : false;
	REGSAVE(TEXT(REGVALUE_CONVERT_ENCODING), g_bConvertEncoding ? 1 : 0);

	g_bFilterUnprintable = IsDlgButtonChecked(window, IDC_PRINTER_FILTER_UNPRINTABLE ) ? true : false;
	REGSAVE(TEXT(REGVALUE_FILTER_UNPRINTABLE), g_bFilterUnprintable ? 1 : 0);

	g_bPrinterAppend = IsDlgButtonChecked(window, IDC_PRINTER_APPEND) ? true : false;
	REGSAVE(TEXT(REGVALUE_PRINTER_APPEND), g_bPrinterAppend ? 1 : 0);

	//

	DWORD NewCloneType = (DWORD)SendDlgItemMessage(window, IDC_CLONETYPE, CB_GETCURSEL, 0, 0);

	REGSAVE(TEXT(REGVALUE_CLONETYPE), NewCloneType);
	REGSAVE(TEXT(REGVALUE_THE_FREEZES_F8_ROM),g_uTheFreezesF8Rom);	// NB. Can also be disabled on Config page (when Apple2Type changes) 
	
    Printer_SetIdleLimit((short)SendDlgItemMessage(window, IDC_SPIN_PRINTER_IDLE , UDM_GETPOS, 0, 0));
	REGSAVE(TEXT(REGVALUE_PRINTER_IDLE_LIMIT),Printer_GetIdleLimit());

	eApple2Type NewApple2Clone = GetApple2Type(4, NewCloneType);

	if (g_Apple2Type >= A2TYPE_CLONE) 	
	{
		if (NewApple2Clone != g_Apple2Type)
		{		
			if ((afterclose == WM_USER_RESTART) ||	// Eg. Changing 'Freeze ROM' & user has already OK'd the restart for this
				MessageBox(window,
							TEXT(
							"You have changed the emulated computer "
							"type. This change will not take effect\n"
							"until the next time you restart the "
							"emulator.\n\n"
							"Would you like to restart the emulator now?"),
							TEXT("Configuration"),
							MB_ICONQUESTION | MB_YESNO | MB_SETFOREGROUND) == IDYES)
			{
				afterclose = WM_USER_RESTART;	
			}
		}
	}
	else
	{
		if (NewApple2Clone != (g_uCloneType + APPLECLONE_MASK|APPLE2E_MASK))
		{
			MessageBox(window,
						TEXT(
						"You have changed the emulated clone type "
						"but in order for the changes to take effect\n"
						"you shall set the emulated computer type "
						"to Clone from the Configuration tab.\n\n"),
						TEXT("Clone type changed"),
						MB_ICONQUESTION | MB_OK  | MB_SETFOREGROUND);
			g_uCloneType = NewApple2Clone - (APPLECLONE_MASK|APPLE2E_MASK);
		}
	}

	if (NewApple2Type > A2TYPE_APPLE2PLUS)		// TC-FIXME: Must be global scope (ie. g_NewApple2Type)
		g_uTheFreezesF8Rom = false;

	//

	if (afterclose)
		PostMessage(g_hFrameWindow,afterclose,0,0);
}

static void AdvancedDlg_CANCEL(HWND window)
{
}

//---------------------------------------------------------------------------

static BOOL CALLBACK AdvancedDlgProc (HWND   window,
									  UINT   message,
									  WPARAM wparam,
									  LPARAM lparam)
{
	static UINT afterclose = 0;

	switch (message)
	{
	case WM_NOTIFY:
		{
			// Property Sheet notifications

			switch (((LPPSHNOTIFY)lparam)->hdr.code)
			{
			case PSN_SETACTIVE:
				// About to become the active page
				InitFreezeDlgButton(window);
				break;
			case PSN_KILLACTIVE:
				SetWindowLong(window, DWL_MSGRESULT, FALSE);			// Changes are valid
				break;
			case PSN_APPLY:
				SetWindowLong(window, DWL_MSGRESULT, PSNRET_NOERROR);	// Changes are valid
				AdvancedDlg_OK(window, afterclose);
				break;
			case PSN_QUERYCANCEL:
				// Can use this to ask user to confirm cancel
				break;
			case PSN_RESET:
				SoundDlg_CANCEL(window);
				break;
			}
		}
		break;

	case WM_COMMAND:
		switch (LOWORD(wparam))
		{
		case IDC_SAVESTATE_FILENAME:
			break;
		case IDC_SAVESTATE_BROWSE:
			if(SaveStateSelectImage(window, TEXT("Select Save State file"), true))
				SendDlgItemMessage(window, IDC_SAVESTATE_FILENAME, WM_SETTEXT, 0, (LPARAM)g_szSSNewFilename);
			break;
		case IDC_PRINTER_DUMP_FILENAME_BROWSE:
			{				
				string strPrinterDumpLoc = BrowseToFile(window, TEXT("Select printer dump file"), REGVALUE_PRINTER_FILENAME, TEXT("Text files (*.txt)\0*.txt\0") TEXT("All Files\0*.*\0"));
				SendDlgItemMessage(window, IDC_PRINTER_DUMP_FILENAME, WM_SETTEXT, 0, (LPARAM)strPrinterDumpLoc.c_str());
			}
			break;
		case IDC_SAVESTATE_ON_EXIT:
			break;
		case IDC_SAVESTATE:
			afterclose = WM_USER_SAVESTATE;
			break;
		case IDC_LOADSTATE:
			afterclose = WM_USER_LOADSTATE;
			break;

			//

		case IDC_THE_FREEZES_F8_ROM_FW:
			{
				UINT uNewState = IsDlgButtonChecked(window, IDC_THE_FREEZES_F8_ROM_FW) ? 1 : 0;
				LPCSTR pMsg = 	TEXT("The emulator needs to restart as the ROM configuration has changed.\n")
					TEXT("Would you like to restart the emulator now?");
				if (MessageBox(window,
					pMsg,
					TEXT("Configuration"),
					MB_ICONQUESTION | MB_YESNO | MB_SETFOREGROUND) == IDYES)
				{
					g_uTheFreezesF8Rom = uNewState;
					afterclose = WM_USER_RESTART;
					PropSheet_PressButton(GetParent(window), PSBTN_OK);
				}
				else
				{
					CheckDlgButton(window, IDC_THE_FREEZES_F8_ROM_FW, g_uTheFreezesF8Rom ? BST_CHECKED : BST_UNCHECKED);
				}
			}
			break;
		}
		break;

	case WM_INITDIALOG:  //Init advanced settings dialog
		{
			g_nLastPage = PG_ADVANCED;

			SendDlgItemMessage(window,IDC_SAVESTATE_FILENAME,WM_SETTEXT,0,(LPARAM)Snapshot_GetFilename());

			CheckDlgButton(window, IDC_SAVESTATE_ON_EXIT, g_bSaveStateOnExit ? BST_CHECKED : BST_UNCHECKED);
			CheckDlgButton(window, IDC_DUMPTOPRINTER, g_bDumpToPrinter ? BST_CHECKED : BST_UNCHECKED);
			CheckDlgButton(window, IDC_PRINTER_CONVERT_ENCODING, g_bConvertEncoding ? BST_CHECKED : BST_UNCHECKED);
			CheckDlgButton(window, IDC_PRINTER_FILTER_UNPRINTABLE, g_bFilterUnprintable ? BST_CHECKED : BST_UNCHECKED);
			CheckDlgButton(window, IDC_PRINTER_APPEND, g_bPrinterAppend ? BST_CHECKED : BST_UNCHECKED);
			SendDlgItemMessage(window, IDC_SPIN_PRINTER_IDLE, UDM_SETRANGE, 0, MAKELONG(999,0));
			SendDlgItemMessage(window, IDC_SPIN_PRINTER_IDLE, UDM_SETPOS, 0, MAKELONG(Printer_GetIdleLimit (),0));
			SendDlgItemMessage(window, IDC_PRINTER_DUMP_FILENAME, WM_SETTEXT, 0, (LPARAM)Printer_GetFilename());

			FillComboBox(window, IDC_CLONETYPE, g_CloneChoices, g_uCloneType);
			InitFreezeDlgButton(window);

			g_szSSNewDirectory[0] = 0x00;

			// Need to specific cmd-line switch: -printer-real to enable this control
			EnableWindow(GetDlgItem(window, IDC_DUMPTOPRINTER), g_bEnableDumpToRealPrinter ? TRUE : FALSE);

			afterclose = 0;
			break;
		}
	}

	return 0;
}

//===========================================================================

static BOOL get_tfename(int number, char **ppname, char **ppdescription)
{
	if (tfe_enumadapter_open())
	{
		char *pname = NULL;
		char *pdescription = NULL;

		while (number--)
		{
			if (!tfe_enumadapter(&pname, &pdescription))
				break;

			lib_free(pname);
			lib_free(pdescription);
		}

		if (tfe_enumadapter(&pname, &pdescription))
		{
			*ppname = pname;
			*ppdescription = pdescription;
			tfe_enumadapter_close();
			return TRUE;
		}

		tfe_enumadapter_close();
	}

	return FALSE;
}

static int gray_ungray_items(HWND hwnd)
{
	int enable;
	int number;

	int disabled = 0;

	//resources_get_value("ETHERNET_DISABLED", (void *)&disabled);
	REGLOAD(TEXT("Uthernet Disabled")  ,(DWORD *)&disabled);
	get_disabled_state(&disabled);

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

static uilib_localize_dialog_param tfe_dialog[] =
{
	{0, IDS_TFE_CAPTION, -1},
	{IDC_TFE_SETTINGS_ENABLE_T, IDS_TFE_ETHERNET, 0},
	{IDC_TFE_SETTINGS_INTERFACE_T, IDS_TFE_INTERFACE, 0},
	{IDOK, IDS_OK, 0},
	{IDCANCEL, IDS_CANCEL, 0},
	{0, 0, 0}
};

static uilib_dialog_group tfe_leftgroup[] =
{
	{IDC_TFE_SETTINGS_ENABLE_T, 0},
	{IDC_TFE_SETTINGS_INTERFACE_T, 0},
	{0, 0}
};

static uilib_dialog_group tfe_rightgroup[] =
{
	{IDC_TFE_SETTINGS_ENABLE, 0},
	{IDC_TFE_SETTINGS_INTERFACE, 0},
	{0, 0}
};

static void init_tfe_dialog(HWND hwnd)
{
	HWND temp_hwnd;
	int active_value;

	int tfe_enable;
	int xsize, ysize;

	char *interface_name = NULL;

	uilib_get_group_extent(hwnd, tfe_leftgroup, &xsize, &ysize);
	uilib_adjust_group_width(hwnd, tfe_leftgroup);
	uilib_move_group(hwnd, tfe_rightgroup, xsize + 30);

	//resources_get_value("ETHERNET_ACTIVE", (void *)&tfe_enabled);
	get_tfe_enabled(&tfe_enable);

	//resources_get_value("ETHERNET_AS_RR", (void *)&tfe_as_rr_net);
	active_value = (tfe_enable ? 1 : 0);

	temp_hwnd=GetDlgItem(hwnd,IDC_TFE_SETTINGS_ENABLE);
	SendMessage(temp_hwnd, CB_ADDSTRING, 0, (LPARAM)"Disabled");
	SendMessage(temp_hwnd, CB_ADDSTRING, 0, (LPARAM)"Uthernet");
	SendMessage(temp_hwnd, CB_SETCURSEL, (WPARAM)active_value, 0);

	//resources_get_value("ETHERNET_INTERFACE", (void *)&interface_name);
	interface_name = (char *) get_tfe_interface();

	if (tfe_enumadapter_open())
	{
		int cnt = 0;

		char *pname;
		char *pdescription;

		temp_hwnd=GetDlgItem(hwnd,IDC_TFE_SETTINGS_INTERFACE);

		for (cnt = 0; tfe_enumadapter(&pname, &pdescription); cnt++)
		{
			BOOL this_entry = FALSE;

			if (strcmp(pname, interface_name) == 0)
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

		tfe_enumadapter_close();
	}

	if (gray_ungray_items(hwnd))
	{
		/* we have a problem: TFE is disabled. Give a message to the user */
		MessageBox( hwnd,
			"TFE support is not available on your system,\n"
			"there is some important part missing. Please have a\n"
			"look at the VICE knowledge base support page\n"
			"\n      http://www.vicekb.de.vu/13-005.htm\n\n"
			"for possible reasons and to activate networking with VICE.",
			"TFE support", MB_ICONINFORMATION|MB_OK);

		/* just quit the dialog before it is open */
		SendMessage( hwnd, WM_COMMAND, IDCANCEL, 0);
	}
}

static void save_tfe_dialog(HWND hwnd)
{
	int active_value;
	int tfe_enabled;
	char buffer[256];

	buffer[255] = 0;
	GetDlgItemText(hwnd, IDC_TFE_SETTINGS_INTERFACE, buffer, sizeof(buffer)-1);

	// RGJ - Added check for NULL interface so we don't set it active without a valid interface selected
	if (strlen(buffer) > 0)
	{
		RegSaveString(TEXT("Configuration"), TEXT("Uthernet Interface"), 1, buffer);

		active_value = SendMessage(GetDlgItem(hwnd, IDC_TFE_SETTINGS_ENABLE), CB_GETCURSEL, 0, 0);

		tfe_enabled = active_value >= 1 ? 1 : 0;
		REGSAVE(TEXT("Uthernet Active")  ,tfe_enabled);
	}
	else
	{
		REGSAVE(TEXT("Uthernet Active")  ,0);
	}
}

static BOOL CALLBACK TfeDlgProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	switch (msg)
	{
	case WM_COMMAND:
		switch (LOWORD(wparam))
		{
		case IDOK:
			save_tfe_dialog(hwnd);
			/* FALL THROUGH */

		case IDCANCEL:
			EndDialog(hwnd,0);
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

void ui_tfe_settings_dialog(HWND hwnd)
{
	DialogBox(g_hInstance, (LPCTSTR)IDD_TFE_SETTINGS_DIALOG, hwnd, TfeDlgProc);
}

//===========================================================================

//Setup
void PSP_Init()
{
	PROPSHEETPAGE PropSheetPages[PG_NUM_SHEETS];

	PropSheetPages[PG_CONFIG].dwSize = sizeof(PROPSHEETPAGE);
	PropSheetPages[PG_CONFIG].dwFlags = PSP_DEFAULT;
	PropSheetPages[PG_CONFIG].hInstance = g_hInstance;
	PropSheetPages[PG_CONFIG].pszTemplate = MAKEINTRESOURCE(IDD_PROPPAGE_CONFIG);
	PropSheetPages[PG_CONFIG].pfnDlgProc = (DLGPROC)ConfigDlgProc;

	PropSheetPages[PG_INPUT].dwSize = sizeof(PROPSHEETPAGE);
	PropSheetPages[PG_INPUT].dwFlags = PSP_DEFAULT;
	PropSheetPages[PG_INPUT].hInstance = g_hInstance;
	PropSheetPages[PG_INPUT].pszTemplate = MAKEINTRESOURCE(IDD_PROPPAGE_INPUT);
	PropSheetPages[PG_INPUT].pfnDlgProc = (DLGPROC)InputDlgProc;

	PropSheetPages[PG_SOUND].dwSize = sizeof(PROPSHEETPAGE);
	PropSheetPages[PG_SOUND].dwFlags = PSP_DEFAULT;
	PropSheetPages[PG_SOUND].hInstance = g_hInstance;
	PropSheetPages[PG_SOUND].pszTemplate = MAKEINTRESOURCE(IDD_PROPPAGE_SOUND);
	PropSheetPages[PG_SOUND].pfnDlgProc = (DLGPROC)SoundDlgProc;

	PropSheetPages[PG_DISK].dwSize = sizeof(PROPSHEETPAGE);
	PropSheetPages[PG_DISK].dwFlags = PSP_DEFAULT;
	PropSheetPages[PG_DISK].hInstance = g_hInstance;
	PropSheetPages[PG_DISK].pszTemplate = MAKEINTRESOURCE(IDD_PROPPAGE_DISK);
	PropSheetPages[PG_DISK].pfnDlgProc = (DLGPROC)DiskDlgProc;

	PropSheetPages[PG_ADVANCED].dwSize = sizeof(PROPSHEETPAGE);
	PropSheetPages[PG_ADVANCED].dwFlags = PSP_DEFAULT;
	PropSheetPages[PG_ADVANCED].hInstance = g_hInstance;
	PropSheetPages[PG_ADVANCED].pszTemplate = MAKEINTRESOURCE(IDD_PROPPAGE_ADVANCED);
	PropSheetPages[PG_ADVANCED].pfnDlgProc = (DLGPROC)AdvancedDlgProc;

	PROPSHEETHEADER PropSheetHeader;

	PropSheetHeader.dwSize = sizeof(PROPSHEETHEADER);
	PropSheetHeader.dwFlags = PSH_NOAPPLYNOW | /* PSH_NOCONTEXTHELP | */ PSH_PROPSHEETPAGE;
	PropSheetHeader.hwndParent = g_hFrameWindow;
	PropSheetHeader.pszCaption = "AppleWin Configuration";
	PropSheetHeader.nPages = PG_NUM_SHEETS;
	PropSheetHeader.nStartPage = g_nLastPage;
	PropSheetHeader.ppsp = PropSheetPages;

	g_bEnableFreezeDlgButton = UNDEFINED;
	int i = PropertySheet(&PropSheetHeader);	// Result: 0=Cancel, 1=OK
}

DWORD PSP_GetVolumeMax()
{
	return VOLUME_MAX;
}

// Called when F11/F12 is pressed
bool PSP_SaveStateSelectImage(HWND hWindow, bool bSave)
{
	g_szSSNewDirectory[0] = 0x00;

	if(SaveStateSelectImage(hWindow, bSave ? TEXT("Select Save State file")
										   : TEXT("Select Load State file"), bSave))
	{
		SaveStateUpdate();
		return true;
	}
	else
	{
		return false;	// Cancelled
	}
}

//===========================================================================

string BrowseToFile(HWND hWindow, TCHAR* pszTitle, TCHAR* REGVALUE,TCHAR* FILEMASKS)
{
	static char PathToFile[MAX_PATH] = {0}; //This is a really awkward way to prevent mixing CiderPress and SaveStated values (RAPCS), but it seem the quickest. Here is its Line 1.
	strcpy(PathToFile, Snapshot_GetFilename()); //RAPCS, line 2.
	TCHAR szDirectory[MAX_PATH] = TEXT("");
	TCHAR szFilename[MAX_PATH];
	strcpy(szFilename, "");
	RegLoadString(TEXT("Configuration"), REGVALUE, 1, szFilename ,MAX_PATH);
	string PathName = szFilename;

	OPENFILENAME ofn;
	ZeroMemory(&ofn,sizeof(OPENFILENAME));
	
	ofn.lStructSize     = sizeof(OPENFILENAME);
	ofn.hwndOwner       = hWindow;
	ofn.hInstance       = g_hInstance;
	ofn.lpstrFilter     = FILEMASKS;
	/*ofn.lpstrFilter     =	TEXT("Applications (*.exe)\0*.exe\0")
							TEXT("Text files (*.txt)\0*.txt\0")
							TEXT("All Files\0*.*\0");*/
	ofn.lpstrFile       = szFilename;
	ofn.nMaxFile        = MAX_PATH;
	ofn.lpstrInitialDir = szDirectory;
	ofn.Flags           = OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
	ofn.lpstrTitle      = pszTitle;
		
	int nRes = GetOpenFileName(&ofn);
	if(nRes)	// Okay is pressed
	{
		strcpy(g_szNewFilename, &szFilename[ofn.nFileOffset]);

		szFilename[ofn.nFileOffset] = 0;
		if (_tcsicmp(szDirectory, szFilename))
			strcpy(g_szSSNewDirectory, szFilename);

		PathName = szFilename;
		PathName.append (g_szNewFilename);	
	}
	else		// Cancel is pressed
	{
		RegLoadString(TEXT("Configuration"), REGVALUE, 1, szFilename,MAX_PATH);
		PathName = szFilename;
	}

	strcpy(g_szNewFilename, PathToFile); //RAPCS, line 3 (last).
	return PathName;
}
