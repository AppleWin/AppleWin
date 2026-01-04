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

#include "StdAfx.h"

#include "PropertySheet.h"
#include "PropertySheetHelper.h"

#include "../Windows/AppleWin.h"	// g_nAppMode, g_uScrollLockToggle, sg_PropertySheet
#include "../CardManager.h"
#include "../Memory.h"
#include "../Disk.h"
#include "../Log.h"
#include "../Registry.h"
#include "../SaveState.h"
#include "../Interface.h"
#include "../Uthernet2.h"
#include "../Tfe/PCapBackend.h"

/*
Config causing AfterClose msgs:
===============================

Page:					Action:							Comment:
------------------------------------------------------------------------
Config
. Computer				WM_USER_RESTART
. Benchmark				PSBTN_OK -> WM_USER_BENCHMARK	Forces PSBTN_OK
Input
. Mouse					WM_USER_RESTART
. CP/M					WM_USER_RESTART
Sound
. MB/Phasor/SAM/None	WM_USER_RESTART
Disk
. HDD enable			WM_USER_RESTART
Advanced
. Save State			WM_USER_SAVESTATE
. Load State			WM_USER_LOADSTATE
. Clone					WM_USER_RESTART
. MrFreeze Rom			WM_USER_RESTART

Requirements:
-------------
. Want to change multiple HW at once.
. Allow change Computer & Load State (ie. multiple AfterClose msgs)

Design:
-------
. At PropSheet init, copy original config state (for above items)
. On last Page close, compare new config state, if changed:
  - Show 1 restart + confirm msg (if necessary)
    - Cancel will rollback to original config (for above items), but other items will be applied
. Load State button
  - Don't action it immediately.
  - .aws should contain config (but doesn't), so should override any config changes.
  - so this can just discard any config changes
  - if any config change, then show msg box to say they won't be applied
. Save State button
  - Don't action it immediately.
  - save state applies to current config (prior to restart).
  - so this can just discard any config changes
  - if any config change, then show msg box to say they won't be applied
. Benchmark button
  - Action it immediately.
  - If config has changed:
    - Prompt to either do benchmark (and lose new config) or cancel benchmark (and drop back to Configuration dialog).
*/

void CPropertySheetHelper::FillComboBox(HWND window, int controlid, LPCTSTR choices, int currentchoice)
{
	_ASSERT(choices);
	HWND combowindow = GetDlgItem(window, controlid);
	SendMessage(combowindow, CB_RESETCONTENT, 0, 0);
	while (choices && *choices)
	{
		SendMessage(combowindow, CB_ADDSTRING, 0, (LPARAM)(LPCTSTR)choices);
		choices += strlen(choices)+1;
	}

	if (SendMessage(combowindow, CB_SETCURSEL, currentchoice, 0) == CB_ERR && currentchoice != -1)
	{
		_ASSERT(0);
		SendMessage(combowindow, CB_SETCURSEL, 0, 0);	// GH#434: Failed to set currentchoice, so select item-0
	}
}

void CPropertySheetHelper::SaveComputerType(eApple2Type NewApple2Type)
{
	if (NewApple2Type == A2TYPE_CLONE)	// Clone picked from Config tab, but no specific one picked from Advanced tab
		NewApple2Type = A2TYPE_PRAVETS82;

	ConfigSaveApple2Type(NewApple2Type);
}

void CPropertySheetHelper::ConfigSaveApple2Type(eApple2Type apple2Type)
{
	REGSAVE(REGVALUE_APPLE2_TYPE, apple2Type);
	LogFileOutput("Config: Apple2 Type changed to %d\n", apple2Type);
}

void CPropertySheetHelper::SaveCpuType(eCpuType NewCpuType)
{
	REGSAVE(REGVALUE_CPU_TYPE, NewCpuType);
}

void CPropertySheetHelper::SetSlot(UINT slot, SS_CARDTYPE newCardType)
{
	if (slot <= SLOT7)
	{
		if (GetCardMgr().QuerySlot(slot) == newCardType)
			return;

		GetCardMgr().Insert(slot, newCardType);
	}
	else if (slot == SLOT_AUX)
	{
		if (GetCardMgr().QueryAux() == newCardType)
			return;

		GetCardMgr().InsertAux(newCardType);
	}
	else
	{
		_ASSERT(0);
	}
}

