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

// "Property Sheet Page flow" (same for all tabs):
// . CConfigNeedingRestart::Reload()
//		Before PSPs are created, emulator state is captured to m_PropertySheetHelper.GetConfigNew()
// . InitOptions()
//		Init based on m_PropertySheetHelper.GetConfigNew()
// . DlgOK() and DlgXXXCardOK()
//		Capture new state to m_PropertySheetHelper.GetConfigNew()
// . ApplyConfigAfterClose()
//		If user confirms state changes are OK, then save new state to Registry & update emulator

#include "StdAfx.h"

#include "PageConfig.h"
#include "PropertySheet.h"

#include "../Windows/Win32Frame.h"
#include "../Registry.h"
#include "../CardManager.h"
#include "../Interface.h"
#include "../Speaker.h"
#include "../resource/resource.h"

CPageConfig* CPageConfig::ms_this = 0;	// reinit'd in ctor

enum APPLEIICHOICE {MENUITEM_IIORIGINAL, MENUITEM_IIPLUS, MENUITEM_IIJPLUS, MENUITEM_IIE, MENUITEM_ENHANCEDIIE, MENUITEM_CLONE};
const char CPageConfig::m_ComputerChoices[] =
				"Apple ][ (Original)\0"
				"Apple ][+\0"
				"Apple ][ J-Plus\0"
				"Apple //e\0"
				"Enhanced Apple //e\0"
				"Clone\0";

INT_PTR CALLBACK CPageConfig::DlgProc(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	// Switch from static func to our instance
	return CPageConfig::ms_this->DlgProcInternal(hWnd, message, wparam, lparam);
}

INT_PTR CPageConfig::DlgProcInternal(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam)
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
					uint32_t NewComputerMenuItem = (uint32_t) SendDlgItemMessage(hWnd, IDC_COMPUTER, CB_GETCURSEL, 0, 0);
					SetWindowLongPtr(hWnd, DWLP_MSGRESULT, FALSE);		// Changes are valid
				}
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
		case IDC_AUTHENTIC_SPEED:	// Authentic Machine Speed
			SendDlgItemMessage(hWnd, IDC_SLIDER_CPU_SPEED, TBM_SETPOS, 1, SPEED_NORMAL);
			EnableTrackbar(hWnd, 0);
			break;

		case IDC_CUSTOM_SPEED:		// Select Custom Speed
			SetFocus(GetDlgItem(hWnd, IDC_SLIDER_CPU_SPEED));
			EnableTrackbar(hWnd, 1);
			break;

		case IDC_SLIDER_CPU_SPEED:	// CPU speed slider
			CheckRadioButton(hWnd, IDC_AUTHENTIC_SPEED, IDC_CUSTOM_SPEED, IDC_CUSTOM_SPEED);
			EnableTrackbar(hWnd, 1);
			break;

		case IDC_MONOCOLOR:
			{
				CHOOSECOLOR cc;
				memset(&cc, 0, sizeof(CHOOSECOLOR));
				cc.lStructSize = sizeof(CHOOSECOLOR);
				cc.hwndOwner = GetFrame().g_hFrameWindow;
				cc.rgbResult = m_PropertySheetHelper.GetConfigNew().m_monochromeRGB;
				cc.lpCustColors = m_customColors + 1;
				cc.Flags = CC_RGBINIT | CC_SOLIDCOLOR;
				if (ChooseColor(&cc))
				{
					m_PropertySheetHelper.GetConfigNew().m_monochromeRGB = cc.rgbResult;
				}
			}
			break;

		case IDC_CHECK_CONFIRM_REBOOT:
		case IDC_CHECK_HALF_SCAN_LINES:
		case IDC_CHECK_VERTICAL_BLEND:
		case IDC_CHECK_FS_SHOW_SUBUNIT_STATUS:
		case IDC_CHECK_50HZ_VIDEO:
		case IDC_SCROLLLOCK_TOGGLE:
			// Checked in DlgOK()
			break;

		case IDC_COMPUTER:
			if(HIWORD(wparam) == CBN_SELCHANGE)
			{
				const uint32_t newComputerMenuItem = (uint32_t) SendDlgItemMessage(hWnd, IDC_COMPUTER, CB_GETCURSEL, 0, 0);
				const eApple2Type newApple2Type = GetApple2Type(newComputerMenuItem);
				m_PropertySheetHelper.GetConfigNew().m_Apple2Type = newApple2Type;
				if (newApple2Type != A2TYPE_CLONE)
				{
					m_PropertySheetHelper.GetConfigNew().m_CpuType = ProbeMainCpuDefault(newApple2Type);
				}
				else // A2TYPE_CLONE
				{
					// NB. A2TYPE_CLONE could be either 6502(Pravets) or 65C02(TK3000 //e)
					// - Set correctly in PageAdvanced.cpp for IDC_CLONETYPE
					m_PropertySheetHelper.GetConfigNew().m_CpuType = CPU_UNKNOWN;
				}

				if (IsApple2Original(newApple2Type))
					m_PropertySheetHelper.GetConfigNew().m_Slot[SLOT0] = CT_Empty;
				else if (IsApple2PlusOrClone(newApple2Type))
					m_PropertySheetHelper.GetConfigNew().m_Slot[SLOT0] = CT_LanguageCard;
				else
					m_PropertySheetHelper.GetConfigNew().m_Slot[SLOT0] = CT_LanguageCardIIe;
			}
			break;

		case IDC_VIDEOTYPE:
			if(HIWORD(wparam) == CBN_SELCHANGE)
			{
				const VideoType_e newVideoType = (VideoType_e) SendDlgItemMessage(hWnd, IDC_VIDEOTYPE, CB_GETCURSEL, 0, 0);
				EnableWindow(GetDlgItem(hWnd, IDC_CHECK_VERTICAL_BLEND), (newVideoType == VT_COLOR_IDEALIZED) ? TRUE : FALSE);
			}
			break;

		case IDC_CONFIG_ALL_DEFAULT:
			if (!m_PropertySheetHelper.IsOkToResetConfig(hWnd))
				break;
			GetPropertySheet().ResetAllToDefault();
			InitOptions(hWnd);
			break;

		} // switch( (LOWORD(wparam))
		break; // WM_COMMAND

	case WM_INITDIALOG:
		InitOptions(hWnd);
		break;

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

