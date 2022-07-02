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

#include "Windows/AppleWin.h"
#include "Windows/HookFilter.h"
#include "Interface.h"
#include "Utilities.h"
#include "CmdLine.h"
#include "Debug.h"
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
#include "LanguageCard.h"
#include "CardManager.h"
#ifdef USE_SPEECH_API
#include "Speech.h"
#endif
#include "Windows/Win32Frame.h"
#include "RGBMonitor.h"
#include "NTSC.h"

#include "Configuration/About.h"
#include "Configuration/PropertySheet.h"

//=================================================

static bool g_bLoadedSaveState = false;
static bool g_bSysClkOK = false;

bool g_bRestartFullScreen = false;

//===========================================================================

bool GetLoadedSaveStateFlag(void)
{
	return g_bLoadedSaveState;
}

void Win32Frame::SetLoadedSaveStateFlag(const bool bFlag)
{
	g_bLoadedSaveState = bFlag;
}

bool GetHookAltTab(void)
{
	return g_bHookAltTab;
}

bool GetHookAltGrControl(void)
{
	return g_bHookAltGrControl;
}

static void ResetToLogoMode(void)
{
	g_nAppMode = MODE_LOGO;
	GetFrame().SetLoadedSaveStateFlag(false);
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
#ifdef LOG_PERF_TIMINGS
	PerfMarker* pPerfMarkerTotal = new PerfMarker(g_timeTotal);
#endif

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
	if (GetPropertySheet().GetScrollLockToggle())
	{
		bScrollLock_FullSpeed = Win32Frame::GetWin32Frame().g_bScrollLock_FullSpeed;
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
					 (GetCardMgr().GetDisk2CardMgr().IsConditionForFullSpeed() && !Spkr_IsActive() && !MB_IsActive()) ||
					 IsDebugSteppingAtFullSpeed();

	if (g_bFullSpeed)
	{
		if (!bWasFullSpeed)
			GetFrame().VideoRedrawScreenDuringFullSpeed(0, true);	// Init for full-speed mode

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
			GetFrame().VideoRedrawScreenAfterFullSpeed(g_dwCyclesThisFrame);

		// Don't call Spkr_Unmute()
		MB_Unmute();
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

	GetCardMgr().Update(uActualCyclesExecuted);

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
	if (g_dwCyclesThisFrame >= dwClksPerFrame && !GetVideo().VideoGetVblBarEx(g_dwCyclesThisFrame))
	{
#ifdef LOG_PERF_TIMINGS
		PerfMarker perfMarkerVideoRefresh(g_timeVideoRefresh);
#endif
		g_dwCyclesThisFrame -= dwClksPerFrame;

		if (g_bFullSpeed)
			GetFrame().VideoRedrawScreenDuringFullSpeed(g_dwCyclesThisFrame);
		else
			GetFrame().VideoPresentScreen(); // Just copy the output of our Apple framebuffer to the system Back Buffer
	}

#ifdef LOG_PERF_TIMINGS
	delete pPerfMarkerTotal;	// Explicitly call dtor *before* SysClk_WaitTimer()
#endif

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

static void GetProgramDirectory(void)
{
	TCHAR programDir[MAX_PATH];
	GetModuleFileName((HINSTANCE)0, programDir, MAX_PATH);
	programDir[MAX_PATH-1] = 0;

	g_sProgramDir = programDir;

	int loop = g_sProgramDir.size();
	while (loop--)
	{
		if ((g_sProgramDir[loop] == TEXT(PATH_SEPARATOR)) || (g_sProgramDir[loop] == TEXT(':')))
		{
			g_sProgramDir.resize(loop + 1);  // this reduces the size
			break;
		}
	}
}

//===========================================================================

void RegisterExtensions(void)
{
	char szModuleFileName[MAX_PATH];
	GetModuleFileName(static_cast<HMODULE>(NULL), szModuleFileName, sizeof(szModuleFileName));

	// Wrap	path & filename	in quotes &	null terminate
	std::string command = std::string("\"") + szModuleFileName + '"';

	std::string const icon = command + ",1";

	command += " \"%1\"";			// Append ' "%1"'
//	command += " -d1 \"%1\"";		// Append ' -d1 "%1"'
//	command += " \"-d1 %1\"";		// Append ' "-d1 %1"'

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
				REG_SZ, "Disk Image", 0);
	if (res != NOERROR) LogFileOutput("RegSetValue(%s) failed (0x%08X)\n", pValueName, res);

	pValueName = "DiskImage\\DefaultIcon";
	res = RegSetValue(HKEY_CLASSES_ROOT,
				pValueName,
				REG_SZ, icon.c_str(), 0);
	if (res != NOERROR) LogFileOutput("RegSetValue(%s) failed (0x%08X)\n", pValueName, res);

// This key can interfere....
// HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Explorer\FileExt\.dsk

	pValueName = "DiskImage\\shell\\open\\command";
	res = RegSetValue(HKEY_CLASSES_ROOT,
				pValueName,
				REG_SZ, command.c_str(), command.length() + 1);
	if (res != NOERROR) LogFileOutput("RegSetValue(%s) failed (0x%08X)\n", pValueName, res);

	pValueName = "DiskImage\\shell\\open\\ddeexec";
	res = RegSetValue(HKEY_CLASSES_ROOT,
				pValueName,
				REG_SZ, "%1", 3);
	if (res != NOERROR) LogFileOutput("RegSetValue(%s) failed (0x%08X)\n", pValueName, res);

	pValueName = "DiskImage\\shell\\open\\ddeexec\\application";
	res = RegSetValue(HKEY_CLASSES_ROOT,
				pValueName,
				REG_SZ, "applewin", strlen("applewin") + 1);
//				REG_SZ, szModuleFileName, strlen(szModuleFileName)+1);
	if (res != NOERROR) LogFileOutput("RegSetValue(%s) failed (0x%08X)\n", pValueName, res);

	pValueName = "DiskImage\\shell\\open\\ddeexec\\topic";
	res = RegSetValue(HKEY_CLASSES_ROOT,
				pValueName,
				REG_SZ, "system", strlen("system") + 1);
	if (res != NOERROR) LogFileOutput("RegSetValue(%s) failed (0x%08X)\n", pValueName, res);
}

//===========================================================================

// NB. On a restart, it's OK to call RegisterHotKey() again since the old g_hFrameWindow has been destroyed
static void RegisterHotKeys(void)
{
	BOOL bStatus[3] = {0,0,0};

	bStatus[0] = RegisterHotKey( 
		GetFrame().g_hFrameWindow , // HWND hWnd
		VK_SNAPSHOT_560, // int id (user/custom id)
		0              , // UINT fsModifiers
		VK_SNAPSHOT      // UINT vk = PrintScreen
	);

	bStatus[1] = RegisterHotKey( 
		GetFrame().g_hFrameWindow , // HWND hWnd
		VK_SNAPSHOT_280, // int id (user/custom id)
		MOD_SHIFT      , // UINT fsModifiers
		VK_SNAPSHOT      // UINT vk = PrintScreen
	);

	bStatus[2] = RegisterHotKey( 
		GetFrame().g_hFrameWindow  , // HWND hWnd
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

		if (GetFrame().GetShowPrintScreenWarningDialog())
			SHMessageBoxCheck( GetFrame().g_hFrameWindow, msg.c_str(), "Warning", MB_ICONASTERISK | MB_OK, MB_OK, "AppleWin-75097740-8e59-444c-bc94-2d4915132599" );

		msg += "\n";
		LogFileOutput(msg.c_str());
	}
}

//---------------------------------------------------------------------------

static void ExceptionHandler(const char* pError)
{
	GetFrame().FrameMessageBox(
				pError,
				TEXT("Runtime Exception"),
				MB_ICONEXCLAMATION | MB_SETFOREGROUND);

	LogFileOutput("Runtime Exception: %s\n", pError);
}

//---------------------------------------------------------------------------

static void GetAppleWinVersion(void);
static void OneTimeInitialization(HINSTANCE passinstance);
static void RepeatInitialization(void);
static void Shutdown(void);

int APIENTRY WinMain(HINSTANCE passinstance, HINSTANCE, LPSTR lpCmdLine, int)
{
	char startDir[_MAX_PATH];
	GetCurrentDirectory(sizeof(startDir), startDir);
	g_sStartDir = startDir;
	if (*(g_sStartDir.end()-1) != PATH_SEPARATOR) g_sStartDir += PATH_SEPARATOR;

	if (!ProcessCmdLine(lpCmdLine))
		return 0;

	LogFileOutput("g_sStartDir = %s\n", g_sStartDir.c_str());
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
				g_cmdLine.setFullScreen = g_bRestartFullScreen ? 1 : 0;
				g_bRestartFullScreen = false;

				CMouseInterface* pMouseCard = GetCardMgr().GetMouseCard();
				if (pMouseCard)
				{
					// dtor removes event from g_SynchronousEventMgr - do before g_SynchronousEventMgr.Reset()
					GetCardMgr().Remove( pMouseCard->GetSlot(), false );
					LogFileOutput("Main: CMouseInterface::dtor\n");
				}

				_ASSERT(g_SynchronousEventMgr.GetHead() == NULL);
				g_SynchronousEventMgr.Reset();
			}

			DSUninit();
			LogFileOutput("Main: DSUninit()\n");

			if (g_bHookSystemKey)
			{
				GetHookFilter().UninitHookThread();
				LogFileOutput("Main: UnhookFilterForKeyboard()\n");
			}
		}
		while (g_bRestart);

		Shutdown();
	}
	catch (const std::exception& exception)
	{
		ExceptionHandler(exception.what());
	}

	return 0;
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

            UINT16 major     = pFixedFileInfo->dwFileVersionMS >> 16;
			UINT16 minor     = pFixedFileInfo->dwFileVersionMS & 0xffff;
			UINT16 fix       = pFixedFileInfo->dwFileVersionLS >> 16;
			UINT16 fix_minor = pFixedFileInfo->dwFileVersionLS & 0xffff;
			SetAppleWinVersion(major, minor, fix, fix_minor);
		}

		delete [] pVerInfoBlock;
    }

	LogFileOutput("AppleWin version: %s\n",  g_VERSIONSTRING.c_str());
}

