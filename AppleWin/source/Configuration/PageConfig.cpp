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

BOOL CALLBACK CPageConfig::DlgProc(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	// Switch from static func to our instance
	return CPageConfig::ms_this->DlgProcInternal(hWnd, message, wparam, lparam);
}

BOOL CPageConfig::DlgProcInternal(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam)
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
				// About to stop being active page
				{
					DWORD NewComputerMenuItem = (DWORD) SendDlgItemMessage(hWnd, IDC_COMPUTER, CB_GETCURSEL, 0, 0);
					SetWindowLong(hWnd, DWL_MSGRESULT, FALSE);		// Changes are valid
				}
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
		case IDC_AUTHENTIC_SPEED:	// Authentic Machine Speed
			SendDlgItemMessage(hWnd,IDC_SLIDER_CPU_SPEED,TBM_SETPOS,1,SPEED_NORMAL);
			EnableTrackbar(hWnd,0);
			break;

		case IDC_CUSTOM_SPEED:		// Select Custom Speed
			SetFocus(GetDlgItem(hWnd,IDC_SLIDER_CPU_SPEED));
			EnableTrackbar(hWnd,1);
			break;

		case IDC_SLIDER_CPU_SPEED:	// CPU speed slider
			CheckRadioButton(hWnd,IDC_AUTHENTIC_SPEED,IDC_CUSTOM_SPEED,IDC_CUSTOM_SPEED);
			EnableTrackbar(hWnd,1);
			break;

		case IDC_BENCHMARK:
			if (!IsOkToBenchmark(hWnd, m_PropertySheetHelper.IsConfigChanged()))
				break;
			m_PropertySheetHelper.SetDoBenchmark();
			PropSheet_PressButton(GetParent(hWnd), PSBTN_OK);
			break;

		case IDC_ETHERNET:
			ui_tfe_settings_dialog(hWnd);
			break;

		case IDC_MONOCOLOR:
			VideoChooseColor();
			break;

		case IDC_CHECK_HALF_SCAN_LINES:
			g_uHalfScanLines = IsDlgButtonChecked(hWnd, IDC_CHECK_HALF_SCAN_LINES) ? 1 : 0;

		case IDC_COMPUTER:
			if(HIWORD(wparam) == CBN_SELCHANGE)
			{
				const DWORD NewComputerMenuItem = (DWORD) SendDlgItemMessage(hWnd, IDC_COMPUTER, CB_GETCURSEL, 0, 0);
				const eApple2Type NewApple2Type = GetApple2Type(NewComputerMenuItem);
				m_PropertySheetHelper.GetConfigNew().m_Apple2Type = NewApple2Type;
			}
			break;

#if 0
		case IDC_RECALIBRATE:
			RegSaveValue(TEXT(""),TEXT("RunningOnOS"),0,0);
			if (MessageBox(hWnd,
				TEXT("The emulator has been set to recalibrate ")
				TEXT("itself the next time it is started.\n\n")
				TEXT("Would you like to restart the emulator now?"),
				TEXT(REG_CONFIG),
				MB_ICONQUESTION | MB_OKCANCEL | MB_SETFOREGROUND) == IDOK)
			{
					PropSheet_PressButton(GetParent(hWnd), PSBTN_OK);
			}
			break;
