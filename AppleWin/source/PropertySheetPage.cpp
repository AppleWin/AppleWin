/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 1994-1996, Michael O'Brien
Copyright (C) 1999-2001, Oliver Schmidt
Copyright (C) 2002-2005, Tom Charlesworth
Copyright (C) 2006-2007, Tom Charlesworth, Michael Pohoreski


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
 * Author: Copyright (c) 2002-2006, Tom Charlesworth
 *                                  Spiro Trikaliotis <Spiro.Trikaliotis@gmx.de>
 */

#include "StdAfx.h"
#pragma  hdrstop
#include "..\resource\resource.h"
#include "Tfe\Tfesupp.h"
#include "Tfe\Uilib.h"


TCHAR   computerchoices[] =  TEXT("Apple ][ (Original Model)\0")
			     TEXT("Apple ][+\0")
                             TEXT("Apple //e\0");

TCHAR* szJoyChoice0 = TEXT("Disabled\0");
TCHAR* szJoyChoice1 = TEXT("PC Joystick #1\0");
TCHAR* szJoyChoice2 = TEXT("PC Joystick #2\0");
TCHAR* szJoyChoice3 = TEXT("Keyboard (standard)\0");
TCHAR* szJoyChoice4 = TEXT("Keyboard (centering)\0");
TCHAR* szJoyChoice5 = TEXT("Mouse\0");

const int g_nMaxJoyChoiceLen = 40;

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

TCHAR   serialchoices[]   =  TEXT("None\0")
                             TEXT("COM1\0")
                             TEXT("COM2\0")
                             TEXT("COM3\0")
                             TEXT("COM4\0");

TCHAR   soundchoices[]    =  TEXT("Disabled\0")
                             TEXT("PC Speaker (direct)\0")
                             TEXT("PC Speaker (translated)\0")
                             TEXT("Sound Card\0");

// Must match VT_NUM_MODES!
TCHAR   videochoices[]    =
	TEXT("Monochrome (Custom)\0")
	TEXT("Color (standard)\0")
	TEXT("Color (text optimized)\0")
	TEXT("Color (TV emulation)\0")
	TEXT("Color (Half-Shift)\0")
	TEXT("Monochrome - Amber\0")
	TEXT("Monochrome - Green\0")
	TEXT("Monochrome - White\0")
	;

TCHAR   discchoices[]     =  TEXT("Authentic Speed\0")
                             TEXT("Enhanced Speed\0");


const UINT VOLUME_MIN = 0;
const UINT VOLUME_MAX = 59;

enum {PG_CONFIG=0, PG_INPUT, PG_SOUND, PG_SAVESTATE, PG_DISK, PG_NUM_SHEETS};

UINT g_nLastPage = PG_CONFIG;

//===========================================================================

static void FillComboBox (HWND window, int controlid, LPCTSTR choices, int currentchoice)
{
  HWND combowindow = GetDlgItem(window,controlid);
  SendMessage(combowindow,CB_RESETCONTENT,0,0);
  while (*choices) {
    SendMessage(combowindow,CB_ADDSTRING,0,(LPARAM)(LPCTSTR)choices);
    choices += _tcslen(choices)+1;
  }
  SendMessage(combowindow,CB_SETCURSEL,currentchoice,0);
}

//===========================================================================

static void EnableTrackbar (HWND window, BOOL enable)
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
		  continue;

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

