#include "StdAfx.h"
#include "PageSound.h"
#include "PropertySheetHelper.h"
#include "..\resource\resource.h"

CPageSound* CPageSound::ms_this = 0;	// reinit'd in ctor

const TCHAR CPageSound::m_soundchoices[] =	TEXT("Disabled\0")
											TEXT("PC Speaker (direct)\0")
											TEXT("PC Speaker (translated)\0")
											TEXT("Sound Card\0");


BOOL CALLBACK CPageSound::DlgProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
	// Switch from static func to our instance
	return CPageSound::ms_this->DlgProcInternal(window, message, wparam, lparam);
}

BOOL CPageSound::DlgProcInternal(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
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
				DlgOK(window, afterclose);
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
		case IDC_SPKR_VOLUME:
			break;
		case IDC_MB_VOLUME:
			break;
		case IDC_MB_ENABLE:
			{
				LPCSTR pMsg =	TEXT("The emulator needs to restart as the slot configuration has changed.\n")
								TEXT("Mockingboard cards will be inserted into slots 4 & 5.\n\n")
								TEXT("Would you like to restart the emulator now?");
				if (NewSoundcardConfigured(window, wparam, pMsg, afterclose))
				{
					m_NewCardType = CT_MockingboardC;
					m_SoundcardSlotChange = CARD_INSERTED;
					EnableWindow(GetDlgItem(window, IDC_MB_VOLUME), TRUE);
				}
			}
			break;
		case IDC_PHASOR_ENABLE:
			{
				LPCSTR pMsg =	TEXT("The emulator needs to restart as the slot configuration has changed.\n")
								TEXT("Phasor card will be inserted into slot 4.\n\n")
								TEXT("Would you like to restart the emulator now?");
				if (NewSoundcardConfigured(window, wparam, pMsg, afterclose))
				{
					m_NewCardType = CT_Phasor;
					m_SoundcardSlotChange = CARD_INSERTED;
					EnableWindow(GetDlgItem(window, IDC_MB_VOLUME), TRUE);
				}
			}
			break;
		case IDC_SOUNDCARD_DISABLE:
			{
				LPCSTR pMsg =	TEXT("The emulator needs to restart as the slot configuration has changed.\n")
								TEXT("Sound card(s) will be removed.\n\n")
								TEXT("Would you like to restart the emulator now?");
				if (NewSoundcardConfigured(window, wparam, pMsg, afterclose))
				{
					m_NewCardType = CT_Empty;
					m_SoundcardSlotChange = CARD_UNPLUGGED;
					EnableWindow(GetDlgItem(window, IDC_MB_VOLUME), FALSE);
				}
			}
			break;
		}
		break;

	case WM_INITDIALOG:
		{
			m_PropertySheetHelper.SetLastPage(m_Page);

			m_PropertySheetHelper.FillComboBox(window,IDC_SOUNDTYPE,m_soundchoices,soundtype);

			SendDlgItemMessage(window,IDC_SPKR_VOLUME,TBM_SETRANGE,1,MAKELONG(VOLUME_MIN,VOLUME_MAX));
			SendDlgItemMessage(window,IDC_SPKR_VOLUME,TBM_SETPAGESIZE,0,10);
			SendDlgItemMessage(window,IDC_SPKR_VOLUME,TBM_SETTICFREQ,10,0);
			SendDlgItemMessage(window,IDC_SPKR_VOLUME,TBM_SETPOS,1,SpkrGetVolume());

			SendDlgItemMessage(window,IDC_MB_VOLUME,TBM_SETRANGE,1,MAKELONG(VOLUME_MIN,VOLUME_MAX));
			SendDlgItemMessage(window,IDC_MB_VOLUME,TBM_SETPAGESIZE,0,10);
			SendDlgItemMessage(window,IDC_MB_VOLUME,TBM_SETTICFREQ,10,0);
			SendDlgItemMessage(window,IDC_MB_VOLUME,TBM_SETPOS,1,MB_GetVolume());

			SS_CARDTYPE SoundcardType = MB_GetSoundcardType();
			if(SoundcardType == CT_MockingboardC)
				m_nCurrentIDCheckButton = IDC_MB_ENABLE;
			else if(SoundcardType == CT_Phasor)
				m_nCurrentIDCheckButton = IDC_PHASOR_ENABLE;
			else
				m_nCurrentIDCheckButton = IDC_SOUNDCARD_DISABLE;

			CheckRadioButton(window, IDC_MB_ENABLE, IDC_SOUNDCARD_DISABLE, m_nCurrentIDCheckButton);

			const bool bIsSlot4Empty = g_Slot4 == CT_Empty;
			const bool bIsSlot5Empty = g_Slot5 == CT_Empty;
			if (!bIsSlot4Empty && g_Slot4 != CT_MockingboardC)
				EnableWindow(GetDlgItem(window, IDC_PHASOR_ENABLE), FALSE);	// Disable Phasor (slot 4)

			if ((!bIsSlot4Empty || !bIsSlot5Empty) && g_Slot4 != CT_Phasor)
				EnableWindow(GetDlgItem(window, IDC_MB_ENABLE), FALSE);		// Disable Mockingboard (slot 4 & 5)

			EnableWindow(GetDlgItem(window, IDC_MB_VOLUME), (m_nCurrentIDCheckButton != IDC_SOUNDCARD_DISABLE) ? TRUE : FALSE);

			afterclose = 0;
			break;
		}
	}

	return FALSE;
}

