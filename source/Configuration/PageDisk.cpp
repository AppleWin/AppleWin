#include "StdAfx.h"
#include "PageDisk.h"
#include "PropertySheetHelper.h"
#include "..\Harddisk.h"
#include "..\resource\resource.h"

CPageDisk* CPageDisk::ms_this = 0;	// reinit'd in ctor

const TCHAR CPageDisk::m_discchoices[] =
				TEXT("Authentic Speed\0")
				TEXT("Enhanced Speed\0");


BOOL CALLBACK CPageDisk::DlgProc(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	// Switch from static func to our instance
	return CPageDisk::ms_this->DlgProcInternal(hWnd, message, wparam, lparam);
}

BOOL CPageDisk::DlgProcInternal(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam)
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
		case IDC_DISK1:
			DiskSelect(DRIVE_1);
			SendDlgItemMessage(hWnd, IDC_EDIT_DISK1, WM_SETTEXT, 0, (LPARAM)DiskGetFullName(DRIVE_1));
			FrameRefreshStatus(DRAW_BUTTON_DRIVES);
			break;

		case IDC_DISK2:
			DiskSelect(DRIVE_2);
			SendDlgItemMessage(hWnd, IDC_EDIT_DISK2, WM_SETTEXT, 0, (LPARAM)DiskGetFullName(DRIVE_2));
			FrameRefreshStatus(DRAW_BUTTON_DRIVES);
			break;

		case IDC_HDD1:
			if(IsDlgButtonChecked(hWnd, IDC_HDD_ENABLE))
			{
				HD_Select(HARDDISK_1);
				SendDlgItemMessage(hWnd, IDC_EDIT_HDD1, WM_SETTEXT, 0, (LPARAM)HD_GetFullName(HARDDISK_1));
			}
			break;

		case IDC_HDD2:
			if(IsDlgButtonChecked(hWnd, IDC_HDD_ENABLE))
			{
				HD_Select(HARDDISK_2);
				SendDlgItemMessage(hWnd, IDC_EDIT_HDD2, WM_SETTEXT, 0, (LPARAM)HD_GetFullName(HARDDISK_2));
			}
			break;

		case IDC_HDD_ENABLE:
			EnableHDD(hWnd, IsDlgButtonChecked(hWnd, IDC_HDD_ENABLE));
			break;

		case IDC_CIDERPRESS_BROWSE:
			{
				string CiderPressLoc = m_PropertySheetHelper.BrowseToFile(hWnd, TEXT("Select path to CiderPress"), REGVALUE_CIDERPRESSLOC, TEXT("Applications (*.exe)\0*.exe\0") TEXT("All Files\0*.*\0") );
				RegSaveString(TEXT(REG_CONFIG), REGVALUE_CIDERPRESSLOC, 1, CiderPressLoc.c_str());
				SendDlgItemMessage(hWnd, IDC_CIDERPRESS_FILENAME, WM_SETTEXT, 0, (LPARAM) CiderPressLoc.c_str());
			}
			break;
		}
		break;

	case WM_INITDIALOG:
		{
			m_PropertySheetHelper.FillComboBox(hWnd,IDC_DISKTYPE,m_discchoices,enhancedisk);

			SendDlgItemMessage(hWnd, IDC_EDIT_DISK1, WM_SETTEXT, 0, (LPARAM)DiskGetFullName(DRIVE_1));
			SendDlgItemMessage(hWnd, IDC_EDIT_DISK2, WM_SETTEXT, 0, (LPARAM)DiskGetFullName(DRIVE_2));

			SendDlgItemMessage(hWnd, IDC_EDIT_HDD1, WM_SETTEXT, 0, (LPARAM)HD_GetFullName(HARDDISK_1));
			SendDlgItemMessage(hWnd, IDC_EDIT_HDD2, WM_SETTEXT, 0, (LPARAM)HD_GetFullName(HARDDISK_2));

			//

			TCHAR PathToCiderPress[MAX_PATH] = "";
			RegLoadString(TEXT("Configuration"), REGVALUE_CIDERPRESSLOC, 1, PathToCiderPress,MAX_PATH);
			SendDlgItemMessage(hWnd, IDC_CIDERPRESS_FILENAME ,WM_SETTEXT, 0, (LPARAM)PathToCiderPress);

			//

			CheckDlgButton(hWnd, IDC_HDD_ENABLE, HD_CardIsEnabled() ? BST_CHECKED : BST_UNCHECKED);

			EnableHDD(hWnd, IsDlgButtonChecked(hWnd, IDC_HDD_ENABLE));

			InitOptions(hWnd);

			break;
		}

	case WM_RBUTTONUP:
		{
			RECT  rect;		// client area
			POINT pt;		// location of mouse click

			// Get the bounding rectangle of the client area.
			GetClientRect(hWnd, (LPRECT) &rect);

			// Get the client coordinates for the mouse click.
			pt.x = GET_X_LPARAM(lparam);
			pt.y = GET_Y_LPARAM(lparam);

			// If the mouse click took place inside the client
			// area, execute the application-defined function
			// that displays the shortcut menu.
			if (PtInRect((LPRECT) &rect, pt))
			{
				//  Load the menu template containing the shortcut menu from the application's resources.
				HMENU hMenu = LoadMenu(g_hInstance, MAKEINTRESOURCE(IDR_MENU_DISK_CFG_POPUP));	// menu template
				_ASSERT(hMenu);
				if (!hMenu)
					break;

				// Get the first shortcut menu in the menu template.
				// This is the menu that TrackPopupMenu displays.
				HMENU hMenuTrackPopup = GetSubMenu(hMenu, 0);	// shortcut menu

				// TrackPopup uses screen coordinates, so convert the coordinates of the mouse click to screen coordinates.
				ClientToScreen(hWnd, (LPPOINT) &pt);

				if (Disk_IsDriveEmpty(DRIVE_1))
					EnableMenuItem(hMenu, ID_DISKMENU_EJECT_DISK1, MF_GRAYED);
				if (Disk_IsDriveEmpty(DRIVE_2))
					EnableMenuItem(hMenu, ID_DISKMENU_EJECT_DISK2, MF_GRAYED);
				if (HD_IsDriveUnplugged(HARDDISK_1))
					EnableMenuItem(hMenu, ID_DISKMENU_UNPLUG_HARDDISK1, MF_GRAYED);
				if (HD_IsDriveUnplugged(HARDDISK_2))
					EnableMenuItem(hMenu, ID_DISKMENU_UNPLUG_HARDDISK2, MF_GRAYED);

				// Draw and track the shortcut menu.
				int iCommand = TrackPopupMenu(
					hMenuTrackPopup
					, TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_RETURNCMD
					, pt.x, pt.y
					, 0
					, hWnd, NULL );

				if (iCommand)
				{
					TCHAR szText[100];
					bool bMsgBox = true;
					if (iCommand == ID_DISKMENU_EJECT_DISK1 || iCommand == ID_DISKMENU_EJECT_DISK2)
						wsprintf(szText, "Do you really want to eject the disk in drive-%c ?", '1'+iCommand-ID_DISKMENU_EJECT_DISK1);
					else if (iCommand == ID_DISKMENU_UNPLUG_HARDDISK1 || iCommand == ID_DISKMENU_UNPLUG_HARDDISK2)
						wsprintf(szText, "Do you really want to unplug harddisk-%c ?", '1'+iCommand-ID_DISKMENU_UNPLUG_HARDDISK1);
					else
						bMsgBox = false;

					if (bMsgBox)
					{
						int nRes = MessageBox(g_hFrameWindow, szText, TEXT("Eject/Unplug Warning"), MB_ICONWARNING | MB_OKCANCEL | MB_SETFOREGROUND);
						if (nRes == IDNO)
							iCommand = 0;
					}
				}

				switch (iCommand)
				{
				case ID_DISKMENU_EJECT_DISK1:
					DiskEject(DRIVE_1);
					SendDlgItemMessage(hWnd, IDC_EDIT_DISK1, WM_SETTEXT, 0, (LPARAM)DiskGetFullName(DRIVE_1));
					break;
				case ID_DISKMENU_EJECT_DISK2:
					DiskEject(DRIVE_2);
					SendDlgItemMessage(hWnd, IDC_EDIT_DISK2, WM_SETTEXT, 0, (LPARAM)DiskGetFullName(DRIVE_2));
					break;
				case ID_DISKMENU_UNPLUG_HARDDISK1:
					HD_Unplug(HARDDISK_1);
					SendDlgItemMessage(hWnd, IDC_EDIT_HDD1, WM_SETTEXT, 0, (LPARAM)HD_GetFullName(HARDDISK_1));
					break;
				case ID_DISKMENU_UNPLUG_HARDDISK2:
					HD_Unplug(HARDDISK_2);
					SendDlgItemMessage(hWnd, IDC_EDIT_HDD2, WM_SETTEXT, 0, (LPARAM)HD_GetFullName(HARDDISK_2));
					break;
				}

				if (iCommand != 0)
					FrameRefreshStatus(DRAW_BUTTON_DRIVES);
			}
			break;
		}

	}

	return FALSE;
}