static void ConfigDlg_OK(HWND window, BOOL afterclose)
{
	BOOL  newcomptype   = (BOOL) SendDlgItemMessage(window,IDC_COMPUTER,CB_GETCURSEL,0,0);
	DWORD newvidtype    = (DWORD)SendDlgItemMessage(window,IDC_VIDEOTYPE,CB_GETCURSEL,0,0);
	DWORD newserialport = (DWORD)SendDlgItemMessage(window,IDC_SERIALPORT,CB_GETCURSEL,0,0);

	if (newcomptype != (g_bApple2e ? 2 : (g_bApple2plus ? 1 : 0)))
	{
		if (MessageBox(window,
			TEXT(
			"You have changed the emulated computer "
			"type.  This change will not take effect "
			"until the next time you restart the "
			"emulator.\n\n"
			"Would you like to restart the emulator now?"),
			TEXT("Configuration"),
			MB_ICONQUESTION | MB_YESNO | MB_SETFOREGROUND) == IDYES)
			afterclose = WM_USER_RESTART;
	}

	if (videotype != newvidtype)
	{
		videotype = newvidtype;
		VideoReinitialize();
		if ((g_nAppMode != MODE_LOGO) && (g_nAppMode != MODE_DEBUG))
			VideoRedrawScreen();
	}
	CommSetSerialPort(window,newserialport);
	
	if (IsDlgButtonChecked(window,IDC_AUTHENTIC_SPEED))
		g_dwSpeed = SPEED_NORMAL;
	else
		g_dwSpeed = SendDlgItemMessage(window,IDC_SLIDER_CPU_SPEED,TBM_GETPOS,0,0);

	SetCurrentCLK6502();
	
	SAVE(TEXT("Computer Emulation"),newcomptype);
	SAVE(TEXT("Serial Port")       ,serialport);
	SAVE(TEXT("Custom Speed")      ,IsDlgButtonChecked(window,IDC_CUSTOM_SPEED));
	SAVE(TEXT("Emulation Speed")   ,g_dwSpeed);
	SAVE(TEXT("Video Emulation")   ,videotype);

	//

	if (afterclose)
		PostMessage(g_hFrameWindow,afterclose,0,0);
}

static void ConfigDlg_CANCEL(HWND window)
{
}

//---------------------------------------------------------------------------

