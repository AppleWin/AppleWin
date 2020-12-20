/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 1994-1996, Michael O'Brien
Copyright (C) 1999-2001, Oliver Schmidt
Copyright (C) 2002-2005, Tom Charlesworth
Copyright (C) 2006-2012, Tom Charlesworth, Michael Pohoreski

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

#include "PropertySheet.h"

#include "../Interface.h"
#include "../Windows/AppleWin.h"
#include "../resource/resource.h"

void CPropertySheet::Init(void)
{
	PROPSHEETPAGE PropSheetPages[PG_NUM_SHEETS];

	PropSheetPages[PG_CONFIG].dwSize = sizeof(PROPSHEETPAGE);
	PropSheetPages[PG_CONFIG].dwFlags = PSP_DEFAULT;
	PropSheetPages[PG_CONFIG].hInstance = GetFrame().g_hInstance;
	PropSheetPages[PG_CONFIG].pszTemplate = MAKEINTRESOURCE(IDD_PROPPAGE_CONFIG);
	PropSheetPages[PG_CONFIG].pfnDlgProc = (DLGPROC)CPageConfig::DlgProc;

	PropSheetPages[PG_INPUT].dwSize = sizeof(PROPSHEETPAGE);
	PropSheetPages[PG_INPUT].dwFlags = PSP_DEFAULT;
	PropSheetPages[PG_INPUT].hInstance = GetFrame().g_hInstance;
	PropSheetPages[PG_INPUT].pszTemplate = MAKEINTRESOURCE(IDD_PROPPAGE_INPUT);
	PropSheetPages[PG_INPUT].pfnDlgProc = (DLGPROC)CPageInput::DlgProc;

	PropSheetPages[PG_SOUND].dwSize = sizeof(PROPSHEETPAGE);
	PropSheetPages[PG_SOUND].dwFlags = PSP_DEFAULT;
	PropSheetPages[PG_SOUND].hInstance = GetFrame().g_hInstance;
	PropSheetPages[PG_SOUND].pszTemplate = MAKEINTRESOURCE(IDD_PROPPAGE_SOUND);
	PropSheetPages[PG_SOUND].pfnDlgProc = (DLGPROC)CPageSound::DlgProc;

	PropSheetPages[PG_DISK].dwSize = sizeof(PROPSHEETPAGE);
	PropSheetPages[PG_DISK].dwFlags = PSP_DEFAULT;
	PropSheetPages[PG_DISK].hInstance = GetFrame().g_hInstance;
	PropSheetPages[PG_DISK].pszTemplate = MAKEINTRESOURCE(IDD_PROPPAGE_DISK);
	PropSheetPages[PG_DISK].pfnDlgProc = (DLGPROC)CPageDisk::DlgProc;

	PropSheetPages[PG_ADVANCED].dwSize = sizeof(PROPSHEETPAGE);
	PropSheetPages[PG_ADVANCED].dwFlags = PSP_DEFAULT;
	PropSheetPages[PG_ADVANCED].hInstance = GetFrame().g_hInstance;
	PropSheetPages[PG_ADVANCED].pszTemplate = MAKEINTRESOURCE(IDD_PROPPAGE_ADVANCED);
	PropSheetPages[PG_ADVANCED].pfnDlgProc = (DLGPROC)CPageAdvanced::DlgProc;

	PROPSHEETHEADER PropSheetHeader;

	PropSheetHeader.dwSize = sizeof(PROPSHEETHEADER);
	PropSheetHeader.dwFlags = PSH_NOAPPLYNOW | /* PSH_NOCONTEXTHELP | */ PSH_PROPSHEETPAGE;
	PropSheetHeader.hwndParent = GetFrame().g_hFrameWindow;
	PropSheetHeader.pszCaption = "AppleWin Configuration";
	PropSheetHeader.nPages = PG_NUM_SHEETS;
	PropSheetHeader.nStartPage = m_PropertySheetHelper.GetLastPage();
	PropSheetHeader.ppsp = PropSheetPages;

	m_PropertySheetHelper.ResetPageMask();
	m_PropertySheetHelper.SaveCurrentConfig();

	INT_PTR nRes = PropertySheet(&PropSheetHeader);	// Result: 0=Cancel, 1=OK
}

DWORD CPropertySheet::GetVolumeMax()
{
	return m_PageSound.GetVolumeMax();
}

// Called when F11/F12 is pressed
bool CPropertySheet::SaveStateSelectImage(HWND hWindow, bool bSave)
{
	if(m_PropertySheetHelper.SaveStateSelectImage(hWindow, bSave ? TEXT("Select Save State file")
																 : TEXT("Select Load State file"), bSave))
	{
		m_PropertySheetHelper.SaveStateUpdate();
		return true;
	}
	else
	{
		return false;	// Cancelled
	}
}
