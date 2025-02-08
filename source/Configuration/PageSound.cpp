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

#include "../Common.h"
#include "../CardManager.h"
#include "../Mockingboard.h"
#include "../Registry.h"
#include "../Speaker.h"
#include "../resource/resource.h"

CPageSound* CPageSound::ms_this = 0;	// reinit'd in ctor

const char CPageSound::m_soundchoices[] =	TEXT("Disabled\0")
											TEXT("Sound Card\0");


const char CPageSound::m_soundCardChoices[] =	"Mockingboard\0"
												"Phasor\0"
												"SAM\0"
												"Empty\0";

// Don't reveal MegaAudio/SD Music cards unless it's been specified from the command line.
// The reasons being are that:
// . this card is purely for regression testing against mb-audit
// . it's confusing to offer this to the end user
const char CPageSound::m_soundCardChoicesEx[] =	"Mockingboard\0"
												"Phasor\0"
												"SAM\0"
												"Empty\0"
												"MEGA Audio\0"
												"SD Music\0";

const char CPageSound::m_soundCardChoice_Unavailable[] = "Unavailable\0\0";	// doubly-null terminate

INT_PTR CALLBACK CPageSound::DlgProc(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	// Switch from static func to our instance
	return CPageSound::ms_this->DlgProcInternal(hWnd, message, wparam, lparam);
}

INT_PTR CPageSound::DlgProcInternal(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam)
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
		case IDC_SOUNDCARD_SLOT4:
		case IDC_SOUNDCARD_SLOT5:
			if (HIWORD(wparam) == CBN_SELCHANGE)
			{
				UINT slot = (LOWORD(wparam) == IDC_SOUNDCARD_SLOT4) ? SLOT4 : SLOT5;
				uint32_t newChoiceItem = (uint32_t)SendDlgItemMessage(hWnd, LOWORD(wparam), CB_GETCURSEL, 0, 0);

				SS_CARDTYPE newCard = CT_Empty;
				switch (newChoiceItem)
				{
				case SC_MOCKINGBOARD: newCard = CT_MockingboardC; break;
				case SC_PHASOR: newCard = CT_Phasor; break;
				case SC_SAM: newCard = CT_SAM; break;
				case SC_EMPTY: newCard = CT_Empty; break;
				case SC_MEGAAUDIO: newCard = CT_MegaAudio; break;
				case SC_SDMUSIC: newCard = CT_SDMusic; break;
				default: _ASSERT(0); break;
				}

				m_PropertySheetHelper.GetConfigNew().m_Slot[slot] = newCard;
			}
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
			SendDlgItemMessage(hWnd,IDC_MB_VOLUME,TBM_SETPOS,1, GetCardMgr().GetMockingboardCardMgr().GetVolume());

			InitOptions(hWnd);

			break;
		}
	}

	return FALSE;
}

void CPageSound::DlgOK(HWND hWnd)
{
	const SoundType_e newSoundType = (SoundType_e) SendDlgItemMessage(hWnd, IDC_SOUNDTYPE, CB_GETCURSEL, 0, 0);

	const uint32_t dwSpkrVolume = SendDlgItemMessage(hWnd, IDC_SPKR_VOLUME, TBM_GETPOS, 0, 0);
	const uint32_t dwMBVolume = SendDlgItemMessage(hWnd, IDC_MB_VOLUME, TBM_GETPOS, 0, 0);

	SpkrSetEmulationType(newSoundType);
	uint32_t dwSoundType = (soundtype == SOUND_NONE) ? REG_SOUNDTYPE_NONE : REG_SOUNDTYPE_WAVE;
	REGSAVE(TEXT(REGVALUE_SOUND_EMULATION), dwSoundType);

	// NB. Volume: 0=Loudest, VOLUME_MAX=Silence
	SpkrSetVolume(dwSpkrVolume, VOLUME_MAX);
	GetCardMgr().GetMockingboardCardMgr().SetVolume(dwMBVolume, VOLUME_MAX);

	REGSAVE(TEXT(REGVALUE_SPKR_VOLUME), SpkrGetVolume());
	REGSAVE(TEXT(REGVALUE_MB_VOLUME), GetCardMgr().GetMockingboardCardMgr().GetVolume());

	m_PropertySheetHelper.PostMsgAfterClose(hWnd, m_Page);
}

CPageSound::SOUNDCARDCHOICE CPageSound::CardTypeToComboItem(SS_CARDTYPE card)
{
	switch (card)
	{
	case CT_MockingboardC: return SC_MOCKINGBOARD;
	case CT_Phasor: return SC_PHASOR;
	case CT_SAM: return SC_SAM;
	case CT_Empty: return SC_EMPTY;
	case CT_MegaAudio: return SC_MEGAAUDIO;
	case CT_SDMusic: return SC_SDMUSIC;
	default: _ASSERT(0); return SC_EMPTY;
	}
}

void CPageSound::InitOptions(HWND hWnd)
{
	const SS_CARDTYPE slot4 = m_PropertySheetHelper.GetConfigNew().m_Slot[SLOT4];
	const SS_CARDTYPE slot5 = m_PropertySheetHelper.GetConfigNew().m_Slot[SLOT5];

	bool isSlot4SoundCard = slot4 == CT_MockingboardC || slot4 == CT_Phasor || slot4 == CT_SAM || slot4 == CT_Empty;
	bool isSlot5SoundCard = slot5 == CT_MockingboardC || slot5 == CT_Phasor || slot5 == CT_SAM || slot5 == CT_Empty;

	bool isSlotXSoundCardEx = GetCardMgr().GetMockingboardCardMgr().GetEnableExtraCardTypes();

	if (isSlotXSoundCardEx)
		isSlot4SoundCard = isSlot5SoundCard = false;

	if (isSlot4SoundCard)
		m_PropertySheetHelper.FillComboBox(hWnd, IDC_SOUNDCARD_SLOT4, m_soundCardChoices, (int)CardTypeToComboItem(slot4));
	else if (isSlotXSoundCardEx)
		m_PropertySheetHelper.FillComboBox(hWnd, IDC_SOUNDCARD_SLOT4, m_soundCardChoicesEx, (int)CardTypeToComboItem(slot4));
	else
		m_PropertySheetHelper.FillComboBox(hWnd, IDC_SOUNDCARD_SLOT4, m_soundCardChoice_Unavailable, 0);

	if (isSlot5SoundCard)
		m_PropertySheetHelper.FillComboBox(hWnd, IDC_SOUNDCARD_SLOT5, m_soundCardChoices, (int)CardTypeToComboItem(slot5));
	else if (isSlotXSoundCardEx)
		m_PropertySheetHelper.FillComboBox(hWnd, IDC_SOUNDCARD_SLOT5, m_soundCardChoicesEx, (int)CardTypeToComboItem(slot5));
	else
		m_PropertySheetHelper.FillComboBox(hWnd, IDC_SOUNDCARD_SLOT5, m_soundCardChoice_Unavailable, 0);

	bool enableMBVolume = slot4 == CT_MockingboardC || slot5 == CT_MockingboardC
						|| slot4 == CT_Phasor || slot5 == CT_Phasor
						|| slot4 == CT_MegaAudio || slot5 == CT_MegaAudio
						|| slot4 == CT_SDMusic || slot5 == CT_SDMusic;
	EnableWindow(GetDlgItem(hWnd, IDC_MB_VOLUME), enableMBVolume ? TRUE : FALSE);
}
