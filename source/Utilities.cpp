/*AppleWin : An Apple //e emulator for Windows

Copyright (C) 1994-1996, Michael O'Brien
Copyright (C) 1999-2001, Oliver Schmidt
Copyright (C) 2002-2005, Tom Charlesworth
Copyright (C) 2006-2014, Tom Charlesworth, Michael Pohoreski

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

/* Description: main
 *
 * Author: Various
 */

#include "StdAfx.h"

#include "Utilities.h"
#include "Core.h"
#include "CardManager.h"
#include "CPU.h"
#include "Joystick.h"
#include "Log.h"
#include "Mockingboard.h"
#include "MouseInterface.h"
#include "ParallelPrinter.h"
#include "Registry.h"
#include "Riff.h"
#include "SaveState.h"
#include "SerialComms.h"
#include "Speaker.h"
#include "Memory.h"
#include "Pravets.h"
#include "Keyboard.h"
#include "Mockingboard.h"
#include "Interface.h"
#include "SoundCore.h"

#include "Configuration/IPropertySheet.h"
#include "Tfe/Tfe.h"

#ifdef USE_SPEECH_API
#include "Speech.h"
#endif

// Backwards compatibility with AppleWin <1.24.0
static void LoadConfigOldJoystick_v1(const UINT uJoyNum)
{
	DWORD dwOldJoyType;
	if (!REGLOAD(TEXT(uJoyNum==0 ? REGVALUE_OLD_JOYSTICK0_EMU_TYPE1 : REGVALUE_OLD_JOYSTICK1_EMU_TYPE1), &dwOldJoyType))
		return;	// EG. Old AppleWin never installed

	UINT uNewJoyType;
	switch (dwOldJoyType)
	{
	case 0:		// Disabled
	default:
		uNewJoyType = J0C_DISABLED;
		break;
	case 1:		// PC Joystick
		uNewJoyType = J0C_JOYSTICK1;
		break;
	case 2:		// Keyboard (standard)
		uNewJoyType = J0C_KEYBD_NUMPAD;
		GetPropertySheet().SetJoystickCenteringControl(JOYSTICK_MODE_FLOATING);
		break;
	case 3:		// Keyboard (centering)
		uNewJoyType = J0C_KEYBD_NUMPAD;
		GetPropertySheet().SetJoystickCenteringControl(JOYSTICK_MODE_CENTERING);
		break;
	case 4:		// Mouse
		uNewJoyType = J0C_MOUSE;
		break;
	}

	JoySetJoyType(uJoyNum, uNewJoyType);
}