// For InitOptions(), DlgOK() and ApplyConfigAfterClose(), see comment in PageConfig.cpp about "Property Sheet Page flow"
void CPageConfig::InitOptions(HWND hWnd)
{
	// Convert Apple2 type to menu item
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

	CheckDlgButton(hWnd, IDC_CHECK_CONFIRM_REBOOT, m_PropertySheetHelper.GetConfigNew().m_confirmReboot ? BST_CHECKED : BST_UNCHECKED);

	// Master Volume

	SendDlgItemMessage(hWnd, IDC_SLIDER_MASTER_VOLUME, TBM_SETRANGE, TRUE, MAKELONG(VOLUME_MIN, VOLUME_MAX));
	SendDlgItemMessage(hWnd, IDC_SLIDER_MASTER_VOLUME, TBM_SETPAGESIZE, 0, 10);
	SendDlgItemMessage(hWnd, IDC_SLIDER_MASTER_VOLUME, TBM_SETTICFREQ, 10, 0);
	SendDlgItemMessage(hWnd, IDC_SLIDER_MASTER_VOLUME, TBM_SETPOS, TRUE, VOLUME_MAX - m_PropertySheetHelper.GetConfigNew().m_masterVolume);	// Invert: L=MIN, R=MAX

	// Video

	m_PropertySheetHelper.FillComboBox(hWnd, IDC_VIDEOTYPE, GetVideo().GetVideoChoices(), m_PropertySheetHelper.GetConfigNew().m_videoType);
	const VideoStyle_e style = m_PropertySheetHelper.GetConfigNew().m_videoStyle;
	CheckDlgButton(hWnd, IDC_CHECK_HALF_SCAN_LINES, GetVideo().IsVideoStyle(style, VS_HALF_SCANLINES) ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(hWnd, IDC_CHECK_VERTICAL_BLEND, GetVideo().IsVideoStyle(style, VS_COLOR_VERTICAL_BLEND) ? BST_CHECKED : BST_UNCHECKED);
	EnableWindow(GetDlgItem(hWnd, IDC_CHECK_VERTICAL_BLEND), (m_PropertySheetHelper.GetConfigNew().m_videoType == VT_COLOR_IDEALIZED) ? TRUE : FALSE);
	CheckDlgButton(hWnd, IDC_CHECK_50HZ_VIDEO, (m_PropertySheetHelper.GetConfigNew().m_videoRefreshRate == VR_50HZ) ? BST_CHECKED : BST_UNCHECKED);

	CheckDlgButton(hWnd, IDC_CHECK_FS_SHOW_SUBUNIT_STATUS, m_PropertySheetHelper.GetConfigNew().m_fullScreen_ShowSubunitStatus ? BST_CHECKED : BST_UNCHECKED);

	// Emulation Speed

	CheckDlgButton(hWnd, IDC_ENHANCE_DISK_ENABLE, m_PropertySheetHelper.GetConfigNew().m_enhanceDiskAccessSpeed ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(hWnd, IDC_SCROLLLOCK_TOGGLE, m_PropertySheetHelper.GetConfigNew().m_scrollLockToggle ? BST_CHECKED : BST_UNCHECKED);

	SendDlgItemMessage(hWnd, IDC_SLIDER_CPU_SPEED, TBM_SETRANGE, TRUE, MAKELONG(0, 40));
	SendDlgItemMessage(hWnd, IDC_SLIDER_CPU_SPEED, TBM_SETPAGESIZE, 0, 5);
	SendDlgItemMessage(hWnd, IDC_SLIDER_CPU_SPEED, TBM_SETTICFREQ, 10, 0);
	SendDlgItemMessage(hWnd, IDC_SLIDER_CPU_SPEED, TBM_SETPOS, TRUE, m_PropertySheetHelper.GetConfigNew().m_machineSpeed);

	BOOL bCustom = m_PropertySheetHelper.GetConfigNew().m_machineSpeed != SPEED_NORMAL ? TRUE : FALSE;
	CheckRadioButton(hWnd, IDC_AUTHENTIC_SPEED, IDC_CUSTOM_SPEED, bCustom ? IDC_CUSTOM_SPEED : IDC_AUTHENTIC_SPEED);
	SetFocus(GetDlgItem(hWnd, bCustom ? IDC_SLIDER_CPU_SPEED : IDC_AUTHENTIC_SPEED));
	EnableTrackbar(hWnd, bCustom);
}

void CPageConfig::DlgOK(HWND hWnd)
{
	// This GetConfigNew() has already been set:
	// . m_Apple2Type, m_CpuType, m_monochromeRGB

	m_PropertySheetHelper.GetConfigNew().m_confirmReboot = IsDlgButtonChecked(hWnd, IDC_CHECK_CONFIRM_REBOOT) ? true : false;

	const uint32_t newMasterVolume = VOLUME_MAX - (uint32_t)SendDlgItemMessage(hWnd, IDC_SLIDER_MASTER_VOLUME, TBM_GETPOS, 0, 0);	// Invert: L=MIN, R=MAX
	m_PropertySheetHelper.GetConfigNew().m_masterVolume = newMasterVolume;

	// Video

	const VideoType_e newVideoType = (VideoType_e)SendDlgItemMessage(hWnd, IDC_VIDEOTYPE, CB_GETCURSEL, 0, 0);
	m_PropertySheetHelper.GetConfigNew().m_videoType = newVideoType;

	UINT newVideoStyle = IsDlgButtonChecked(hWnd, IDC_CHECK_HALF_SCAN_LINES) ? VS_HALF_SCANLINES : VS_NONE;
	newVideoStyle |= IsDlgButtonChecked(hWnd, IDC_CHECK_VERTICAL_BLEND) ? VS_COLOR_VERTICAL_BLEND : VS_NONE;
	m_PropertySheetHelper.GetConfigNew().m_videoStyle = (VideoStyle_e)newVideoStyle;

	m_PropertySheetHelper.GetConfigNew().m_videoRefreshRate = IsDlgButtonChecked(hWnd, IDC_CHECK_50HZ_VIDEO) ? VR_50HZ : VR_60HZ;

	m_PropertySheetHelper.GetConfigNew().m_fullScreen_ShowSubunitStatus = IsDlgButtonChecked(hWnd, IDC_CHECK_FS_SHOW_SUBUNIT_STATUS) ? true : false;

	// Emulation speed control

	m_PropertySheetHelper.GetConfigNew().m_enhanceDiskAccessSpeed = IsDlgButtonChecked(hWnd, IDC_ENHANCE_DISK_ENABLE) ? true : false;
	m_PropertySheetHelper.GetConfigNew().m_scrollLockToggle = IsDlgButtonChecked(hWnd, IDC_SCROLLLOCK_TOGGLE) ? 1 : 0;

	m_PropertySheetHelper.GetConfigNew().m_machineSpeed = IsDlgButtonChecked(hWnd, IDC_AUTHENTIC_SPEED)	? SPEED_NORMAL
		: (uint32_t)SendDlgItemMessage(hWnd, IDC_SLIDER_CPU_SPEED, TBM_GETPOS, 0, 0);

	m_PropertySheetHelper.PostMsgAfterClose(hWnd, m_Page);
}

void CPageConfig::ApplyConfigAfterClose()
{
	Win32Frame& win32Frame = Win32Frame::GetWin32Frame();

	const BOOL bNewConfirmReboot = m_PropertySheetHelper.GetConfigNew().m_confirmReboot ? 1 : 0;
	if (win32Frame.g_bConfirmReboot != bNewConfirmReboot)
	{
		REGSAVE(REGVALUE_CONFIRM_REBOOT, bNewConfirmReboot);
		win32Frame.g_bConfirmReboot = bNewConfirmReboot;
	}

	// NB. Volume: 0=Loudest, VOLUME_MAX=Silence
	const uint32_t newMasterVolume = m_PropertySheetHelper.GetConfigNew().m_masterVolume;
	if (SpkrGetVolume() != newMasterVolume)
	{
		SpkrSetVolume(newMasterVolume, VOLUME_MAX);
		GetCardMgr().GetMockingboardCardMgr().SetVolume(newMasterVolume, VOLUME_MAX);
		REGSAVE(REGVALUE_MASTER_VOLUME, newMasterVolume);
	}

	// Video

	bool bVideoReinit = false;

	const VideoType_e newVideoType = m_PropertySheetHelper.GetConfigNew().m_videoType;
	if (GetVideo().GetVideoType() != newVideoType)
	{
		GetVideo().SetVideoType(newVideoType);
		bVideoReinit = true;
	}

	const bool newHalfScanLines = ((UINT)m_PropertySheetHelper.GetConfigNew().m_videoStyle & (UINT)VS_HALF_SCANLINES) ? true : false;
	const bool currentHalfScanLines = GetVideo().IsVideoStyle(VS_HALF_SCANLINES);
	if (currentHalfScanLines != newHalfScanLines)
	{
		if (newHalfScanLines)
			GetVideo().SetVideoStyle((VideoStyle_e)(GetVideo().GetVideoStyle() | VS_HALF_SCANLINES));
		else
			GetVideo().SetVideoStyle((VideoStyle_e)(GetVideo().GetVideoStyle() & ~VS_HALF_SCANLINES));
		bVideoReinit = true;
	}

	const bool newVerticalBlend = ((UINT)m_PropertySheetHelper.GetConfigNew().m_videoStyle & (UINT)VS_COLOR_VERTICAL_BLEND) ? true : false;
	const bool currentVerticalBlend = GetVideo().IsVideoStyle(VS_COLOR_VERTICAL_BLEND);
	if (currentVerticalBlend != newVerticalBlend)
	{
		if (newVerticalBlend)
			GetVideo().SetVideoStyle((VideoStyle_e)(GetVideo().GetVideoStyle() | VS_COLOR_VERTICAL_BLEND));
		else
			GetVideo().SetVideoStyle((VideoStyle_e)(GetVideo().GetVideoStyle() & ~VS_COLOR_VERTICAL_BLEND));
		bVideoReinit = true;
	}

	if (GetVideo().GetMonochromeRGB() != m_PropertySheetHelper.GetConfigNew().m_monochromeRGB)
	{
		GetVideo().SetMonochromeRGB(m_PropertySheetHelper.GetConfigNew().m_monochromeRGB);
		bVideoReinit = true;
	}

	if (GetVideo().GetVideoRefreshRate() != m_PropertySheetHelper.GetConfigNew().m_videoRefreshRate)
	{
		GetVideo().SetVideoRefreshRate(m_PropertySheetHelper.GetConfigNew().m_videoRefreshRate);
		bVideoReinit = true;
	}

	if (bVideoReinit)
	{
		win32Frame.FrameRefreshStatus(DRAW_TITLE);
		win32Frame.ApplyVideoModeChange();
	}

	const bool bNewFSSubunitStatus = m_PropertySheetHelper.GetConfigNew().m_fullScreen_ShowSubunitStatus;
	if (win32Frame.GetFullScreenShowSubunitStatus() != bNewFSSubunitStatus)
	{
		REGSAVE(REGVALUE_FS_SHOW_SUBUNIT_STATUS, bNewFSSubunitStatus ? 1 : 0);
		win32Frame.SetFullScreenShowSubunitStatus(bNewFSSubunitStatus);

		if (win32Frame.IsFullScreen())
			win32Frame.FrameRefreshStatus(DRAW_BACKGROUND | DRAW_LEDS | DRAW_DISK_STATUS);
	}

	// Emulation speed control

	const bool bNewEnhanceDisk = m_PropertySheetHelper.GetConfigNew().m_enhanceDiskAccessSpeed;
	if (GetCardMgr().GetDisk2CardMgr().GetEnhanceDisk() != bNewEnhanceDisk)
	{
		GetCardMgr().GetDisk2CardMgr().SetEnhanceDisk(bNewEnhanceDisk);
		REGSAVE(REGVALUE_ENHANCE_DISK_SPEED, bNewEnhanceDisk ? 1 : 0);
	}

	const UINT newScrollLockToggle = m_PropertySheetHelper.GetConfigNew().m_scrollLockToggle;
	if (m_uScrollLockToggle != newScrollLockToggle)
	{
		m_uScrollLockToggle = newScrollLockToggle;
		REGSAVE(REGVALUE_SCROLLLOCK_TOGGLE, m_uScrollLockToggle);
	}

	if (g_dwSpeed != m_PropertySheetHelper.GetConfigNew().m_machineSpeed)
	{
		g_dwSpeed = m_PropertySheetHelper.GetConfigNew().m_machineSpeed;
		REGSAVE(REGVALUE_EMULATION_SPEED, g_dwSpeed);
		SetCurrentCLK6502();
	}
}

// Config->Computer: Menu item to eApple2Type
eApple2Type CPageConfig::GetApple2Type(uint32_t NewMenuItem)
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


void CPageConfig::ui_tfe_settings_dialog(HWND hWnd)
{
	DialogBox(GetFrame().g_hInstance, (LPCTSTR)IDD_TFE_SETTINGS_DIALOG, hWnd, CPageConfigTfe::DlgProc);
}

void CPageConfig::ResetToDefault()
{
	const eApple2Type apple2Type = A2TYPE_APPLE2EENHANCED;
	m_PropertySheetHelper.GetConfigNew().m_Apple2Type = apple2Type;
	m_PropertySheetHelper.GetConfigNew().m_CpuType = ProbeMainCpuDefault(apple2Type);

	m_PropertySheetHelper.GetConfigNew().m_confirmReboot = kConfirmReboot_Default;
	m_PropertySheetHelper.GetConfigNew().m_masterVolume = kUserVolume_Default;

	m_PropertySheetHelper.GetConfigNew().m_videoType = VT_DEFAULT;
	m_PropertySheetHelper.GetConfigNew().m_videoStyle = VS_DEFAULT;
	m_PropertySheetHelper.GetConfigNew().m_videoRefreshRate = VR_DEFAULT;
	m_PropertySheetHelper.GetConfigNew().m_monochromeRGB = Video::MONO_COLOR_DEFAULT;

	m_PropertySheetHelper.GetConfigNew().m_fullScreen_ShowSubunitStatus = Win32Frame::kFullScreen_ShowSubunitStatus_Default;

	m_PropertySheetHelper.GetConfigNew().m_enhanceDiskAccessSpeed = kEnhanceDiskAccessSpeed_Default;
	m_PropertySheetHelper.GetConfigNew().m_scrollLockToggle = kScrollLockToggle_Default;
	m_PropertySheetHelper.GetConfigNew().m_machineSpeed = kMachineSpeed_Default;
}
