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

#include "PageConfig.h"
#include "PropertySheet.h"

#include "../Windows/AppleWin.h"
#include "../Windows/WinFrame.h"
#include "../Registry.h"
#include "../SerialComms.h"
#include "../Windows/WinVideo.h"
#include "../resource/resource.h"

CPageConfig* CPageConfig::ms_this = 0;	// reinit'd in ctor

enum APPLEIICHOICE {MENUITEM_IIORIGINAL, MENUITEM_IIPLUS, MENUITEM_IIJPLUS, MENUITEM_IIE, MENUITEM_ENHANCEDIIE, MENUITEM_CLONE};
const TCHAR CPageConfig::m_ComputerChoices[] =
				TEXT("Apple ][ (Original)\0")
				TEXT("Apple ][+\0")
				TEXT("Apple ][ J-Plus\0")
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
			GetVideo().ChooseMonochromeColor();
			break;

		case IDC_CHECK_CONFIRM_REBOOT:
		case IDC_CHECK_HALF_SCAN_LINES:
		case IDC_CHECK_VERTICAL_BLEND:
		case IDC_CHECK_FS_SHOW_SUBUNIT_STATUS:
		case IDC_CHECK_50HZ_VIDEO:
			// Checked in DlgOK()
			break;

		case IDC_COMPUTER:
			if(HIWORD(wparam) == CBN_SELCHANGE)
			{
				const DWORD NewComputerMenuItem = (DWORD) SendDlgItemMessage(hWnd, IDC_COMPUTER, CB_GETCURSEL, 0, 0);
				const eApple2Type NewApple2Type = GetApple2Type(NewComputerMenuItem);
				m_PropertySheetHelper.GetConfigNew().m_Apple2Type = NewApple2Type;
				if (NewApple2Type != A2TYPE_CLONE)
				{
					m_PropertySheetHelper.GetConfigNew().m_CpuType = ProbeMainCpuDefault(NewApple2Type);
				}
				else // A2TYPE_CLONE
				{
					// NB. A2TYPE_CLONE could be either 6502(Pravets) or 65C02(TK3000 //e)
					// - Set correctly in PageAdvanced.cpp for IDC_CLONETYPE
					m_PropertySheetHelper.GetConfigNew().m_CpuType = CPU_UNKNOWN;
				}
			}
			break;

		case IDC_VIDEOTYPE:
			if(HIWORD(wparam) == CBN_SELCHANGE)
			{
				const VideoType_e newVideoType = (VideoType_e) SendDlgItemMessage(hWnd, IDC_VIDEOTYPE, CB_GETCURSEL, 0, 0);
				EnableWindow(GetDlgItem(hWnd, IDC_CHECK_VERTICAL_BLEND), (newVideoType == VT_COLOR_IDEALIZED) ? TRUE : FALSE);
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
				case A2TYPE_APPLE2JPLUS:	nCurrentChoice = MENUITEM_IIJPLUS; break;
				case A2TYPE_APPLE2E:		nCurrentChoice = MENUITEM_IIE; break;
				case A2TYPE_APPLE2EENHANCED:nCurrentChoice = MENUITEM_ENHANCEDIIE; break;
				case A2TYPE_PRAVETS82:		nCurrentChoice = MENUITEM_CLONE; break;
				case A2TYPE_PRAVETS8M:		nCurrentChoice = MENUITEM_CLONE; break;
				case A2TYPE_PRAVETS8A:		nCurrentChoice = MENUITEM_CLONE; break;
				case A2TYPE_TK30002E:		nCurrentChoice = MENUITEM_CLONE; break;
				case A2TYPE_BASE64A:		nCurrentChoice = MENUITEM_CLONE; break;
				default: _ASSERT(0); break;
				}

				m_PropertySheetHelper.FillComboBox(hWnd, IDC_COMPUTER, m_ComputerChoices, nCurrentChoice);
			}

			CheckDlgButton(hWnd, IDC_CHECK_CONFIRM_REBOOT, GetFrame().g_bConfirmReboot ? BST_CHECKED : BST_UNCHECKED );

			m_PropertySheetHelper.FillComboBox(hWnd,IDC_VIDEOTYPE, GetVideo().GetVideoChoices(), GetVideo().GetVideoType());
			CheckDlgButton(hWnd, IDC_CHECK_HALF_SCAN_LINES, GetVideo().IsVideoStyle(VS_HALF_SCANLINES) ? BST_CHECKED : BST_UNCHECKED);
			CheckDlgButton(hWnd, IDC_CHECK_FS_SHOW_SUBUNIT_STATUS, GetFullScreenShowSubunitStatus() ? BST_CHECKED : BST_UNCHECKED);

			CheckDlgButton(hWnd, IDC_CHECK_VERTICAL_BLEND, GetVideo().IsVideoStyle(VS_COLOR_VERTICAL_BLEND) ? BST_CHECKED : BST_UNCHECKED);
			EnableWindow(GetDlgItem(hWnd, IDC_CHECK_VERTICAL_BLEND), (GetVideo().GetVideoType() == VT_COLOR_IDEALIZED) ? TRUE : FALSE);

			if (GetCardMgr().IsSSCInstalled())
			{
				CSuperSerialCard* pSSC = GetCardMgr().GetSSC();
				m_PropertySheetHelper.FillComboBox(hWnd, IDC_SERIALPORT, pSSC->GetSerialPortChoices(), pSSC->GetSerialPort());
				EnableWindow(GetDlgItem(hWnd, IDC_SERIALPORT), !pSSC->IsActive() ? TRUE : FALSE);
			}
			else
			{
				EnableWindow(GetDlgItem(hWnd, IDC_SERIALPORT), FALSE);
			}

			CheckDlgButton(hWnd, IDC_CHECK_50HZ_VIDEO, (GetVideo().GetVideoRefreshRate() == VR_50HZ) ? BST_CHECKED : BST_UNCHECKED);

			SendDlgItemMessage(hWnd,IDC_SLIDER_CPU_SPEED,TBM_SETRANGE,1,MAKELONG(0,40));
			SendDlgItemMessage(hWnd,IDC_SLIDER_CPU_SPEED,TBM_SETPAGESIZE,0,5);
			SendDlgItemMessage(hWnd,IDC_SLIDER_CPU_SPEED,TBM_SETTICFREQ,10,0);
			SendDlgItemMessage(hWnd,IDC_SLIDER_CPU_SPEED,TBM_SETPOS,1,g_dwSpeed);

			{
				BOOL bCustom = TRUE;
				if (g_dwSpeed == SPEED_NORMAL)
				{
					DWORD dwCustomSpeed;
					REGLOAD_DEFAULT(TEXT(REGVALUE_CUSTOM_SPEED), &dwCustomSpeed, 0);
					bCustom = dwCustomSpeed ? TRUE : FALSE;
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
	bool bVideoReinit = false;

	const VideoType_e newVideoType = (VideoType_e) SendDlgItemMessage(hWnd, IDC_VIDEOTYPE, CB_GETCURSEL, 0, 0);
	if (GetVideo().GetVideoType() != newVideoType)
	{
		GetVideo().SetVideoType(newVideoType);
		bVideoReinit = true;
	}

	const bool newHalfScanLines = IsDlgButtonChecked(hWnd, IDC_CHECK_HALF_SCAN_LINES) != 0;
	const bool currentHalfScanLines = GetVideo().IsVideoStyle(VS_HALF_SCANLINES);
	if (currentHalfScanLines != newHalfScanLines)
	{
		if (newHalfScanLines)
			GetVideo().SetVideoStyle( (VideoStyle_e) (GetVideo().GetVideoStyle() | VS_HALF_SCANLINES) );
		else
			GetVideo().SetVideoStyle( (VideoStyle_e) (GetVideo().GetVideoStyle() & ~VS_HALF_SCANLINES) );
		bVideoReinit = true;
	}

	const bool newVerticalBlend = IsDlgButtonChecked(hWnd, IDC_CHECK_VERTICAL_BLEND) != 0;
	const bool currentVerticalBlend = GetVideo().IsVideoStyle(VS_COLOR_VERTICAL_BLEND);
	if (currentVerticalBlend != newVerticalBlend)
	{
		if (newVerticalBlend)
			GetVideo().SetVideoStyle( (VideoStyle_e) (GetVideo().GetVideoStyle() | VS_COLOR_VERTICAL_BLEND) );
		else
			GetVideo().SetVideoStyle( (VideoStyle_e) (GetVideo().GetVideoStyle() & ~VS_COLOR_VERTICAL_BLEND) );
		bVideoReinit = true;
	}

	const bool isNewVideoRate50Hz = IsDlgButtonChecked(hWnd, IDC_CHECK_50HZ_VIDEO) != 0;
	const bool isCurrentVideoRate50Hz = GetVideo().GetVideoRefreshRate() == VR_50HZ;
	if (isCurrentVideoRate50Hz != isNewVideoRate50Hz)
	{
		m_PropertySheetHelper.GetConfigNew().m_videoRefreshRate = isNewVideoRate50Hz ? VR_50HZ : VR_60HZ;
	}

	if (bVideoReinit)
	{
		GetVideo().Config_Save_Video();

		GetFrame().FrameRefreshStatus(DRAW_TITLE, false);

		GetVideo().VideoReinitialize();
		if ((g_nAppMode != MODE_LOGO) && (g_nAppMode != MODE_DEBUG))
		{
			GetFrame().VideoRedrawScreen();
		}
	}

	//

	const bool bNewFSSubunitStatus = IsDlgButtonChecked(hWnd, IDC_CHECK_FS_SHOW_SUBUNIT_STATUS) ? true : false;
	if (GetFullScreenShowSubunitStatus() != bNewFSSubunitStatus)
	{
		REGSAVE(TEXT(REGVALUE_FS_SHOW_SUBUNIT_STATUS), bNewFSSubunitStatus ? 1 : 0);
		GetFrame().SetFullScreenShowSubunitStatus(bNewFSSubunitStatus);

		if (IsFullScreen())
			GetFrame().FrameRefreshStatus(DRAW_BACKGROUND | DRAW_LEDS | DRAW_DISK_STATUS);
	}

	//

	const BOOL bNewConfirmReboot = IsDlgButtonChecked(hWnd, IDC_CHECK_CONFIRM_REBOOT) ? 1 : 0;
	if (GetFrame().g_bConfirmReboot != bNewConfirmReboot)
	{
		REGSAVE(TEXT(REGVALUE_CONFIRM_REBOOT), bNewConfirmReboot);
		GetFrame().g_bConfirmReboot = bNewConfirmReboot;
	}

	//

	if (GetCardMgr().IsSSCInstalled())
	{
		CSuperSerialCard* pSSC = GetCardMgr().GetSSC();
		const DWORD uNewSerialPort = (DWORD) SendDlgItemMessage(hWnd, IDC_SERIALPORT, CB_GETCURSEL, 0, 0);
		pSSC->CommSetSerialPort(hWnd, uNewSerialPort);
		RegSaveString(	TEXT(REG_CONFIG),
						TEXT(REGVALUE_SERIAL_PORT_NAME),
						TRUE,
						pSSC->GetSerialPortName() );
	}

	//

	if (IsDlgButtonChecked(hWnd, IDC_AUTHENTIC_SPEED))
		g_dwSpeed = SPEED_NORMAL;
	else
		g_dwSpeed = SendDlgItemMessage(hWnd, IDC_SLIDER_CPU_SPEED,TBM_GETPOS, 0, 0);

	SetCurrentCLK6502();

	REGSAVE(TEXT(REGVALUE_CUSTOM_SPEED), IsDlgButtonChecked(hWnd, IDC_CUSTOM_SPEED));
	REGSAVE(TEXT(REGVALUE_EMULATION_SPEED), g_dwSpeed);

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
		case MENUITEM_IIJPLUS:		return A2TYPE_APPLE2JPLUS;
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
	DialogBox(GetFrame().g_hInstance, (LPCTSTR)IDD_TFE_SETTINGS_DIALOG, hwnd, CPageConfigTfe::DlgProc);
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