// Used by:
// . CPageDisk:		IDC_CIDERPRESS_BROWSE
// . CPageAdvanced:	IDC_PRINTER_DUMP_FILENAME_BROWSE
std::string CPropertySheetHelper::BrowseToFile(HWND hWindow, const char* pszTitle, const char* REGVALUE, const char* FILEMASKS)
{
	char szFilename[MAX_PATH];
	RegLoadString(REG_CONFIG, REGVALUE, 1, szFilename, MAX_PATH, "");
	std::string pathname = szFilename;

	OPENFILENAME ofn;
	memset(&ofn, 0, sizeof(OPENFILENAME));

	ofn.lStructSize     = sizeof(OPENFILENAME);
	ofn.hwndOwner       = hWindow;
	ofn.hInstance       = GetFrame().g_hInstance;
	ofn.lpstrFilter     = FILEMASKS;
	/*ofn.lpstrFilter     =	"Applications (*.exe)\0*.exe\0"
							"Text files (*.txt)\0*.txt\0"
							"All Files\0*.*\0";*/
	ofn.lpstrFile       = szFilename;
	ofn.nMaxFile        = MAX_PATH;
	ofn.lpstrInitialDir = "";
	ofn.Flags           = OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
	ofn.lpstrTitle      = pszTitle;

	int nRes = GetOpenFileName(&ofn);
	if (nRes)	// OK is pressed
		pathname = szFilename;

	return pathname;
}

void CPropertySheetHelper::SaveStateUpdate()
{
	if (m_bSSNewFilename)
	{
		Snapshot_SetFilename(m_szSSNewFilename, m_szSSNewDirectory);
		RegSaveString(REG_CONFIG, REGVALUE_SAVESTATE_FILENAME, 1, Snapshot_GetPathname());
	}
}

// NB. OK'ing this property sheet will call SaveStateUpdate()->Snapshot_SetFilename() with this new path & filename
int CPropertySheetHelper::SaveStateSelectImage(HWND hWindow, const char* pszTitle, bool bSave)
{
	// Whenever harddisks/disks are inserted (or removed) and *if path has changed* then:
	// . Snapshot's path & Snapshot's filename will be updated to reflect the new defaults.

	std::string szDirectory = Snapshot_GetPath();
	if (szDirectory.empty())
		szDirectory = g_sCurrentDir;

	char szFilename[MAX_PATH];
	strcpy(szFilename, Snapshot_GetFilename().c_str());

	//

	OPENFILENAME ofn;
	memset(&ofn, 0, sizeof(OPENFILENAME));

	ofn.lStructSize     = sizeof(OPENFILENAME);
	ofn.hwndOwner       = hWindow;
	ofn.hInstance       = GetFrame().g_hInstance;
	ofn.lpstrFilter     = "Save State files (*.aws.yaml)\0*.aws.yaml\0"
						  "All Files\0*.*\0";
	ofn.lpstrFile       = szFilename;	// Dialog strips the last .EXT from this string (eg. file.aws.yaml is displayed as: file.aws
	ofn.nMaxFile        = sizeof(szFilename);
	ofn.lpstrInitialDir = szDirectory.c_str();
	ofn.Flags           = OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
	ofn.lpstrTitle      = pszTitle;

	int nRes = bSave ? GetSaveFileName(&ofn) : GetOpenFileName(&ofn);
	if (nRes)
	{
		if (bSave)	// Only for saving (allow loading of any file for backwards compatibility)
		{
			// Append .aws.yaml if it's not there
			const char szAWS_EXT1[] = ".aws";
			const char szAWS_EXT2[] = ".yaml";
			const char szAWS_EXT3[] = ".aws.yaml";
			const size_t uStrLenFile  = strlen(&szFilename[ofn.nFileOffset]);
			const size_t uStrLenExt1  = strlen(szAWS_EXT1);
			const size_t uStrLenExt2  = strlen(szAWS_EXT2);
			const size_t uStrLenExt3  = strlen(szAWS_EXT3);
			if (uStrLenFile <= uStrLenExt1)
			{
				strcpy(&szFilename[ofn.nFileOffset+uStrLenFile], szAWS_EXT3);					// "file" += ".aws.yaml"
			}
			else if (uStrLenFile <= uStrLenExt2)
			{
				if (strcmp(&szFilename[ofn.nFileOffset+uStrLenFile-uStrLenExt1], szAWS_EXT1) == 0)
					strcpy(&szFilename[ofn.nFileOffset+uStrLenFile-uStrLenExt1], szAWS_EXT3);	// "file.aws" -> "file" + ".aws.yaml"
				else
					strcpy(&szFilename[ofn.nFileOffset+uStrLenFile], szAWS_EXT3);				// "file" += ".aws.yaml"
			}
			else if ((uStrLenFile <= uStrLenExt3) || (strcmp(&szFilename[ofn.nFileOffset+uStrLenFile-uStrLenExt3], szAWS_EXT3) != 0))
			{
				if (strcmp(&szFilename[ofn.nFileOffset+uStrLenFile-uStrLenExt1], szAWS_EXT1) == 0)
					strcpy(&szFilename[ofn.nFileOffset+uStrLenFile-uStrLenExt1], szAWS_EXT3);	// "file.aws" -> "file" + ".aws.yaml"
				else if (strcmp(&szFilename[ofn.nFileOffset+uStrLenFile-uStrLenExt2], szAWS_EXT2) == 0)
					strcpy(&szFilename[ofn.nFileOffset+uStrLenFile-uStrLenExt2], szAWS_EXT3);	// "file.yaml" -> "file" + ".aws.yaml"
				else
					strcpy(&szFilename[ofn.nFileOffset+uStrLenFile], szAWS_EXT3);				// "file" += ".aws.yaml"
			}
		}

		m_szSSNewFilename = &szFilename[ofn.nFileOffset];

		szFilename[ofn.nFileOffset] = 0;
		m_szSSNewDirectory = szFilename;	// always set this, even if unchanged
	}

	m_bSSNewFilename = nRes ? true : false;
	return nRes;
}