#endif
		} // switch( (LOWORD(wparam))
		break; // WM_COMMAND

	case WM_HSCROLL:
		CheckRadioButton(hWnd, IDC_AUTHENTIC_SPEED, IDC_CUSTOM_SPEED, IDC_CUSTOM_SPEED);	// FirstButton, LastButton, CheckButton
		break;

	case WM_INITDIALOG:
		{
			// Convert Apple2 type to menu item
			{
				int nCurrentChoice = 0;
				switch (m_PropertySheetHelper.GetConfigNew().m_Apple2Type)
				{
				case A2TYPE_APPLE2:			nCurrentChoice = MENUITEM_IIORIGINAL; break;
				case A2TYPE_APPLE2PLUS:		nCurrentChoice = MENUITEM_IIPLUS; break;
				case A2TYPE_APPLE2E:		nCurrentChoice = MENUITEM_IIE; break;
				case A2TYPE_APPLE2EENHANCED:nCurrentChoice = MENUITEM_ENHANCEDIIE; break;
				case A2TYPE_PRAVETS82:		nCurrentChoice = MENUITEM_CLONE; break;
				case A2TYPE_PRAVETS8M:		nCurrentChoice = MENUITEM_CLONE; break;
				case A2TYPE_PRAVETS8A:		nCurrentChoice = MENUITEM_CLONE; break;
				}

				m_PropertySheetHelper.FillComboBox(hWnd, IDC_COMPUTER, m_ComputerChoices, nCurrentChoice);
			}

			m_PropertySheetHelper.FillComboBox(hWnd,IDC_VIDEOTYPE,g_aVideoChoices,g_eVideoType);
			CheckDlgButton(hWnd, IDC_CHECK_HALF_SCAN_LINES, g_uHalfScanLines ? BST_CHECKED : BST_UNCHECKED);

			m_PropertySheetHelper.FillComboBox(hWnd,IDC_SERIALPORT, sg_SSC.GetSerialPortChoices(), sg_SSC.GetSerialPort());
			EnableWindow(GetDlgItem(hWnd, IDC_SERIALPORT), !sg_SSC.IsActive() ? TRUE : FALSE);

			SendDlgItemMessage(hWnd,IDC_SLIDER_CPU_SPEED,TBM_SETRANGE,1,MAKELONG(0,40));
			SendDlgItemMessage(hWnd,IDC_SLIDER_CPU_SPEED,TBM_SETPAGESIZE,0,5);
			SendDlgItemMessage(hWnd,IDC_SLIDER_CPU_SPEED,TBM_SETTICFREQ,10,0);
			SendDlgItemMessage(hWnd,IDC_SLIDER_CPU_SPEED,TBM_SETPOS,1,g_dwSpeed);

			{
				BOOL bCustom = TRUE;
				if (g_dwSpeed == SPEED_NORMAL)
				{
					bCustom = FALSE;
					REGLOAD(TEXT(REGVALUE_CUSTOM_SPEED),(DWORD *)&bCustom);
				}
				CheckRadioButton(hWnd, IDC_AUTHENTIC_SPEED, IDC_CUSTOM_SPEED, bCustom ? IDC_CUSTOM_SPEED : IDC_AUTHENTIC_SPEED);
				SetFocus(GetDlgItem(hWnd, bCustom ? IDC_SLIDER_CPU_SPEED : IDC_AUTHENTIC_SPEED));
				EnableTrackbar(hWnd, bCustom);
			}

			InitOptions(hWnd);

			break;
		}

	case WM_LBUTTONDOWN:
		{
			POINT pt = { LOWORD(lparam), HIWORD(lparam) };
			ClientToScreen(hWnd,&pt);
			RECT rect;
			GetWindowRect(GetDlgItem(hWnd,IDC_SLIDER_CPU_SPEED),&rect);
			if ((pt.x >= rect.left) && (pt.x <= rect.right) &&
				(pt.y >= rect.top) && (pt.y <= rect.bottom))
			{
				CheckRadioButton(hWnd, IDC_AUTHENTIC_SPEED, IDC_CUSTOM_SPEED, IDC_CUSTOM_SPEED);
				EnableTrackbar(hWnd,1);
				SetFocus(GetDlgItem(hWnd,IDC_SLIDER_CPU_SPEED));
				ScreenToClient(GetDlgItem(hWnd,IDC_SLIDER_CPU_SPEED),&pt);
				PostMessage(GetDlgItem(hWnd,IDC_SLIDER_CPU_SPEED),WM_LBUTTONDOWN,wparam,MAKELONG(pt.x,pt.y));
			}
			break;
		}

	case WM_SYSCOLORCHANGE:
		SendDlgItemMessage(hWnd,IDC_SLIDER_CPU_SPEED,WM_SYSCOLORCHANGE,0,0);
		break;
	}

	return FALSE;
}

