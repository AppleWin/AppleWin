#include "stdafx.h"
#include "PropertySheetHelper.h"
#include "..\AppleWin.h"	// g_nAppMode, g_uScrollLockToggle

// NB. This used to be in FrameWndProc() for case WM_USER_RESTART:
// - but this is too late to cancel, since new configurations have already been changed.
bool CPropertySheetHelper::IsOkToRestart(HWND window)
{
	if (g_nAppMode == MODE_LOGO)
		return true;

	if (MessageBox(window,
		TEXT("Restarting the emulator will reset the state ")
		TEXT("of the emulated machine, causing you to lose any ")
		TEXT("unsaved work.\n\n")
		TEXT("Are you sure you want to do this?"),
		TEXT("Configuration"),
		MB_ICONQUESTION | MB_OKCANCEL | MB_SETFOREGROUND) == IDCANCEL)
		return false;

	return true;
}

void CPropertySheetHelper::FillComboBox(HWND window, int controlid, LPCTSTR choices, int currentchoice)
{
	_ASSERT(choices);
	HWND combowindow = GetDlgItem(window, controlid);
	SendMessage(combowindow, CB_RESETCONTENT, 0, 0);
	while (choices && *choices)
	{
		SendMessage(combowindow, CB_ADDSTRING, 0, (LPARAM)(LPCTSTR)choices);
		choices += _tcslen(choices)+1;
	}
	SendMessage(combowindow, CB_SETCURSEL, currentchoice, 0);
}

void CPropertySheetHelper::SaveComputerType(eApple2Type NewApple2Type)
{
	if (NewApple2Type == A2TYPE_CLONE)	// Clone picked from Config tab, but no specific one picked from Advanced tab
		NewApple2Type = A2TYPE_PRAVETS82;

	REGSAVE(TEXT(REGVALUE_APPLE2_TYPE), NewApple2Type);
}

void CPropertySheetHelper::SetSlot4(SS_CARDTYPE NewCardType)
{
	g_Slot4 = NewCardType;
	REGSAVE(TEXT(REGVALUE_SLOT4),(DWORD)g_Slot4);
}

void CPropertySheetHelper::SetSlot5(SS_CARDTYPE NewCardType)
{
	g_Slot5 = NewCardType;
	REGSAVE(TEXT(REGVALUE_SLOT5),(DWORD)g_Slot5);
}

// Looks like a (bad) C&P from SaveStateSelectImage()
// - eg. see "RAPCS" tags below...
// Used by:
// . CPageDisk:		IDC_CIDERPRESS_BROWSE
// . CPageAdvanced:	IDC_PRINTER_DUMP_FILENAME_BROWSE
string CPropertySheetHelper::BrowseToFile(HWND hWindow, TCHAR* pszTitle, TCHAR* REGVALUE, TCHAR* FILEMASKS)
{
	static char PathToFile[MAX_PATH] = {0}; //This is a really awkward way to prevent mixing CiderPress and SaveStated values (RAPCS), but it seem the quickest. Here is its Line 1.
	strcpy(PathToFile, Snapshot_GetFilename()); //RAPCS, line 2.
	TCHAR szDirectory[MAX_PATH] = TEXT("");
	TCHAR szFilename[MAX_PATH];
	strcpy(szFilename, "");
	RegLoadString(TEXT("Configuration"), REGVALUE, 1, szFilename ,MAX_PATH);
	string PathName = szFilename;

	OPENFILENAME ofn;
	ZeroMemory(&ofn,sizeof(OPENFILENAME));
	
	ofn.lStructSize     = sizeof(OPENFILENAME);
	ofn.hwndOwner       = hWindow;
	ofn.hInstance       = g_hInstance;
	ofn.lpstrFilter     = FILEMASKS;
	/*ofn.lpstrFilter     =	TEXT("Applications (*.exe)\0*.exe\0")
							TEXT("Text files (*.txt)\0*.txt\0")
							TEXT("All Files\0*.*\0");*/
	ofn.lpstrFile       = szFilename;
	ofn.nMaxFile        = MAX_PATH;
	ofn.lpstrInitialDir = szDirectory;
	ofn.Flags           = OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
	ofn.lpstrTitle      = pszTitle;
		
	int nRes = GetOpenFileName(&ofn);
	if(nRes)	// Okay is pressed
	{
		strcpy(m_szNewFilename, &szFilename[ofn.nFileOffset]);	// TODO:TC: m_szNewFilename not used! (Was g_szNewFilename)

		szFilename[ofn.nFileOffset] = 0;
		if (_tcsicmp(szDirectory, szFilename))
			strcpy(m_szSSNewDirectory, szFilename);				// TODO:TC: m_szSSNewDirectory looks dodgy! (Was g_szSSNewDirectory)

		PathName = szFilename;
		PathName.append (m_szNewFilename);	
	}
	else		// Cancel is pressed
	{
		RegLoadString(TEXT("Configuration"), REGVALUE, 1, szFilename,MAX_PATH);
		PathName = szFilename;
	}

	strcpy(m_szNewFilename, PathToFile); //RAPCS, line 3 (last).
	return PathName;
}