static BOOL CALLBACK ConfigDlgProc (HWND   window,
                             UINT   message,
                             WPARAM wparam,
                             LPARAM lparam) {
  static BOOL afterclose = 0;

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

#if 0
        case IDC_RECALIBRATE:
          RegSaveValue(TEXT(""),TEXT("RunningOnOS"),0,0);
          if (MessageBox(window,
                         TEXT("The emulator has been set to recalibrate ")
                         TEXT("itself the next time it is started.\n\n")
                         TEXT("Would you like to restart the emulator now?"),
                         TEXT("Configuration"),
                         MB_ICONQUESTION | MB_YESNO | MB_SETFOREGROUND) == IDYES) {
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

    case WM_INITDIALOG:
	{
      g_nLastPage = PG_CONFIG;

      FillComboBox(window,IDC_COMPUTER,computerchoices,g_bApple2e ? 2 : (g_bApple2plus ? 1 : 0));
      FillComboBox(window,IDC_VIDEOTYPE,videochoices,videotype);
      FillComboBox(window,IDC_SERIALPORT,serialchoices,serialport);
      SendDlgItemMessage(window,IDC_SLIDER_CPU_SPEED,TBM_SETRANGE,1,MAKELONG(0,40));
      SendDlgItemMessage(window,IDC_SLIDER_CPU_SPEED,TBM_SETPAGESIZE,0,5);
      SendDlgItemMessage(window,IDC_SLIDER_CPU_SPEED,TBM_SETTICFREQ,10,0);
      SendDlgItemMessage(window,IDC_SLIDER_CPU_SPEED,TBM_SETPOS,1,g_dwSpeed);
      {
        BOOL custom = 1;
        if (g_dwSpeed == SPEED_NORMAL) {
          custom = 0;
          RegLoadValue(TEXT("Configuration"),TEXT("Custom Speed"),1,(DWORD *)&custom);
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
      POINT pt = {LOWORD(lparam),
                  HIWORD(lparam)};
      ClientToScreen(window,&pt);
      RECT rect;
      GetWindowRect(GetDlgItem(window,IDC_SLIDER_CPU_SPEED),&rect);
      if ((pt.x >= rect.left) && (pt.x <= rect.right) &&
          (pt.y >= rect.top) && (pt.y <= rect.bottom)) {
        CheckRadioButton(window, IDC_AUTHENTIC_SPEED, IDC_CUSTOM_SPEED, IDC_CUSTOM_SPEED);
        EnableTrackbar(window,1);
        SetFocus(GetDlgItem(window,IDC_SLIDER_CPU_SPEED));
        ScreenToClient(GetDlgItem(window,IDC_SLIDER_CPU_SPEED),&pt);
        PostMessage(GetDlgItem(window,IDC_SLIDER_CPU_SPEED),WM_LBUTTONDOWN,
                    wparam,MAKELONG(pt.x,pt.y));
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

static void InputDlg_OK(HWND window, BOOL afterclose)
{
	DWORD newjoytype0   = (DWORD)SendDlgItemMessage(window,IDC_JOYSTICK0,CB_GETCURSEL,0,0);
	DWORD newjoytype1   = (DWORD)SendDlgItemMessage(window,IDC_JOYSTICK1,CB_GETCURSEL,0,0);

//	bool bNewKeybBufferEnable = IsDlgButtonChecked(window, IDC_KEYB_BUFFER_ENABLE) ? true : false;

	if (!JoySetEmulationType(window,g_nJoy0ChoiceTranlationTbl[newjoytype0],JN_JOYSTICK0))
	{
		afterclose = 0;
		return;
	}
	
	if (!JoySetEmulationType(window,g_nJoy1ChoiceTranlationTbl[newjoytype1],JN_JOYSTICK1))
	{
		afterclose = 0;
		return;
	}

	JoySetTrim((short)SendDlgItemMessage(window, IDC_SPIN_XTRIM, UDM_GETPOS, 0, 0), true);
	JoySetTrim((short)SendDlgItemMessage(window, IDC_SPIN_YTRIM, UDM_GETPOS, 0, 0), false);

//	KeybSetBufferMode(bNewKeybBufferEnable);

	SAVE(TEXT("Joystick 0 Emulation"),joytype[0]);
	SAVE(TEXT("Joystick 1 Emulation"),joytype[1]);
	SAVE(TEXT(REGVALUE_PDL_XTRIM),JoyGetTrim(true));
	SAVE(TEXT(REGVALUE_PDL_YTRIM),JoyGetTrim(false));
//	SAVE(TEXT(REGVALUE_KEYB_BUFFER_ENABLE),KeybGetBufferMode() ? 1 : 0);

	//

	if (afterclose)
		PostMessage(g_hFrameWindow,afterclose,0,0);
}

static void InputDlg_CANCEL(HWND window)
{
}

//---------------------------------------------------------------------------

static BOOL CALLBACK InputDlgProc (HWND   window,
                             UINT   message,
                             WPARAM wparam,
                             LPARAM lparam)
{
  static BOOL afterclose = 0;

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
//		case IDC_KEYB_BUFFER_ENABLE:
//			break;
		case IDC_PASTE_FROM_CLIPBOARD:
			ClipboardInitiatePaste();
			break;
      }
      break;

    case WM_INITDIALOG:
	{
      g_nLastPage = PG_INPUT;

      InitJoystickChoices(window, JN_JOYSTICK0, IDC_JOYSTICK0);
      InitJoystickChoices(window, JN_JOYSTICK1, IDC_JOYSTICK1);

	  SendDlgItemMessage(window, IDC_SPIN_XTRIM, UDM_SETRANGE, 0, MAKELONG(128,-127));
	  SendDlgItemMessage(window, IDC_SPIN_YTRIM, UDM_SETRANGE, 0, MAKELONG(128,-127));

	  SendDlgItemMessage(window, IDC_SPIN_XTRIM, UDM_SETPOS, 0, MAKELONG(JoyGetTrim(true),0));
	  SendDlgItemMessage(window, IDC_SPIN_YTRIM, UDM_SETPOS, 0, MAKELONG(JoyGetTrim(false),0));

//	  CheckDlgButton(window, IDC_KEYB_BUFFER_ENABLE, KeybGetBufferMode() ? BST_CHECKED : BST_UNCHECKED);
	}
  }

  return 0;
}

//===========================================================================

static void SoundDlg_OK(HWND window, BOOL afterclose, UINT uNewSoundcardType)
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

	SAVE(TEXT("Sound Emulation")   ,soundtype);
	SAVE(TEXT(REGVALUE_SPKR_VOLUME),SpkrGetVolume());
	SAVE(TEXT(REGVALUE_MB_VOLUME),MB_GetVolume());
	SAVE(TEXT(REGVALUE_SOUNDCARD_TYPE),(DWORD)MB_GetSoundcardType());

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
  static BOOL afterclose = 0;
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

      afterclose = 0;
	}
  }

  return 0;
}

//===========================================================================

static char g_szNewDirectory[MAX_PATH];
static char g_szNewFilename[MAX_PATH];

static void SaveStateUpdate()
{
	Snapshot_SetFilename(g_szNewFilename);

	RegSaveString(TEXT("Configuration"),REGVALUE_SAVESTATE_FILENAME,1,Snapshot_GetFilename());

	if(g_szNewDirectory[0])
		RegSaveString(TEXT("Preferences"),REGVALUE_PREF_START_DIR,1,g_szNewDirectory);
}

static void SaveStateDlg_OK(HWND window, BOOL afterclose)
{
	char szFilename[MAX_PATH];

	memset(szFilename, 0, sizeof(szFilename));
	* ((USHORT*) szFilename) = sizeof(szFilename);

	UINT nLineLength = SendDlgItemMessage(window,IDC_SAVESTATE_FILENAME,EM_LINELENGTH,0,0);

	SendDlgItemMessage(window,IDC_SAVESTATE_FILENAME,EM_GETLINE,0,(LPARAM)szFilename);

	nLineLength = nLineLength > sizeof(szFilename)-1 ? sizeof(szFilename)-1 : nLineLength;
	szFilename[nLineLength] = 0x00;

	SaveStateUpdate();

	g_bSaveStateOnExit = IsDlgButtonChecked(window, IDC_SAVESTATE_ON_EXIT) ? true : false;

	SAVE(TEXT(REGVALUE_SAVE_STATE_ON_EXIT), g_bSaveStateOnExit ? 1 : 0);

	//

	if (afterclose)
		PostMessage(g_hFrameWindow,afterclose,0,0);
}

static void SaveStateDlg_CANCEL(HWND window)
{
}

//---------------------------------------------------------------------------

static int SaveStateSelectImage(HWND hWindow, TCHAR* pszTitle, bool bSave)
{
	TCHAR szDirectory[MAX_PATH] = TEXT("");
	TCHAR szFilename[MAX_PATH];
	
	strcpy(szFilename, Snapshot_GetFilename());
	
	RegLoadString(TEXT("Preferences"),REGVALUE_PREF_START_DIR,1,szDirectory,MAX_PATH);
	
	
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
		strcpy(g_szNewFilename, &szFilename[ofn.nFileOffset]);

		szFilename[ofn.nFileOffset] = 0;
		if (_tcsicmp(szDirectory, szFilename))
			strcpy(g_szNewDirectory, szFilename);
	}

	return nRes;
}

