#include "StdAfx.h"
#include "PageSound.h"
#include "PropertySheetHelper.h"
#include "..\resource\resource.h"

CPageSound* CPageSound::ms_this = 0;	// reinit'd in ctor

const TCHAR CPageSound::m_soundchoices[] =	TEXT("Disabled\0")
											TEXT("PC Speaker (direct)\0")
											TEXT("PC Speaker (translated)\0")
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
		case IDC_SOUNDCARD_DISABLE:
			if (NewSoundcardConfigured(hWnd, wparam, CT_Empty))
				InitOptions(hWnd);	// re-init
			break;
		}
		break;

	case WM_INITDIALOG:
		{
			m_PropertySheetHelper.FillComboBox(hWnd,IDC_SOUNDTYPE,m_soundchoices,soundtype);

			SendDlgItemMessage(hWnd,IDC_SPKR_VOLUME,TBM_SETRANGE,1,MAKELONG(VOLUME_MIN,VOLUME_MAX));
			SendDlgItemMessage(hWnd,IDC_SPKR_VOLUME,TBM_SETPAGESIZE,0,10);
			SendDlgItemMessage(hWnd,IDC_SPKR_VOLUME,TBM_SETTICFREQ,10,0);
			SendDlgItemMessage(hWnd,IDC_SPKR_VOLUME,TBM_SETPOS,1,SpkrGetVolume());

			SendDlgItemMessage(hWnd,IDC_MB_VOLUME,TBM_SETRANGE,1,MAKELONG(VOLUME_MIN,VOLUME_MAX));
			SendDlgItemMessage(hWnd,IDC_MB_VOLUME,TBM_SETPAGESIZE,0,10);
			SendDlgItemMessage(hWnd,IDC_MB_VOLUME,TBM_SETTICFREQ,10,0);
			SendDlgItemMessage(hWnd,IDC_MB_VOLUME,TBM_SETPOS,1,MB_GetVolume());

			m_NewCardType = MB_GetSoundcardType();	// Reinit 1st time page is activated (fires before PSN_SETACTIVE)
			InitOptions(hWnd);

			break;
		}
	}

	return FALSE;
}

void CPageSound::DlgOK(HWND hWnd)
{
	const DWORD dwNewSoundType = (DWORD) SendDlgItemMessage(hWnd, IDC_SOUNDTYPE, CB_GETCURSEL, 0, 0);

	const DWORD dwSpkrVolume = SendDlgItemMessage(hWnd, IDC_SPKR_VOLUME, TBM_GETPOS, 0, 0);
	const DWORD dwMBVolume = SendDlgItemMessage(hWnd, IDC_MB_VOLUME, TBM_GETPOS, 0, 0);

	if (SpkrSetEmulationType(hWnd, dwNewSoundType))
	{
		REGSAVE(TEXT("Sound Emulation"), soundtype);
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
		const BOOL bEnable = bIsSlot4Empty || Slot4 == CT_MockingboardC;
		EnableWindow(GetDlgItem(hWnd, IDC_PHASOR_ENABLE), bEnable);	// Disable Phasor (slot 4)
	}

	// Mockingboard button
	{
		const BOOL bEnable = (bIsSlot4Empty || Slot4 == CT_Phasor) && bIsSlot5Empty;
		EnableWindow(GetDlgItem(hWnd, IDC_MB_ENABLE), bEnable);		// Disable Mockingboard (slot 4 & 5)
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

	const SS_CARDTYPE Slot5 = m_PropertySheetHelper.GetConfigNew().m_Slot[5];

	if (NewCardType == CT_MockingboardC)
	{
		m_PropertySheetHelper.GetConfigNew().m_Slot[4] = CT_MockingboardC;
		m_PropertySheetHelper.GetConfigNew().m_Slot[5] = CT_MockingboardC;
	}
	else if (NewCardType == CT_Phasor)
	{
		m_PropertySheetHelper.GetConfigNew().m_Slot[4] = CT_Phasor;
		if (Slot5 == CT_MockingboardC)
			m_PropertySheetHelper.GetConfigNew().m_Slot[5] = CT_Empty;
	}
	else
	{
		m_PropertySheetHelper.GetConfigNew().m_Slot[4] = CT_Empty;
		if (Slot5 == CT_MockingboardC)
			m_PropertySheetHelper.GetConfigNew().m_Slot[5] = CT_Empty;
	}

	return true;
}