// On OK: Optionally post a single "uAfterClose" msg after last page closes
void CPropertySheetHelper::PostMsgAfterClose(HWND hWnd, PAGETYPE page)
{
	m_bmPages &= ~(1<<(UINT32)page);
	if (m_bmPages)
		return;		// Still pages to close

	//

	if (m_ConfigNew.m_uSaveLoadStateMsg && IsOkToSaveLoadState(hWnd, IsConfigChanged()))
	{
		// Drop any config change, and do load/save state
		PostMessage(GetFrame().g_hFrameWindow, m_ConfigNew.m_uSaveLoadStateMsg, 0, 0);
		return;
	}
	
	if (m_bDoBenchmark)
	{
		// Drop any config change, and do benchmark
		PostMessage(GetFrame().g_hFrameWindow, WM_USER_BENCHMARK, 0, 0);	// NB. doesn't do WM_USER_RESTART
		return;
	}

	bool restart = false;

	if (m_ConfigNew.m_Apple2Type == A2TYPE_CLONE)
	{
		MessageBox(hWnd, "Error - Unable to change configuration\n\nReason: A specific clone wasn't selected from the Advanced tab", g_pAppTitle.c_str(), MB_ICONSTOP | MB_SETFOREGROUND);
		return;
	}

	_ASSERT(m_ConfigNew.m_CpuType != CPU_UNKNOWN);	// NB. Could only ever be CPU_UNKNOWN for a clone (and only if a mistake was made when adding a new clone)
	if (m_ConfigNew.m_CpuType == CPU_UNKNOWN)
	{
		m_ConfigNew.m_CpuType = ProbeMainCpuDefault(m_ConfigNew.m_Apple2Type);
	}

	if (IsConfigChanged())
	{
		if (!CheckChangesForRestart(hWnd))
		{
			// Cancelled
			RestoreCurrentConfig();
			return;
		}

		ApplyNewConfig();

		restart = true;
	}

	if (restart)
		GetFrame().Restart();
}

bool CPropertySheetHelper::CheckChangesForRestart(HWND hWnd)
{
	if (!HardwareConfigChanged(hWnd))
		return false;	// Cancelled

	if (!IsOkToRestart(hWnd))
		return false;	// Cancelled

	return true;		// OK
}

#define CONFIG_CHANGED_LOCAL(var) \
	(ConfigOld.var != ConfigNew.var)

