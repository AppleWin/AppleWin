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


BOOL CALLBACK CPageAdvanced::DlgProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
	// Switch from static func to our instance
	return CPageAdvanced::ms_this->DlgProcInternal(window, message, wparam, lparam);
}

BOOL CPageAdvanced::DlgProcInternal(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
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
				InitFreezeDlgButton(window);
				InitCloneDropdownMenu(window);
				break;
			case PSN_KILLACTIVE:
				SetWindowLong(window, DWL_MSGRESULT, FALSE);			// Changes are valid
				break;
			case PSN_APPLY:
				DlgOK(window);
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
		case IDC_SAVESTATE_FILENAME:
			break;
		case IDC_SAVESTATE_BROWSE:
			if(m_PropertySheetHelper.SaveStateSelectImage(window, TEXT("Select Save State file"), true))
				SendDlgItemMessage(window, IDC_SAVESTATE_FILENAME, WM_SETTEXT, 0, (LPARAM)m_PropertySheetHelper.GetSSNewFilename());
			break;
		case IDC_PRINTER_DUMP_FILENAME_BROWSE:
			{				
				string strPrinterDumpLoc = m_PropertySheetHelper.BrowseToFile(window, TEXT("Select printer dump file"), REGVALUE_PRINTER_FILENAME, TEXT("Text files (*.txt)\0*.txt\0") TEXT("All Files\0*.*\0"));
				SendDlgItemMessage(window, IDC_PRINTER_DUMP_FILENAME, WM_SETTEXT, 0, (LPARAM)strPrinterDumpLoc.c_str());
			}
			break;
		case IDC_SAVESTATE_ON_EXIT:
			break;
		case IDC_SAVESTATE:
			m_uAfterClose = WM_USER_SAVESTATE;
			break;
		case IDC_LOADSTATE:
			m_uAfterClose = WM_USER_LOADSTATE;
			break;

			//

		case IDC_THE_FREEZES_F8_ROM_FW:
			{
				UINT uNewState = IsDlgButtonChecked(window, IDC_THE_FREEZES_F8_ROM_FW) ? 1 : 0;
				LPCSTR pMsg =	TEXT("The emulator needs to restart as the ROM configuration has changed.\n")
								TEXT("Would you like to restart the emulator now?");
				if ( (MessageBox(window,
						pMsg,
						TEXT("Configuration"),
						MB_ICONQUESTION | MB_OKCANCEL | MB_SETFOREGROUND) == IDOK)
					&& m_PropertySheetHelper.IsOkToRestart(window) )
				{
					m_uTheFreezesF8Rom = uNewState;
					m_uAfterClose = WM_USER_RESTART;
					PropSheet_PressButton(GetParent(window), PSBTN_OK);
				}
				else
				{
					CheckDlgButton(window, IDC_THE_FREEZES_F8_ROM_FW, m_uTheFreezesF8Rom ? BST_CHECKED : BST_UNCHECKED);
				}
			}
			break;
		}
		break;

	case WM_INITDIALOG:  //Init advanced settings dialog
		{
			SendDlgItemMessage(window,IDC_SAVESTATE_FILENAME,WM_SETTEXT,0,(LPARAM)Snapshot_GetFilename());

			CheckDlgButton(window, IDC_SAVESTATE_ON_EXIT, g_bSaveStateOnExit ? BST_CHECKED : BST_UNCHECKED);
			CheckDlgButton(window, IDC_DUMPTOPRINTER, g_bDumpToPrinter ? BST_CHECKED : BST_UNCHECKED);
			CheckDlgButton(window, IDC_PRINTER_CONVERT_ENCODING, g_bConvertEncoding ? BST_CHECKED : BST_UNCHECKED);
			CheckDlgButton(window, IDC_PRINTER_FILTER_UNPRINTABLE, g_bFilterUnprintable ? BST_CHECKED : BST_UNCHECKED);
			CheckDlgButton(window, IDC_PRINTER_APPEND, g_bPrinterAppend ? BST_CHECKED : BST_UNCHECKED);
			SendDlgItemMessage(window, IDC_SPIN_PRINTER_IDLE, UDM_SETRANGE, 0, MAKELONG(999,0));
			SendDlgItemMessage(window, IDC_SPIN_PRINTER_IDLE, UDM_SETPOS, 0, MAKELONG(Printer_GetIdleLimit (),0));
			SendDlgItemMessage(window, IDC_PRINTER_DUMP_FILENAME, WM_SETTEXT, 0, (LPARAM)Printer_GetFilename());

			InitFreezeDlgButton(window);
			InitCloneDropdownMenu(window);

			m_PropertySheetHelper.ClearSSNewDirectory();

			// Need to specific cmd-line switch: -printer-real to enable this control
			EnableWindow(GetDlgItem(window, IDC_DUMPTOPRINTER), g_bEnableDumpToRealPrinter ? TRUE : FALSE);

			m_uAfterClose = 0;
			break;
		}
	}

	return FALSE;
}

