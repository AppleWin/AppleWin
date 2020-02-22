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

#include "Applewin.h"
#include "CardManager.h"
#include "CPU.h"
#include "Debug.h"
#include "Disk.h"
#include "DiskImage.h"
#include "Frame.h"
#include "Harddisk.h"
#include "Joystick.h"
#include "Keyboard.h"
#include "LanguageCard.h"
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
#include "RGBMonitor.h"
#include "NTSC.h"

#include "Configuration/About.h"
#include "Configuration/PropertySheet.h"
#include "Tfe/Tfe.h"

static const UINT VERSIONSTRING_SIZE = 16;

static UINT16 g_AppleWinVersion[4] = {0};
static UINT16 g_OldAppleWinVersion[4] = {0};
TCHAR VERSIONSTRING[VERSIONSTRING_SIZE] = "xx.yy.zz.ww";

std::string g_pAppTitle;

eApple2Type	g_Apple2Type = A2TYPE_APPLE2EENHANCED;

bool      g_bFullSpeed      = false;

//=================================================

// Win32
HINSTANCE g_hInstance          = (HINSTANCE)0;

AppMode_e	g_nAppMode = MODE_LOGO;
static bool g_bLoadedSaveState = false;
static bool g_bSysClkOK = false;

std::string g_sProgramDir; // Directory of where AppleWin executable resides
std::string g_sDebugDir; // TODO: Not currently used
std::string g_sScreenShotDir; // TODO: Not currently used
bool      g_bCapturePrintScreenKey = true;
static bool g_bHookSystemKey = true;
static bool g_bHookAltTab = false;
static bool g_bHookAltGrControl = false;

std::string g_sCurrentDir; // Also Starting Dir.  Debugger uses this when load/save
bool      g_bRestart = false;
bool      g_bRestartFullScreen = false;

DWORD		g_dwSpeed		= SPEED_NORMAL;	// Affected by Config dialog's speed slider bar
double		g_fCurrentCLK6502 = CLK_6502_NTSC;	// Affected by Config dialog's speed slider bar
static double g_fMHz		= 1.0;			// Affected by Config dialog's speed slider bar

int			g_nCpuCyclesFeedback = 0;
DWORD       g_dwCyclesThisFrame = 0;

bool		g_bDisableDirectInput = false;
bool		g_bDisableDirectSound = false;
bool		g_bDisableDirectSoundMockingboard = false;
int			g_nMemoryClearType = MIP_FF_FF_00_00; // Note: -1 = random MIP in Memory.cpp MemReset()

CardManager g_CardMgr;
IPropertySheet&		sg_PropertySheet = * new CPropertySheet;

HANDLE		g_hCustomRomF8 = INVALID_HANDLE_VALUE;	// Cmd-line specified custom ROM at $F800..$FFFF
static bool	g_bCustomRomF8Failed = false;			// Set if custom ROM file failed

static bool	g_bEnableSpeech = false;
#ifdef USE_SPEECH_API
CSpeech		g_Speech;
#endif

//===========================================================================

static DWORD dwLogKeyReadTickStart;
static bool bLogKeyReadDone = false;

void LogFileTimeUntilFirstKeyReadReset(void)
{
	if (!g_fh)
		return;

	dwLogKeyReadTickStart = GetTickCount();

	bLogKeyReadDone = false;
}

// Log the time from emulation restart/reboot until the first key read: BIT $C000
// . AZTEC.DSK (DOS 3.3) does prior LDY $C000 reads, but the BIT $C000 is at the "Press any key" message
// . Phasor1.dsk / ProDOS 1.1.1: PC=E797: B1 50: LDA ($50),Y / "Select an Option:" message
// . Rescue Raiders v1.3,v1.5: PC=895: LDA $C000 / boot to intro
void LogFileTimeUntilFirstKeyRead(void)
{
	if (!g_fh || bLogKeyReadDone)
		return;

	if ( (mem[regs.pc-3] != 0x2C)	// AZTEC: bit $c000
		&& !((regs.pc-2) == 0xE797 && mem[regs.pc-2] == 0xB1 && mem[regs.pc-1] == 0x50)	// Phasor1: lda ($50),y
		&& !((regs.pc-3) == 0x0895 && mem[regs.pc-3] == 0xAD)	// Rescue Raiders v1.3,v1.5: lda $c000
		)
		return;

	DWORD dwTime = GetTickCount() - dwLogKeyReadTickStart;

	LogFileOutput("Time from emulation reboot until first $C000 access: %d msec\n", dwTime);

	bLogKeyReadDone = true;
}

//---------------------------------------------------------------------------

eApple2Type GetApple2Type(void)
{
	return g_Apple2Type;
}

void SetApple2Type(eApple2Type type)
{
	g_Apple2Type = type;
	SetMainCpuDefault(type);
}

const UINT16* GetOldAppleWinVersion(void)
{
	return g_OldAppleWinVersion;
}

bool GetLoadedSaveStateFlag(void)
{
	return g_bLoadedSaveState;
}

void SetLoadedSaveStateFlag(const bool bFlag)
{
	g_bLoadedSaveState = bFlag;
}