// DO ONE-TIME INITIALIZATION
static void OneTimeInitialization(HINSTANCE passinstance)
{
	// Currently only support one RIFF file
	if (!g_cmdLine.wavFileSpeaker.empty())
	{
		if (RiffInitWriteFile(g_cmdLine.wavFileSpeaker.c_str(), SPKR_SAMPLE_RATE, 1))
			Spkr_OutputToRiff();
	}
	else if (!g_cmdLine.wavFileMockingboard.empty())
	{
		if (RiffInitWriteFile(g_cmdLine.wavFileMockingboard.c_str(), 44100, 2))
			MB_OutputToRiff();
	}

	// Initialize COM - so we can use CoCreateInstance
	// . DSInit() & DIMouse::DirectInputInit are done when g_hFrameWindow is created (WM_CREATE)
	// . DDInit() is done in RepeatInitialization() by GetVideo().Initialize()
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

	GetFrame().g_hInstance = passinstance;
	GdiSetBatchLimit(512);
	LogFileOutput("Init: GdiSetBatchLimit()\n");

	GetProgramDirectory();
	LogFileOutput("Init: GetProgramDirectory()\n");

	if (g_bRegisterFileTypes)
	{
		RegisterExtensions();
		LogFileOutput("Init: RegisterExtensions()\n");
	}

	Win32Frame::GetWin32Frame().FrameRegisterClass();
	LogFileOutput("Init: FrameRegisterClass()\n");
}