void CPageAdvanced::DlgOK(HWND window)
{
	// Update save-state filename
	{
		char szFilename[MAX_PATH];
		memset(szFilename, 0, sizeof(szFilename));
		* (USHORT*) szFilename = sizeof(szFilename);

		UINT nLineLength = SendDlgItemMessage(window, IDC_SAVESTATE_FILENAME, EM_LINELENGTH, 0, 0);

		SendDlgItemMessage(window, IDC_SAVESTATE_FILENAME, EM_GETLINE, 0, (LPARAM)szFilename);

		nLineLength = nLineLength > sizeof(szFilename)-1 ? sizeof(szFilename)-1 : nLineLength;
		szFilename[nLineLength] = 0x00;

		m_PropertySheetHelper.SaveStateUpdate();
	}

	// Update printer dump filename
	{
		char szFilename[MAX_PATH];
		memset(szFilename, 0, sizeof(szFilename));
		* (USHORT*) szFilename = sizeof(szFilename);

		UINT nLineLength = SendDlgItemMessage(window, IDC_PRINTER_DUMP_FILENAME, EM_LINELENGTH, 0, 0);

		SendDlgItemMessage(window, IDC_PRINTER_DUMP_FILENAME, EM_GETLINE, 0, (LPARAM)szFilename);

		nLineLength = nLineLength > sizeof(szFilename)-1 ? sizeof(szFilename)-1 : nLineLength;
		szFilename[nLineLength] = 0x00;

		Printer_SetFilename(szFilename);
		RegSaveString(TEXT(REG_CONFIG), REGVALUE_PRINTER_FILENAME, 1, Printer_GetFilename());
	}

	g_bSaveStateOnExit = IsDlgButtonChecked(window, IDC_SAVESTATE_ON_EXIT) ? true : false;
	REGSAVE(TEXT(REGVALUE_SAVE_STATE_ON_EXIT), g_bSaveStateOnExit ? 1 : 0);

	g_bDumpToPrinter = IsDlgButtonChecked(window, IDC_DUMPTOPRINTER ) ? true : false;
	REGSAVE(TEXT(REGVALUE_DUMP_TO_PRINTER), g_bDumpToPrinter ? 1 : 0);

	g_bConvertEncoding = IsDlgButtonChecked(window, IDC_PRINTER_CONVERT_ENCODING ) ? true : false;
	REGSAVE(TEXT(REGVALUE_CONVERT_ENCODING), g_bConvertEncoding ? 1 : 0);

	g_bFilterUnprintable = IsDlgButtonChecked(window, IDC_PRINTER_FILTER_UNPRINTABLE ) ? true : false;
	REGSAVE(TEXT(REGVALUE_FILTER_UNPRINTABLE), g_bFilterUnprintable ? 1 : 0);

	g_bPrinterAppend = IsDlgButtonChecked(window, IDC_PRINTER_APPEND) ? true : false;
	REGSAVE(TEXT(REGVALUE_PRINTER_APPEND), g_bPrinterAppend ? 1 : 0);

	//

	REGSAVE(TEXT(REGVALUE_THE_FREEZES_F8_ROM),m_uTheFreezesF8Rom);	// NB. Can also be disabled on Config page (when Apple2Type changes) 
	
    Printer_SetIdleLimit((short)SendDlgItemMessage(window, IDC_SPIN_PRINTER_IDLE , UDM_GETPOS, 0, 0));
	REGSAVE(TEXT(REGVALUE_PRINTER_IDLE_LIMIT),Printer_GetIdleLimit());

	const DWORD NewCloneMenuItem = (DWORD) SendDlgItemMessage(window, IDC_CLONETYPE, CB_GETCURSEL, 0, 0);
	const eApple2Type NewCloneType = GetCloneType(NewCloneMenuItem);

	// Get 2 identical msg-boxs:
	// . Config tab: Change to 'Clone'
	// . Advanced tab: Change clone type, then OK
	// . ConfigDlg_OK() msgbox asks "restart now?", click OK
	// . AdvancedDlg_OK() msgbox asks "restart now?
	if (IS_CLONE() || (m_PropertySheetHelper.GetUIControlCloneDropdownMenu() == UI_ENABLE))
	{
		if (NewCloneType != g_Apple2Type)
		{		
			if ((m_uAfterClose == WM_USER_RESTART) ||	// Eg. Changing 'Freeze ROM' & user has already OK'd the restart for this
				((MessageBox(window,
							TEXT(
							"You have changed the emulated computer "
							"type. This change will not take effect "
							"until the next time you restart the "
							"emulator.\n\n"
							"Would you like to restart the emulator now?"),
							TEXT("Configuration"),
							MB_ICONQUESTION | MB_OKCANCEL | MB_SETFOREGROUND) == IDOK)
				&& m_PropertySheetHelper.IsOkToRestart(window)) )
			{
				m_uAfterClose = WM_USER_RESTART;
				m_PropertySheetHelper.SaveComputerType(NewCloneType);
			}
		}
	}

	if (g_Apple2Type > A2TYPE_APPLE2PLUS)
		m_uTheFreezesF8Rom = 0;

	m_PropertySheetHelper.PostMsgAfterClose(m_Page, m_uAfterClose);
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
	if (!IS_CLONE())
		return MENUITEM_CLONEMIN;

	int nMenuItem = g_Apple2Type - A2TYPE_PRAVETS;
	if (nMenuItem < 0 || nMenuItem >= MENUITEM_CLONEMAX)
		return MENUITEM_CLONEMIN;

	return nMenuItem;
}