//---------------------------------------------------------------------------

static BOOL CALLBACK SaveStateDlgProc (HWND   window,
                             UINT   message,
                             WPARAM wparam,
                             LPARAM lparam)
{
  static BOOL afterclose = 0;

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
			SaveStateDlg_OK(window, afterclose);
			break;
		case PSN_QUERYCANCEL:
			// Can use this to ask user to confirm cancel
			break;
		case PSN_RESET:
			SaveStateDlg_CANCEL(window);
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
				SendDlgItemMessage(window, IDC_SAVESTATE_FILENAME, WM_SETTEXT, 0, (LPARAM) g_szNewFilename);
			break;
		case IDC_SAVESTATE_ON_EXIT:
			break;
		case IDC_SAVESTATE:
			afterclose = WM_USER_SAVESTATE;
			break;
		case IDC_LOADSTATE:
			afterclose = WM_USER_LOADSTATE;
			break;
      }
      break;

    case WM_INITDIALOG:
	{
      g_nLastPage = PG_SAVESTATE;

	  SendDlgItemMessage(window,IDC_SAVESTATE_FILENAME,WM_SETTEXT,0,(LPARAM)Snapshot_GetFilename());

	  CheckDlgButton(window, IDC_SAVESTATE_ON_EXIT, g_bSaveStateOnExit ? BST_CHECKED : BST_UNCHECKED);

	  g_szNewDirectory[0] = 0x00;

      afterclose = 0;
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

static void DiskDlg_OK(HWND window, BOOL afterclose)
{
	BOOL  newdisktype   = (BOOL) SendDlgItemMessage(window,IDC_DISKTYPE,CB_GETCURSEL,0,0);

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

	SAVE(TEXT("Enhance Disk Speed"),newdisktype);
	SAVE(TEXT(REGVALUE_HDD_ENABLED), bHDDIsEnabled ? 1 : 0);
	RegSaveString(TEXT("Configuration"), TEXT(REGVALUE_HDD_IMAGE1), 1, HD_GetFullName(0));
	RegSaveString(TEXT("Configuration"), TEXT(REGVALUE_HDD_IMAGE2), 1, HD_GetFullName(1));

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
  static BOOL afterclose = 0;

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
			DiskSelect(0);
			SendDlgItemMessage(window,IDC_EDIT_DISK1,WM_SETTEXT,0,(LPARAM)DiskGetFullName(0));
			break;
		case IDC_DISK2:
			DiskSelect(1);
			SendDlgItemMessage(window,IDC_EDIT_DISK2,WM_SETTEXT,0,(LPARAM)DiskGetFullName(1));
			break;
		case IDC_HDD1:
			if(IsDlgButtonChecked(window, IDC_HDD_ENABLE))
			{
				HD_Select(0);
				SendDlgItemMessage(window,IDC_EDIT_HDD1,WM_SETTEXT,0,(LPARAM)HD_GetFullName(0));
			}
			break;
		case IDC_HDD2:
			if(IsDlgButtonChecked(window, IDC_HDD_ENABLE))
			{
				HD_Select(1);
				SendDlgItemMessage(window,IDC_EDIT_HDD2,WM_SETTEXT,0,(LPARAM)HD_GetFullName(1));
			}
			break;
		case IDC_HDD_ENABLE:
			EnableHDD(window, IsDlgButtonChecked(window, IDC_HDD_ENABLE));
			break;
      }
      break;

    case WM_INITDIALOG:
	{
		g_nLastPage = PG_DISK;

		FillComboBox(window,IDC_DISKTYPE,discchoices,enhancedisk);

		SendDlgItemMessage(window,IDC_EDIT_DISK1,WM_SETTEXT,0,(LPARAM)DiskGetFullName(0));
		SendDlgItemMessage(window,IDC_EDIT_DISK2,WM_SETTEXT,0,(LPARAM)DiskGetFullName(1));

		SendDlgItemMessage(window,IDC_EDIT_HDD1,WM_SETTEXT,0,(LPARAM)HD_GetFullName(0));
		SendDlgItemMessage(window,IDC_EDIT_HDD2,WM_SETTEXT,0,(LPARAM)HD_GetFullName(1));

		CheckDlgButton(window, IDC_HDD_ENABLE, HD_CardIsEnabled() ? BST_CHECKED : BST_UNCHECKED);

		EnableHDD(window, IsDlgButtonChecked(window, IDC_HDD_ENABLE));

		afterclose = 0;
	}
  }

  return 0;
}

