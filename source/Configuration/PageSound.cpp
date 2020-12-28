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

#include "../SaveState_Structs_common.h"
#include "../Common.h"
#include "../CardManager.h"
#include "../Mockingboard.h"
#include "../Registry.h"
#include "../Speaker.h"
#include "../resource/resource.h"

CPageSound* CPageSound::ms_this = 0;	// reinit'd in ctor

const TCHAR CPageSound::m_soundchoices[] =	TEXT("Disabled\0")
											TEXT("Sound Card\0");


BOOL CALLBACK CPageSound::DlgProc(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	// Switch from static func to our instance
	return CPageSound::ms_this->DlgProcInternal(hWnd, message, wparam, lparam);
}

BOOL CPageSound::DlgProcInternal(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam)
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
				SetWindowLong(hWnd, DWL_MSGRESULT, FALSE);			// Changes are valid
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
		case IDC_SPKR_VOLUME:
			break;
		case IDC_MB_VOLUME:
			break;
		case IDC_MB_ENABLE:
			if (NewSoundcardConfigured(hWnd, wparam, CT_MockingboardC))
				InitOptions(hWnd);	// re-init
			break;
		case IDC_PHASOR_ENABLE:
			if (NewSoundcardConfigured(hWnd, wparam, CT_Phasor))
				InitOptions(hWnd);	// re-init
			break;
		case IDC_SAM_ENABLE:
			if (NewSoundcardConfigured(hWnd, wparam, CT_SAM))
				InitOptions(hWnd);	// re-init
			break;
		case IDC_SOUNDCARD_DISABLE:
			if (NewSoundcardConfigured(hWnd, wparam, CT_Empty))
				InitOptions(hWnd);	// re-init
			break;
		}
		break;

	case WM_INITDIALOG:
		{
			m_PropertySheetHelper.FillComboBox(hWnd,IDC_SOUNDTYPE, m_soundchoices, (int)soundtype);

			SendDlgItemMessage(hWnd,IDC_SPKR_VOLUME,TBM_SETRANGE,1,MAKELONG(VOLUME_MIN,VOLUME_MAX));
			SendDlgItemMessage(hWnd,IDC_SPKR_VOLUME,TBM_SETPAGESIZE,0,10);
			SendDlgItemMessage(hWnd,IDC_SPKR_VOLUME,TBM_SETTICFREQ,10,0);
			SendDlgItemMessage(hWnd,IDC_SPKR_VOLUME,TBM_SETPOS,1,SpkrGetVolume());

			SendDlgItemMessage(hWnd,IDC_MB_VOLUME,TBM_SETRANGE,1,MAKELONG(VOLUME_MIN,VOLUME_MAX));
			SendDlgItemMessage(hWnd,IDC_MB_VOLUME,TBM_SETPAGESIZE,0,10);
			SendDlgItemMessage(hWnd,IDC_MB_VOLUME,TBM_SETTICFREQ,10,0);
			SendDlgItemMessage(hWnd,IDC_MB_VOLUME,TBM_SETPOS,1,MB_GetVolume());

			if (GetCardMgr().QuerySlot(SLOT5) == CT_SAM)
				m_NewCardType = CT_SAM;
			else
				m_NewCardType = MB_GetSoundcardType();	// Reinit 1st time page is activated (fires before PSN_SETACTIVE)

			InitOptions(hWnd);

			break;
		}
	}

	return FALSE;
}

void CPageSound::DlgOK(HWND hWnd)
{
	const SoundType_e newSoundType = (SoundType_e) SendDlgItemMessage(hWnd, IDC_SOUNDTYPE, CB_GETCURSEL, 0, 0);

	const DWORD dwSpkrVolume = SendDlgItemMessage(hWnd, IDC_SPKR_VOLUME, TBM_GETPOS, 0, 0);
	const DWORD dwMBVolume = SendDlgItemMessage(hWnd, IDC_MB_VOLUME, TBM_GETPOS, 0, 0);

	if (SpkrSetEmulationType(hWnd, newSoundType))
	{
		DWORD dwSoundType = (soundtype == SOUND_NONE) ? REG_SOUNDTYPE_NONE : REG_SOUNDTYPE_WAVE;
		REGSAVE(TEXT(REGVALUE_SOUND_EMULATION), dwSoundType);
	}

	// NB. Volume: 0=Loudest, VOLUME_MAX=Silence
	SpkrSetVolume(dwSpkrVolume, VOLUME_MAX);
	MB_SetVolume(dwMBVolume, VOLUME_MAX);

	REGSAVE(TEXT(REGVALUE_SPKR_VOLUME), SpkrGetVolume());
	REGSAVE(TEXT(REGVALUE_MB_VOLUME), MB_GetVolume());

	m_PropertySheetHelper.PostMsgAfterClose(hWnd, m_Page);
}