void CPropertySheetHelper::SaveStateUpdate()
{
	if (m_bSSNewFilename)
	{
		Snapshot_SetFilename(m_szSSNewFilename);

		RegSaveString(TEXT(REG_CONFIG), REGVALUE_SAVESTATE_FILENAME, 1, Snapshot_GetFilename());

		if(m_szSSNewDirectory[0])
			RegSaveString(TEXT(REG_PREFS), REGVALUE_PREF_START_DIR, 1 ,m_szSSNewDirectory);
	}
}

void CPropertySheetHelper::GetDiskBaseNameWithAWS(TCHAR* pszFilename)
{
	LPCTSTR pDiskName = DiskGetBaseName(DRIVE_1);
	if (pDiskName && pDiskName[0])
	{
		strcpy(pszFilename, pDiskName);
		strcpy(&pszFilename[strlen(pDiskName)], ".aws");
	}
}

// NB. OK'ing this property sheet will call Snapshot_SetFilename() with this new filename
int CPropertySheetHelper::SaveStateSelectImage(HWND hWindow, TCHAR* pszTitle, bool bSave)
{
	TCHAR szDirectory[MAX_PATH] = TEXT("");
	TCHAR szFilename[MAX_PATH] = {0};

	if (bSave)
	{
		// Attempt to use drive1's image name as the name for the .aws file
		// Else Attempt to use the Prop Sheet's filename
		GetDiskBaseNameWithAWS(szFilename);
		if (szFilename[0] == 0)
		{
			strcpy(szFilename, Snapshot_GetFilename());
		}
	}
	else	// Load
	{
		// Attempt to use the Prop Sheet's filename first
		// Else attempt to use drive1's image name as the name for the .aws file
		strcpy(szFilename, Snapshot_GetFilename());
		if (szFilename[0] == 0)
		{
			GetDiskBaseNameWithAWS(szFilename);
		}
	}
	
	RegLoadString(TEXT(REG_PREFS),REGVALUE_PREF_START_DIR,1,szDirectory,MAX_PATH);

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
		strcpy(m_szSSNewFilename, &szFilename[ofn.nFileOffset]);

		if (bSave)	// Only for saving (allow loading of any file for backwards compatibility)
		{
			// Append .aws if it's not there
			const char szAWS_EXT[] = ".aws";
			const UINT uStrLenFile = strlen(m_szSSNewFilename);
			const UINT uStrLenExt  = strlen(szAWS_EXT);
			if ((uStrLenFile <= uStrLenExt) || (strcmp(&m_szSSNewFilename[uStrLenFile-uStrLenExt], szAWS_EXT) != 0))
				strcpy(&m_szSSNewFilename[uStrLenFile], szAWS_EXT);
		}

		szFilename[ofn.nFileOffset] = 0;
		if (_tcsicmp(szDirectory, szFilename))
			strcpy(m_szSSNewDirectory, szFilename);
	}

	m_bSSNewFilename = nRes ? true : false;
	return nRes;
}
