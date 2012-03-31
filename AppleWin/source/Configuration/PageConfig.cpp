#include "StdAfx.h"
#include "PageConfig.h"
#include "PropertySheetHelper.h"
#include "..\resource\resource.h"

CPageConfig* CPageConfig::ms_this = 0;	// reinit'd in ctor

enum APPLEIICHOICE {MENUITEM_IIORIGINAL, MENUITEM_IIPLUS, MENUITEM_IIE, MENUITEM_ENHANCEDIIE, MENUITEM_CLONE};
const TCHAR CPageConfig::m_ComputerChoices[] =
				TEXT("Apple ][ (Original)\0")
				TEXT("Apple ][+\0")
				TEXT("Apple //e\0")
				TEXT("Enhanced Apple //e\0")
				TEXT("Clone\0");

BOOL CALLBACK CPageConfig::DlgProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
	// Switch from static func to our instance
	return CPageConfig::ms_this->DlgProcInternal(window, message, wparam, lparam);
}

BOOL CPageConfig::DlgProcInternal(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
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
				break;
			case PSN_KILLACTIVE:
				// About to stop being active page
				{
					DWORD NewComputerMenuItem = (DWORD) SendDlgItemMessage(window, IDC_COMPUTER, CB_GETCURSEL, 0, 0);
					m_PropertySheetHelper.SetUIControlFreezeDlgButton( GetApple2Type(NewComputerMenuItem) <= A2TYPE_APPLE2PLUS ? UI_ENABLE : UI_DISABLE );
					m_PropertySheetHelper.SetUIControlCloneDropdownMenu( GetApple2Type(NewComputerMenuItem) == A2TYPE_CLONE ? UI_ENABLE : UI_DISABLE );
					SetWindowLong(window, DWL_MSGRESULT, FALSE);		// Changes are valid
				}
				break;
			case PSN_APPLY:
				DlgOK(window);
				SetWindowLong(window, DWL_MSGRESULT, PSNRET_NOERROR);	// Changes are valid
				break;
			case PSN_QUERYCANCEL:
				// Can use this to ask user to confirm cancel
				break;
			case PSN_RESET:
				DlgCANCEL(window);
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
			m_uAfterClose = WM_USER_BENCHMARK;
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
				MB_ICONQUESTION | MB_OKCANCEL | MB_SETFOREGROUND) == IDOK)
			{
					m_uAfterClose = WM_USER_RESTART;
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
			// Convert Apple2 type to menu item
			{
				int nCurrentChoice = 0;
				switch (g_Apple2Type)
				{
				case A2TYPE_APPLE2:			nCurrentChoice = MENUITEM_IIORIGINAL; break;
				case A2TYPE_APPLE2PLUS:		nCurrentChoice = MENUITEM_IIPLUS; break;
				case A2TYPE_APPLE2E:		nCurrentChoice = MENUITEM_IIE; break;
				case A2TYPE_APPLE2EEHANCED:	nCurrentChoice = MENUITEM_ENHANCEDIIE; break;
				case A2TYPE_PRAVETS82:		nCurrentChoice = MENUITEM_CLONE; break;
				case A2TYPE_PRAVETS8M:		nCurrentChoice = MENUITEM_CLONE; break;
				case A2TYPE_PRAVETS8A:		nCurrentChoice = MENUITEM_CLONE; break;
				}

				m_PropertySheetHelper.FillComboBox(window, IDC_COMPUTER, m_ComputerChoices, nCurrentChoice);
			}

			m_PropertySheetHelper.FillComboBox(window,IDC_VIDEOTYPE,g_aVideoChoices,g_eVideoType);
			CheckDlgButton(window, IDC_CHECK_HALF_SCAN_LINES, g_uHalfScanLines ? BST_CHECKED : BST_UNCHECKED);

			m_PropertySheetHelper.FillComboBox(window,IDC_SERIALPORT, sg_SSC.GetSerialPortChoices(), sg_SSC.GetSerialPort());
			EnableWindow(GetDlgItem(window, IDC_SERIALPORT), !sg_SSC.IsActive() ? TRUE : FALSE);

			SendDlgItemMessage(window,IDC_SLIDER_CPU_SPEED,TBM_SETRANGE,1,MAKELONG(0,40));
			SendDlgItemMessage(window,IDC_SLIDER_CPU_SPEED,TBM_SETPAGESIZE,0,5);
			SendDlgItemMessage(window,IDC_SLIDER_CPU_SPEED,TBM_SETTICFREQ,10,0);
			SendDlgItemMessage(window,IDC_SLIDER_CPU_SPEED,TBM_SETPOS,1,g_dwSpeed);

			{
				BOOL bCustom = TRUE;
				if (g_dwSpeed == SPEED_NORMAL)
				{
					bCustom = FALSE;
					REGLOAD(TEXT(REGVALUE_CUSTOM_SPEED),(DWORD *)&bCustom);
				}
				CheckRadioButton(window, IDC_AUTHENTIC_SPEED, IDC_CUSTOM_SPEED, bCustom ? IDC_CUSTOM_SPEED : IDC_AUTHENTIC_SPEED);
				SetFocus(GetDlgItem(window, bCustom ? IDC_SLIDER_CPU_SPEED : IDC_AUTHENTIC_SPEED));
				EnableTrackbar(window, bCustom);
			}

			m_uAfterClose = 0;
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

	return FALSE;
}