//Reads configuration from the registry entries
void LoadConfiguration(void)
{
	DWORD dwComputerType = 0;
	eApple2Type apple2Type = A2TYPE_APPLE2EENHANCED;

	if (REGLOAD(TEXT(REGVALUE_APPLE2_TYPE), &dwComputerType))
	{
		const DWORD dwLoadedComputerType = dwComputerType;

		if ( (dwComputerType >= A2TYPE_MAX) ||
			 (dwComputerType >= A2TYPE_UNDEFINED && dwComputerType < A2TYPE_CLONE) ||
			 (dwComputerType >= A2TYPE_CLONE_A2_MAX && dwComputerType < A2TYPE_CLONE_A2E) )
			dwComputerType = A2TYPE_APPLE2EENHANCED;

		// Remap the bad Pravets models (before AppleWin v1.26)
		if (dwComputerType == A2TYPE_BAD_PRAVETS82) dwComputerType = A2TYPE_PRAVETS82;
		if (dwComputerType == A2TYPE_BAD_PRAVETS8M) dwComputerType = A2TYPE_PRAVETS8M;

		// Remap the bad Pravets models (at AppleWin v1.26) - GH#415
		if (dwComputerType == A2TYPE_CLONE) dwComputerType = A2TYPE_PRAVETS82;

		if (dwLoadedComputerType != dwComputerType)
		{
			char sText[100];
			StringCbPrintf(sText, sizeof(sText), "Unsupported Apple2Type(%d). Changing to %d", dwLoadedComputerType, dwComputerType);

			LogFileOutput("%s\n", sText);

			MessageBox(
				GetDesktopWindow(),		// NB. g_hFrameWindow is not yet valid
				sText,
				"Load Configuration",
				MB_ICONSTOP | MB_SETFOREGROUND);

			GetPropertySheet().ConfigSaveApple2Type((eApple2Type)dwComputerType);
		}

		apple2Type = (eApple2Type) dwComputerType;
	}
	else if (REGLOAD(TEXT(REGVALUE_OLD_APPLE2_TYPE), &dwComputerType))	// Support older AppleWin registry entries
	{
		switch (dwComputerType)
		{
			// NB. No A2TYPE_APPLE2E (this is correct)
		case 0:		apple2Type = A2TYPE_APPLE2; break;
		case 1:		apple2Type = A2TYPE_APPLE2PLUS; break;
		case 2:		apple2Type = A2TYPE_APPLE2EENHANCED; break;
		default:	apple2Type = A2TYPE_APPLE2EENHANCED; break;
		}
	}

	SetApple2Type(apple2Type);

	//

	DWORD dwMainCpuType;
	REGLOAD_DEFAULT(TEXT(REGVALUE_CPU_TYPE), &dwMainCpuType, CPU_65C02);
	if (dwMainCpuType != CPU_6502 && dwMainCpuType != CPU_65C02)
		dwMainCpuType = CPU_65C02;
	SetMainCpu((eCpuType)dwMainCpuType);

	//

	DWORD dwJoyType;
	if (REGLOAD(TEXT(REGVALUE_JOYSTICK0_EMU_TYPE), &dwJoyType))
		JoySetJoyType(JN_JOYSTICK0, dwJoyType);
	else if (REGLOAD(TEXT(REGVALUE_OLD_JOYSTICK0_EMU_TYPE2), &dwJoyType))	// GH#434
		JoySetJoyType(JN_JOYSTICK0, dwJoyType);
	else
		LoadConfigOldJoystick_v1(JN_JOYSTICK0);

	if (REGLOAD(TEXT(REGVALUE_JOYSTICK1_EMU_TYPE), &dwJoyType))
		JoySetJoyType(JN_JOYSTICK1, dwJoyType);
	else if (REGLOAD(TEXT(REGVALUE_OLD_JOYSTICK1_EMU_TYPE2), &dwJoyType))	// GH#434
		JoySetJoyType(JN_JOYSTICK1, dwJoyType);
	else
		LoadConfigOldJoystick_v1(JN_JOYSTICK1);

	DWORD dwSoundType;
	REGLOAD_DEFAULT(TEXT(REGVALUE_SOUND_EMULATION), &dwSoundType, REG_SOUNDTYPE_WAVE);
	switch (dwSoundType)
	{
	case REG_SOUNDTYPE_NONE:
	case REG_SOUNDTYPE_DIRECT:	// Not supported from 1.26
	case REG_SOUNDTYPE_SMART:	// Not supported from 1.26
	default:
		soundtype = SOUND_NONE;
		break;
	case REG_SOUNDTYPE_WAVE:
		soundtype = SOUND_WAVE;
		break;
	}

	TCHAR serialPortName[CSuperSerialCard::SIZEOF_SERIALCHOICE_ITEM];
	if (RegLoadString(
		TEXT(REG_CONFIG),
		TEXT(REGVALUE_SERIAL_PORT_NAME),
		TRUE,
		serialPortName,
		CSuperSerialCard::SIZEOF_SERIALCHOICE_ITEM))
	{
		if (GetCardMgr().IsSSCInstalled())
			GetCardMgr().GetSSC()->SetSerialPortName(serialPortName);
	}

	REGLOAD_DEFAULT(TEXT(REGVALUE_EMULATION_SPEED), &g_dwSpeed, SPEED_NORMAL);
	GetVideo().Config_Load_Video();
	SetCurrentCLK6502();	// Pre: g_dwSpeed && Config_Load_Video()->SetVideoRefreshRate()

	DWORD dwEnhanceDisk;
	REGLOAD_DEFAULT(TEXT(REGVALUE_ENHANCE_DISK_SPEED), &dwEnhanceDisk, 1);
	GetCardMgr().GetDisk2CardMgr().SetEnhanceDisk(dwEnhanceDisk ? true : false);

	//

	DWORD dwTmp = 0;

	if(REGLOAD(TEXT(REGVALUE_FS_SHOW_SUBUNIT_STATUS), &dwTmp))
		GetFrame().SetFullScreenShowSubunitStatus(dwTmp ? true : false);

	if(REGLOAD(TEXT(REGVALUE_THE_FREEZES_F8_ROM), &dwTmp))
		GetPropertySheet().SetTheFreezesF8Rom(dwTmp);

	if(REGLOAD(TEXT(REGVALUE_SPKR_VOLUME), &dwTmp))
		SpkrSetVolume(dwTmp, GetPropertySheet().GetVolumeMax());

	if(REGLOAD(TEXT(REGVALUE_MB_VOLUME), &dwTmp))
		MB_SetVolume(dwTmp, GetPropertySheet().GetVolumeMax());

	if(REGLOAD(TEXT(REGVALUE_SAVE_STATE_ON_EXIT), &dwTmp))
		g_bSaveStateOnExit = dwTmp ? true : false;


	if(REGLOAD(TEXT(REGVALUE_DUMP_TO_PRINTER), &dwTmp))
		g_bDumpToPrinter = dwTmp ? true : false;

	if(REGLOAD(TEXT(REGVALUE_CONVERT_ENCODING), &dwTmp))
		g_bConvertEncoding = dwTmp ? true : false;

	if(REGLOAD(TEXT(REGVALUE_FILTER_UNPRINTABLE), &dwTmp))
		g_bFilterUnprintable = dwTmp ? true : false;

	if(REGLOAD(TEXT(REGVALUE_PRINTER_APPEND), &dwTmp))
		g_bPrinterAppend = dwTmp ? true : false;


	if(REGLOAD(TEXT(REGVALUE_HDD_ENABLED), &dwTmp))	// TODO: Change to REGVALUE_SLOT7
		HD_SetEnabled(dwTmp ? true : false);

	if(REGLOAD(TEXT(REGVALUE_PDL_XTRIM), &dwTmp))
		JoySetTrim((short)dwTmp, true);
	if(REGLOAD(TEXT(REGVALUE_PDL_YTRIM), &dwTmp))
		JoySetTrim((short)dwTmp, false);

	if(REGLOAD(TEXT(REGVALUE_SCROLLLOCK_TOGGLE), &dwTmp))
		GetPropertySheet().SetScrollLockToggle(dwTmp);

	if(REGLOAD(TEXT(REGVALUE_CURSOR_CONTROL), &dwTmp))
		GetPropertySheet().SetJoystickCursorControl(dwTmp);
	if(REGLOAD(TEXT(REGVALUE_AUTOFIRE), &dwTmp))
		GetPropertySheet().SetAutofire(dwTmp);
	if(REGLOAD(TEXT(REGVALUE_SWAP_BUTTONS_0_AND_1), &dwTmp))
		GetPropertySheet().SetButtonsSwapState(dwTmp ? true : false);
	if(REGLOAD(TEXT(REGVALUE_CENTERING_CONTROL), &dwTmp))
		GetPropertySheet().SetJoystickCenteringControl(dwTmp);

	if(REGLOAD(TEXT(REGVALUE_MOUSE_CROSSHAIR), &dwTmp))
		GetPropertySheet().SetMouseShowCrosshair(dwTmp);
	if(REGLOAD(TEXT(REGVALUE_MOUSE_RESTRICT_TO_WINDOW), &dwTmp))
		GetPropertySheet().SetMouseRestrictToWindow(dwTmp);

	if(REGLOAD(TEXT(REGVALUE_SLOT4), &dwTmp))
		GetCardMgr().Insert(4, (SS_CARDTYPE)dwTmp);
	if(REGLOAD(TEXT(REGVALUE_SLOT5), &dwTmp))
		GetCardMgr().Insert(5, (SS_CARDTYPE)dwTmp);

	//

	TCHAR szFilename[MAX_PATH];

	// Load save-state pathname *before* inserting any harddisk/disk images (for both init & reinit cases)
	// NB. inserting harddisk/disk can change snapshot pathname
	RegLoadString(TEXT(REG_CONFIG), TEXT(REGVALUE_SAVESTATE_FILENAME), 1, szFilename, MAX_PATH, TEXT(""));	// Can be pathname or just filename
	Snapshot_SetFilename(szFilename);	// If not in Registry than default will be used (ie. g_sCurrentDir + default filename)

	//

	RegLoadString(TEXT(REG_PREFS), TEXT(REGVALUE_PREF_HDV_START_DIR), 1, szFilename, MAX_PATH, TEXT(""));
	if (szFilename[0] == '\0')
		GetCurrentDirectory(sizeof(szFilename), szFilename);
	SetCurrentImageDir(szFilename);

	HD_LoadLastDiskImage(HARDDISK_1);
	HD_LoadLastDiskImage(HARDDISK_2);

	//

	// Current/Starting Dir is the "root" of where the user keeps their disk images
	RegLoadString(TEXT(REG_PREFS), TEXT(REGVALUE_PREF_START_DIR), 1, szFilename, MAX_PATH, TEXT(""));
	if (szFilename[0] == '\0')
		GetCurrentDirectory(sizeof(szFilename), szFilename);
	SetCurrentImageDir(szFilename);

	GetCardMgr().GetDisk2CardMgr().LoadLastDiskImage();

	//

	DWORD dwTfeEnabled;
	REGLOAD_DEFAULT(TEXT(REGVALUE_UTHERNET_ACTIVE), &dwTfeEnabled, 0);
	tfe_enabled = dwTfeEnabled ? 1 : 0;

	RegLoadString(TEXT(REG_CONFIG), TEXT(REGVALUE_UTHERNET_INTERFACE), 1, szFilename, MAX_PATH, TEXT(""));
	update_tfe_interface(szFilename, NULL);

	//

	RegLoadString(TEXT(REG_CONFIG), TEXT(REGVALUE_PRINTER_FILENAME), 1, szFilename, MAX_PATH, TEXT(""));
	Printer_SetFilename(szFilename);	// If not in Registry than default will be used

	REGLOAD_DEFAULT(TEXT(REGVALUE_PRINTER_IDLE_LIMIT), &dwTmp, 10);
	Printer_SetIdleLimit(dwTmp);

	if (REGLOAD(TEXT(REGVALUE_WINDOW_SCALE), &dwTmp))
		GetFrame().SetViewportScale(dwTmp);

	if (REGLOAD(TEXT(REGVALUE_CONFIRM_REBOOT), &dwTmp))
		GetFrame().g_bConfirmReboot = dwTmp;
}