void CPageSound::InitOptions(HWND hWnd)
{
	// CheckRadioButton
	if(m_NewCardType == CT_MockingboardC)
		m_nCurrentIDCheckButton = IDC_MB_ENABLE;
	else if(m_NewCardType == CT_Phasor)
		m_nCurrentIDCheckButton = IDC_PHASOR_ENABLE;
	else if(m_NewCardType == CT_SAM) 
		m_nCurrentIDCheckButton = IDC_SAM_ENABLE; 
	else
		m_nCurrentIDCheckButton = IDC_SOUNDCARD_DISABLE;

	CheckRadioButton(hWnd, IDC_MB_ENABLE, IDC_SOUNDCARD_DISABLE, m_nCurrentIDCheckButton);

	//

	const SS_CARDTYPE Slot4 = m_PropertySheetHelper.GetConfigNew().m_Slot[4];
	const SS_CARDTYPE Slot5 = m_PropertySheetHelper.GetConfigNew().m_Slot[5];

	const bool bIsSlot4Empty = Slot4 == CT_Empty;
	const bool bIsSlot5Empty = Slot5 == CT_Empty;

	// Phasor button
	{
		const BOOL bEnable = bIsSlot4Empty || Slot4 == CT_MockingboardC || Slot4 == CT_Phasor;
		EnableWindow(GetDlgItem(hWnd, IDC_PHASOR_ENABLE), bEnable);	// Disable Phasor (slot 4)
	}

	// Mockingboard button
	{
		const BOOL bEnable = (bIsSlot4Empty || Slot4 == CT_Phasor || Slot4 == CT_MockingboardC) &&
							 (bIsSlot5Empty || Slot5 == CT_SAM    || Slot5 == CT_MockingboardC);
		EnableWindow(GetDlgItem(hWnd, IDC_MB_ENABLE), bEnable);		// Disable Mockingboard (slot 4 & 5)
	}

	// SAM button
	{
		const BOOL bEnable = bIsSlot5Empty || Slot5 == CT_MockingboardC || Slot5 == CT_SAM;
		EnableWindow(GetDlgItem(hWnd, IDC_SAM_ENABLE), bEnable);	// Disable SAM (slot 5)
	}

	EnableWindow(GetDlgItem(hWnd, IDC_MB_VOLUME), (m_nCurrentIDCheckButton != IDC_SOUNDCARD_DISABLE) ? TRUE : FALSE);
}

bool CPageSound::NewSoundcardConfigured(HWND hWnd, WPARAM wparam, SS_CARDTYPE NewCardType)
{
	if (HIWORD(wparam) != BN_CLICKED)
		return false;

	if (LOWORD(wparam) == m_nCurrentIDCheckButton)
		return false;

	m_NewCardType = NewCardType;

	const SS_CARDTYPE Slot4 = m_PropertySheetHelper.GetConfigNew().m_Slot[4];
	const SS_CARDTYPE Slot5 = m_PropertySheetHelper.GetConfigNew().m_Slot[5];

	if (NewCardType == CT_MockingboardC)
	{
		m_PropertySheetHelper.GetConfigNew().m_Slot[4] = CT_MockingboardC;
		m_PropertySheetHelper.GetConfigNew().m_Slot[5] = CT_MockingboardC;
	}
	else if (NewCardType == CT_Phasor)
	{
		m_PropertySheetHelper.GetConfigNew().m_Slot[4] = CT_Phasor;
		if ((Slot5 == CT_MockingboardC) || (Slot5 == CT_SAM))
			m_PropertySheetHelper.GetConfigNew().m_Slot[5] = CT_Empty;
	}
	else if (NewCardType == CT_SAM)
	{
	if ((Slot4 == CT_MockingboardC) || (Slot4 == CT_Phasor))
		m_PropertySheetHelper.GetConfigNew().m_Slot[4] = CT_Empty;
		m_PropertySheetHelper.GetConfigNew().m_Slot[5] = CT_SAM;
	}
	else
	{
		if ((Slot4 == CT_MockingboardC) || (Slot4 == CT_Phasor))
			m_PropertySheetHelper.GetConfigNew().m_Slot[4] = CT_Empty;
		if ((Slot5 == CT_MockingboardC) || (Slot5 == CT_SAM))
			m_PropertySheetHelper.GetConfigNew().m_Slot[5] = CT_Empty;
	}

	return true;
}
