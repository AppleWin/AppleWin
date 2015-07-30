/*
AppleWin : An Apple //e emulator for Windows

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

#include "AppleWin.h"
#include "CPU.h"
#include "Debug.h"
#include "Disk.h"
#include "DiskImage.h"
#include "Frame.h"
#include "Harddisk.h"
#include "Joystick.h"
#include "Log.h"
#include "Memory.h"
#include "Mockingboard.h"
#include "MouseInterface.h"
#include "ParallelPrinter.h"
#include "Registry.h"
#include "Riff.h"
#include "SaveState.h"
#include "SerialComms.h"
#include "SoundCore.h"
#include "Speaker.h"
#ifdef USE_SPEECH_API
#include "Speech.h"
#endif
#include "Video.h"

#include "Configuration\About.h"
#include "Configuration\PropertySheet.h"
#include "Tfe\Tfe.h"

char VERSIONSTRING[16] = "xx.yy.zz.ww";

TCHAR *g_pAppTitle = TITLE_APPLE_2E_ENHANCED;

eApple2Type	g_Apple2Type = A2TYPE_APPLE2EENHANCED;

bool      g_bFullSpeed      = false;

//Pravets 8A/C variables
bool P8CAPS_ON = false;
bool P8Shift = false;
//=================================================

// Win32
HINSTANCE g_hInstance          = (HINSTANCE)0;

AppMode_e	g_nAppMode = MODE_LOGO;
static bool g_bLoadedSaveState = false;

TCHAR     g_sProgramDir[MAX_PATH] = TEXT(""); // Directory of where AppleWin executable resides
TCHAR     g_sDebugDir  [MAX_PATH] = TEXT(""); // TODO: Not currently used
TCHAR     g_sScreenShotDir[MAX_PATH] = TEXT(""); // TODO: Not currently used
TCHAR     g_sCurrentDir[MAX_PATH] = TEXT(""); // Also Starting Dir.  Debugger uses this when load/save
BOOL      restart           = 0;

DWORD		g_dwSpeed		= SPEED_NORMAL;	// Affected by Config dialog's speed slider bar
double		g_fCurrentCLK6502 = CLK_6502;	// Affected by Config dialog's speed slider bar
static double g_fMHz		= 1.0;			// Affected by Config dialog's speed slider bar

int			g_nCpuCyclesFeedback = 0;
DWORD       g_dwCyclesThisFrame = 0;

FILE*		g_fh			= NULL;
bool		g_bDisableDirectInput = false;
bool		g_bDisableDirectSound = false;
bool		g_bDisableDirectSoundMockingboard = false;
int			g_nMemoryClearType = MIP_FF_FF_00_00; // Note: -1 = random MIP in Memory.cpp MemReset()

IPropertySheet&		sg_PropertySheet = * new CPropertySheet;
CSuperSerialCard	sg_SSC;
CMouseInterface		sg_Mouse;

SS_CARDTYPE	g_Slot4 = CT_Empty;
SS_CARDTYPE	g_Slot5 = CT_Empty;

eCPU		g_ActiveCPU = CPU_6502;

HANDLE		g_hCustomRomF8 = INVALID_HANDLE_VALUE;	// Cmd-line specified custom ROM at $F800..$FFFF
static bool	g_bCustomRomF8Failed = false;			// Set if custom ROM file failed

static bool	g_bEnableSpeech = false;
#ifdef USE_SPEECH_API
CSpeech		g_Speech;
#endif

//===========================================================================

bool GetLoadedSaveStateFlag(void)
{
	return g_bLoadedSaveState;
}

void SetLoadedSaveStateFlag(const bool bFlag)
{
	g_bLoadedSaveState = bFlag;
}

static void ResetToLogoMode(void)
{
	g_nAppMode = MODE_LOGO;
	SetLoadedSaveStateFlag(false);
}

//---------------------------------------------------------------------------

static bool g_bPriorityNormal = true;

// Make APPLEWIN process higher priority
void SetPriorityAboveNormal(void)
{
	if (!g_bPriorityNormal)
		return;

	if ( SetPriorityClass(GetCurrentProcess(), ABOVE_NORMAL_PRIORITY_CLASS) )
	{
		SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
		g_bPriorityNormal = false;
	}
}

// Make APPLEWIN process normal priority
void SetPriorityNormal(void)
{
	if (g_bPriorityNormal)
		return;

	if ( SetPriorityClass(GetCurrentProcess(), NORMAL_PRIORITY_CLASS) )
	{
		SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);
		g_bPriorityNormal = true;
	}
}

//---------------------------------------------------------------------------

void ContinueExecution(void)
{
	const double fUsecPerSec        = 1.e6;
#if 1
	const UINT nExecutionPeriodUsec = 1000;		// 1.0ms
//	const UINT nExecutionPeriodUsec = 100;		// 0.1ms
	const double fExecutionPeriodClks = g_fCurrentCLK6502 * ((double)nExecutionPeriodUsec / fUsecPerSec);
#else
	const double fExecutionPeriodClks = 1800.0;
	const UINT nExecutionPeriodUsec = (UINT) (fUsecPerSec * (fExecutionPeriodClks / g_fCurrentCLK6502));
#endif

	//

	bool bScrollLock_FullSpeed = sg_PropertySheet.GetScrollLockToggle()
									? g_bScrollLock_FullSpeed
									: (GetKeyState(VK_SCROLL) < 0);

	g_bFullSpeed = ( (g_dwSpeed == SPEED_MAX) || 
					 bScrollLock_FullSpeed ||
					 (DiskIsSpinning() && enhancedisk && !Spkr_IsActive() && !MB_IsActive()) );

	if (g_bFullSpeed)
	{
		// Don't call Spkr_Mute() - will get speaker clicks
		MB_Mute();
		SysClk_StopTimer();
#ifdef USE_SPEECH_API
		g_Speech.Reset();			// TODO: Put this on a timer (in emulated cycles)... otherwise CATALOG cuts out
#endif

		g_nCpuCyclesFeedback = 0;	// For the case when this is a big -ve number

		// Switch to normal priority so that APPLEWIN process doesn't hog machine!
		//. EG: No disk in Drive-1, and boot Apple: Windows will start to crawl!
		SetPriorityNormal();
	}
	else
	{
		// Don't call Spkr_Demute()
		MB_Demute();
		SysClk_StartTimerUsec(nExecutionPeriodUsec);

		// Switch to higher priority, eg. for audio (BUG #015394)
		SetPriorityAboveNormal();
	}

	//

	int nCyclesToExecute = (int) fExecutionPeriodClks + g_nCpuCyclesFeedback;
	if (nCyclesToExecute < 0)
		nCyclesToExecute = 0;

	const DWORD uActualCyclesExecuted = CpuExecute(nCyclesToExecute);
	g_dwCyclesThisFrame += uActualCyclesExecuted;

	DiskUpdatePosition(uActualCyclesExecuted);
	JoyUpdateButtonLatch(nExecutionPeriodUsec);	// Button latch time is independent of CPU clock frequency

	SpkrUpdate(uActualCyclesExecuted);
	sg_SSC.CommUpdate(uActualCyclesExecuted);
	PrintUpdate(uActualCyclesExecuted);

	//

	if (g_dwCyclesThisFrame >= dwClksPerFrame)
	{
		g_dwCyclesThisFrame -= dwClksPerFrame;
		VideoEndOfVideoFrame();
		MB_EndOfVideoFrame();
	}

	if (!g_bFullSpeed)
	{
		SysClk_WaitTimer();
	}
}

//===========================================================================

void SetCurrentCLK6502(void)
{
	static DWORD dwPrevSpeed = (DWORD) -1;

	if(dwPrevSpeed == g_dwSpeed)
		return;

	dwPrevSpeed = g_dwSpeed;

	// SPEED_MIN    =  0 = 0.50 MHz
	// SPEED_NORMAL = 10 = 1.00 MHz
	//                20 = 2.00 MHz
	// SPEED_MAX-1  = 39 = 3.90 MHz
	// SPEED_MAX    = 40 = ???? MHz (run full-speed, /g_fCurrentCLK6502/ is ignored)

	if(g_dwSpeed < SPEED_NORMAL)
		g_fMHz = 0.5 + (double)g_dwSpeed * 0.05;
	else
		g_fMHz = (double)g_dwSpeed / 10.0;

	g_fCurrentCLK6502 = CLK_6502 * g_fMHz;

	//
	// Now re-init modules that are dependent on /g_fCurrentCLK6502/
	//

	SpkrReinitialize();
	MB_Reinitialize();
}

//===========================================================================
void EnterMessageLoop(void)
{
	MSG message;

	PeekMessage(&message, NULL, 0, 0, PM_NOREMOVE);

	while (message.message!=WM_QUIT)
	{
		if (PeekMessage(&message, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&message);
			DispatchMessage(&message);

			while ((g_nAppMode == MODE_RUNNING) || (g_nAppMode == MODE_STEPPING))
			{
				if (PeekMessage(&message,0,0,0,PM_REMOVE))
				{
					if (message.message == WM_QUIT)
						return;

					TranslateMessage(&message);
					DispatchMessage(&message);
				}
				else if (g_nAppMode == MODE_STEPPING)
				{
					DebugContinueStepping();
				}
				else
				{
					ContinueExecution();
					if (g_nAppMode != MODE_DEBUG)
					{
						if (g_bFullSpeed)
							ContinueExecution();
					}
				}
			}
		}
		else
		{
			if (g_nAppMode == MODE_DEBUG)
				DebuggerUpdate();
			else if ((g_nAppMode == MODE_LOGO) || (g_nAppMode == MODE_PAUSED))
				Sleep(100);		// Stop process hogging CPU
		}
	}
}

//===========================================================================
void GetProgramDirectory(void)
{
	GetModuleFileName((HINSTANCE)0, g_sProgramDir, MAX_PATH);
	g_sProgramDir[MAX_PATH-1] = 0;

	int loop = _tcslen(g_sProgramDir);
	while (loop--)
	{
		if ((g_sProgramDir[loop] == TEXT('\\')) || (g_sProgramDir[loop] == TEXT(':')))
		{
			g_sProgramDir[loop+1] = 0;
			break;
		}
	}
}

//===========================================================================

// Backwards compatibility with AppleWin <1.24.0
static void LoadConfigOldJoystick(const UINT uJoyNum)
{
	DWORD dwOldJoyType;
	if (!REGLOAD(TEXT(uJoyNum==0 ? REGVALUE_OLD_JOYSTICK0_EMU_TYPE : REGVALUE_OLD_JOYSTICK1_EMU_TYPE), &dwOldJoyType))
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
		sg_PropertySheet.SetJoystickCenteringControl(JOYSTICK_MODE_FLOATING);
		break;
	case 3:		// Keyboard (centering)
		uNewJoyType = J0C_KEYBD_NUMPAD;
		sg_PropertySheet.SetJoystickCenteringControl(JOYSTICK_MODE_CENTERING);
		break;
	case 4:		// Mouse
		uNewJoyType = J0C_MOUSE;
		break;
	}

	joytype[uJoyNum] = uNewJoyType;
}

//Reads configuration from the registry entries
void LoadConfiguration(void)
{
	DWORD dwComputerType;

	if (REGLOAD(TEXT(REGVALUE_APPLE2_TYPE), &dwComputerType))
	{
		if ((dwComputerType >= A2TYPE_MAX) || (dwComputerType >= A2TYPE_UNDEFINED && dwComputerType < A2TYPE_CLONE))
			dwComputerType = A2TYPE_APPLE2EENHANCED;

		g_Apple2Type = (eApple2Type) dwComputerType;
	}
	else	// Support older AppleWin registry entries
	{
		REGLOAD(TEXT(REGVALUE_OLD_APPLE2_TYPE), &dwComputerType);
		switch (dwComputerType)
		{
			// NB. No A2TYPE_APPLE2E (this is correct)
		case 0:	g_Apple2Type = A2TYPE_APPLE2;
		case 1:	g_Apple2Type = A2TYPE_APPLE2PLUS;
		case 2:	g_Apple2Type = A2TYPE_APPLE2EENHANCED;
		default:	g_Apple2Type = A2TYPE_APPLE2EENHANCED;
		}
	}

	switch (g_Apple2Type) //Sets the character set for the Apple model/clone
	{
	case A2TYPE_APPLE2:			g_nCharsetType  = 0; break; 
	case A2TYPE_APPLE2PLUS:		g_nCharsetType  = 0; break; 
	case A2TYPE_APPLE2E:		g_nCharsetType  = 0; break; 
	case A2TYPE_APPLE2EENHANCED:g_nCharsetType  = 0; break; 
	case A2TYPE_PRAVETS82:	    g_nCharsetType  = 1; break; 
	case A2TYPE_PRAVETS8A:	    g_nCharsetType  = 2; break; 
	case A2TYPE_PRAVETS8M:	    g_nCharsetType  = 3; break; //This charset has a very small difference with the PRAVETS82 one an probably has some misplaced characters. Still the Pravets82 charset is used, because setting charset to 3 results in some problems.
	}

	//

	if (!REGLOAD(TEXT(REGVALUE_JOYSTICK0_EMU_TYPE), &joytype[JN_JOYSTICK0]))
		LoadConfigOldJoystick(JN_JOYSTICK0);
	if (!REGLOAD(TEXT(REGVALUE_JOYSTICK1_EMU_TYPE), &joytype[JN_JOYSTICK1]))
		LoadConfigOldJoystick(JN_JOYSTICK1);

	REGLOAD(TEXT("Sound Emulation")     ,&soundtype);

	char aySerialPortName[ CSuperSerialCard::SIZEOF_SERIALCHOICE_ITEM ];
	if (RegLoadString(	TEXT("Configuration"),
		TEXT(REGVALUE_SERIAL_PORT_NAME),
		TRUE,
		aySerialPortName,
		sizeof(aySerialPortName) ) )
	{
		sg_SSC.SetSerialPortName(aySerialPortName);
	}

	REGLOAD(TEXT(REGVALUE_EMULATION_SPEED)   ,&g_dwSpeed);
	REGLOAD(TEXT(REGVALUE_ENHANCE_DISK_SPEED),(DWORD *)&enhancedisk);

	Config_Load_Video();

	REGLOAD(TEXT("Uthernet Active")   ,(DWORD *)&tfe_enabled);

	SetCurrentCLK6502();

	//

	DWORD dwTmp;

	if(REGLOAD(TEXT(REGVALUE_THE_FREEZES_F8_ROM), &dwTmp))
		sg_PropertySheet.SetTheFreezesF8Rom(dwTmp);

	if(REGLOAD(TEXT(REGVALUE_SPKR_VOLUME), &dwTmp))
		SpkrSetVolume(dwTmp, sg_PropertySheet.GetVolumeMax());

	if(REGLOAD(TEXT(REGVALUE_MB_VOLUME), &dwTmp))
		MB_SetVolume(dwTmp, sg_PropertySheet.GetVolumeMax());

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


	if(REGLOAD(TEXT(REGVALUE_HDD_ENABLED), &dwTmp))
		HD_SetEnabled(dwTmp ? true : false);

	char szHDVPathname[MAX_PATH] = {0};
	if(RegLoadString(TEXT(REG_PREFS), TEXT(REGVALUE_PREF_LAST_HARDDISK_1), 1, szHDVPathname, sizeof(szHDVPathname)))
		HD_InsertDisk(HARDDISK_1, szHDVPathname);
	if(RegLoadString(TEXT(REG_PREFS), TEXT(REGVALUE_PREF_LAST_HARDDISK_2), 1, szHDVPathname, sizeof(szHDVPathname)))
		HD_InsertDisk(HARDDISK_2, szHDVPathname);

	if(REGLOAD(TEXT(REGVALUE_PDL_XTRIM), &dwTmp))
		JoySetTrim((short)dwTmp, true);
	if(REGLOAD(TEXT(REGVALUE_PDL_YTRIM), &dwTmp))
		JoySetTrim((short)dwTmp, false);

	if(REGLOAD(TEXT(REGVALUE_SCROLLLOCK_TOGGLE), &dwTmp))
		sg_PropertySheet.SetScrollLockToggle(dwTmp);

	if(REGLOAD(TEXT(REGVALUE_CURSOR_CONTROL), &dwTmp))
		sg_PropertySheet.SetJoystickCursorControl(dwTmp);
	if(REGLOAD(TEXT(REGVALUE_AUTOFIRE), &dwTmp))
		sg_PropertySheet.SetAutofire(dwTmp);
	if(REGLOAD(TEXT(REGVALUE_CENTERING_CONTROL), &dwTmp))
		sg_PropertySheet.SetJoystickCenteringControl(dwTmp);

	if(REGLOAD(TEXT(REGVALUE_MOUSE_CROSSHAIR), &dwTmp))
		sg_PropertySheet.SetMouseShowCrosshair(dwTmp);
	if(REGLOAD(TEXT(REGVALUE_MOUSE_RESTRICT_TO_WINDOW), &dwTmp))
		sg_PropertySheet.SetMouseRestrictToWindow(dwTmp);

	if(REGLOAD(TEXT(REGVALUE_SLOT4), &dwTmp))
		g_Slot4 = (SS_CARDTYPE) dwTmp;
	if(REGLOAD(TEXT(REGVALUE_SLOT5), &dwTmp))
		g_Slot5 = (SS_CARDTYPE) dwTmp;

	if (g_Slot4 == CT_MockingboardC || g_Slot4 == CT_Phasor)
		MB_SetSoundcardType(g_Slot4);
	else
		MB_SetSoundcardType(CT_Empty);

	//

	char szFilename[MAX_PATH] = {0};

	// Current/Starting Dir is the "root" of where the user keeps his disk images
	RegLoadString(TEXT(REG_PREFS), TEXT(REGVALUE_PREF_START_DIR), 1, szFilename, MAX_PATH);
	if (szFilename[0] == 0)
		GetCurrentDirectory(sizeof(szFilename), szFilename);
	SetCurrentImageDir(szFilename);

	Disk_LoadLastDiskImage(DRIVE_1);
	Disk_LoadLastDiskImage(DRIVE_2);

	//

	szFilename[0] = 0;
	RegLoadString(TEXT(REG_CONFIG),TEXT(REGVALUE_SAVESTATE_FILENAME),1,szFilename,sizeof(szFilename));
	Snapshot_SetFilename(szFilename);	// If not in Registry than default will be used (ie. g_sCurrentDir + default filename)

	szFilename[0] = 0;
	RegLoadString(TEXT(REG_CONFIG),TEXT(REGVALUE_PRINTER_FILENAME),1,szFilename,sizeof(szFilename));
	Printer_SetFilename(szFilename);	// If not in Registry than default will be used

	dwTmp = 10;
	REGLOAD(TEXT(REGVALUE_PRINTER_IDLE_LIMIT), &dwTmp);
	Printer_SetIdleLimit(dwTmp);

	char szUthernetInt[MAX_PATH] = {0};
	RegLoadString(TEXT(REG_CONFIG),TEXT("Uthernet Interface"),1,szUthernetInt,MAX_PATH);  
	update_tfe_interface(szUthernetInt,NULL);

	if (REGLOAD(TEXT(REGVALUE_WINDOW_SCALE), &dwTmp))
		SetViewportScale(dwTmp);

	if (REGLOAD(TEXT(REGVALUE_CONFIRM_REBOOT), &dwTmp))
		g_bConfirmReboot = dwTmp;
}

//===========================================================================

void SetCurrentImageDir(const char* pszImageDir)
{
	strcpy(g_sCurrentDir, pszImageDir);

	int nLen = strlen( g_sCurrentDir );
	if ((nLen > 0) && (g_sCurrentDir[ nLen - 1 ] != '\\'))
	{
		g_sCurrentDir[ nLen + 0 ] = '\\';
		g_sCurrentDir[ nLen + 1 ] = 0;
	}

	SetCurrentDirectory(g_sCurrentDir);
}

//===========================================================================

// TODO: Added dialog option of which file extensions to registry
static bool g_bRegisterFileTypes = true;
//static bool g_bRegistryFileBin = false;
static bool g_bRegistryFileDo  = true;
static bool g_bRegistryFileDsk = true;
static bool g_bRegistryFileNib = true;
static bool g_bRegistryFilePo  = true;


void RegisterExtensions(void)
{
	TCHAR szCommandTmp[MAX_PATH];
	GetModuleFileName((HMODULE)0,szCommandTmp,MAX_PATH);

#ifdef TEST_REG_BUG
	TCHAR command[MAX_PATH];
	wsprintf(command, "%s",	szCommandTmp);	// Wrap	path & filename	in quotes &	null terminate

	TCHAR icon[MAX_PATH];
	wsprintf(icon,TEXT("\"%s,1\""),(LPCTSTR)command);
#endif

	TCHAR command[MAX_PATH];
	wsprintf(command, "\"%s\"",	szCommandTmp);	// Wrap	path & filename	in quotes &	null terminate

	TCHAR icon[MAX_PATH];
	wsprintf(icon,TEXT("%s,1"),(LPCTSTR)command);

	_tcscat(command,TEXT(" \"%1\""));			// Append "%1"
//	_tcscat(command,TEXT("-d1 %1\""));			// Append "%1"
//	sprintf(command, "\"%s\" \"-d1 %%1\"", szCommandTmp);	// Wrap	path & filename	in quotes &	null terminate

	// NB. Reflect extensions in DELREG.INF
//	RegSetValue(HKEY_CLASSES_ROOT,".bin",REG_SZ,"DiskImage",10);	// Removed as .bin is too generic
	long Res = RegDeleteValue(HKEY_CLASSES_ROOT, ".bin");			// TODO: This isn't working :-/

	RegSetValue(HKEY_CLASSES_ROOT,".do"	,REG_SZ,"DiskImage",10);
	RegSetValue(HKEY_CLASSES_ROOT,".dsk",REG_SZ,"DiskImage",10);
	RegSetValue(HKEY_CLASSES_ROOT,".nib",REG_SZ,"DiskImage",10);
	RegSetValue(HKEY_CLASSES_ROOT,".po"	,REG_SZ,"DiskImage",10);
//	RegSetValue(HKEY_CLASSES_ROOT,".2mg",REG_SZ,"DiskImage",10);	// Don't grab this, as not all .2mg images are supported (so defer to CiderPress)
//	RegSetValue(HKEY_CLASSES_ROOT,".2img",REG_SZ,"DiskImage",10);	// Don't grab this, as not all .2mg images are supported (so defer to CiderPress)
//	RegSetValue(HKEY_CLASSES_ROOT,".aws",REG_SZ,"DiskImage",10);	// TO DO
//	RegSetValue(HKEY_CLASSES_ROOT,".hdv",REG_SZ,"DiskImage",10);	// TO DO

	RegSetValue(HKEY_CLASSES_ROOT,
				"DiskImage",
				REG_SZ,"Disk Image",21);

	RegSetValue(HKEY_CLASSES_ROOT,
				"DiskImage\\DefaultIcon",
				REG_SZ,icon,_tcslen(icon)+1);

// This key can interfere....
// HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Explorer\FileExt\.dsk

	RegSetValue(HKEY_CLASSES_ROOT,
				"DiskImage\\shell\\open\\command",
				REG_SZ,command,_tcslen(command)+1);

	RegSetValue(HKEY_CLASSES_ROOT,
				"DiskImage\\shell\\open\\ddeexec",
				REG_SZ,"%1",3);

	RegSetValue(HKEY_CLASSES_ROOT,
				"DiskImage\\shell\\open\\ddeexec\\application",
				REG_SZ,"applewin",_tcslen("applewin")+1);
//				REG_SZ,szCommandTmp,_tcslen(szCommandTmp)+1);

	RegSetValue(HKEY_CLASSES_ROOT,
				"DiskImage\\shell\\open\\ddeexec\\topic",
				REG_SZ,"system",_tcslen("system")+1);
}

//===========================================================================
void AppleWin_RegisterHotKeys(void)
{
	BOOL bStatus = true;
	
	bStatus &= RegisterHotKey( 
		g_hFrameWindow , // HWND hWnd
		VK_SNAPSHOT_560, // int id (user/custom id)
		0              , // UINT fsModifiers
		VK_SNAPSHOT      // UINT vk = PrintScreen
	);

	bStatus &= RegisterHotKey( 
		g_hFrameWindow , // HWND hWnd
		VK_SNAPSHOT_280, // int id (user/custom id)
		MOD_SHIFT      , // UINT fsModifiers
		VK_SNAPSHOT      // UINT vk = PrintScreen
	);

	bStatus &= RegisterHotKey( 
		g_hFrameWindow  , // HWND hWnd
		VK_SNAPSHOT_TEXT, // int id (user/custom id)
		MOD_CONTROL     , // UINT fsModifiers
		VK_SNAPSHOT       // UINT vk = PrintScreen
	);

	if (!bStatus && g_bShowPrintScreenWarningDialog)
	{
		MessageBox( g_hFrameWindow, "Unable to capture PrintScreen key", "Warning", MB_OK );
	}
}

//===========================================================================

LPSTR GetCurrArg(LPSTR lpCmdLine)
{
	if(*lpCmdLine == '\"')
		lpCmdLine++;

	return lpCmdLine;
}

LPSTR GetNextArg(LPSTR lpCmdLine)
{
	int bInQuotes = 0;

	while(*lpCmdLine)
	{
		if(*lpCmdLine == '\"')
		{
			bInQuotes ^= 1;
			if(!bInQuotes)
			{
				*lpCmdLine++ = 0x00;	// Assume end-quote is end of this arg
				continue;
			}
		}

		if((*lpCmdLine == ' ') && !bInQuotes)
		{
			*lpCmdLine++ = 0x00;
			break;
		}

		lpCmdLine++;
	}

	return lpCmdLine;
}

//---------------------------------------------------------------------------

static int DoDiskInsert(const int nDrive, LPCSTR szFileName)
{
	std::string strPathName;

	if (szFileName[0] == '\\' || szFileName[1] == ':')
	{
		// Abs pathname
		strPathName = szFileName;
	}
	else
	{
		// Rel pathname
		char szCWD[_MAX_PATH] = {0};
		if (!GetCurrentDirectory(sizeof(szCWD), szCWD))
			return false;

		strPathName = szCWD;
		strPathName.append("\\");
		strPathName.append(szFileName);
	}

	ImageError_e Error = DiskInsert(nDrive, strPathName.c_str(), IMAGE_USE_FILES_WRITE_PROTECT_STATUS, IMAGE_DONT_CREATE);
	return Error == eIMAGE_ERROR_NONE;
}

//---------------------------------------------------------------------------

int APIENTRY WinMain(HINSTANCE passinstance, HINSTANCE, LPSTR lpCmdLine, int)
{
	bool bShutdown = false;
	bool bSetFullScreen = false;
	bool bBoot = false;
	LPSTR szImageName_drive1 = NULL;
	LPSTR szImageName_drive2 = NULL;
	const std::string strCmdLine(lpCmdLine);		// Keep a copy for log ouput

	while (*lpCmdLine)
	{
		LPSTR lpNextArg = GetNextArg(lpCmdLine);

		if (((strcmp(lpCmdLine, "-l") == 0) || (strcmp(lpCmdLine, "-log") == 0)) && (g_fh == NULL))
		{
			g_fh = fopen("AppleWin.log", "a+t");	// Open log file (append & text mode)
			setvbuf(g_fh, NULL, _IONBF, 0);			// No buffering (so implicit fflush after every fprintf)
			CHAR aDateStr[80], aTimeStr[80];
			GetDateFormat(LOCALE_SYSTEM_DEFAULT, 0, NULL, NULL, (LPTSTR)aDateStr, sizeof(aDateStr));
			GetTimeFormat(LOCALE_SYSTEM_DEFAULT, 0, NULL, NULL, (LPTSTR)aTimeStr, sizeof(aTimeStr));
			fprintf(g_fh, "*** Logging started: %s %s\n", aDateStr, aTimeStr);
		}
		else if (strcmp(lpCmdLine, "-noreg") == 0)
		{
			g_bRegisterFileTypes = false;
		}
		else if (strcmp(lpCmdLine, "-d1") == 0)
		{
			lpCmdLine = GetCurrArg(lpNextArg);
			lpNextArg = GetNextArg(lpNextArg);
			szImageName_drive1 = lpCmdLine;
		}
		else if (strcmp(lpCmdLine, "-d2") == 0)
		{
			lpCmdLine = GetCurrArg(lpNextArg);
			lpNextArg = GetNextArg(lpNextArg);
			szImageName_drive2 = lpCmdLine;
		}
		else if (strcmp(lpCmdLine, "-f") == 0)
		{
			bSetFullScreen = true;
		}
		else if (strcmp(lpCmdLine, "-fs8bit") == 0)
		{
			SetFullScreen32Bit(false);				// Support old v1.24 fullscreen 8-bit palette mode
		}
		else if (strcmp(lpCmdLine, "-no-di") == 0)
		{
			g_bDisableDirectInput = true;
		}
		else if (strcmp(lpCmdLine, "-m") == 0)
		{
			g_bDisableDirectSound = true;
		}
		else if (strcmp(lpCmdLine, "-no-mb") == 0)
		{
			g_bDisableDirectSoundMockingboard = true;
		}
		else if (strcmp(lpCmdLine, "-memclear") == 0)
		{
			lpCmdLine = GetCurrArg(lpNextArg);
			lpNextArg = GetNextArg(lpNextArg);
			g_nMemoryClearType = atoi(lpCmdLine);
			if (g_nMemoryClearType < 0)
				g_nMemoryClearType = 0;
			else
			if (g_nMemoryClearType >= NUM_MIP)
				g_nMemoryClearType = NUM_MIP - 1;
		}
#ifdef RAMWORKS
		else if (strcmp(lpCmdLine, "-r") == 0)		// RamWorks size [1..127]
		{
			lpCmdLine = GetCurrArg(lpNextArg);
			lpNextArg = GetNextArg(lpNextArg);
			g_uMaxExPages = atoi(lpCmdLine);
			if (g_uMaxExPages > 127)
				g_uMaxExPages = 128;
			else if (g_uMaxExPages < 1)
				g_uMaxExPages = 1;
		}
#endif
		else if (strcmp(lpCmdLine, "-f8rom") == 0)		// Use custom 2K ROM at [$F800..$FFFF]
		{
			lpCmdLine = GetCurrArg(lpNextArg);
			lpNextArg = GetNextArg(lpNextArg);
			g_hCustomRomF8 = CreateFile(lpCmdLine, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_READONLY, NULL);
			if ((g_hCustomRomF8 == INVALID_HANDLE_VALUE) || (GetFileSize(g_hCustomRomF8, NULL) != 0x800))
				g_bCustomRomF8Failed = true;
		}
		else if (strcmp(lpCmdLine, "-printscreen") == 0)		// Turn on display of the last filename print screen was saved to
		{
			g_bDisplayPrintScreenFileName = true;
		}
		else if (strcmp(lpCmdLine, "-no-printscreen-dlg") == 0)		// Turn off the PrintScreen warning message dialog (if PrintScreen key can't be grabbed)
		{
			g_bShowPrintScreenWarningDialog = false;
		}
		else if (strcmp(lpCmdLine, "-spkr-inc") == 0)
		{
			lpCmdLine = GetCurrArg(lpNextArg);
			lpNextArg = GetNextArg(lpNextArg);
			const int nErrorInc = atoi(lpCmdLine);
			SoundCore_SetErrorInc( nErrorInc );
		}
		else if (strcmp(lpCmdLine, "-spkr-max") == 0)
		{
			lpCmdLine = GetCurrArg(lpNextArg);
			lpNextArg = GetNextArg(lpNextArg);
			const int nErrorMax = atoi(lpCmdLine);
			SoundCore_SetErrorMax( nErrorMax );
		}
		else if (strcmp(lpCmdLine, "-use-real-printer") == 0)	// Enable control in Advanced config to allow dumping to a real printer
		{
			g_bEnableDumpToRealPrinter = true;
		}
		else if (strcmp(lpCmdLine, "-speech") == 0)
		{
			g_bEnableSpeech = true;
		}
		else if (strcmp(lpCmdLine, "-multimon") == 0)
		{
			g_bMultiMon = true;
		}
		else	// unsupported
		{
			LogFileOutput("Unsupported arg: %s\n", lpCmdLine);
		}

		lpCmdLine = lpNextArg;
	}

	LogFileOutput("CmdLine: %s\n",  strCmdLine.c_str());

#if 0
#ifdef RIFF_SPKR
	RiffInitWriteFile("Spkr.wav", SPKR_SAMPLE_RATE, 1);
#endif
#ifdef RIFF_MB
	RiffInitWriteFile("Mockingboard.wav", 44100, 2);
#endif
#endif

	//-----

    char szPath[_MAX_PATH];

    if (0 == GetModuleFileName(NULL, szPath, sizeof(szPath)))
    {
        strcpy(szPath, __argv[0]);
    }

    // Extract application version and store in a global variable
    DWORD dwHandle, dwVerInfoSize;

    dwVerInfoSize = GetFileVersionInfoSize(szPath, &dwHandle);

    if (dwVerInfoSize > 0)
    {
        char* pVerInfoBlock = new char[dwVerInfoSize];

        if (GetFileVersionInfo(szPath, NULL, dwVerInfoSize, pVerInfoBlock))
        {
            VS_FIXEDFILEINFO* pFixedFileInfo;
            UINT pFixedFileInfoLen;

            VerQueryValue(pVerInfoBlock, TEXT("\\"), (LPVOID*) &pFixedFileInfo, (PUINT) &pFixedFileInfoLen);

            // Construct version string from fixed file info block

            unsigned long major     = pFixedFileInfo->dwFileVersionMS >> 16;
            unsigned long minor     = pFixedFileInfo->dwFileVersionMS & 0xffff;
            unsigned long fix       = pFixedFileInfo->dwFileVersionLS >> 16;
			unsigned long fix_minor = pFixedFileInfo->dwFileVersionLS & 0xffff;
            sprintf(VERSIONSTRING, "%d.%d.%d.%d", major, minor, fix, fix_minor); // potential buffer overflow
		}
    }

	LogFileOutput("AppleWin version: %s\n",  VERSIONSTRING);

	//-----

	// Initialize COM - so we can use CoCreateInstance
	// . NB. DSInit() & DIMouse::DirectInputInit are done when g_hFrameWindow is created (WM_CREATE)
	HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
	LogFileOutput("Init: CoInitializeEx(), hr=0x%08X\n", hr);

	const bool bSysClkOK = SysClk_InitTimer();
	LogFileOutput("Init: SysClk_InitTimer(), res=%d\n", bSysClkOK ? 1:0);
#ifdef USE_SPEECH_API
	if (g_bEnableSpeech)
	{
		const bool bSpeechOK = g_Speech.Init();
		LogFileOutput("Init: SysClk_InitTimer(), res=%d\n", bSpeechOK ? 1:0);
	}
#endif

	// DO ONE-TIME INITIALIZATION
	g_hInstance = passinstance;
	GdiSetBatchLimit(512);
	LogFileOutput("Init: GdiSetBatchLimit()\n");

	GetProgramDirectory();
	LogFileOutput("Init: GetProgramDirectory()\n");

	if (g_bRegisterFileTypes)
	{
		RegisterExtensions();
		LogFileOutput("Init: RegisterExtensions()\n");
	}

	FrameRegisterClass();
	LogFileOutput("Init: FrameRegisterClass()\n");

	ImageInitialize();
	LogFileOutput("Init: ImageInitialize()\n");

	DiskInitialize();
	LogFileOutput("Init: DiskInitialize()\n");

	CreateColorMixMap();	// For tv emulation mode
	LogFileOutput("Init: CreateColorMixMap()\n");

	int nError = 0;	// TODO: Show error MsgBox if we get a DiskInsert error
	if (szImageName_drive1)
	{
		nError = DoDiskInsert(DRIVE_1, szImageName_drive1);
		LogFileOutput("Init: DoDiskInsert(D1), res=%d\n", nError);
		FrameRefreshStatus(DRAW_LEDS | DRAW_BUTTON_DRIVES);
		bBoot = true;
	}
	if (szImageName_drive2)
	{
		nError |= DoDiskInsert(DRIVE_2, szImageName_drive2);
		LogFileOutput("Init: DoDiskInsert(D2), res=%d\n", nError);
	}

	//

	do
	{
		// DO INITIALIZATION THAT MUST BE REPEATED FOR A RESTART
		restart = 0;
		ResetToLogoMode();

		LoadConfiguration();
		LogFileOutput("Main: LoadConfiguration()\n");

		DebugInitialize();
		LogFileOutput("Main: DebugInitialize()\n");

		JoyInitialize();
		LogFileOutput("Main: JoyInitialize()\n");

		MemInitialize();
		LogFileOutput("Main: MemInitialize()\n");

		VideoInitialize(); // g_pFramebufferinfo been created now
		LogFileOutput("Main: VideoInitialize()\n");

		LogFileOutput("Main: FrameCreateWindow() - pre\n");
		FrameCreateWindow();	// g_hFrameWindow is now valid
		LogFileOutput("Main: FrameCreateWindow() - post\n");

		char szOldAppleWinVersion[sizeof(VERSIONSTRING)] = {0};
		RegLoadString(TEXT(REG_CONFIG), TEXT(REGVALUE_VERSION), 1, szOldAppleWinVersion, sizeof(szOldAppleWinVersion));

		const bool bShowAboutDlg = strcmp(szOldAppleWinVersion, VERSIONSTRING) != 0;
		if (bShowAboutDlg)
		{
			if (!AboutDlg())
				bShutdown = true;															// Close everything down
			else
				RegSaveString(TEXT(REG_CONFIG), TEXT(REGVALUE_VERSION), 1, VERSIONSTRING);	// Only save version after user accepts license
		}

		// PrintScrn support
		AppleWin_RegisterHotKeys(); // needs valid g_hFrameWindow
		LogFileOutput("Main: AppleWin_RegisterHotKeys()\n");

		// Need to test if it's safe to call ResetMachineState(). In the meantime, just call DiskReset():
		DiskReset();	// Switch from a booting A][+ to a non-autostart A][, so need to turn off floppy motor
		LogFileOutput("Main: DiskReset()\n");

		if (!bSysClkOK)
		{
			MessageBox(g_hFrameWindow, "DirectX failed to create SystemClock instance", TEXT("AppleWin Error"), MB_OK);
			bShutdown = true;
		}

		if (g_bCustomRomF8Failed)
		{
			MessageBox(g_hFrameWindow, "Failed to load custom F8 rom (not found or not exactly 2KB)", TEXT("AppleWin Error"), MB_OK);
			bShutdown = true;
		}

		tfe_init();
		LogFileOutput("Main: tfe_init()\n");

		Snapshot_Startup();		// Do this after everything has been init'ed
		LogFileOutput("Main: Snapshot_Startup()\n");

		if (bShutdown)
		{
			PostMessage(g_hFrameWindow, WM_DESTROY, 0, 0);	// Close everything down
			// NB. If shutting down, then don't post any other messages (GH#286)
		}
		else
		{
			if (bSetFullScreen)
			{
				PostMessage(g_hFrameWindow, WM_USER_FULLSCREEN, 0, 0);
				bSetFullScreen = false;
			}

			if (bBoot)
			{
				PostMessage(g_hFrameWindow, WM_USER_BOOT, 0, 0);
				bBoot = false;
			}
		}

		// ENTER THE MAIN MESSAGE LOOP
		LogFileOutput("Main: EnterMessageLoop()\n");
		EnterMessageLoop();
		LogFileOutput("Main: LeaveMessageLoop()\n");

		MB_Reset();
		LogFileOutput("Main: MB_Reset()\n");

		sg_Mouse.Uninitialize();	// Maybe restarting due to switching slot-4 card from MouseCard to Mockingboard
		LogFileOutput("Main: sg_Mouse.Uninitialize()\n");
	}
	while (restart);

	// Release COM
	DSUninit();
	LogFileOutput("Exit: DSUninit()\n");

	SysClk_UninitTimer();
	LogFileOutput("Exit: SysClk_UninitTimer()\n");

	CoUninitialize();
	LogFileOutput("Exit: CoUninitialize()\n");

	tfe_shutdown();
	LogFileOutput("Exit: tfe_shutdown()\n");

	if (g_fh)
	{
		fprintf(g_fh,"*** Logging ended\n\n");
		fclose(g_fh);
		g_fh = NULL;
	}

	RiffFinishWriteFile();

	if (g_hCustomRomF8 != INVALID_HANDLE_VALUE)
		CloseHandle(g_hCustomRomF8);

	return 0;
}