// DO INITIALIZATION THAT MUST BE REPEATED FOR A RESTART
static void RepeatInitialization(void)
{
		GetVideo().SetVidHD(false);	// Set true later only if VidHDCard is instantiated
		ResetToLogoMode();

		// NB. g_OldAppleWinVersion needed by LoadConfiguration() -> Config_Load_Video()
		const bool bShowAboutDlg = CheckOldAppleWinVersion();	// Post: g_OldAppleWinVersion

		// Load configuration from Registry
		{
			bool loadImages = g_cmdLine.szSnapshotName == NULL;	// don't load floppy/harddisk images if a snapshot is to be loaded later on
			LoadConfiguration(loadImages);
			LogFileOutput("Main: LoadConfiguration()\n");
		}

		if (g_cmdLine.model != A2TYPE_MAX)
			SetApple2Type(g_cmdLine.model);

		RGB_SetVideocard(g_cmdLine.rgbCard, g_cmdLine.rgbCardForegroundColor, g_cmdLine.rgbCardBackgroundColor);

		if (g_cmdLine.newVideoType >= 0)
		{
			GetVideo().SetVideoType( (VideoType_e)g_cmdLine.newVideoType );
			g_cmdLine.newVideoType = -1;	// Don't reapply after a restart
		}
		GetVideo().SetVideoStyle( (VideoStyle_e) ((GetVideo().GetVideoStyle() | g_cmdLine.newVideoStyleEnableMask) & ~g_cmdLine.newVideoStyleDisableMask) );

		if (g_cmdLine.newVideoRefreshRate != VR_NONE)
		{
			GetVideo().SetVideoRefreshRate(g_cmdLine.newVideoRefreshRate);
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
			Saturn128K::SetSaturnMemorySize(g_cmdLine.uSaturnBanks);	// Set number of banks before constructing Saturn card
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
			GetPropertySheet().SetButtonsSwapState(true);
			// Reapply after a restart - TODO: grey-out the Config UI for "Swap 0/1" when this cmd line is passed in
		}

		JoyInitialize();
		LogFileOutput("Main: JoyInitialize()\n");

		// Init palette color
		VideoSwitchVideocardPalette(RGB_GetVideocard(), GetVideo().GetVideoType());

		// Allow the 4 hardcoded slots to be configurated as empty
		// NB. this state *is* persisted to the Registry/conf.ini (just like '-s7 empty' is)
		// TODO: support bSlotEmpty[] for slots: 0,4,5
		if (g_cmdLine.bSlotEmpty[SLOT1])
			GetCardMgr().Remove(SLOT1);
		if (g_cmdLine.bSlotEmpty[SLOT2])
			GetCardMgr().Remove(SLOT2);
		if (g_cmdLine.bSlotEmpty[SLOT3])
			GetCardMgr().Remove(SLOT3);
		if (g_cmdLine.bSlotEmpty[SLOT6])
			GetCardMgr().Remove(SLOT6);

		if (g_cmdLine.supportDCD && GetCardMgr().IsSSCInstalled())
		{
			GetCardMgr().GetSSC()->SupportDCD(true);
		}

		if (g_cmdLine.slotInsert[SLOT1] != CT_Empty && g_cmdLine.slotInsert[SLOT1] == CT_GenericPrinter)	// For now just support Printer card in slot 1
		{
			GetCardMgr().Insert(SLOT1, g_cmdLine.slotInsert[SLOT1]);
		}

		if (g_cmdLine.slotInsert[SLOT2] != CT_Empty && g_cmdLine.slotInsert[SLOT2] == CT_SSC)	// For now just support SSC in slot 2
		{
			GetCardMgr().Insert(SLOT2, g_cmdLine.slotInsert[SLOT2]);
		}

		if (g_cmdLine.enableDumpToRealPrinter && GetCardMgr().IsParallelPrinterCardInstalled())
		{
			GetCardMgr().GetParallelPrinterCard()->SetEnableDumpToRealPrinter(true);
		}

		if (g_cmdLine.slotInsert[SLOT3] != CT_Empty && g_cmdLine.slotInsert[SLOT3] == CT_VidHD)	// For now just support VidHD in slot 3
		{
			GetCardMgr().Insert(SLOT3, g_cmdLine.slotInsert[SLOT3]);
		}

		if (g_cmdLine.slotInsert[SLOT5] != CT_Empty)
		{
			if (GetCardMgr().QuerySlot(SLOT4) == CT_MockingboardC && g_cmdLine.slotInsert[SLOT5] != CT_MockingboardC)	// Currently MB occupies slot4+5 when enabled
			{
				GetCardMgr().Remove(SLOT4);
				GetCardMgr().Remove(SLOT5);
			}

			GetCardMgr().Insert(SLOT5, g_cmdLine.slotInsert[SLOT5]);
		}

		// Create window after inserting/removing VidHD card (as it affects width & height)
		{
			Win32Frame::GetWin32Frame().SetViewportScale(Win32Frame::GetWin32Frame().GetViewportScale(), true);

			GetFrame().Initialize(true); // g_pFramebufferinfo been created now & COM init'ed
			LogFileOutput("Main: VideoInitialize()\n");

			LogFileOutput("Main: FrameCreateWindow() - pre\n");
			Win32Frame::GetWin32Frame().FrameCreateWindow();	// GetFrame().g_hFrameWindow is now valid
			LogFileOutput("Main: FrameCreateWindow() - post\n");
		}

		// Set best W,H resolution after inserting/removing VidHD card
		if (g_cmdLine.bestFullScreenResolution || g_cmdLine.userSpecifiedWidth || g_cmdLine.userSpecifiedHeight)
		{
			bool res = false;
			UINT bestWidth = 0, bestHeight = 0;

			if (g_cmdLine.bestFullScreenResolution)
				res = GetFrame().GetBestDisplayResolutionForFullScreen(bestWidth, bestHeight);
			else
				res = GetFrame().GetBestDisplayResolutionForFullScreen(bestWidth, bestHeight, g_cmdLine.userSpecifiedWidth, g_cmdLine.userSpecifiedHeight);

			if (res)
				LogFileOutput("Best resolution for -fs-width/height=x switch(es): Width=%d, Height=%d\n", bestWidth, bestHeight);
			else
				LogFileOutput("Failed to set parameter for -fs-width/height=x switch(es)\n");
		}

		// Pre: may need g_hFrameWindow for MessageBox errors
		// Post: may enable HDD, required for MemInitialize()->MemInitializeIO()
		{
			bool temp = false;
			InsertFloppyDisks(SLOT5, g_cmdLine.szImageName_drive[SLOT5], g_cmdLine.driveConnected[SLOT5], temp);
			g_cmdLine.szImageName_drive[SLOT5][DRIVE_1] = g_cmdLine.szImageName_drive[SLOT5][DRIVE_2] = NULL;	// Don't insert on a restart

			InsertFloppyDisks(SLOT6, g_cmdLine.szImageName_drive[SLOT6], g_cmdLine.driveConnected[SLOT6], g_cmdLine.bBoot);
			g_cmdLine.szImageName_drive[SLOT6][DRIVE_1] = g_cmdLine.szImageName_drive[SLOT6][DRIVE_2] = NULL;	// Don't insert on a restart

			InsertHardDisks(g_cmdLine.szImageName_harddisk, g_cmdLine.bBoot);
			g_cmdLine.szImageName_harddisk[HARDDISK_1] = g_cmdLine.szImageName_harddisk[HARDDISK_2] = NULL;	// Don't insert on a restart

			if (g_cmdLine.bSlotEmpty[SLOT7])
			{
				GetCardMgr().Remove(SLOT7);	// Disable HDD controller, and persist this to Registry/conf.ini (consistent with other '-sn empty' cmds)
				Snapshot_UpdatePath();		// If save-state's filename is a harddisk, and the floppy is in the same path, then the filename won't be updated
			}
		}

		// Set *after* InsertFloppyDisks() & InsertHardDisks(), which both update g_sCurrentDir
		if (!g_cmdLine.strCurrentDir.empty())
			SetCurrentImageDir(g_cmdLine.strCurrentDir);

		if (g_cmdLine.bRemoveNoSlotClock)
			MemRemoveNoSlotClock();

		if (g_cmdLine.noDisk2StepperDefer)
			GetCardMgr().GetDisk2CardMgr().SetStepperDefer(false);

		// Call DebugInitialize() after SetCurrentImageDir()
		DebugInitialize();
		LogFileOutput("Main: DebugInitialize()\n");

		MemInitialize();
		LogFileOutput("Main: MemInitialize()\n");

		// Show About dialog after creating main window (need g_hFrameWindow)
		if (bShowAboutDlg)
		{
			if (!AboutDlg())
				g_cmdLine.bShutdown = true;											// Close everything down
			else
				RegSaveString(REG_CONFIG, REGVALUE_VERSION, TRUE, g_VERSIONSTRING);	// Only save version after user accepts license
		}

		if (g_bCapturePrintScreenKey)
		{
			RegisterHotKeys();		// needs valid g_hFrameWindow
			LogFileOutput("Main: RegisterHotKeys()\n");
		}

		if (g_bHookSystemKey)
		{
			if (GetHookFilter().InitHookThread())	// needs valid g_hFrameWindow (for message pump)
				LogFileOutput("Main: HookFilterForKeyboard()\n");
		}

		// Need to test if it's safe to call ResetMachineState(). In the meantime, just call Disk2Card's Reset():
		GetCardMgr().GetDisk2CardMgr().Reset(true);	// Switch from a booting A][+ to a non-autostart A][, so need to turn off floppy motor
		LogFileOutput("Main: DiskReset()\n");
		if (GetCardMgr().QuerySlot(SLOT7) == CT_GenericHDD)
			GetCardMgr().GetRef(SLOT7).Reset(true);	// GH#515
		LogFileOutput("Main: HDDReset()\n");

		if (!g_bSysClkOK)
		{
			GetFrame().FrameMessageBox("DirectX failed to create SystemClock instance", TEXT("AppleWin Error"), MB_OK);
			g_cmdLine.bShutdown = true;
		}

		if (g_bCustomRomF8Failed || g_bCustomRomFailed || (g_hCustomRomF8 != INVALID_HANDLE_VALUE && g_hCustomRom != INVALID_HANDLE_VALUE))
		{
			std::string msg = g_bCustomRomF8Failed ? "Failed to load custom F8 rom (not found or not exactly 2KiB)\n"
							: g_bCustomRomFailed ? "Failed to load custom rom (not found or not exactly 12KiB or 16KiB)\n"
							: "Unsupported -rom and -f8rom being used at the same time\n";

			LogFileOutput("%s", msg.c_str());
			GetFrame().FrameMessageBox(msg.c_str(), TEXT("AppleWin Error"), MB_OK);
			g_cmdLine.bShutdown = true;
		}

		if (g_cmdLine.szSnapshotName)
		{
			std::string strPathname(g_cmdLine.szSnapshotName);
			int nIdx = strPathname.find_last_of(PATH_SEPARATOR);
			if (nIdx >= 0 && nIdx+1 < (int)strPathname.length())	// path exists?
			{
				const std::string strPath = strPathname.substr(0, nIdx+1);
				SetCurrentImageDir(strPath);
			}

			// Override value just loaded from Registry by LoadConfiguration()
			// . NB. Registry value is not updated with this cmd-line value
			Snapshot_SetFilename(g_cmdLine.szSnapshotName);
			Snapshot_LoadState();
			g_cmdLine.bBoot = true;
			g_cmdLine.szSnapshotName = NULL;
		}
		else
		{
			Snapshot_Startup();		// Do this after everything has been init'ed
			LogFileOutput("Main: Snapshot_Startup()\n");
		}

		if (g_cmdLine.szScreenshotFilename)
		{
			GetFrame().Video_RedrawAndTakeScreenShot(g_cmdLine.szScreenshotFilename);
			g_cmdLine.bShutdown = true;
		}

		if (g_cmdLine.bShutdown)
		{
			PostMessage(GetFrame().g_hFrameWindow, WM_DESTROY, 0, 0);	// Close everything down
			// NB. If shutting down, then don't post any other messages (GH#286)
		}
		else
		{
			if (g_cmdLine.setFullScreen > 0)
			{
				PostMessage(GetFrame().g_hFrameWindow, WM_USER_FULLSCREEN, 0, 0);
				g_cmdLine.setFullScreen = 0;
			}

			if (g_cmdLine.bBoot)
			{
				PostMessage(GetFrame().g_hFrameWindow, WM_USER_BOOT, 0, 0);
				g_cmdLine.bBoot = false;
			}
		}
}

static void Shutdown(void)
{
	// NB. WM_CLOSE has already called SetNormalMode() to exit full screen mode & restore default resolution

	// Release COM
	SysClk_UninitTimer();
	LogFileOutput("Exit: SysClk_UninitTimer()\n");

	CoUninitialize();
	LogFileOutput("Exit: CoUninitialize()\n");

	LogDone();

	RiffFinishWriteFile();

	if (g_hCustomRomF8 != INVALID_HANDLE_VALUE)
		CloseHandle(g_hCustomRomF8);

	if (g_hCustomRom != INVALID_HANDLE_VALUE)
		CloseHandle(g_hCustomRom);

	if (g_cmdLine.bSlot7EmptyOnExit)
		GetCardMgr().Remove(SLOT7);
}

IPropertySheet& GetPropertySheet(void)
{
	static CPropertySheet sg_PropertySheet;
	return sg_PropertySheet;
}

FrameBase& GetFrame(void)
{
	static Win32Frame sg_Win32Frame;
	return sg_Win32Frame;
}

Video& GetVideo(void)
{
	static Video video;
	return video;
}