bool GetHookAltGrControl(void)
{
	return g_bHookAltGrControl;
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

static UINT g_uModeStepping_Cycles = 0;
static bool g_uModeStepping_LastGetKey_ScrollLock = false;

static void ContinueExecution(void)
{
	_ASSERT(g_nAppMode == MODE_RUNNING || g_nAppMode == MODE_STEPPING);

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

	bool bScrollLock_FullSpeed = false;
	if (sg_PropertySheet.GetScrollLockToggle())
	{
		bScrollLock_FullSpeed = g_bScrollLock_FullSpeed;
	}
	else
	{
		if (g_nAppMode == MODE_RUNNING)
		{
			bScrollLock_FullSpeed = GetKeyState(VK_SCROLL) < 0;
		}
		else if (!IsDebugSteppingAtFullSpeed()) // Implicitly: MODE_STEPPING
		{
			// NB. For MODE_STEPPING: GetKeyState() is slow, so only call periodically
			// . 0x3FFF is roughly the number of cycles in a video frame, which seems a reasonable rate to call GetKeyState()
			if ((g_uModeStepping_Cycles & 0x3FFF) == 0)
				g_uModeStepping_LastGetKey_ScrollLock = GetKeyState(VK_SCROLL) < 0;

			bScrollLock_FullSpeed = g_uModeStepping_LastGetKey_ScrollLock;
		}
	}

	const bool bWasFullSpeed = g_bFullSpeed;
	g_bFullSpeed =	 (g_dwSpeed == SPEED_MAX) || 
					 bScrollLock_FullSpeed ||
					 (g_CardMgr.GetDisk2CardMgr().IsConditionForFullSpeed() && !Spkr_IsActive() && !MB_IsActive()) ||
					 IsDebugSteppingAtFullSpeed();

	if (g_bFullSpeed)
	{
		if (!bWasFullSpeed)
			VideoRedrawScreenDuringFullSpeed(0, true);	// Init for full-speed mode

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
		if (bWasFullSpeed)
			VideoRedrawScreenAfterFullSpeed(g_dwCyclesThisFrame);

		// Don't call Spkr_Demute()
		MB_Demute();
		SysClk_StartTimerUsec(nExecutionPeriodUsec);

		// Switch to higher priority, eg. for audio (BUG #015394)
		SetPriorityAboveNormal();
	}

	//

	int nCyclesWithFeedback = (int) fExecutionPeriodClks + g_nCpuCyclesFeedback;
	const UINT uCyclesToExecuteWithFeedback = (nCyclesWithFeedback >= 0) ? nCyclesWithFeedback
																		 : 0;

	const DWORD uCyclesToExecute = (g_nAppMode == MODE_RUNNING)		? uCyclesToExecuteWithFeedback
												/* MODE_STEPPING */ : 0;

	const bool bVideoUpdate = !g_bFullSpeed;
	const DWORD uActualCyclesExecuted = CpuExecute(uCyclesToExecute, bVideoUpdate);
	g_dwCyclesThisFrame += uActualCyclesExecuted;

	g_CardMgr.GetDisk2CardMgr().UpdateDriveState(uActualCyclesExecuted);
	JoyUpdateButtonLatch(nExecutionPeriodUsec);	// Button latch time is independent of CPU clock frequency
	PrintUpdate(uActualCyclesExecuted);
	MB_PeriodicUpdate(uActualCyclesExecuted);

	//

	DWORD uSpkrActualCyclesExecuted = uActualCyclesExecuted;

	bool bModeStepping_WaitTimer = false;
	if (g_nAppMode == MODE_STEPPING && !IsDebugSteppingAtFullSpeed())
	{
		g_uModeStepping_Cycles += uActualCyclesExecuted;
		if (g_uModeStepping_Cycles >= uCyclesToExecuteWithFeedback)
		{
			uSpkrActualCyclesExecuted = g_uModeStepping_Cycles;

			g_uModeStepping_Cycles -= uCyclesToExecuteWithFeedback;
			bModeStepping_WaitTimer = true;
		}
	}

	// For MODE_STEPPING: do this speaker update periodically
	// - Otherwise kills performance due to sound-buffer lock/unlock for every 6502 opcode!
	if (g_nAppMode == MODE_RUNNING || bModeStepping_WaitTimer)
		SpkrUpdate(uSpkrActualCyclesExecuted);

	//

	const UINT dwClksPerFrame = NTSC_GetCyclesPerFrame();
	if (g_dwCyclesThisFrame >= dwClksPerFrame && !VideoGetVblBarEx(g_dwCyclesThisFrame))
	{
		g_dwCyclesThisFrame -= dwClksPerFrame;

		if (g_bFullSpeed)
			VideoRedrawScreenDuringFullSpeed(g_dwCyclesThisFrame);
		else
			VideoRefreshScreen(); // Just copy the output of our Apple framebuffer to the system Back Buffer
	}

	if ((g_nAppMode == MODE_RUNNING && !g_bFullSpeed) || bModeStepping_WaitTimer)
	{
		SysClk_WaitTimer();
	}
}

void SingleStep(bool bReinit)
{
	if (bReinit)
	{
		g_uModeStepping_Cycles = 0;
		g_uModeStepping_LastGetKey_ScrollLock = false;
	}

	ContinueExecution();
}

//===========================================================================

double Get6502BaseClock(void)
{
	return (GetVideoRefreshRate() == VR_50HZ) ? CLK_6502_PAL : CLK_6502_NTSC;
}

void UseClockMultiplier(double clockMultiplier)
{
	if (clockMultiplier == 0.0)
		return;

	if (clockMultiplier < 1.0)
	{
		if (clockMultiplier < 0.5)
			clockMultiplier = 0.5;
		g_dwSpeed = (ULONG)((clockMultiplier - 0.5) * 20);	// [0.5..0.9] -> [0..9]
	}
	else
	{
		g_dwSpeed = (ULONG)(clockMultiplier * 10);
		if (g_dwSpeed >= SPEED_MAX)
			g_dwSpeed = SPEED_MAX - 1;
	}

	SetCurrentCLK6502();
}

void SetCurrentCLK6502(void)
{
	static DWORD dwPrevSpeed = (DWORD) -1;
	static VideoRefreshRate_e prevVideoRefreshRate = VR_NONE;

	if (dwPrevSpeed == g_dwSpeed && GetVideoRefreshRate() == prevVideoRefreshRate)
		return;

	dwPrevSpeed = g_dwSpeed;
	prevVideoRefreshRate = GetVideoRefreshRate();

	// SPEED_MIN    =  0 = 0.50 MHz
	// SPEED_NORMAL = 10 = 1.00 MHz
	//                20 = 2.00 MHz
	// SPEED_MAX-1  = 39 = 3.90 MHz
	// SPEED_MAX    = 40 = ???? MHz (run full-speed, /g_fCurrentCLK6502/ is ignored)

	if(g_dwSpeed < SPEED_NORMAL)
		g_fMHz = 0.5 + (double)g_dwSpeed * 0.05;
	else
		g_fMHz = (double)g_dwSpeed / 10.0;

	g_fCurrentCLK6502 = Get6502BaseClock() * g_fMHz;

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
			else if (g_nAppMode == MODE_PAUSED)
				Sleep(1);		// Stop process hogging CPU - 1ms, as need to fade-out speaker sound buffer
			else if (g_nAppMode == MODE_LOGO)
				Sleep(1);		// Stop process hogging CPU (NB. don't delay for too long otherwise key input can be slow in other apps - GH#569)
		}
	}
}

//===========================================================================
void GetProgramDirectory(void)
{
	TCHAR programDir[MAX_PATH];
	GetModuleFileName((HINSTANCE)0, programDir, MAX_PATH);
	programDir[MAX_PATH-1] = 0;

	g_sProgramDir = programDir;

	int loop = g_sProgramDir.size();
	while (loop--)
	{
		if ((g_sProgramDir[loop] == TEXT('\\')) || (g_sProgramDir[loop] == TEXT(':')))
		{
			g_sProgramDir.resize(loop + 1);  // this reduces the size
			break;
		}
	}
}

//===========================================================================

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

			sg_PropertySheet.ConfigSaveApple2Type((eApple2Type)dwComputerType);
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
		if (g_CardMgr.IsSSCInstalled())
			g_CardMgr.GetSSC()->SetSerialPortName(serialPortName);
	}

	REGLOAD_DEFAULT(TEXT(REGVALUE_EMULATION_SPEED), &g_dwSpeed, SPEED_NORMAL);
	Config_Load_Video();
	SetCurrentCLK6502();	// Pre: g_dwSpeed && Config_Load_Video()->SetVideoRefreshRate()

	DWORD dwEnhanceDisk;
	REGLOAD_DEFAULT(TEXT(REGVALUE_ENHANCE_DISK_SPEED), &dwEnhanceDisk, 1);
	g_CardMgr.GetDisk2CardMgr().SetEnhanceDisk(dwEnhanceDisk ? true : false);

	//

	DWORD dwTmp = 0;

	if(REGLOAD(TEXT(REGVALUE_FS_SHOW_SUBUNIT_STATUS), &dwTmp))
		SetFullScreenShowSubunitStatus(dwTmp ? true : false);

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


	if(REGLOAD(TEXT(REGVALUE_HDD_ENABLED), &dwTmp))	// TODO: Change to REGVALUE_SLOT7
		HD_SetEnabled(dwTmp ? true : false);

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
	if(REGLOAD(TEXT(REGVALUE_SWAP_BUTTONS_0_AND_1), &dwTmp))
		sg_PropertySheet.SetButtonsSwapState(dwTmp ? true : false);
	if(REGLOAD(TEXT(REGVALUE_CENTERING_CONTROL), &dwTmp))
		sg_PropertySheet.SetJoystickCenteringControl(dwTmp);

	if(REGLOAD(TEXT(REGVALUE_MOUSE_CROSSHAIR), &dwTmp))
		sg_PropertySheet.SetMouseShowCrosshair(dwTmp);
	if(REGLOAD(TEXT(REGVALUE_MOUSE_RESTRICT_TO_WINDOW), &dwTmp))
		sg_PropertySheet.SetMouseRestrictToWindow(dwTmp);

	if(REGLOAD(TEXT(REGVALUE_SLOT4), &dwTmp))
		g_CardMgr.Insert(4, (SS_CARDTYPE)dwTmp);
	if(REGLOAD(TEXT(REGVALUE_SLOT5), &dwTmp))
		g_CardMgr.Insert(5, (SS_CARDTYPE)dwTmp);

	//

	TCHAR szFilename[MAX_PATH];

	RegLoadString(TEXT(REG_PREFS), TEXT(REGVALUE_PREF_HDV_START_DIR), 1, szFilename, MAX_PATH, TEXT(""));
	if (szFilename[0] == '\0')
		GetCurrentDirectory(sizeof(szFilename), szFilename);
	SetCurrentImageDir(szFilename);

	HD_LoadLastDiskImage(HARDDISK_1);
	HD_LoadLastDiskImage(HARDDISK_2);

	//

	// Current/Starting Dir is the "root" of where the user keeps his disk images
	RegLoadString(TEXT(REG_PREFS), TEXT(REGVALUE_PREF_START_DIR), 1, szFilename, MAX_PATH, TEXT(""));
	if (szFilename[0] == '\0')
		GetCurrentDirectory(sizeof(szFilename), szFilename);
	SetCurrentImageDir(szFilename);

	g_CardMgr.GetDisk2CardMgr().LoadLastDiskImage();

	//

	DWORD dwTfeEnabled;
	REGLOAD_DEFAULT(TEXT(REGVALUE_UTHERNET_ACTIVE), &dwTfeEnabled, 0);
	tfe_enabled = dwTfeEnabled ? 1 : 0;

	RegLoadString(TEXT(REG_CONFIG), TEXT(REGVALUE_UTHERNET_INTERFACE), 1, szFilename, MAX_PATH, TEXT(""));
	update_tfe_interface(szFilename, NULL);

	//

	RegLoadString(TEXT(REG_CONFIG), TEXT(REGVALUE_SAVESTATE_FILENAME), 1, szFilename, MAX_PATH, TEXT(""));
	Snapshot_SetFilename(szFilename);	// If not in Registry than default will be used (ie. g_sCurrentDir + default filename)

	RegLoadString(TEXT(REG_CONFIG), TEXT(REGVALUE_PRINTER_FILENAME), 1, szFilename, MAX_PATH, TEXT(""));
	Printer_SetFilename(szFilename);	// If not in Registry than default will be used

	REGLOAD_DEFAULT(TEXT(REGVALUE_PRINTER_IDLE_LIMIT), &dwTmp, 10);
	Printer_SetIdleLimit(dwTmp);

	if (REGLOAD(TEXT(REGVALUE_WINDOW_SCALE), &dwTmp))
		SetViewportScale(dwTmp);

	if (REGLOAD(TEXT(REGVALUE_CONFIRM_REBOOT), &dwTmp))
		g_bConfirmReboot = dwTmp;
}