void CPageAdvanced::InitFreezeDlgButton(HWND window)
{
	if (m_PropertySheetHelper.GetUIControlFreezeDlgButton() == UI_UNDEFINED)
		EnableWindow(GetDlgItem(window, IDC_THE_FREEZES_F8_ROM_FW), IS_APPLE2 ? TRUE : FALSE);
	else
		EnableWindow(GetDlgItem(window, IDC_THE_FREEZES_F8_ROM_FW), (m_PropertySheetHelper.GetUIControlFreezeDlgButton() == UI_ENABLE) ? TRUE : FALSE);

	CheckDlgButton(window, IDC_THE_FREEZES_F8_ROM_FW, m_uTheFreezesF8Rom ? BST_CHECKED : BST_UNCHECKED);
}

void CPageAdvanced::InitCloneDropdownMenu(HWND window)
{
	// Set clone menu choice (ok even if it's not a clone)
	int nCurrentChoice = GetCloneMenuItem();
	m_PropertySheetHelper.FillComboBox(window, IDC_CLONETYPE, m_CloneChoices, nCurrentChoice);

	if (m_PropertySheetHelper.GetUIControlCloneDropdownMenu() == UI_UNDEFINED)
		EnableWindow(GetDlgItem(window, IDC_CLONETYPE), IS_CLONE() ? TRUE : FALSE);
	else
		EnableWindow(GetDlgItem(window, IDC_CLONETYPE), (IS_CLONE() || (m_PropertySheetHelper.GetUIControlCloneDropdownMenu() == UI_ENABLE)) ? TRUE : FALSE);
}