static std::string GetFullPath(LPCSTR szFileName)
{
	std::string strPathName;

	if (szFileName[0] == '\\' || szFileName[1] == ':')
	{
		// Abs pathname
		strPathName = szFileName;
	}
	else
	{
		// Rel pathname (GH#663)
		strPathName = g_sStartDir;
		strPathName.append(szFileName);
	}

	return strPathName;
}

static void SetCurrentDir(std::string pathname)
{
	// Due to the order HDDs/disks are inserted, then s7 insertions take priority over s6 & s5; and d2 takes priority over d1:
	// . if -s6[dN] and -hN are specified, then g_sCurrentDir will be set to the HDD image's path
	// . if -s5[dN] and -s6[dN] are specified, then g_sCurrentDir will be set to the s6 image's path
	// . if -[sN]d1 and -[sN]d2 are specified, then g_sCurrentDir will be set to the d2 image's path
	// This is purely dependent on the current order of InsertFloppyDisks() & InsertHardDisks() - ie. very brittle!
	// . better to use -current-dir to be explicit
	std::size_t found = pathname.find_last_of("\\");
	std::string path = pathname.substr(0, found);
	SetCurrentImageDir(path);
}

bool DoDiskInsert(const UINT slot, const int nDrive, LPCSTR szFileName)
{
	Disk2InterfaceCard& disk2Card = dynamic_cast<Disk2InterfaceCard&>(GetCardMgr().GetRef(slot));

	if (szFileName[0] == '\0')
	{
		disk2Card.EjectDisk(nDrive);
		return true;
	}

	std::string strPathName = GetFullPath(szFileName);
	if (strPathName.empty()) return false;

	ImageError_e Error = disk2Card.InsertDisk(nDrive, strPathName.c_str(), IMAGE_USE_FILES_WRITE_PROTECT_STATUS, IMAGE_DONT_CREATE);
	bool res = (Error == eIMAGE_ERROR_NONE);
	if (res)
		SetCurrentDir(strPathName);
	return res;
}