//===========================================================================

bool SetCurrentImageDir(const std::string & pszImageDir)
{
	g_sCurrentDir = pszImageDir;

	int nLen = g_sCurrentDir.size();
	if ((nLen > 0) && (g_sCurrentDir[ nLen - 1 ] != '\\'))
		g_sCurrentDir += '\\';

	if( SetCurrentDirectory(g_sCurrentDir.c_str()) )
		return true;

	return false;
}

//===========================================================================

// TODO: Added dialog option of which file extensions to registry
static bool g_bRegisterFileTypes = true;


void RegisterExtensions(void)
{
	TCHAR szCommandTmp[MAX_PATH];
	GetModuleFileName((HMODULE)0,szCommandTmp,MAX_PATH);

	TCHAR command[MAX_PATH];
	wsprintf(command, "\"%s\"",	szCommandTmp);	// Wrap	path & filename	in quotes &	null terminate

	TCHAR icon[MAX_PATH];
	wsprintf(icon,TEXT("%s,1"),(LPCTSTR)command);

	_tcscat(command,TEXT(" \"%1\""));			// Append "%1"
//	_tcscat(command,TEXT("-d1 %1\""));			// Append "%1"
//	sprintf(command, "\"%s\" \"-d1 %%1\"", szCommandTmp);	// Wrap	path & filename	in quotes &	null terminate

	// NB. Registry access to HKLM typically results in ErrorCode 5(ACCESS DENIED), as UAC requires elevated permissions (Run as administrator).
	// . HKEY_CLASSES_ROOT\CLSID is a merged view of HKLM\SOFTWARE\Classes and HKCU\SOFTWARE\Classes

	// NB. Reflect extensions in DELREG.INF
//	RegSetValue(HKEY_CLASSES_ROOT,".bin",REG_SZ,"DiskImage",0);	// Removed as .bin is too generic

	const char* pValueName = ".bin";
	LSTATUS res = RegDeleteValue(HKEY_CLASSES_ROOT, pValueName);
	if (res != NOERROR && res != ERROR_FILE_NOT_FOUND) LogFileOutput("RegDeleteValue(%s) failed (0x%08X)\n", pValueName, res);

	pValueName = ".do";
	res = RegSetValue(HKEY_CLASSES_ROOT, pValueName ,REG_SZ,"DiskImage",0);
	if (res != NOERROR) LogFileOutput("RegSetValue(%s) failed (0x%08X)\n", pValueName, res);

	pValueName = ".dsk";
	res = RegSetValue(HKEY_CLASSES_ROOT, pValueName, REG_SZ,"DiskImage",0);
	if (res != NOERROR) LogFileOutput("RegSetValue(%s) failed (0x%08X)\n", pValueName, res);

	pValueName = ".nib";
	res = RegSetValue(HKEY_CLASSES_ROOT, pValueName, REG_SZ,"DiskImage",0);
	if (res != NOERROR) LogFileOutput("RegSetValue(%s) failed (0x%08X)\n", pValueName, res);

	pValueName = ".po";
	res = RegSetValue(HKEY_CLASSES_ROOT, pValueName, REG_SZ,"DiskImage",0);
	if (res != NOERROR) LogFileOutput("RegSetValue(%s) failed (0x%08X)\n", pValueName, res);

	pValueName = ".woz";
	res = RegSetValue(HKEY_CLASSES_ROOT, pValueName, REG_SZ,"DiskImage",0);
	if (res != NOERROR) LogFileOutput("RegSetValue(%s) failed (0x%08X)\n", pValueName, res);

//	RegSetValue(HKEY_CLASSES_ROOT,".2mg",REG_SZ,"DiskImage",0);	// Don't grab this, as not all .2mg images are supported (so defer to CiderPress)
//	RegSetValue(HKEY_CLASSES_ROOT,".2img",REG_SZ,"DiskImage",0);	// Don't grab this, as not all .2mg images are supported (so defer to CiderPress)
//	RegSetValue(HKEY_CLASSES_ROOT,".aws.yaml",REG_SZ,"DiskImage",0);	// NB. Can't grab this extension (even though it returns 0!) with embedded period (and .yaml is too generic) - GH#548
//	RegSetValue(HKEY_CLASSES_ROOT,".hdv",REG_SZ,"DiskImage",0);	// TO DO

	pValueName = "DiskImage";
	res = RegSetValue(HKEY_CLASSES_ROOT,
				pValueName,
				REG_SZ,"Disk Image",0);
	if (res != NOERROR) LogFileOutput("RegSetValue(%s) failed (0x%08X)\n", pValueName, res);

	pValueName = "DiskImage\\DefaultIcon";
	res = RegSetValue(HKEY_CLASSES_ROOT,
				pValueName,
				REG_SZ,icon,0);
	if (res != NOERROR) LogFileOutput("RegSetValue(%s) failed (0x%08X)\n", pValueName, res);

// This key can interfere....
// HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Explorer\FileExt\.dsk

	pValueName = "DiskImage\\shell\\open\\command";
	res = RegSetValue(HKEY_CLASSES_ROOT,
				pValueName,
				REG_SZ,command,_tcslen(command)+1);
	if (res != NOERROR) LogFileOutput("RegSetValue(%s) failed (0x%08X)\n", pValueName, res);

	pValueName = "DiskImage\\shell\\open\\ddeexec";
	res = RegSetValue(HKEY_CLASSES_ROOT,
				pValueName,
				REG_SZ,"%1",3);
	if (res != NOERROR) LogFileOutput("RegSetValue(%s) failed (0x%08X)\n", pValueName, res);

	pValueName = "DiskImage\\shell\\open\\ddeexec\\application";
	res = RegSetValue(HKEY_CLASSES_ROOT,
				pValueName,
				REG_SZ,"applewin",_tcslen("applewin")+1);
//				REG_SZ,szCommandTmp,_tcslen(szCommandTmp)+1);
	if (res != NOERROR) LogFileOutput("RegSetValue(%s) failed (0x%08X)\n", pValueName, res);

	pValueName = "DiskImage\\shell\\open\\ddeexec\\topic";
	res = RegSetValue(HKEY_CLASSES_ROOT,
				pValueName,
				REG_SZ,"system",_tcslen("system")+1);
	if (res != NOERROR) LogFileOutput("RegSetValue(%s) failed (0x%08X)\n", pValueName, res);
}

//===========================================================================

