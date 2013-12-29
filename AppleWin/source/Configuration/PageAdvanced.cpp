#include "StdAfx.h"
#include "PageAdvanced.h"
#include "PropertySheetHelper.h"
#include "..\resource\resource.h"

CPageAdvanced* CPageAdvanced::ms_this = 0;	// reinit'd in ctor

enum CLONECHOICE {MENUITEM_CLONEMIN, MENUITEM_PRAVETS82=MENUITEM_CLONEMIN, MENUITEM_PRAVETS8M, MENUITEM_PRAVETS8A, MENUITEM_CLONEMAX};
const TCHAR CPageAdvanced::m_CloneChoices[] =
				TEXT("Pravets 82\0")	// Bulgarian
				TEXT("Pravets 8M\0")    // Bulgarian
				TEXT("Pravets 8A\0");	// Bulgarian


BOOL CALLBACK CPageAdvanced::DlgProc(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	// Switch from static func to our instance
	return CPageAdvanced::ms_this->DlgProcInternal(hWnd, message, wparam, lparam);
}

BOOL CPageAdvanced::DlgProcInternal(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam)
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
		case IDC_SAVESTATE_FILENAME:
			break;
		case IDC_SAVESTATE_BROWSE:
			if(m_PropertySheetHelper.SaveStateSelectImage(hWnd, TEXT("Select Save State file"), false))
				SendDlgItemMessage(hWnd, IDC_SAVESTATE_FILENAME, WM_SETTEXT, 0, (LPARAM)m_PropertySheetHelper.GetSSNewFilename());
			break;
		case IDC_PRINTER_DUMP_FILENAME_BROWSE:
			{				
				string strPrinterDumpLoc = m_PropertySheetHelper.BrowseToFile(hWnd, TEXT("Select printer dump file"), REGVALUE_PRINTER_FILENAME, TEXT("Text files (*.txt)\0*.txt\0") TEXT("All Files\0*.*\0"));
				SendDlgItemMessage(hWnd, IDC_PRINTER_DUMP_FILENAME, WM_SETTEXT, 0, (LPARAM)strPrinterDumpLoc.c_str());
			}
			break;
		case IDC_SAVESTATE_ON_EXIT:
			break;
		case IDC_SAVESTATE:
			m_PropertySheetHelper.GetConfigNew().m_uSaveLoadStateMsg = WM_USER_SAVESTATE;
			break;
		case IDC_LOADSTATE:
			m_PropertySheetHelper.GetConfigNew().m_uSaveLoadStateMsg = WM_USER_LOADSTATE;
			break;

		//

		case IDC_THE_FREEZES_F8_ROM_FW:
			{
				const UINT uNewState = IsDlgButtonChecked(hWnd, IDC_THE_FREEZES_F8_ROM_FW) ? 1 : 0;
				m_PropertySheetHelper.GetConfigNew().m_bEnableTheFreezesF8Rom = uNewState;
			}
			break;

		case IDC_CLONETYPE:
			if(HIWORD(wparam) == CBN_SELCHANGE)
			{
				const DWORD NewCloneMenuItem = (DWORD) SendDlgItemMessage(hWnd, IDC_CLONETYPE, CB_GETCURSEL, 0, 0);
				const eApple2Type NewCloneType = GetCloneType(NewCloneMenuItem);
				m_PropertySheetHelper.GetConfigNew().m_Apple2Type = NewCloneType;
			}
			break;
		}
		break;

	case WM_INITDIALOG:
		{
			SendDlgItemMessage(hWnd,IDC_SAVESTATE_FILENAME,WM_SETTEXT,0,(LPARAM)Snapshot_GetFilename());

			CheckDlgButton(hWnd, IDC_SAVESTATE_ON_EXIT, g_bSaveStateOnExit ? BST_CHECKED : BST_UNCHECKED);
			CheckDlgButton(hWnd, IDC_DUMPTOPRINTER, g_bDumpToPrinter ? BST_CHECKED : BST_UNCHECKED);
			CheckDlgButton(hWnd, IDC_PRINTER_CONVERT_ENCODING, g_bConvertEncoding ? BST_CHECKED : BST_UNCHECKED);
			CheckDlgButton(hWnd, IDC_PRINTER_FILTER_UNPRINTABLE, g_bFilterUnprintable ? BST_CHECKED : BST_UNCHECKED);
			CheckDlgButton(hWnd, IDC_PRINTER_APPEND, g_bPrinterAppend ? BST_CHECKED : BST_UNCHECKED);
			SendDlgItemMessage(hWnd, IDC_SPIN_PRINTER_IDLE, UDM_SETRANGE, 0, MAKELONG(999,0));
			SendDlgItemMessage(hWnd, IDC_SPIN_PRINTER_IDLE, UDM_SETPOS, 0, MAKELONG(Printer_GetIdleLimit (),0));
			SendDlgItemMessage(hWnd, IDC_PRINTER_DUMP_FILENAME, WM_SETTEXT, 0, (LPARAM)Printer_GetFilename());

			InitOptions(hWnd);

			m_PropertySheetHelper.ClearSSNewDirectory();

			// Need to specify cmd-line switch: -printer-real to enable this control
			EnableWindow(GetDlgItem(hWnd, IDC_DUMPTOPRINTER), g_bEnableDumpToRealPrinter ? TRUE : FALSE);

			break;
		}
	}

	return FALSE;
}