void CPageDisk::DlgOK(HWND hWnd)
{
	const BOOL bNewEnhanceDisk = (BOOL) SendDlgItemMessage(hWnd, IDC_DISKTYPE,CB_GETCURSEL, 0, 0);
	if (bNewEnhanceDisk != enhancedisk)
	{
		m_PropertySheetHelper.GetConfigNew().m_bEnhanceDisk = bNewEnhanceDisk;
	}

	const bool bNewHDDIsEnabled = IsDlgButtonChecked(hWnd, IDC_HDD_ENABLE) ? true : false;
	if (bNewHDDIsEnabled != HD_CardIsEnabled())
	{
		m_PropertySheetHelper.GetConfigNew().m_bEnableHDD = bNewHDDIsEnabled;
	}

	RegSaveString(TEXT(REG_PREFS), TEXT(REGVALUE_PREF_LAST_HARDDISK_1), 1, HD_GetFullPathName(HARDDISK_1));
	RegSaveString(TEXT(REG_PREFS), TEXT(REGVALUE_PREF_LAST_HARDDISK_2), 1, HD_GetFullPathName(HARDDISK_2));

	m_PropertySheetHelper.PostMsgAfterClose(hWnd, m_Page);
}

void CPageDisk::InitOptions(HWND hWnd)
{
	// Nothing to do:
	// - no changes made on any other pages affect this page
}

void CPageDisk::EnableHDD(HWND hWnd, BOOL bEnable)
{
	EnableWindow(GetDlgItem(hWnd, IDC_HDD1), bEnable);
	EnableWindow(GetDlgItem(hWnd, IDC_EDIT_HDD1), bEnable);

	EnableWindow(GetDlgItem(hWnd, IDC_HDD2), bEnable);
	EnableWindow(GetDlgItem(hWnd, IDC_EDIT_HDD2), bEnable);
}