bool DoHardDiskInsert(const int nDrive, LPCSTR szFileName)
{
	if (szFileName[0] == '\0')
	{
		HD_Unplug(nDrive);
		return true;
	}

	std::string strPathName = GetFullPath(szFileName);
	if (strPathName.empty()) return false;

	BOOL bRes = HD_Insert(nDrive, strPathName.c_str());
	bool res = (bRes == TRUE);
	if (res)
		SetCurrentDir(strPathName);
	return res;
}

void InsertFloppyDisks(const UINT slot, LPSTR szImageName_drive[NUM_DRIVES], bool driveConnected[NUM_DRIVES], bool& bBoot)
{
	_ASSERT(slot == 5 || slot == 6);

	bool bRes = true;

	if (!driveConnected[DRIVE_1])
	{
		dynamic_cast<Disk2InterfaceCard&>(GetCardMgr().GetRef(slot)).UnplugDrive(DRIVE_1);
	}
	else if (szImageName_drive[DRIVE_1])
	{
		bRes = DoDiskInsert(slot, DRIVE_1, szImageName_drive[DRIVE_1]);
		LogFileOutput("Init: S%d, DoDiskInsert(D1), res=%d\n", slot, bRes);
		GetFrame().FrameRefreshStatus(DRAW_LEDS | DRAW_BUTTON_DRIVES);	// floppy activity LEDs and floppy buttons
		bBoot = true;
	}

	if (!driveConnected[DRIVE_2])
	{
		dynamic_cast<Disk2InterfaceCard&>(GetCardMgr().GetRef(slot)).UnplugDrive(DRIVE_2);
	}
	else if (szImageName_drive[DRIVE_2])
	{
		bRes |= DoDiskInsert(slot, DRIVE_2, szImageName_drive[DRIVE_2]);
		LogFileOutput("Init: S%d, DoDiskInsert(D2), res=%d\n", slot, bRes);
	}

	if (!bRes)
		MessageBox(GetFrame().g_hFrameWindow, "Failed to insert floppy disk(s) - see log file", "Warning", MB_ICONASTERISK | MB_OK);
}