void CPageConfig::DlgOK(HWND hWnd)
{
	const DWORD newvidtype = (DWORD) SendDlgItemMessage(hWnd, IDC_VIDEOTYPE, CB_GETCURSEL, 0, 0);
	if (g_eVideoType != newvidtype)
	{
		g_eVideoType = newvidtype;
		VideoReinitialize();
		if ((g_nAppMode != MODE_LOGO) && (g_nAppMode != MODE_DEBUG))
		{
			VideoRedrawScreen();
		}
	}

	const DWORD newserialport = (DWORD) SendDlgItemMessage(hWnd, IDC_SERIALPORT, CB_GETCURSEL, 0, 0);
	sg_SSC.CommSetSerialPort(hWnd, newserialport);
	RegSaveString(	TEXT(REG_CONFIG),
					TEXT(REGVALUE_SERIAL_PORT_NAME),
					TRUE,
					sg_SSC.GetSerialPortName() );
	
	if (IsDlgButtonChecked(hWnd, IDC_AUTHENTIC_SPEED))
		g_dwSpeed = SPEED_NORMAL;
	else
		g_dwSpeed = SendDlgItemMessage(hWnd, IDC_SLIDER_CPU_SPEED,TBM_GETPOS, 0, 0);

	SetCurrentCLK6502();

	REGSAVE(TEXT(REGVALUE_CUSTOM_SPEED), IsDlgButtonChecked(hWnd, IDC_CUSTOM_SPEED));
	REGSAVE(TEXT(REGVALUE_EMULATION_SPEED), g_dwSpeed);

	Config_Save_Video();

	m_PropertySheetHelper.PostMsgAfterClose(hWnd, m_Page);
}

void CPageConfig::InitOptions(HWND hWnd)
{
	// Nothing to do:
	// - no changes made on any other pages affect this page
}

// Config->Computer: Menu item to eApple2Type
eApple2Type CPageConfig::GetApple2Type(DWORD NewMenuItem)
{
	switch (NewMenuItem)
	{
		case MENUITEM_IIORIGINAL:	return A2TYPE_APPLE2;
		case MENUITEM_IIPLUS:		return A2TYPE_APPLE2PLUS;
		case MENUITEM_IIE:			return A2TYPE_APPLE2E;
		case MENUITEM_ENHANCEDIIE:	return A2TYPE_APPLE2EENHANCED;
		case MENUITEM_CLONE:		return A2TYPE_CLONE;
		default:					return A2TYPE_APPLE2EENHANCED;
	}
}

void CPageConfig::EnableTrackbar(HWND hWnd, BOOL enable)
{
	EnableWindow(GetDlgItem(hWnd,IDC_SLIDER_CPU_SPEED),enable);
	EnableWindow(GetDlgItem(hWnd,IDC_0_5_MHz),enable);
	EnableWindow(GetDlgItem(hWnd,IDC_1_0_MHz),enable);
	EnableWindow(GetDlgItem(hWnd,IDC_2_0_MHz),enable);
	EnableWindow(GetDlgItem(hWnd,IDC_MAX_MHz),enable);
}


void CPageConfig::ui_tfe_settings_dialog(HWND hwnd)
{
	DialogBox(g_hInstance, (LPCTSTR)IDD_TFE_SETTINGS_DIALOG, hwnd, CPageConfigTfe::DlgProc);
}

bool CPageConfig::IsOkToBenchmark(HWND hWnd, const bool bConfigChanged)
{
	if (bConfigChanged)
	{
		if (MessageBox(hWnd,
				TEXT("The hardware configuration has changed. Benchmarking will lose these changes.\n\n")
				TEXT("Are you sure you want to do this?"),
				TEXT("Benchmarks"),
				MB_ICONQUESTION | MB_OKCANCEL | MB_SETFOREGROUND) == IDCANCEL)
			return false;
	}

	if (g_nAppMode == MODE_LOGO)
		return true;

	if (MessageBox(hWnd,
			TEXT("Running the benchmarks will reset the state of ")
			TEXT("the emulated machine, causing you to lose any ")
			TEXT("unsaved work.\n\n")
			TEXT("Are you sure you want to do this?"),
			TEXT("Benchmarks"),
			MB_ICONQUESTION | MB_OKCANCEL | MB_SETFOREGROUND) == IDCANCEL)
		return false;

	return true;
}