// Apply changes to Registry
void CPropertySheetHelper::ApplyNewConfig(const CConfigNeedingRestart& ConfigNew, const CConfigNeedingRestart& ConfigOld)
{
	if (CONFIG_CHANGED_LOCAL(m_Apple2Type))
	{
		SaveComputerType(ConfigNew.m_Apple2Type);
		SetApple2Type(ConfigNew.m_Apple2Type);	// Needed by InitializeIO() so that SLOT0 LC matches Apple2Type
	}

	if (CONFIG_CHANGED_LOCAL(m_CpuType))
		SaveCpuType(ConfigNew.m_CpuType);

	// For all slots (except aux) that have changed:
	// . first empty them
	// . then insert the new card
	// Reason: s1=empty, s2=SSC -> s1=SSC, s2=empty
	// . if we just insert, then we'll go via the intermediate state of "s1=SSC, s2=SSC" - but only 1 instance of SSC is permitted
	for (UINT slot = SLOT0; slot < NUM_SLOTS; slot++)
	{
		if (CONFIG_CHANGED_LOCAL(m_Slot[slot]))
			SetSlot(slot, CT_Empty);
	}

	for (UINT slot = SLOT0; slot < NUM_SLOTS; slot++)
	{
		if (CONFIG_CHANGED_LOCAL(m_Slot[slot]))
			SetSlot(slot, ConfigNew.m_Slot[slot]);

		// Keep going, as card may not have changed, but card's config could have

		if (ConfigNew.m_Slot[slot] == CT_Uthernet || ConfigNew.m_Slot[slot] == CT_Uthernet2)
		{
			// NB. Assume we don't have both cards inserted
			PCapBackend::SetRegistryInterface(slot, ConfigNew.m_tfeInterface);
			Uthernet2::SetRegistryVirtualDNS(slot, ConfigNew.m_tfeVirtualDNS);
		}

		if (ConfigNew.m_Slot[slot] == CT_SSC)
		{
			GetCardMgr().GetSSC()->SetSerialPortItem(ConfigNew.m_serialPortItem);
		}

		if (ConfigNew.m_Slot[slot] == CT_GenericPrinter)
		{
			CConfigNeedingRestart& config = const_cast<CConfigNeedingRestart&>(ConfigNew);
			config.m_parallelPrinterCard.SetRegistryConfig();
		}

		if (ConfigNew.m_Slot[slot] == CT_Disk2)
		{
			for (UINT i = DRIVE_1; i < NUM_DRIVES; i++)
				dynamic_cast<Disk2InterfaceCard&>(GetCardMgr().GetRef(slot)).InsertDisk(i, ConfigNew.m_slotInfoForFDC[slot].pathname[i], false, false);
		}

		if (ConfigNew.m_Slot[slot] == CT_GenericHDD)
		{
			for (UINT i = HARDDISK_1; i < NUM_HARDDISKS; i++)
				dynamic_cast<HarddiskInterfaceCard&>(GetCardMgr().GetRef(slot)).Insert(i, ConfigNew.m_slotInfoForHDC[slot].pathname[i]);
		}
	}

	// Initialize I/O after setting config
	// NB. Uthernet cards check network interface in InitializeIO(), so need SetRegistryInterface() called first
	GetCardMgr().InitializeIO(GetCxRomPeripheral());

	if (CONFIG_CHANGED_LOCAL(m_SlotAux))
		SetSlot(SLOT_AUX, ConfigNew.m_SlotAux);

	if (CONFIG_CHANGED_LOCAL(m_bEnableTheFreezesF8Rom))
		REGSAVE(REGVALUE_THE_FREEZES_F8_ROM, ConfigNew.m_bEnableTheFreezesF8Rom);

	if (CONFIG_CHANGED_LOCAL(m_videoRefreshRate))
		REGSAVE(REGVALUE_VIDEO_REFRESH_RATE, ConfigNew.m_videoRefreshRate);

	if (CONFIG_CHANGED_LOCAL(m_RamWorksMemorySize))
		SetRamWorksMemorySize(ConfigNew.m_RamWorksMemorySize);
}

// Called from Snapshot_LoadState_v2()
// . A convenient way to save newly loaded state to Registry
void CPropertySheetHelper::ApplyNewConfigFromSnapshot(const CConfigNeedingRestart& ConfigNew)
{
	SaveComputerType(ConfigNew.m_Apple2Type);
	SaveCpuType(ConfigNew.m_CpuType);
	//REGSAVE(REGVALUE_THE_FREEZES_F8_ROM, ConfigNew.m_bEnableTheFreezesF8Rom);	// Not currently in save-state
	REGSAVE(REGVALUE_VIDEO_REFRESH_RATE, ConfigNew.m_videoRefreshRate);
	SetRamWorksMemorySize(ConfigNew.m_RamWorksMemorySize);
}