void CPageConfig::DlgOK(HWND window)
{
	const DWORD NewComputerMenuItem = (DWORD) SendDlgItemMessage(window,IDC_COMPUTER,CB_GETCURSEL,0,0);
	const DWORD newvidtype    = (DWORD)SendDlgItemMessage(window,IDC_VIDEOTYPE,CB_GETCURSEL,0,0);
	const DWORD newserialport = (DWORD)SendDlgItemMessage(window,IDC_SERIALPORT,CB_GETCURSEL,0,0);

	const eApple2Type NewApple2Type = GetApple2Type(NewComputerMenuItem);
	const eApple2Type OldApple2Type = IS_CLONE() ? (eApple2Type)A2TYPE_CLONE : g_Apple2Type;	// For clones, normalise to generic clone type

	if (NewApple2Type != OldApple2Type)
	{
		if ((MessageBox(window,
						TEXT(
						"You have changed the emulated computer "
						"type.  This change will not take effect "
						"until the next time you restart the "
						"emulator.\n\n"
						"Would you like to restart the emulator now?"),
						TEXT("Configuration"),
						MB_ICONQUESTION | MB_OKCANCEL | MB_SETFOREGROUND) == IDOK)
			&& m_PropertySheetHelper.IsOkToRestart(window))
		{
			m_uAfterClose = WM_USER_RESTART;
			m_PropertySheetHelper.SaveComputerType(NewApple2Type);
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

	RegSaveString(	TEXT("Configuration"),
					TEXT(REGVALUE_SERIAL_PORT_NAME),
					TRUE,
					sg_SSC.GetSerialPortName() );

	REGSAVE(TEXT(REGVALUE_CUSTOM_SPEED)      ,IsDlgButtonChecked(window,IDC_CUSTOM_SPEED));
	REGSAVE(TEXT(REGVALUE_EMULATION_SPEED)   ,g_dwSpeed);

	Config_Save_Video();

	m_PropertySheetHelper.PostMsgAfterClose(m_Page, m_uAfterClose);
}

// Config->Computer: Menu item to eApple2Type
eApple2Type CPageConfig::GetApple2Type(DWORD NewMenuItem)
{
	switch (NewMenuItem)
	{
		case MENUITEM_IIORIGINAL:	return A2TYPE_APPLE2;
		case MENUITEM_IIPLUS:		return A2TYPE_APPLE2PLUS;
		case MENUITEM_IIE:			return A2TYPE_APPLE2E;
		case MENUITEM_ENHANCEDIIE:	return A2TYPE_APPLE2EEHANCED;
		case MENUITEM_CLONE:		return A2TYPE_CLONE;
		default:					return A2TYPE_APPLE2EEHANCED;
	}
}

void CPageConfig::EnableTrackbar(HWND window, BOOL enable)
{
	EnableWindow(GetDlgItem(window,IDC_SLIDER_CPU_SPEED),enable);
	EnableWindow(GetDlgItem(window,IDC_0_5_MHz),enable);
	EnableWindow(GetDlgItem(window,IDC_1_0_MHz),enable);
	EnableWindow(GetDlgItem(window,IDC_2_0_MHz),enable);
	EnableWindow(GetDlgItem(window,IDC_MAX_MHz),enable);
}


void CPageConfig::ui_tfe_settings_dialog(HWND hwnd)
{
	DialogBox(g_hInstance, (LPCTSTR)IDD_TFE_SETTINGS_DIALOG, hwnd, CPageConfigTfe::DlgProc);
}