void InsertHardDisks(LPSTR szImageName_harddisk[NUM_HARDDISKS], bool& bBoot)
{
	if (!szImageName_harddisk[HARDDISK_1] && !szImageName_harddisk[HARDDISK_2])
		return;

	// Enable the Harddisk controller card

	HD_SetEnabled(true);

	DWORD dwTmp;
	BOOL res = REGLOAD(TEXT(REGVALUE_HDD_ENABLED), &dwTmp);
	if (!res || !dwTmp)
		REGSAVE(TEXT(REGVALUE_HDD_ENABLED), 1);	// Config: HDD Enabled

	//

	bool bRes = true;

	if (szImageName_harddisk[HARDDISK_1])
	{
		bRes = DoHardDiskInsert(HARDDISK_1, szImageName_harddisk[HARDDISK_1]);
		LogFileOutput("Init: DoHardDiskInsert(HDD1), res=%d\n", bRes);
		GetFrame().FrameRefreshStatus(DRAW_LEDS);	// harddisk activity LED
		bBoot = true;
	}

	if (szImageName_harddisk[HARDDISK_2])
	{
		bRes |= DoHardDiskInsert(HARDDISK_2, szImageName_harddisk[HARDDISK_2]);
		LogFileOutput("Init: DoHardDiskInsert(HDD2), res=%d\n", bRes);
	}

	if (!bRes)
		MessageBox(GetFrame().g_hFrameWindow, "Failed to insert harddisk(s) - see log file", "Warning", MB_ICONASTERISK | MB_OK);
}

void UnplugHardDiskControllerCard(void)
{
	HD_SetEnabled(false);

	DWORD dwTmp;
	BOOL res = REGLOAD(TEXT(REGVALUE_HDD_ENABLED), &dwTmp);
	if (!res || dwTmp)
		REGSAVE(TEXT(REGVALUE_HDD_ENABLED), 0);	// Config: HDD Disabled
}