// NB. On a restart, it's OK to call RegisterHotKey() again since the old g_hFrameWindow has been destroyed
static void RegisterHotKeys(void)
{
	BOOL bStatus[3] = {0,0,0};

	bStatus[0] = RegisterHotKey( 
		g_hFrameWindow , // HWND hWnd
		VK_SNAPSHOT_560, // int id (user/custom id)
		0              , // UINT fsModifiers
		VK_SNAPSHOT      // UINT vk = PrintScreen
	);

	bStatus[1] = RegisterHotKey( 
		g_hFrameWindow , // HWND hWnd
		VK_SNAPSHOT_280, // int id (user/custom id)
		MOD_SHIFT      , // UINT fsModifiers
		VK_SNAPSHOT      // UINT vk = PrintScreen
	);

	bStatus[2] = RegisterHotKey( 
		g_hFrameWindow  , // HWND hWnd
		VK_SNAPSHOT_TEXT, // int id (user/custom id)
		MOD_CONTROL     , // UINT fsModifiers
		VK_SNAPSHOT       // UINT vk = PrintScreen
	);

	if ((!bStatus[0] || !bStatus[1] || !bStatus[2]))
	{
		std::string msg("Unable to register for PrintScreen key(s):\n");

		if (!bStatus[0])
			msg += "\n. PrintScreen";
		if (!bStatus[1])
			msg += "\n. Shift+PrintScreen";
		if (!bStatus[2])
			msg += "\n. Ctrl+PrintScreen";

		if (g_bShowPrintScreenWarningDialog)
			MessageBox( g_hFrameWindow, msg.c_str(), "Warning", MB_ICONASTERISK | MB_OK );

		msg += "\n";
		LogFileOutput(msg.c_str());
	}
}

//---------------------------------------------------------------------------

static HINSTANCE g_hinstDLL = 0;
static HHOOK g_hhook = 0;

static HANDLE g_hHookThread = NULL;
static DWORD g_HookThreadId = 0;

// Pre: g_hFrameWindow must be valid
static bool HookFilterForKeyboard()
{
	g_hinstDLL = LoadLibrary(TEXT("HookFilter.dll"));

	_ASSERT(g_hFrameWindow);

	typedef void (*RegisterHWNDProc)(HWND, bool, bool);
	RegisterHWNDProc RegisterHWND = (RegisterHWNDProc) GetProcAddress(g_hinstDLL, "RegisterHWND");
	if (RegisterHWND)
		RegisterHWND(g_hFrameWindow, g_bHookAltTab, g_bHookAltGrControl);

	HOOKPROC hkprcLowLevelKeyboardProc = (HOOKPROC) GetProcAddress(g_hinstDLL, "LowLevelKeyboardProc");

	g_hhook = SetWindowsHookEx(
						WH_KEYBOARD_LL,
						hkprcLowLevelKeyboardProc,
						g_hinstDLL,
						0);

	if (g_hhook != 0 && g_hFrameWindow != 0)
		return true;

	std::string msg("Failed to install hook filter for system keys");

	DWORD dwErr = GetLastError();
	MessageBox(GetDesktopWindow(), msg.c_str(), "Warning", MB_ICONASTERISK | MB_OK);

	msg += "\n";
	LogFileOutput(msg.c_str());
	return false;
}

static void UnhookFilterForKeyboard()
{
	UnhookWindowsHookEx(g_hhook);
	FreeLibrary(g_hinstDLL);
}