void CPropertySheetHelper::ApplyNewConfig(void)
{
	ApplyNewConfig(m_ConfigNew, m_ConfigOld);
}

void CPropertySheetHelper::SaveCurrentConfig(void)
{
	// NB. clone-type is encoded in g_Apple2Type
	m_ConfigOld.Reload();

	// Reset flags each time:
	m_bDoBenchmark = false;

	// Setup ConfigNew
	m_ConfigNew = m_ConfigOld;
}

void CPropertySheetHelper::RestoreCurrentConfig(void)
{
	// NB. clone-type is encoded in g_Apple2Type
	SetApple2Type(m_ConfigOld.m_Apple2Type);
	SetMainCpu(m_ConfigOld.m_CpuType);

	// Just like for ApplyNewConfig(), don't want to have an intermediate state of "s1=SSC, s2=SSC"
	for (UINT slot = SLOT0; slot < NUM_SLOTS; slot++)
		SetSlot(slot, CT_Empty);

	for (UINT slot = SLOT0; slot < NUM_SLOTS; slot++)
		SetSlot(slot, m_ConfigOld.m_Slot[slot]);

	for (UINT slot = SLOT0; slot < NUM_SLOTS; slot++)
	{
		if (m_ConfigOld.m_Slot[slot] == CT_Uthernet || m_ConfigOld.m_Slot[slot] == CT_Uthernet2)
		{
			// Assume only one CT_Uthernet or CT_Uthernet2 inserted
			PCapBackend::SetRegistryInterface(slot, m_ConfigOld.m_tfeInterface);
			Uthernet2::SetRegistryVirtualDNS(slot, m_ConfigOld.m_tfeVirtualDNS);
		}

		if (m_ConfigOld.m_Slot[slot] == CT_SSC)
		{
			GetCardMgr().GetSSC()->SetSerialPortItem(m_ConfigOld.m_serialPortItem);
		}

		if (m_ConfigOld.m_Slot[slot] == CT_GenericPrinter)
		{
			CConfigNeedingRestart& config = const_cast<CConfigNeedingRestart&>(m_ConfigOld);
			config.m_parallelPrinterCard.SetRegistryConfig();
		}

		if (m_ConfigOld.m_Slot[slot] == CT_Disk2)
		{
			for (UINT i = DRIVE_1; i < NUM_DRIVES; i++)
				dynamic_cast<Disk2InterfaceCard&>(GetCardMgr().GetRef(slot)).InsertDisk(i, m_ConfigOld.m_slotInfoForFDC[slot].pathname[i], false, false);
		}

		if (m_ConfigOld.m_Slot[slot] == CT_GenericHDD)
		{
			for (UINT i = HARDDISK_1; i < NUM_HARDDISKS; i++)
				dynamic_cast<HarddiskInterfaceCard&>(GetCardMgr().GetRef(slot)).Insert(i, m_ConfigOld.m_slotInfoForHDC[slot].pathname[i]);
		}
	}

	GetCardMgr().InitializeIO(GetCxRomPeripheral());

	SetSlot(SLOT_AUX, m_ConfigOld.m_SlotAux);

	//GetPropertySheet().SetTheFreezesF8Rom(m_ConfigOld.m_bEnableTheFreezesF8Rom);// -- not applied yet, so do nothing
	//m_ConfigOld.m_videoRefreshRate -- not applied yet, so do nothing
	//m_ConfigOld.m_RamWorksMemorySize -- not applied yet, so do nothing
}

bool CPropertySheetHelper::IsOkToSaveLoadState(HWND hWnd, const bool bConfigChanged)
{
	if (bConfigChanged)
	{
		if (MessageBox(hWnd,
				"The hardware configuration has changed. Save/Load state will lose these changes.\n\n"
				"Are you sure you want to do this?",
				REG_CONFIG,
				MB_ICONQUESTION | MB_OKCANCEL | MB_SETFOREGROUND) == IDCANCEL)
			return false;
	}

	return true;
}

bool CPropertySheetHelper::IsOkToRestart(HWND hWnd)
{
	if (g_nAppMode == MODE_LOGO)
		return true;

	if (MessageBox(hWnd,
			"Restarting the emulator will reset the state "
			"of the emulated machine, causing you to lose any "
			"unsaved work.\n\n"
			"Are you sure you want to do this?",
			REG_CONFIG,
			MB_ICONQUESTION | MB_OKCANCEL | MB_SETFOREGROUND) == IDCANCEL)
		return false;

	return true;
}