void GetAppleWindowTitle()
{
	switch (g_Apple2Type)
	{
	default:
	case A2TYPE_APPLE2:			 g_pAppTitle = TITLE_APPLE_2; break;
	case A2TYPE_APPLE2PLUS:		 g_pAppTitle = TITLE_APPLE_2_PLUS; break;
	case A2TYPE_APPLE2JPLUS:	 g_pAppTitle = TITLE_APPLE_2_JPLUS; break;
	case A2TYPE_APPLE2E:		 g_pAppTitle = TITLE_APPLE_2E; break;
	case A2TYPE_APPLE2EENHANCED: g_pAppTitle = TITLE_APPLE_2E_ENHANCED; break;
	case A2TYPE_PRAVETS82:		 g_pAppTitle = TITLE_PRAVETS_82; break;
	case A2TYPE_PRAVETS8M:		 g_pAppTitle = TITLE_PRAVETS_8M; break;
	case A2TYPE_PRAVETS8A:		 g_pAppTitle = TITLE_PRAVETS_8A; break;
	case A2TYPE_TK30002E:		 g_pAppTitle = TITLE_TK3000_2E; break;
	case A2TYPE_BASE64A:		 g_pAppTitle = TITLE_BASE64A; break;
	}

#if _DEBUG
	g_pAppTitle += " *DEBUG* ";
#endif

	if (g_nAppMode == MODE_LOGO)
		return;

	g_pAppTitle += " - ";

	if (GetVideo().IsVideoStyle(VS_HALF_SCANLINES))
		g_pAppTitle += " 50% ";

	g_pAppTitle += GetVideo().VideoGetAppWindowTitle();

	if (GetCardMgr().GetDisk2CardMgr().IsAnyFirmware13Sector())
		g_pAppTitle += " (S6-13) ";

	if (g_hCustomRomF8 != INVALID_HANDLE_VALUE)
		g_pAppTitle += TEXT(" (custom rom)");
	else if (GetPropertySheet().GetTheFreezesF8Rom() && IS_APPLE2)
		g_pAppTitle += TEXT(" (The Freeze's non-autostart F8 rom)");

	switch (g_nAppMode)
	{
	case MODE_PAUSED: g_pAppTitle += std::string(TEXT(" [")) + TITLE_PAUSED + TEXT("]"); break;
	case MODE_STEPPING: g_pAppTitle += std::string(TEXT(" [")) + TITLE_STEPPING + TEXT("]"); break;
	}
}

//===========================================================================

// CtrlReset() vs ResetMachineState():
// . CPU:
//		Ctrl+Reset : 6502.sp=-3    / CpuReset()
//		Power cycle: 6502.sp=0x1ff / CpuInitialize()
// . Disk][:
//		Ctrl+Reset : if motor-on, then motor-off but continue to spin for 1s
//		Power cycle: motor-off & immediately stop spinning

// todo: consolidate CtrlReset() and ResetMachineState()
void ResetMachineState()
{
	GetCardMgr().GetDisk2CardMgr().Reset(true);
	HD_Reset();
	g_bFullSpeed = 0;	// Might've hit reset in middle of InternalCpuExecute() - so beep may get (partially) muted

	MemReset();	// calls CpuInitialize(), CNoSlotClock.Reset()
	PravetsReset();
	if (GetCardMgr().QuerySlot(SLOT6) == CT_Disk2)
		dynamic_cast<Disk2InterfaceCard&>(GetCardMgr().GetRef(SLOT6)).Boot();
	GetVideo().VideoResetState();
	KeybReset();
	if (GetCardMgr().IsSSCInstalled())
		GetCardMgr().GetSSC()->CommReset();
	PrintReset();
	JoyReset();
	MB_Reset();
	SpkrReset();
	if (GetCardMgr().IsMouseCardInstalled())
		GetCardMgr().GetMouseCard()->Reset();
	SetActiveCpu(GetMainCpu());
#ifdef USE_SPEECH_API
	g_Speech.Reset();
#endif

	SoundCore_SetFade(FADE_NONE);
	LogFileTimeUntilFirstKeyReadReset();
}


//===========================================================================

/*
 * In comments, UTAII is an abbreviation for a reference to "Understanding the Apple II" by James Sather
 */

 // todo: consolidate CtrlReset() and ResetMachineState()
void CtrlReset()
{
	if (!IS_APPLE2)
	{
		// For A][ & A][+, reset doesn't reset the LC switches (UTAII:5-29)
		MemResetPaging();

		// For A][ & A][+, reset doesn't reset the video mode (UTAII:4-4)
		GetVideo().VideoResetState();	// Switch Alternate char set off
	}

	if (IsAppleIIeOrAbove(GetApple2Type()) || IsCopamBase64A(GetApple2Type()))
	{
		// For A][ & A][+, reset doesn't reset the annunciators (UTAIIe:I-5)
		// Base 64A: on RESET does reset to ROM page 0 (GH#807)
		MemAnnunciatorReset();
	}

	PravetsReset();
	GetCardMgr().GetDisk2CardMgr().Reset();
	HD_Reset();
	KeybReset();
	if (GetCardMgr().IsSSCInstalled())
		GetCardMgr().GetSSC()->CommReset();
	MB_Reset();
	if (GetCardMgr().IsMouseCardInstalled())
		GetCardMgr().GetMouseCard()->Reset();		// Deassert any pending IRQs - GH#514
#ifdef USE_SPEECH_API
	g_Speech.Reset();
#endif

	CpuReset();
	GetFrame().g_bFreshReset = true;
}