static BOOL get_tfename(int number, char **ppname, char **ppdescription)
{
    if (tfe_enumadapter_open()) {
        char *pname = NULL;
        char *pdescription = NULL;

        while (number--) {
            if (!tfe_enumadapter(&pname, &pdescription))
                break;

            lib_free(pname);
            lib_free(pdescription);
        }

        if (tfe_enumadapter(&pname, &pdescription)) {
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
	  LOAD(TEXT("Uthernet Disabled")  ,(DWORD *)&disabled);
	  get_disabled_state(&disabled);

    if (disabled) {
        EnableWindow(GetDlgItem(hwnd, IDC_TFE_SETTINGS_ENABLE_T), 0);
        EnableWindow(GetDlgItem(hwnd, IDC_TFE_SETTINGS_ENABLE), 0);
        EnableWindow(GetDlgItem(hwnd, IDOK), 0);
        SetWindowText(GetDlgItem(hwnd,IDC_TFE_SETTINGS_INTERFACE_NAME), "");
        SetWindowText(GetDlgItem(hwnd,IDC_TFE_SETTINGS_INTERFACE_DESC), "");
        enable = 0;
    } else {
        enable = SendMessage(GetDlgItem(hwnd, IDC_TFE_SETTINGS_ENABLE),
                             CB_GETCURSEL, 0, 0) ? 1 : 0;
    }

    EnableWindow(GetDlgItem(hwnd, IDC_TFE_SETTINGS_INTERFACE_T), enable);
    EnableWindow(GetDlgItem(hwnd, IDC_TFE_SETTINGS_INTERFACE), enable);

    if (enable) {
        char *pname = NULL;
        char *pdescription = NULL;

        number = SendMessage(GetDlgItem(hwnd, IDC_TFE_SETTINGS_INTERFACE),
                             CB_GETCURSEL, 0, 0);

        if (get_tfename(number, &pname, &pdescription)) {
            SetWindowText(GetDlgItem(hwnd, IDC_TFE_SETTINGS_INTERFACE_NAME),
                          pname);
            SetWindowText(GetDlgItem(hwnd, IDC_TFE_SETTINGS_INTERFACE_DESC),
                          pdescription);
            lib_free(pname);
            lib_free(pdescription);
        }
    } else {
        SetWindowText(GetDlgItem(hwnd, IDC_TFE_SETTINGS_INTERFACE_NAME), "");
        SetWindowText(GetDlgItem(hwnd, IDC_TFE_SETTINGS_INTERFACE_DESC), "");
    }

    return disabled ? 1 : 0;
}

static uilib_localize_dialog_param tfe_dialog[] = {
    {0, IDS_TFE_CAPTION, -1},
    {IDC_TFE_SETTINGS_ENABLE_T, IDS_TFE_ETHERNET, 0},
    {IDC_TFE_SETTINGS_INTERFACE_T, IDS_TFE_INTERFACE, 0},
    {IDOK, IDS_OK, 0},
    {IDCANCEL, IDS_CANCEL, 0},
    {0, 0, 0}
};

static uilib_dialog_group tfe_leftgroup[] = {
    {IDC_TFE_SETTINGS_ENABLE_T, 0},
    {IDC_TFE_SETTINGS_INTERFACE_T, 0},
    {0, 0}
};

static uilib_dialog_group tfe_rightgroup[] = {
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

//    resources_get_value("ETHERNET_ACTIVE", (void *)&tfe_enabled);
	get_tfe_enabled(&tfe_enable);

    //resources_get_value("ETHERNET_AS_RR", (void *)&tfe_as_rr_net);
    active_value = (tfe_enable ? 1 : 0);

    temp_hwnd=GetDlgItem(hwnd,IDC_TFE_SETTINGS_ENABLE);
    SendMessage(temp_hwnd, CB_ADDSTRING, 0, (LPARAM)"Disabled");
    SendMessage(temp_hwnd, CB_ADDSTRING, 0, (LPARAM)"Uthernet");
    SendMessage(temp_hwnd, CB_SETCURSEL, (WPARAM)active_value, 0);

//    resources_get_value("ETHERNET_INTERFACE", (void *)&interface_name);
	interface_name = (char *) get_tfe_interface();

    if (tfe_enumadapter_open()) {
        int cnt = 0;

        char *pname;
        char *pdescription;

        temp_hwnd=GetDlgItem(hwnd,IDC_TFE_SETTINGS_INTERFACE);

        for (cnt = 0; tfe_enumadapter(&pname, &pdescription); cnt++) {
            BOOL this_entry = FALSE;

            if (strcmp(pname, interface_name) == 0) {
                this_entry = TRUE;
            }

            SetWindowText(GetDlgItem(hwnd, IDC_TFE_SETTINGS_INTERFACE_NAME),
                          pname);
            SetWindowText(GetDlgItem(hwnd, IDC_TFE_SETTINGS_INTERFACE_DESC),
                          pdescription);
            SendMessage(temp_hwnd, CB_ADDSTRING, 0, (LPARAM)pname);
            lib_free(pname);
            lib_free(pdescription);

            if (this_entry) {
                SendMessage(GetDlgItem(hwnd, IDC_TFE_SETTINGS_INTERFACE),
                            CB_SETCURSEL, (WPARAM)cnt, 0);
            }
        }

        tfe_enumadapter_close();
    }

    if (gray_ungray_items(hwnd)) {
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
    GetDlgItemText(hwnd, IDC_TFE_SETTINGS_INTERFACE, buffer, 255);
	
	// RGJ - Added check for NULL interface so we don't set it active without a valid interface selected
	if (strlen(buffer) > 0) {
		RegSaveString(TEXT("Configuration"), TEXT("Uthernet Interface"), 1, buffer);
	
		active_value = SendMessage(GetDlgItem(hwnd, IDC_TFE_SETTINGS_ENABLE),
			                       CB_GETCURSEL, 0, 0);

		tfe_enabled = active_value >= 1 ? 1 : 0;
		SAVE(TEXT("Uthernet Active")  ,tfe_enabled);
	} else {
		SAVE(TEXT("Uthernet Active")  ,0);
	}

	
}

static BOOL CALLBACK TfeDlgProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    switch (msg) {
      case WM_COMMAND:
        switch (LOWORD(wparam)) {
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
    DialogBox(g_hInstance, (LPCTSTR)IDD_TFE_SETTINGS_DIALOG, hwnd,
              TfeDlgProc);
}


//===========================================================================

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

	PropSheetPages[PG_SAVESTATE].dwSize = sizeof(PROPSHEETPAGE);
	PropSheetPages[PG_SAVESTATE].dwFlags = PSP_DEFAULT;
	PropSheetPages[PG_SAVESTATE].hInstance = g_hInstance;
	PropSheetPages[PG_SAVESTATE].pszTemplate = MAKEINTRESOURCE(IDD_PROPPAGE_SAVESTATE);
	PropSheetPages[PG_SAVESTATE].pfnDlgProc = (DLGPROC)SaveStateDlgProc;

	PropSheetPages[PG_DISK].dwSize = sizeof(PROPSHEETPAGE);
	PropSheetPages[PG_DISK].dwFlags = PSP_DEFAULT;
	PropSheetPages[PG_DISK].hInstance = g_hInstance;
	PropSheetPages[PG_DISK].pszTemplate = MAKEINTRESOURCE(IDD_PROPPAGE_DISK);
	PropSheetPages[PG_DISK].pfnDlgProc = (DLGPROC)DiskDlgProc;

	PROPSHEETHEADER PropSheetHeader;

	PropSheetHeader.dwSize = sizeof(PROPSHEETHEADER);
	PropSheetHeader.dwFlags = PSH_NOAPPLYNOW | /* PSH_NOCONTEXTHELP | */ PSH_PROPSHEETPAGE;
	PropSheetHeader.hwndParent = g_hFrameWindow;
	PropSheetHeader.pszCaption = "AppleWin Configuration";
	PropSheetHeader.nPages = PG_NUM_SHEETS;
	PropSheetHeader.nStartPage = g_nLastPage;
	PropSheetHeader.ppsp = PropSheetPages;

	int i = PropertySheet(&PropSheetHeader);	// Result: 0=Cancel, 1=OK
}

DWORD PSP_GetVolumeMax()
{
	return VOLUME_MAX;
}

bool PSP_SaveStateSelectImage(HWND hWindow, bool bSave)
{
	g_szNewDirectory[0] = 0x00;

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