static DWORD WINAPI HookThread(LPVOID lpParameter)
{
	if (!HookFilterForKeyboard())
		return -1;

	MSG msg;
	while(GetMessage(&msg, NULL, 0, 0) > 0)
	{
		if (msg.message == WM_QUIT)
			break;

		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	UnhookFilterForKeyboard();
	return 0;
}

static bool InitHookThread()
{
	g_hHookThread = CreateThread(NULL,			// lpThreadAttributes
								0,				// dwStackSize
								(LPTHREAD_START_ROUTINE) HookThread,
								0,				// lpParameter
								0,				// dwCreationFlags : 0 = Run immediately
								&g_HookThreadId);	// lpThreadId
	if (g_hHookThread == NULL)
		return false;

	return true;
}

static void UninitHookThread()
{
	if (g_hHookThread)
	{
		if (!PostThreadMessage(g_HookThreadId, WM_QUIT, 0, 0))
		{
			_ASSERT(0);
			return;
		}

		do
		{
			DWORD dwExitCode;
			if (GetExitCodeThread(g_hHookThread, &dwExitCode))
			{
				if(dwExitCode == STILL_ACTIVE)
					Sleep(10);
				else
					break;
			}
		}
		while(1);

		CloseHandle(g_hHookThread);
		g_hHookThread = NULL;
		g_HookThreadId = 0;
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
		// Rel pathname
		char szCWD[_MAX_PATH] = {0};
		if (!GetCurrentDirectory(sizeof(szCWD), szCWD))
			return "";

		strPathName = szCWD;
		strPathName.append("\\");
		strPathName.append(szFileName);
	}

	return strPathName;
}

static bool DoDiskInsert(const UINT slot, const int nDrive, LPCSTR szFileName)
{
	Disk2InterfaceCard& disk2Card = dynamic_cast<Disk2InterfaceCard&>(g_CardMgr.GetRef(slot));

	std::string strPathName = GetFullPath(szFileName);
	if (strPathName.empty()) return false;

	ImageError_e Error = disk2Card.InsertDisk(nDrive, strPathName.c_str(), IMAGE_USE_FILES_WRITE_PROTECT_STATUS, IMAGE_DONT_CREATE);
	return Error == eIMAGE_ERROR_NONE;
}

static bool DoHardDiskInsert(const int nDrive, LPCSTR szFileName)
{
	std::string strPathName = GetFullPath(szFileName);
	if (strPathName.empty()) return false;

	BOOL bRes = HD_Insert(nDrive, strPathName.c_str());
	return bRes ? true : false;
}

static void InsertFloppyDisks(const UINT slot, LPSTR szImageName_drive[NUM_DRIVES], bool& bBoot)
{
	_ASSERT(slot == 5 || slot == 6);

	if (!szImageName_drive[DRIVE_1] && !szImageName_drive[DRIVE_2])
		return;

	bool bRes = true;

	if (szImageName_drive[DRIVE_1])
	{
		bRes = DoDiskInsert(slot, DRIVE_1, szImageName_drive[DRIVE_1]);
		LogFileOutput("Init: S%d, DoDiskInsert(D1), res=%d\n", slot, bRes);
		FrameRefreshStatus(DRAW_LEDS | DRAW_BUTTON_DRIVES);	// floppy activity LEDs and floppy buttons
		bBoot = true;
	}

	if (szImageName_drive[DRIVE_2])
	{
		bRes |= DoDiskInsert(slot, DRIVE_2, szImageName_drive[DRIVE_2]);
		LogFileOutput("Init: S%d, DoDiskInsert(D2), res=%d\n", slot, bRes);
	}

	if (!bRes)
		MessageBox(g_hFrameWindow, "Failed to insert floppy disk(s) - see log file", "Warning", MB_ICONASTERISK | MB_OK);
}

static void InsertHardDisks(LPSTR szImageName_harddisk[NUM_HARDDISKS], bool& bBoot)
{
	if (!szImageName_harddisk[HARDDISK_1] && !szImageName_harddisk[HARDDISK_2])
		return;

	// Enable the Harddisk controller card

	HD_SetEnabled(true);

	DWORD dwTmp;
	if (REGLOAD(TEXT(REGVALUE_HDD_ENABLED), &dwTmp))
	{
		if (!dwTmp)
			REGSAVE(TEXT(REGVALUE_HDD_ENABLED), 1);	// Config: HDD Enabled
	}

	//

	bool bRes = true;

	if (szImageName_harddisk[HARDDISK_1])
	{
		bRes = DoHardDiskInsert(HARDDISK_1, szImageName_harddisk[HARDDISK_1]);
		LogFileOutput("Init: DoHardDiskInsert(HDD1), res=%d\n", bRes);
		FrameRefreshStatus(DRAW_LEDS);	// harddisk activity LED
		bBoot = true;
	}

	if (szImageName_harddisk[HARDDISK_2])
	{
		bRes |= DoHardDiskInsert(HARDDISK_2, szImageName_harddisk[HARDDISK_2]);
		LogFileOutput("Init: DoHardDiskInsert(HDD2), res=%d\n", bRes);
	}

	if (!bRes)
		MessageBox(g_hFrameWindow, "Failed to insert harddisk(s) - see log file", "Warning", MB_ICONASTERISK | MB_OK);
}

static void UnplugHardDiskControllerCard(void)
{
	HD_SetEnabled(false);

	DWORD dwTmp;
	if (REGLOAD(TEXT(REGVALUE_HDD_ENABLED), &dwTmp))
	{
		if (dwTmp)
			REGSAVE(TEXT(REGVALUE_HDD_ENABLED), 0);	// Config: HDD Disabled
	}
}

static bool CheckOldAppleWinVersion(void)
{
	TCHAR szOldAppleWinVersion[VERSIONSTRING_SIZE + 1];
	RegLoadString(TEXT(REG_CONFIG), TEXT(REGVALUE_VERSION), 1, szOldAppleWinVersion, VERSIONSTRING_SIZE, TEXT(""));
	const bool bShowAboutDlg = strcmp(szOldAppleWinVersion, VERSIONSTRING) != 0;

	// version: xx.yy.zz.ww
	char* p0 = szOldAppleWinVersion;
	int len = strlen(szOldAppleWinVersion);
	szOldAppleWinVersion[len] = '.';	// append a null terminator
	szOldAppleWinVersion[len + 1] = '\0';
	for (UINT i=0; i<4; i++)
	{
		char* p1 = strstr(p0, ".");
		if (!p1)
			break;
		*p1 = 0;
		g_OldAppleWinVersion[i] = atoi(p0);
		p0 = p1+1;
	}

	return bShowAboutDlg;
}

//---------------------------------------------------------------------------

static void ExceptionHandler(const char* pError)
{
		MessageBox(	g_hFrameWindow,
					pError,
					TEXT("Runtime Exception"),
					MB_ICONEXCLAMATION | MB_SETFOREGROUND);

		LogFileOutput("Runtime Exception: %s\n", pError);
}

//---------------------------------------------------------------------------

static bool ProcessCmdLine(LPSTR lpCmdLine);
static void GetAppleWinVersion(void);
static void OneTimeInitialization(HINSTANCE passinstance);
static void RepeatInitialization(void);
static void Shutdown(void);

struct CmdLine
{
	CmdLine()
	{
		bShutdown = false;
		bSetFullScreen = false;
		bBoot = false;
		bChangedDisplayResolution = false;
		bSlot0LanguageCard = false;
		bSlot7EmptyOnExit = false;
		bSwapButtons0and1 = false;
		bestWidth = 0;
		bestHeight = 0;
		szImageName_harddisk[HARDDISK_1] = NULL;
		szImageName_harddisk[HARDDISK_2] = NULL;
		szSnapshotName = NULL;
		szScreenshotFilename = NULL;
		uRamWorksExPages = 0;
		uSaturnBanks = 0;
		newVideoType = -1;
		newVideoStyleEnableMask = 0;
		newVideoStyleDisableMask = 0;
		newVideoRefreshRate = VR_NONE;
		clockMultiplier = 0.0;	// 0 => not set from cmd-line
		model = A2TYPE_MAX;

		for (UINT i = 0; i < NUM_SLOTS; i++)
		{
			bSlotEmpty[i] = false;
			slotInsert[i] = CT_Empty;
			szImageName_drive[i][DRIVE_1] = NULL;
			szImageName_drive[i][DRIVE_2] = NULL;
		}
	}

	bool bShutdown;
	bool bSetFullScreen;
	bool bBoot;
	bool bChangedDisplayResolution;
	bool bSlot0LanguageCard;
	bool bSlotEmpty[NUM_SLOTS];
	bool bSlot7EmptyOnExit;
	bool bSwapButtons0and1;
	SS_CARDTYPE slotInsert[NUM_SLOTS];
	UINT bestWidth;
	UINT bestHeight;
	LPSTR szImageName_drive[NUM_SLOTS][NUM_DRIVES];
	LPSTR szImageName_harddisk[NUM_HARDDISKS];
	LPSTR szSnapshotName;
	LPSTR szScreenshotFilename;
	UINT uRamWorksExPages;
	UINT uSaturnBanks;
	int newVideoType;
	int newVideoStyleEnableMask;
	int newVideoStyleDisableMask;
	VideoRefreshRate_e newVideoRefreshRate;
	double clockMultiplier;
	eApple2Type model;
};

static CmdLine g_cmdLine;

int APIENTRY WinMain(HINSTANCE passinstance, HINSTANCE, LPSTR lpCmdLine, int)
{
	if (!ProcessCmdLine(lpCmdLine))
		return 0;

	GetAppleWinVersion();
	OneTimeInitialization(passinstance);

	try
	{
		do
		{
			g_bRestart = false;

			RepeatInitialization();

			// ENTER THE MAIN MESSAGE LOOP
			LogFileOutput("Main: EnterMessageLoop()\n");
			EnterMessageLoop();
			LogFileOutput("Main: LeaveMessageLoop()\n");

			if (g_bRestart)
			{
				g_cmdLine.bSetFullScreen = g_bRestartFullScreen;
				g_bRestartFullScreen = false;
			}

			MB_Reset();
			LogFileOutput("Main: MB_Reset()\n");

			CMouseInterface* pMouseCard = g_CardMgr.GetMouseCard();
			if (pMouseCard)
			{
				pMouseCard->Reset();		// Deassert any pending IRQs - GH#514
				LogFileOutput("Main: CMouseInterface::Uninitialize()\n");
			}

			DSUninit();
			LogFileOutput("Main: DSUninit()\n");

			if (g_bHookSystemKey)
			{
				UninitHookThread();
				LogFileOutput("Main: UnhookFilterForKeyboard()\n");
			}
		}
		while (g_bRestart);
	}
	catch(std::runtime_error exception)
	{
		ExceptionHandler(exception.what());
	}
	catch(std::exception exception)
	{
		ExceptionHandler(exception.what());
	}

	Shutdown();
	return 0;
}

static bool ProcessCmdLine(LPSTR lpCmdLine)
{
	const std::string strCmdLine(lpCmdLine);		// Keep a copy for log ouput
	std::string strUnsupported;

	// If 1st param looks like an abs pathname then assume that an associated filetype has been double-clicked
	// NB. Handled by WM_DDE_INITIATE & WM_DDE_EXECUTE msgs
	if ((lpCmdLine[0] >= '\"' && lpCmdLine[1] >= 'A' && lpCmdLine[1] <= 'Z' && lpCmdLine[2] == ':')	// always in quotes
		|| strncmp("\\\\?\\", lpCmdLine, 4) == 0)
		return true;

	while (*lpCmdLine)
	{
		LPSTR lpNextArg = GetNextArg(lpCmdLine);

		if (((strcmp(lpCmdLine, "-l") == 0) || (strcmp(lpCmdLine, "-log") == 0)) && (g_fh == NULL))
		{
			LogInit();
		}
		else if (strcmp(lpCmdLine, "-noreg") == 0)
		{
			g_bRegisterFileTypes = false;
		}
		else if (strcmp(lpCmdLine, "-d1") == 0)
		{
			lpCmdLine = GetCurrArg(lpNextArg);
			lpNextArg = GetNextArg(lpNextArg);
			g_cmdLine.szImageName_drive[SLOT6][DRIVE_1] = lpCmdLine;
		}
		else if (strcmp(lpCmdLine, "-d2") == 0)
		{
			lpCmdLine = GetCurrArg(lpNextArg);
			lpNextArg = GetNextArg(lpNextArg);
			g_cmdLine.szImageName_drive[SLOT6][DRIVE_2] = lpCmdLine;
		}
		else if (strcmp(lpCmdLine, "-h1") == 0)
		{
			lpCmdLine = GetCurrArg(lpNextArg);
			lpNextArg = GetNextArg(lpNextArg);
			g_cmdLine.szImageName_harddisk[HARDDISK_1] = lpCmdLine;
		}
		else if (strcmp(lpCmdLine, "-h2") == 0)
		{
			lpCmdLine = GetCurrArg(lpNextArg);
			lpNextArg = GetNextArg(lpNextArg);
			g_cmdLine.szImageName_harddisk[HARDDISK_2] = lpCmdLine;
		}
		else if (lpCmdLine[0] == '-' && lpCmdLine[1] == 's' && lpCmdLine[2] >= '1' && lpCmdLine[2] <= '7')
		{
			const UINT slot = lpCmdLine[2] - '0';

			if (lpCmdLine[3] == 0)	// -s[1..7] <card>
			{
				lpCmdLine = GetCurrArg(lpNextArg);
				lpNextArg = GetNextArg(lpNextArg);
				if (strcmp(lpCmdLine, "empty") == 0)
					g_cmdLine.bSlotEmpty[slot] = true;
				if (strcmp(lpCmdLine, "diskii") == 0)
					g_cmdLine.slotInsert[slot] = CT_Disk2;
			}
			else if (lpCmdLine[3] == 'd' && (lpCmdLine[4] == '1' || lpCmdLine[4] == '2'))	// -s[1..7]d[1|2] <dsk-image>
			{
				const UINT drive = lpCmdLine[4] == '1' ? DRIVE_1 : DRIVE_2;

				if (slot != 5 && slot != 6)
				{
					LogFileOutput("Unsupported arg: %s\n", lpCmdLine);
				}
				else
				{
					lpCmdLine = GetCurrArg(lpNextArg);
					lpNextArg = GetNextArg(lpNextArg);
					g_cmdLine.szImageName_drive[slot][drive] = lpCmdLine;
				}
			}
			else
			{
				LogFileOutput("Unsupported arg: %s\n", lpCmdLine);
			}
		}
		else if (strcmp(lpCmdLine, "-s7-empty-on-exit") == 0)
		{
			g_cmdLine.bSlot7EmptyOnExit = true;
		}
		else if (strcmp(lpCmdLine, "-load-state") == 0)
		{
			lpCmdLine = GetCurrArg(lpNextArg);
			lpNextArg = GetNextArg(lpNextArg);
			g_cmdLine.szSnapshotName = lpCmdLine;
		}
		else if (strcmp(lpCmdLine, "-f") == 0)
		{
			g_cmdLine.bSetFullScreen = true;
		}
#define CMD_FS_HEIGHT "-fs-height="
		else if (strncmp(lpCmdLine, CMD_FS_HEIGHT, sizeof(CMD_FS_HEIGHT)-1) == 0)
		{
			g_cmdLine.bSetFullScreen = true;	// Implied

			LPSTR lpTmp = lpCmdLine + sizeof(CMD_FS_HEIGHT)-1;
			bool bRes = false;
			if (strcmp(lpTmp, "best") == 0)
			{
				bRes = GetBestDisplayResolutionForFullScreen(g_cmdLine.bestWidth, g_cmdLine.bestHeight);
			}
			else
			{
				UINT userSpecifiedHeight = atoi(lpTmp);
				if (userSpecifiedHeight)
					bRes = GetBestDisplayResolutionForFullScreen(g_cmdLine.bestWidth, g_cmdLine.bestHeight, userSpecifiedHeight);
				else
					LogFileOutput("Invalid cmd-line parameter for -fs-height=x switch\n");
			}
			if (bRes)
				LogFileOutput("Best resolution for -fs-height=x switch: Width=%d, Height=%d\n", g_cmdLine.bestWidth, g_cmdLine.bestHeight);
			else
				LogFileOutput("Failed to set parameter for -fs-height=x switch\n");
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
			g_cmdLine.uRamWorksExPages = atoi(lpCmdLine);
			if (g_cmdLine.uRamWorksExPages > kMaxExMemoryBanks)
				g_cmdLine.uRamWorksExPages = kMaxExMemoryBanks;
			else
			if (g_cmdLine.uRamWorksExPages < 1)
				g_cmdLine.uRamWorksExPages = 1;
		}
#endif
		else if (strcmp(lpCmdLine, "-s0") == 0)
		{
			lpCmdLine = GetCurrArg(lpNextArg);
			lpNextArg = GetNextArg(lpNextArg);

			if (strcmp(lpCmdLine, "saturn") == 0 || strcmp(lpCmdLine, "saturn128") == 0)
				g_cmdLine.uSaturnBanks = Saturn128K::kMaxSaturnBanks;
			else if (strcmp(lpCmdLine, "saturn64") == 0)
				g_cmdLine.uSaturnBanks = Saturn128K::kMaxSaturnBanks/2;
			else if (strcmp(lpCmdLine, "languagecard") == 0 || strcmp(lpCmdLine, "lc") == 0)
				g_cmdLine.bSlot0LanguageCard = true;
		}
		else if (strcmp(lpCmdLine, "-f8rom") == 0)		// Use custom 2K ROM at [$F800..$FFFF]
		{
			lpCmdLine = GetCurrArg(lpNextArg);
			lpNextArg = GetNextArg(lpNextArg);

			if (g_hCustomRomF8 != INVALID_HANDLE_VALUE)	// Stop resource leak if -f8rom is specified twice!
				CloseHandle(g_hCustomRomF8);

			g_hCustomRomF8 = CreateFile(lpCmdLine, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_READONLY, NULL);
			if ((g_hCustomRomF8 == INVALID_HANDLE_VALUE) || (GetFileSize(g_hCustomRomF8, NULL) != 0x800))
				g_bCustomRomF8Failed = true;
		}
		else if (strcmp(lpCmdLine, "-videorom") == 0)			// Use 2K (for II/II+). Use 4K,8K or 16K video ROM (for Enhanced //e)
		{
			lpCmdLine = GetCurrArg(lpNextArg);
			lpNextArg = GetNextArg(lpNextArg);

			if (!ReadVideoRomFile(lpCmdLine))
			{
				std::string msg = "Failed to load video rom (not found or not exactly 2/4/8/16KiB)\n";
				LogFileOutput("%s", msg.c_str());
				MessageBox(g_hFrameWindow, msg.c_str(), TEXT("AppleWin Error"), MB_OK);
			}
			else
			{
				SetVideoRomRockerSwitch(true);	// Use PAL char set
			}
		}
		else if (strcmp(lpCmdLine, "-printscreen") == 0)		// Turn on display of the last filename print screen was saved to
		{
			g_bDisplayPrintScreenFileName = true;
		}
		else if (strcmp(lpCmdLine, "-no-printscreen-key") == 0)		// Don't try to capture PrintScreen key GH#469
		{
			g_bCapturePrintScreenKey = false;
		}
		else if (strcmp(lpCmdLine, "-no-printscreen-dlg") == 0)		// Turn off the PrintScreen warning message dialog (if PrintScreen key can't be grabbed)
		{
			g_bShowPrintScreenWarningDialog = false;
		}
		else if (strcmp(lpCmdLine, "-no-hook-system-key") == 0)		// Don't hook the System keys (eg. Left-ALT+ESC/SPACE/TAB) GH#556
		{
			g_bHookSystemKey = false;
		}
		else if (strcmp(lpCmdLine, "-hook-alt-tab") == 0)			// GH#556
		{
			g_bHookAltTab = true;
		}
		else if (strcmp(lpCmdLine, "-hook-altgr-control") == 0)		// GH#556
		{
			g_bHookAltGrControl = true;
		}
		else if (strcmp(lpCmdLine, "-altgr-sends-wmchar") == 0)		// GH#625
		{
			KeybSetAltGrSendsWM_CHAR(true);
		}
		else if (strcmp(lpCmdLine, "-no-hook-alt") == 0)			// GH#583
		{
			JoySetHookAltKeys(false);
		}
		else if (strcmp(lpCmdLine, "-left-alt-control-buttons") == 0)	// GH#743
		{
			JoySetButtonVirtualKey(0, VK_CONTROL);
			JoySetButtonVirtualKey(1, VK_MENU);
		}
		else if (strcmp(lpCmdLine, "-right-alt-control-buttons") == 0)	// GH#743
		{
			JoySetButtonVirtualKey(0, VK_MENU | KF_EXTENDED);
			JoySetButtonVirtualKey(1, VK_CONTROL | KF_EXTENDED);
		}
		else if (strcmp(lpCmdLine, "-swap-buttons") == 0)
		{
			g_cmdLine.bSwapButtons0and1 = true;
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
		else if ((strcmp(lpCmdLine, "-dcd") == 0) || (strcmp(lpCmdLine, "-modem") == 0))	// GH#386
		{
			if (g_CardMgr.IsSSCInstalled())
				g_CardMgr.GetSSC()->SupportDCD(true);
		}
		else if (strcmp(lpCmdLine, "-alt-enter=toggle-full-screen") == 0)	// GH#556
		{
			SetAltEnterToggleFullScreen(true);
		}
		else if (strcmp(lpCmdLine, "-alt-enter=open-apple-enter") == 0)		// GH#556
		{
			SetAltEnterToggleFullScreen(false);
		}
		else if (strcmp(lpCmdLine, "-video-mode=rgb-monitor") == 0)			// GH#616
		{
			g_cmdLine.newVideoType = VT_COLOR_MONITOR_RGB;
		}
		else if (strcmp(lpCmdLine, "-video-style=vertical-blend") == 0)		// GH#616
		{
			g_cmdLine.newVideoStyleEnableMask = VS_COLOR_VERTICAL_BLEND;
		}
		else if (strcmp(lpCmdLine, "-video-style=no-vertical-blend") == 0)	// GH#616
		{
			g_cmdLine.newVideoStyleDisableMask = VS_COLOR_VERTICAL_BLEND;
		}
		else if (strcmp(lpCmdLine, "-rgb-card-invert-bit7") == 0)	// GH#633
		{
			RGB_SetInvertBit7(true);
		}
		else if (strcmp(lpCmdLine, "-screenshot-and-exit") == 0)	// GH#616: For testing - Use in combination with -load-state
		{
			g_cmdLine.szScreenshotFilename = GetCurrArg(lpNextArg);
			lpNextArg = GetNextArg(lpNextArg);
		}
		else if (strcmp(lpCmdLine, "-clock-multiplier") == 0)
		{
			lpCmdLine = GetCurrArg(lpNextArg);
			lpNextArg = GetNextArg(lpNextArg);
			g_cmdLine.clockMultiplier = atof(lpCmdLine);
		}
		else if (strcmp(lpCmdLine, "-model") == 0)
		{
			lpCmdLine = GetCurrArg(lpNextArg);
			lpNextArg = GetNextArg(lpNextArg);

			if (strcmp(lpCmdLine, "apple2") == 0)
				g_cmdLine.model = A2TYPE_APPLE2;
			else if (strcmp(lpCmdLine, "apple2p") == 0)
				g_cmdLine.model = A2TYPE_APPLE2PLUS;
			else if (strcmp(lpCmdLine, "apple2e") == 0)
				g_cmdLine.model = A2TYPE_APPLE2E;
			else if (strcmp(lpCmdLine, "apple2ee") == 0)
				g_cmdLine.model = A2TYPE_APPLE2EENHANCED;
			else
				LogFileOutput("-model: unsupported type: %s\n", lpCmdLine);
		}
		else if (_stricmp(lpCmdLine, "-50hz") == 0)	// (case-insensitive)
		{
			g_cmdLine.newVideoRefreshRate = VR_50HZ;
		}
		else if (_stricmp(lpCmdLine, "-60hz") == 0)	// (case-insensitive)
		{
			g_cmdLine.newVideoRefreshRate = VR_60HZ;
		}
		else	// unsupported
		{
			LogFileOutput("Unsupported arg: %s\n", lpCmdLine);
			strUnsupported += lpCmdLine;
			strUnsupported += "\n";
		}

		lpCmdLine = lpNextArg;
	}

	LogFileOutput("CmdLine: %s\n",  strCmdLine.c_str());

	bool ok = true;

	if (!strUnsupported.empty())
	{
		std::string msg("Unsupported commands:\n\n");
		msg += strUnsupported;
		msg += "\n";
		msg += "Continue running AppleWin?";
		int res = MessageBox(GetDesktopWindow(),		// NB. g_hFrameWindow is not yet valid
				msg.c_str(),
				"AppleWin Command Line",
				MB_ICONSTOP | MB_SETFOREGROUND | MB_YESNO);
		ok = (res != IDNO);
	}

	return ok;
}

static void GetAppleWinVersion(void)
{
    char szPath[_MAX_PATH];

    if (0 == GetModuleFileName(NULL, szPath, sizeof(szPath)))
        strcpy_s(szPath, sizeof(szPath), __argv[0]);

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

            unsigned long major     = g_AppleWinVersion[0] = pFixedFileInfo->dwFileVersionMS >> 16;
            unsigned long minor     = g_AppleWinVersion[1] = pFixedFileInfo->dwFileVersionMS & 0xffff;
            unsigned long fix       = g_AppleWinVersion[2] = pFixedFileInfo->dwFileVersionLS >> 16;
			unsigned long fix_minor = g_AppleWinVersion[3] = pFixedFileInfo->dwFileVersionLS & 0xffff;
			StringCbPrintf(VERSIONSTRING, VERSIONSTRING_SIZE, "%d.%d.%d.%d", major, minor, fix, fix_minor);
		}

		delete [] pVerInfoBlock;
    }

	LogFileOutput("AppleWin version: %s\n",  VERSIONSTRING);
}