void CPageAdvanced::DlgOK(HWND hWnd)
{
	// Update save-state filename
	{
		char szFilename[MAX_PATH];
		memset(szFilename, 0, sizeof(szFilename));
		* (USHORT*) szFilename = sizeof(szFilename);

		UINT nLineLength = SendDlgItemMessage(hWnd, IDC_SAVESTATE_FILENAME, EM_LINELENGTH, 0, 0);

		SendDlgItemMessage(hWnd, IDC_SAVESTATE_FILENAME, EM_GETLINE, 0, (LPARAM)szFilename);

		nLineLength = nLineLength > sizeof(szFilename)-1 ? sizeof(szFilename)-1 : nLineLength;
		szFilename[nLineLength] = 0x00;

		m_PropertySheetHelper.SaveStateUpdate();
	}

	// Update printer dump filename
	{
		char szFilename[MAX_PATH];
		memset(szFilename, 0, sizeof(szFilename));
		* (USHORT*) szFilename = sizeof(szFilename);

		UINT nLineLength = SendDlgItemMessage(hWnd, IDC_PRINTER_DUMP_FILENAME, EM_LINELENGTH, 0, 0);

		SendDlgItemMessage(hWnd, IDC_PRINTER_DUMP_FILENAME, EM_GETLINE, 0, (LPARAM)szFilename);

		nLineLength = nLineLength > sizeof(szFilename)-1 ? sizeof(szFilename)-1 : nLineLength;
		szFilename[nLineLength] = 0x00;

		Printer_SetFilename(szFilename);
		RegSaveString(TEXT(REG_CONFIG), REGVALUE_PRINTER_FILENAME, 1, Printer_GetFilename());
	}

	g_bSaveStateOnExit = IsDlgButtonChecked(hWnd, IDC_SAVESTATE_ON_EXIT) ? true : false;
	REGSAVE(TEXT(REGVALUE_SAVE_STATE_ON_EXIT), g_bSaveStateOnExit ? 1 : 0);

	g_bDumpToPrinter = IsDlgButtonChecked(hWnd, IDC_DUMPTOPRINTER ) ? true : false;
	REGSAVE(TEXT(REGVALUE_DUMP_TO_PRINTER), g_bDumpToPrinter ? 1 : 0);

	g_bConvertEncoding = IsDlgButtonChecked(hWnd, IDC_PRINTER_CONVERT_ENCODING ) ? true : false;
	REGSAVE(TEXT(REGVALUE_CONVERT_ENCODING), g_bConvertEncoding ? 1 : 0);

	g_bFilterUnprintable = IsDlgButtonChecked(hWnd, IDC_PRINTER_FILTER_UNPRINTABLE ) ? true : false;
	REGSAVE(TEXT(REGVALUE_FILTER_UNPRINTABLE), g_bFilterUnprintable ? 1 : 0);

	g_bPrinterAppend = IsDlgButtonChecked(hWnd, IDC_PRINTER_APPEND) ? true : false;
	REGSAVE(TEXT(REGVALUE_PRINTER_APPEND), g_bPrinterAppend ? 1 : 0);

	//

    Printer_SetIdleLimit((short)SendDlgItemMessage(hWnd, IDC_SPIN_PRINTER_IDLE , UDM_GETPOS, 0, 0));
	REGSAVE(TEXT(REGVALUE_PRINTER_IDLE_LIMIT),Printer_GetIdleLimit());

	m_PropertySheetHelper.PostMsgAfterClose(hWnd, m_Page);
}

void CPageAdvanced::InitOptions(HWND hWnd)
{
	InitFreezeDlgButton(hWnd);
	InitCloneDropdownMenu(hWnd);
}

// Advanced->Clone: Menu item to eApple2Type
eApple2Type CPageAdvanced::GetCloneType(DWORD NewMenuItem)
{
	switch (NewMenuItem)
	{
		case MENUITEM_PRAVETS82:	return A2TYPE_PRAVETS82;
		case MENUITEM_PRAVETS8M:	return A2TYPE_PRAVETS8M;
		case MENUITEM_PRAVETS8A:	return A2TYPE_PRAVETS8A;
		default:					return A2TYPE_PRAVETS82;
	}
}

int CPageAdvanced::GetCloneMenuItem(void)
{
	const eApple2Type type = m_PropertySheetHelper.GetConfigNew().m_Apple2Type;
	const bool bIsClone = IsClone(type);
	if (!bIsClone)
		return MENUITEM_CLONEMIN;

	int nMenuItem = type - A2TYPE_PRAVETS;
	if (nMenuItem < 0 || nMenuItem >= MENUITEM_CLONEMAX)
		return MENUITEM_CLONEMIN;

	return nMenuItem;
}

void CPageAdvanced::InitFreezeDlgButton(HWND hWnd)
{
	const bool bIsApple2 = IsApple2( m_PropertySheetHelper.GetConfigNew().m_Apple2Type );
	EnableWindow(GetDlgItem(hWnd, IDC_THE_FREEZES_F8_ROM_FW), bIsApple2 ? TRUE : FALSE);

	const UINT CheckTheFreezesRom = m_PropertySheetHelper.GetConfigNew().m_bEnableTheFreezesF8Rom ? BST_CHECKED : BST_UNCHECKED;
	CheckDlgButton(hWnd, IDC_THE_FREEZES_F8_ROM_FW, CheckTheFreezesRom);
}

void CPageAdvanced::InitCloneDropdownMenu(HWND hWnd)
{
	// Set clone menu choice (ok even if it's not a clone)
	const int nCurrentChoice = GetCloneMenuItem();
	m_PropertySheetHelper.FillComboBox(hWnd, IDC_CLONETYPE, m_CloneChoices, nCurrentChoice);

	const bool bIsClone = IsClone( m_PropertySheetHelper.GetConfigNew().m_Apple2Type );
	EnableWindow(GetDlgItem(hWnd, IDC_CLONETYPE), bIsClone ? TRUE : FALSE);
}