void CPageSound::DlgOK(HWND window, UINT afterclose)
{
	DWORD newsoundtype  = (DWORD)SendDlgItemMessage(window,IDC_SOUNDTYPE,CB_GETCURSEL,0,0);

	DWORD dwSpkrVolume = SendDlgItemMessage(window,IDC_SPKR_VOLUME,TBM_GETPOS,0,0);
	DWORD dwMBVolume   = SendDlgItemMessage(window,IDC_MB_VOLUME,TBM_GETPOS,0,0);

	if (!SpkrSetEmulationType(window,newsoundtype))
	{
		//afterclose = 0;	// TC: does nothing
		return;
	}

	// NB. Volume: 0=Loudest, VOLUME_MAX=Silence
	SpkrSetVolume(dwSpkrVolume, VOLUME_MAX);
	MB_SetVolume(dwMBVolume, VOLUME_MAX);

	REGSAVE(TEXT("Sound Emulation"),soundtype);
	REGSAVE(TEXT(REGVALUE_SPKR_VOLUME),SpkrGetVolume());
	REGSAVE(TEXT(REGVALUE_MB_VOLUME),MB_GetVolume());

	if (m_SoundcardSlotChange != CARD_UNCHANGED)
	{
		MB_SetSoundcardType(m_NewCardType);

		if (m_NewCardType == CT_MockingboardC)
		{
			m_PropertySheetHelper.SetSlot4(CT_MockingboardC);
			m_PropertySheetHelper.SetSlot5(CT_MockingboardC);
		}
		else if (m_NewCardType == CT_Phasor)
		{
			m_PropertySheetHelper.SetSlot4(CT_Phasor);
			if (g_Slot5 == CT_MockingboardC)
				m_PropertySheetHelper.SetSlot5(CT_Empty);
		}
		else
		{
			m_PropertySheetHelper.SetSlot4(CT_Empty);
			if (g_Slot5 == CT_MockingboardC)
				m_PropertySheetHelper.SetSlot5(CT_Empty);
		}
	}

	//

	if (afterclose)
		PostMessage(g_hFrameWindow,afterclose,0,0);
}

bool CPageSound::NewSoundcardConfigured(HWND window, WPARAM wparam, LPCSTR pMsg, UINT& afterclose)
{
	if (HIWORD(wparam) != BN_CLICKED)
		return false;

	if (LOWORD(wparam) == m_nCurrentIDCheckButton)
		return false;

	if ( (MessageBox(window,
			pMsg,
			TEXT("Configuration"),
			MB_ICONQUESTION | MB_OKCANCEL | MB_SETFOREGROUND) == IDOK)
		&& m_PropertySheetHelper.IsOkToRestart(window) )
	{
		m_nCurrentIDCheckButton = LOWORD(wparam);
		afterclose = WM_USER_RESTART;
		PropSheet_PressButton(GetParent(window), PSBTN_OK);
		return true;
	}

	// Restore original state
	CheckRadioButton(window, IDC_MB_ENABLE, IDC_SOUNDCARD_DISABLE, m_nCurrentIDCheckButton);
	return false;
}