// DO ONE-TIME INITIALIZATION
static void OneTimeInitialization(HINSTANCE passinstance)
{
#if 0
#ifdef RIFF_SPKR
	RiffInitWriteFile("Spkr.wav", SPKR_SAMPLE_RATE, 1);
#endif
#ifdef RIFF_MB
	RiffInitWriteFile("Mockingboard.wav", 44100, 2);
#endif
#endif

	// Initialize COM - so we can use CoCreateInstance
	// . NB. DSInit() & DIMouse::DirectInputInit are done when g_hFrameWindow is created (WM_CREATE)
	HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
	LogFileOutput("Init: CoInitializeEx(), hr=0x%08X\n", hr);

	g_bSysClkOK = SysClk_InitTimer();
	LogFileOutput("Init: SysClk_InitTimer(), res=%d\n", g_bSysClkOK ? 1:0);
#ifdef USE_SPEECH_API
	if (g_bEnableSpeech)
	{
		const bool bSpeechOK = g_Speech.Init();
		LogFileOutput("Init: SysClk_InitTimer(), res=%d\n", bSpeechOK ? 1:0);
	}
#endif
#if 0
	DDInit();	// For WaitForVerticalBlank()
#endif

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
}

// DO INITIALIZATION THAT MUST BE REPEATED FOR A RESTART
static void RepeatInitialization(void)
{
		ResetToLogoMode();

		// NB. g_OldAppleWinVersion needed by LoadConfiguration() -> Config_Load_Video()
		const bool bShowAboutDlg = CheckOldAppleWinVersion();	// Post: g_OldAppleWinVersion

		LoadConfiguration();
		LogFileOutput("Main: LoadConfiguration()\n");

		if (g_cmdLine.model != A2TYPE_MAX)
			SetApple2Type(g_cmdLine.model);

		if (g_cmdLine.newVideoType >= 0)
		{
			SetVideoType( (VideoType_e)g_cmdLine.newVideoType );
			g_cmdLine.newVideoType = -1;	// Don't reapply after a restart
		}
		SetVideoStyle( (VideoStyle_e) ((GetVideoStyle() | g_cmdLine.newVideoStyleEnableMask) & ~g_cmdLine.newVideoStyleDisableMask) );

		if (g_cmdLine.newVideoRefreshRate != VR_NONE)
		{
			SetVideoRefreshRate(g_cmdLine.newVideoRefreshRate);
			g_cmdLine.newVideoRefreshRate = VR_NONE;	// Don't reapply after a restart
			SetCurrentCLK6502();
		}

		UseClockMultiplier(g_cmdLine.clockMultiplier);
		g_cmdLine.clockMultiplier = 0.0;

		// Apply the memory expansion switches after loading the Apple II machine type
#ifdef RAMWORKS
		if (g_cmdLine.uRamWorksExPages)
		{
			SetRamWorksMemorySize(g_cmdLine.uRamWorksExPages);
			SetExpansionMemType(CT_RamWorksIII);
			g_cmdLine.uRamWorksExPages = 0;	// Don't reapply after a restart
		}
#endif
		if (g_cmdLine.uSaturnBanks)
		{
			SetSaturnMemorySize(g_cmdLine.uSaturnBanks);	// Set number of banks before constructing Saturn card
			SetExpansionMemType(CT_Saturn128K);
			g_cmdLine.uSaturnBanks = 0;		// Don't reapply after a restart
		}

		if (g_cmdLine.bSlot0LanguageCard)
		{
			SetExpansionMemType(CT_LanguageCard);
			g_cmdLine.bSlot0LanguageCard = false;	// Don't reapply after a restart
		}

		if (g_cmdLine.bSwapButtons0and1)
		{
			sg_PropertySheet.SetButtonsSwapState(true);
			// Reapply after a restart - TODO: grey-out the Config UI for "Swap 0/1" when this cmd line is passed in
		}

		DebugInitialize();
		LogFileOutput("Main: DebugInitialize()\n");

		JoyInitialize();
		LogFileOutput("Main: JoyInitialize()\n");

		VideoInitialize(); // g_pFramebufferinfo been created now
		LogFileOutput("Main: VideoInitialize()\n");

		LogFileOutput("Main: FrameCreateWindow() - pre\n");
		FrameCreateWindow();	// g_hFrameWindow is now valid
		LogFileOutput("Main: FrameCreateWindow() - post\n");

		// Allow the 4 hardcoded slots to be configurated as empty
		if (g_cmdLine.bSlotEmpty[SLOT1])
			g_CardMgr.Remove(SLOT1);
		if (g_cmdLine.bSlotEmpty[SLOT2])
			g_CardMgr.Remove(SLOT2);
		if (g_cmdLine.bSlotEmpty[SLOT3])
			g_CardMgr.Remove(SLOT3);
		if (g_cmdLine.bSlotEmpty[SLOT6])
			g_CardMgr.Remove(SLOT6);

		if (g_cmdLine.slotInsert[SLOT5] != CT_Empty)
		{
			if (g_CardMgr.QuerySlot(SLOT4) == CT_MockingboardC && g_cmdLine.slotInsert[SLOT5] != CT_MockingboardC)	// Currently MB occupies slot4+5 when enabled
			{
				g_CardMgr.Remove(SLOT4);
				g_CardMgr.Remove(SLOT5);
			}

			g_CardMgr.Insert(SLOT5, g_cmdLine.slotInsert[SLOT5]);
		}

		// Pre: may need g_hFrameWindow for MessageBox errors
		// Post: may enable HDD, required for MemInitialize()->MemInitializeIO()
		{
			bool temp = false;
			InsertFloppyDisks(SLOT5, g_cmdLine.szImageName_drive[SLOT5], temp);
			//g_cmdLine.szImageName_drive[SLOT5][DRIVE_1] = g_cmdLine.szImageName_drive[SLOT5][DRIVE_2] = NULL;	// *Do* insert on a restart (since no way they could have changed)

			InsertFloppyDisks(SLOT6, g_cmdLine.szImageName_drive[SLOT6], g_cmdLine.bBoot);
			g_cmdLine.szImageName_drive[SLOT6][DRIVE_1] = g_cmdLine.szImageName_drive[SLOT6][DRIVE_2] = NULL;	// Don't insert on a restart

			InsertHardDisks(g_cmdLine.szImageName_harddisk, g_cmdLine.bBoot);
			g_cmdLine.szImageName_harddisk[HARDDISK_1] = g_cmdLine.szImageName_harddisk[HARDDISK_2] = NULL;	// Don't insert on a restart

			if (g_cmdLine.bSlotEmpty[7])
				HD_SetEnabled(false);
		}

		MemInitialize();
		LogFileOutput("Main: MemInitialize()\n");

		// Show About dialog after creating main window (need g_hFrameWindow)
		if (bShowAboutDlg)
		{
			if (!AboutDlg())
				g_cmdLine.bShutdown = true;															// Close everything down
			else
				RegSaveString(TEXT(REG_CONFIG), TEXT(REGVALUE_VERSION), 1, VERSIONSTRING);	// Only save version after user accepts license
		}

		if (g_bCapturePrintScreenKey)
		{
			RegisterHotKeys();		// needs valid g_hFrameWindow
			LogFileOutput("Main: RegisterHotKeys()\n");
		}

		if (g_bHookSystemKey)
		{
			if (InitHookThread())	// needs valid g_hFrameWindow (for message pump)
				LogFileOutput("Main: HookFilterForKeyboard()\n");
		}

		// Need to test if it's safe to call ResetMachineState(). In the meantime, just call Disk2Card's Reset():
		g_CardMgr.GetDisk2CardMgr().Reset(true);	// Switch from a booting A][+ to a non-autostart A][, so need to turn off floppy motor
		LogFileOutput("Main: DiskReset()\n");
		HD_Reset();		// GH#515
		LogFileOutput("Main: HDDReset()\n");

		if (!g_bSysClkOK)
		{
			MessageBox(g_hFrameWindow, "DirectX failed to create SystemClock instance", TEXT("AppleWin Error"), MB_OK);
			g_cmdLine.bShutdown = true;
		}

		if (g_bCustomRomF8Failed)
		{
			std::string msg = "Failed to load custom F8 rom (not found or not exactly 2KiB)\n";
			LogFileOutput("%s", msg.c_str());
			MessageBox(g_hFrameWindow, msg.c_str(), TEXT("AppleWin Error"), MB_OK);
			g_cmdLine.bShutdown = true;
		}

		tfe_init();
		LogFileOutput("Main: tfe_init()\n");

		if (g_cmdLine.szSnapshotName)
		{
			std::string strPathname(g_cmdLine.szSnapshotName);
			int nIdx = strPathname.find_last_of('\\');
			if (nIdx >= 0 && nIdx+1 < (int)strPathname.length())
			{
				const std::string strPath = strPathname.substr(0, nIdx+1);
				SetCurrentImageDir(strPath);
			}

			// Override value just loaded from Registry by LoadConfiguration()
			// . NB. Registry value is not updated with this cmd-line value
			Snapshot_SetFilename(g_cmdLine.szSnapshotName);
			Snapshot_LoadState();
			g_cmdLine.bBoot = true;
#if _DEBUG && 0	// Debug/test: Save a duplicate of the save-state file in tmp folder
			std::string saveName = std::string("tmp\\") + std::string(szSnapshotName); 
			Snapshot_SetFilename(saveName);
			g_bSaveStateOnExit = true;
			bShutdown = true;
#endif
			g_cmdLine.szSnapshotName = NULL;
		}
		else
		{
			Snapshot_Startup();		// Do this after everything has been init'ed
			LogFileOutput("Main: Snapshot_Startup()\n");
		}

		if (g_cmdLine.szScreenshotFilename)
		{
			Video_RedrawAndTakeScreenShot(g_cmdLine.szScreenshotFilename);
			g_cmdLine.bShutdown = true;
		}

		if (g_cmdLine.bShutdown)
		{
			PostMessage(g_hFrameWindow, WM_DESTROY, 0, 0);	// Close everything down
			// NB. If shutting down, then don't post any other messages (GH#286)
		}
		else
		{
			if (g_cmdLine.bSetFullScreen)
			{
				if (g_cmdLine.bestWidth && g_cmdLine.bestHeight)
				{
					DEVMODE devMode;
					memset(&devMode, 0, sizeof(devMode));
					devMode.dmSize = sizeof(devMode);
					devMode.dmPelsWidth = g_cmdLine.bestWidth;
					devMode.dmPelsHeight = g_cmdLine.bestHeight;
					devMode.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT;

					DWORD dwFlags = 0;
					LONG res = ChangeDisplaySettings(&devMode, dwFlags);
					if (res == 0)
						g_cmdLine.bChangedDisplayResolution = true;
				}

				PostMessage(g_hFrameWindow, WM_USER_FULLSCREEN, 0, 0);
				g_cmdLine.bSetFullScreen = false;
			}

			if (g_cmdLine.bBoot)
			{
				PostMessage(g_hFrameWindow, WM_USER_BOOT, 0, 0);
				g_cmdLine.bBoot = false;
			}
		}

		SetMouseCardInstalled( g_CardMgr.IsMouseCardInstalled() );
}

static void Shutdown(void)
{
	if (g_cmdLine.bChangedDisplayResolution)
		ChangeDisplaySettings(NULL, 0);	// restore default

	// Release COM
	DDUninit();
	SysClk_UninitTimer();
	LogFileOutput("Exit: SysClk_UninitTimer()\n");

	CoUninitialize();
	LogFileOutput("Exit: CoUninitialize()\n");

	tfe_shutdown();
	LogFileOutput("Exit: tfe_shutdown()\n");

	LogDone();

	RiffFinishWriteFile();

	if (g_hCustomRomF8 != INVALID_HANDLE_VALUE)
		CloseHandle(g_hCustomRomF8);

	if (g_cmdLine.bSlot7EmptyOnExit)
		UnplugHardDiskControllerCard();
}