#define CONFIG_CHANGED(var) \
	(m_ConfigOld.var != m_ConfigNew.var)

bool CPropertySheetHelper::HardwareConfigChanged(HWND hWnd)
{
	std::string strMsg("The emulator needs to restart as the hardware configuration has changed:\n");
	strMsg += "\n";

	std::string strMsgMain;
	{
		if (CONFIG_CHANGED(m_Apple2Type))
			strMsgMain += ". Emulated computer has changed\n";

		if (CONFIG_CHANGED(m_CpuType))
			strMsgMain += ". Emulated main CPU has changed\n";

		if (CONFIG_CHANGED(m_videoRefreshRate))
			strMsgMain += ". Video refresh rate has changed\n";

		for (UINT slot = SLOT0; slot < NUM_SLOTS; slot++)
		{
			if (CONFIG_CHANGED(m_Slot[slot]))
				strMsgMain += GetSlot(slot);
		}

		if (CONFIG_CHANGED(m_SlotAux))
			strMsgMain += GetSlot(SLOT_AUX);

		if (CONFIG_CHANGED(m_tfeInterface))
			strMsgMain += ". Uthernet interface has changed\n";

		if (CONFIG_CHANGED(m_tfeVirtualDNS))
			strMsgMain += ". Uthernet Virtual DNS has changed\n";

		if (CONFIG_CHANGED(m_bEnableTheFreezesF8Rom))
			strMsgMain += ". F8 ROM changed (The Freeze's F8 Rom)\n";

		if (CONFIG_CHANGED(m_RamWorksMemorySize))
			strMsgMain += ". RamWorks III memory size changed\n";

		if (CONFIG_CHANGED(m_parallelPrinterCard))
			strMsgMain += ". Parallel Printer config has changed\n";

		if (CONFIG_CHANGED(m_serialPortItem))
			strMsgMain += ". SSC config has changed\n";
	}

	std::string strMsgPost("\n");
	strMsgPost += "This change will not take effect until the next time you restart the emulator.\n\n";
	strMsgPost += "Would you like to restart the emulator now?";

	strMsg += strMsgMain;
	strMsg += strMsgPost;

	if (MessageBox(hWnd,
			strMsg.c_str(),
			REG_CONFIG,
			MB_ICONQUESTION | MB_OKCANCEL | MB_SETFOREGROUND) == IDCANCEL)
		return false;

	return true;
}

std::string CPropertySheetHelper::GetSlot(const UINT slot)
{
	std::string strMsg;
	SS_CARDTYPE oldCardType, newCardType;

	if (slot <= SLOT7)
	{
		// strMsg = ". Slot n: ";
		strMsg = ". Slot ";
		strMsg += '0' + slot;
		strMsg += ": ";

		oldCardType = m_ConfigOld.m_Slot[slot];
		newCardType = m_ConfigNew.m_Slot[slot];
	}
	else if (slot == SLOT_AUX)
	{
		strMsg = ". Slot Aux:";

		oldCardType = m_ConfigOld.m_SlotAux;
		newCardType = m_ConfigNew.m_SlotAux;
	}
	else
	{
		_ASSERT(0);
		return "Error: Illegal Slot!";
	}

	if (oldCardType == CT_LanguageCardIIe || newCardType == CT_LanguageCardIIe)
	{
		// Switch model: II/II+ (slot 0) <-> //e (no slot 0)
		if (newCardType == CT_LanguageCardIIe)
		{
			if (oldCardType != CT_Empty)
			{
				strMsg += Card::GetCardName(oldCardType);
				strMsg += " card removed\n";
			}
			else
			{
				strMsg = "";
			}
		}
		else
		{
			if (newCardType != CT_Empty)
			{
				strMsg += Card::GetCardName(newCardType);
				strMsg += " card added\n";
			}
			else
			{
				strMsg = "";
			}
		}
	}
	else if (oldCardType == CT_Empty || newCardType == CT_Empty)
	{
		if (newCardType == CT_Empty)
		{
			strMsg += Card::GetCardName(oldCardType);
			strMsg += " card removed\n";
		}
		else
		{
			strMsg += Card::GetCardName(newCardType);
			strMsg += " card added\n";
		}
	}
	else
	{
		strMsg += Card::GetCardName(oldCardType);
		strMsg += " card removed & ";
		strMsg += Card::GetCardName(newCardType);
		strMsg += " card added\n";
	}

	return strMsg;
}
