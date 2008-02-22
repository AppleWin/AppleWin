/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 1994-1996, Michael O'Brien
Copyright (C) 1999-2001, Oliver Schmidt
Copyright (C) 2002-2005, Tom Charlesworth
Copyright (C) 2006-2007, Tom Charlesworth, Michael Pohoreski

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
#pragma  hdrstop
#include <objbase.h>
#include "MouseInterface.h"

char VERSIONSTRING[] = "xx.yy.zz.ww";

TCHAR *g_pAppTitle = TITLE_APPLE_2E_ENHANCED;

eApple2Type	g_Apple2Type	= A2TYPE_APPLE2EEHANCED;

BOOL      behind            = 0;			// Redundant
DWORD     cumulativecycles  = 0;			// Wraps after ~1hr 9mins
DWORD     cyclenum          = 0;			// Used by SpkrToggle() for non-wave sound
DWORD     emulmsec          = 0;
static DWORD emulmsec_frac  = 0;
bool      g_bFullSpeed      = false;

// Win32
HINSTANCE g_hInstance          = (HINSTANCE)0;

AppMode_e	g_nAppMode = MODE_LOGO;

static int lastmode         = MODE_LOGO;
DWORD     needsprecision    = 0;			// Redundant
TCHAR     g_sProgramDir[MAX_PATH] = TEXT("");
TCHAR     g_sCurrentDir[MAX_PATH] = TEXT(""); // Also Starting Dir
bool      g_bResetTiming    = false;			// Redundant
BOOL      restart           = 0;

DWORD		g_dwSpeed		= SPEED_NORMAL;	// Affected by Config dialog's speed slider bar
double		g_fCurrentCLK6502 = CLK_6502;	// Affected by Config dialog's speed slider bar
static double g_fMHz		= 1.0;			// Affected by Config dialog's speed slider bar

int			g_nCpuCyclesFeedback = 0;
DWORD       g_dwCyclesThisFrame = 0;

FILE*		g_fh			= NULL;
bool		g_bDisableDirectSound = false;

CSuperSerialCard	sg_SSC;
CMouseInterface		sg_Mouse;

UINT		g_Slot4 = CT_Mockingboard;		// CT_Mockingboard or CT_MouseInterface

HANDLE		g_hCustomRomF8 = INVALID_HANDLE_VALUE;	// Cmd-line specified custom ROM at $F800..$FFFF
static bool	g_bCustomRomF8Failed = false;			// Set if custom ROM file failed

//===========================================================================

#define DBG_CALC_FREQ 0
#if DBG_CALC_FREQ
const UINT MAX_CNT = 256;
double g_fDbg[MAX_CNT];
UINT g_nIdx = 0;
double g_fMeanPeriod,g_fMeanFreq;
ULONGLONG g_nPerfFreq = 0;
#endif

//---------------------------------------------------------------------------

void ContinueExecution()
{
	static BOOL pageflipping    = 0; //?

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

	bool bScrollLock_FullSpeed = g_uScrollLockToggle
									? g_bScrollLock_FullSpeed
									: (GetKeyState(VK_SCROLL) < 0);

	g_bFullSpeed = ( (g_dwSpeed == SPEED_MAX) || 
					 bScrollLock_FullSpeed ||
					 (DiskIsSpinning() && enhancedisk && !Spkr_IsActive() && !MB_IsActive()) );

	if(g_bFullSpeed)
	{
		// Don't call Spkr_Mute() - will get speaker clicks
		MB_Mute();
		SysClk_StopTimer();

		g_nCpuCyclesFeedback = 0;	// For the case when this is a big -ve number
	}
	else
	{
		// Don't call Spkr_Demute()
		MB_Demute();
		SysClk_StartTimerUsec(nExecutionPeriodUsec);
	}

	//

	int nCyclesToExecute = (int) fExecutionPeriodClks + g_nCpuCyclesFeedback;
	if(nCyclesToExecute < 0)
		nCyclesToExecute = 0;

	DWORD dwExecutedCycles = CpuExecute(nCyclesToExecute);

	g_dwCyclesThisFrame += dwExecutedCycles;

	//

	cyclenum = dwExecutedCycles;

	DiskUpdatePosition(dwExecutedCycles);
	JoyUpdatePosition();
	VideoUpdateVbl(g_dwCyclesThisFrame);

	SpkrUpdate(cyclenum);
	sg_SSC.CommUpdate(cyclenum);
	PrintUpdate(cyclenum);

	//

	const DWORD CLKS_PER_MS = (DWORD)g_fCurrentCLK6502 / 1000;

	emulmsec_frac += dwExecutedCycles;
	if(emulmsec_frac > CLKS_PER_MS)
	{
		emulmsec += emulmsec_frac / CLKS_PER_MS;
		emulmsec_frac %= CLKS_PER_MS;
	}

	//
	// DETERMINE WHETHER THE SCREEN WAS UPDATED, THE DISK WAS SPINNING,
	// OR THE KEYBOARD I/O PORTS WERE BEING EXCESSIVELY QUERIED THIS CLOCKTICK
	VideoCheckPage(0);
	BOOL screenupdated = VideoHasRefreshed();
	BOOL systemidle    = 0;	//(KeybGetNumQueries() > (clockgran << 2));	//  && (!ranfinegrain);	// TO DO

	if(screenupdated)
		pageflipping = 3;

	//

	if(g_dwCyclesThisFrame >= dwClksPerFrame)
	{
		g_dwCyclesThisFrame -= dwClksPerFrame;

		if(g_nAppMode != MODE_LOGO)
		{
			VideoUpdateFlash();

			static BOOL  anyupdates     = 0;
			static DWORD lastcycles     = 0;
			static BOOL  lastupdates[2] = {0,0};

			anyupdates |= screenupdated;

			//

			lastcycles = cumulativecycles;
			if ((!anyupdates) && (!lastupdates[0]) && (!lastupdates[1]) && VideoApparentlyDirty())
			{
				VideoCheckPage(1);
				static DWORD lasttime = 0;
				DWORD currtime = GetTickCount();
				if ((!g_bFullSpeed) ||
					(currtime-lasttime >= (DWORD)((graphicsmode || !systemidle) ? 100 : 25)))
				{
					VideoRefreshScreen();
					lasttime = currtime;
				}
				screenupdated = 1;
			}

			lastupdates[1] = lastupdates[0];
			lastupdates[0] = anyupdates;
			anyupdates     = 0;

			if (pageflipping)
				pageflipping--;
		}

		MB_EndOfVideoFrame();
	}

	//

	if(!g_bFullSpeed)
	{
		SysClk_WaitTimer();

#if DBG_CALC_FREQ
		if(g_nPerfFreq)
		{
			QueryPerformanceCounter((LARGE_INTEGER*)&nTime1);
			LONGLONG nTimeDiff = nTime1 - nTime0;
			double fTime = (double)nTimeDiff / (double)(LONGLONG)g_nPerfFreq;

			g_fDbg[g_nIdx] = fTime;
			g_nIdx = (g_nIdx+1) & (MAX_CNT-1);
			g_fMeanPeriod = 0.0;
			for(UINT n=0; n<MAX_CNT; n++)
				g_fMeanPeriod += g_fDbg[n];
			g_fMeanPeriod /= (double)MAX_CNT;
			g_fMeanFreq = 1.0 / g_fMeanPeriod;
		}
#endif
	}
}

//===========================================================================

void SetCurrentCLK6502()
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
LRESULT CALLBACK DlgProc (HWND   window,
                          UINT   message,
                          WPARAM wparam,
                          LPARAM lparam) {
  if (message == WM_CREATE) {
    RECT rect;
    GetWindowRect(window,&rect);
    SIZE size;
    size.cx = rect.right-rect.left;
    size.cy = rect.bottom-rect.top;
    rect.left   = (GetSystemMetrics(SM_CXSCREEN)-size.cx) >> 1;
    rect.top    = (GetSystemMetrics(SM_CYSCREEN)-size.cy) >> 1;
    rect.right  = rect.left+size.cx;
    rect.bottom = rect.top +size.cy;
    MoveWindow(window,
	       rect.left,
	       rect.top,
	       rect.right-rect.left,
	       rect.bottom-rect.top,
	       0);
    ShowWindow(window,SW_SHOW);
  }
  return DefWindowProc(window,message,wparam,lparam);
}

//===========================================================================
void EnterMessageLoop ()
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
			else if (g_nAppMode == MODE_LOGO)
				Sleep(100);		// Stop process hogging CPU
		}
	}
}

//===========================================================================
void GetProgramDirectory () {
  GetModuleFileName((HINSTANCE)0,g_sProgramDir,MAX_PATH);
  g_sProgramDir[MAX_PATH-1] = 0;
  int loop = _tcslen(g_sProgramDir);
  while (loop--)
    if ((g_sProgramDir[loop] == TEXT('\\')) ||
        (g_sProgramDir[loop] == TEXT(':'))) {
      g_sProgramDir[loop+1] = 0;
      break;
    }
}

//===========================================================================
void LoadConfiguration ()
{
  DWORD dwComputerType;

  if (LOAD(TEXT(REGVALUE_APPLE2_TYPE),&dwComputerType))
  {
	  if (dwComputerType >= A2TYPE_MAX)
		dwComputerType = A2TYPE_APPLE2EEHANCED;
	  g_Apple2Type = (eApple2Type) dwComputerType;
  }
  else
  {
	  LOAD(TEXT("Computer Emulation"),&dwComputerType);
	  switch (dwComputerType)
	  {
      // NB. No A2TYPE_APPLE2E
	  case 0:	g_Apple2Type = A2TYPE_APPLE2;
	  case 1:	g_Apple2Type = A2TYPE_APPLE2PLUS;
	  case 2:	g_Apple2Type = A2TYPE_APPLE2EEHANCED;
	  default:	g_Apple2Type = A2TYPE_APPLE2EEHANCED;
	  }
  }

  LOAD(TEXT("Joystick 0 Emulation"),&joytype[0]);
  LOAD(TEXT("Joystick 1 Emulation"),&joytype[1]);
  LOAD(TEXT("Sound Emulation")   ,&soundtype);

  DWORD dwSerialPort;
  LOAD(TEXT("Serial Port")       ,&dwSerialPort);
  sg_SSC.SetSerialPort(dwSerialPort);

  LOAD(TEXT("Emulation Speed")   ,&g_dwSpeed);
  LOAD(TEXT("Enhance Disk Speed"),(DWORD *)&enhancedisk);
  LOAD(TEXT("Video Emulation")   ,&videotype);
  LOAD(TEXT("Monochrome Color")  ,&monochrome);
  LOAD(TEXT("Uthernet Active")  ,(DWORD *)&tfe_enabled);

  SetCurrentCLK6502();

  //

  DWORD dwTmp;

  if(LOAD(TEXT(REGVALUE_THE_FREEZES_F8_ROM), &dwTmp))
	  g_uTheFreezesF8Rom = dwTmp;

  if(LOAD(TEXT(REGVALUE_SPKR_VOLUME), &dwTmp))
      SpkrSetVolume(dwTmp, PSP_GetVolumeMax());

  if(LOAD(TEXT(REGVALUE_MB_VOLUME), &dwTmp))
      MB_SetVolume(dwTmp, PSP_GetVolumeMax());

  if(LOAD(TEXT(REGVALUE_SOUNDCARD_TYPE), &dwTmp))
	  MB_SetSoundcardType((eSOUNDCARDTYPE)dwTmp);

  if(LOAD(TEXT(REGVALUE_SAVE_STATE_ON_EXIT), &dwTmp))
	  g_bSaveStateOnExit = dwTmp ? true : false;

  if(LOAD(TEXT(REGVALUE_HDD_ENABLED), &dwTmp))
	  HD_SetEnabled(dwTmp ? true : false);

  char szHDFilename[MAX_PATH] = {0};
  if(RegLoadString(TEXT("Configuration"), TEXT(REGVALUE_HDD_IMAGE1), 1, szHDFilename, sizeof(szHDFilename)))
	  HD_InsertDisk2(0, szHDFilename);
  if(RegLoadString(TEXT("Configuration"), TEXT(REGVALUE_HDD_IMAGE2), 1, szHDFilename, sizeof(szHDFilename)))
	  HD_InsertDisk2(1, szHDFilename);

  if(LOAD(TEXT(REGVALUE_PDL_XTRIM), &dwTmp))
      JoySetTrim((short)dwTmp, true);
  if(LOAD(TEXT(REGVALUE_PDL_YTRIM), &dwTmp))
      JoySetTrim((short)dwTmp, false);

  if(LOAD(TEXT(REGVALUE_SCROLLLOCK_TOGGLE), &dwTmp))
	  g_uScrollLockToggle = dwTmp;

  if(LOAD(TEXT(REGVALUE_MOUSE_IN_SLOT4), &dwTmp))
	  g_uMouseInSlot4 = dwTmp;
  g_Slot4 = g_uMouseInSlot4 ? CT_MouseInterface : CT_Mockingboard;

  //

  char szFilename[MAX_PATH] = {0};

  RegLoadString(TEXT("Configuration"),TEXT(REGVALUE_SAVESTATE_FILENAME),1,szFilename,sizeof(szFilename));
  Snapshot_SetFilename(szFilename);	// If not in Registry than default will be used

  // Current/Starting Dir is the "root" of where the user keeps his disk images
  RegLoadString(TEXT("Preferences"),REGVALUE_PREF_START_DIR,1,g_sCurrentDir,MAX_PATH);
  SetCurrentDirectory(g_sCurrentDir);
  
  char szUthernetInt[MAX_PATH] = {0};
  RegLoadString(TEXT("Configuration"),TEXT("Uthernet Interface"),1,szUthernetInt,MAX_PATH);  
  update_tfe_interface(szUthernetInt,NULL);

}

//===========================================================================
void RegisterExtensions ()
{
	TCHAR szCommandTmp[MAX_PATH];
	GetModuleFileName((HMODULE)0,szCommandTmp,MAX_PATH);

	TCHAR command[MAX_PATH];
	wsprintf(command, "\"%s\"",	szCommandTmp);	// Wrap	path & filename	in quotes &	null terminate

	TCHAR icon[MAX_PATH];
	wsprintf(icon,TEXT("%s,1"),(LPCTSTR)command);

	_tcscat(command,TEXT(" \"%1\""));			// Append "%1"

	RegSetValue(HKEY_CLASSES_ROOT,".bin",REG_SZ,"DiskImage",10);
	RegSetValue(HKEY_CLASSES_ROOT,".do"	,REG_SZ,"DiskImage",10);
	RegSetValue(HKEY_CLASSES_ROOT,".dsk",REG_SZ,"DiskImage",10);
	RegSetValue(HKEY_CLASSES_ROOT,".nib",REG_SZ,"DiskImage",10);
	RegSetValue(HKEY_CLASSES_ROOT,".po"	,REG_SZ,"DiskImage",10);
//	RegSetValue(HKEY_CLASSES_ROOT,".aws",REG_SZ,"DiskImage",10);	// TO DO
//	RegSetValue(HKEY_CLASSES_ROOT,".hdv",REG_SZ,"DiskImage",10);	// TO DO

	RegSetValue(HKEY_CLASSES_ROOT,
				"DiskImage",
				REG_SZ,"Disk Image",21);

	RegSetValue(HKEY_CLASSES_ROOT,
				"DiskImage\\DefaultIcon",
				REG_SZ,icon,_tcslen(icon)+1);

	RegSetValue(HKEY_CLASSES_ROOT,
				"DiskImage\\shell\\open\\command",
				REG_SZ,command,_tcslen(command)+1);

	RegSetValue(HKEY_CLASSES_ROOT,
				"DiskImage\\shell\\open\\ddeexec",
				REG_SZ,"%1",3);

	RegSetValue(HKEY_CLASSES_ROOT,
				"DiskImage\\shell\\open\\ddeexec\\application",
				REG_SZ,"applewin",9);

	RegSetValue(HKEY_CLASSES_ROOT,
				"DiskImage\\shell\\open\\ddeexec\\topic",
				REG_SZ,"system",7);
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

int DoDiskInsert(int nDrive, LPSTR szFileName)
{
	DWORD dwAttributes = GetFileAttributes(szFileName);
	if(dwAttributes == INVALID_FILE_ATTRIBUTES)
	{
		return -1;
	}

	BOOL bWriteProtected = (dwAttributes & FILE_ATTRIBUTE_READONLY) ? TRUE : FALSE;

	return DiskInsert(nDrive, szFileName, bWriteProtected, 0);
}

//---------------------------------------------------------------------------

int APIENTRY WinMain (HINSTANCE passinstance, HINSTANCE, LPSTR lpCmdLine, int)
{
	bool bSetFullScreen = false;
	bool bBoot = false;
	LPSTR szImageName_drive1 = NULL;
	LPSTR szImageName_drive2 = NULL;

	while(*lpCmdLine)
	{
		LPSTR lpNextArg = GetNextArg(lpCmdLine);

		if(strcmp(lpCmdLine, "-d1") == 0)
		{
			lpCmdLine = GetCurrArg(lpNextArg);
			lpNextArg = GetNextArg(lpNextArg);
			szImageName_drive1 = lpCmdLine;
		}
		else if(strcmp(lpCmdLine, "-d2") == 0)
		{
			lpCmdLine = GetCurrArg(lpNextArg);
			lpNextArg = GetNextArg(lpNextArg);
			szImageName_drive2 = lpCmdLine;
		}
		else if(strcmp(lpCmdLine, "-f") == 0)
		{
			bSetFullScreen = true;
		}
		else if((strcmp(lpCmdLine, "-l") == 0) && (g_fh == NULL))
		{
			g_fh = fopen("AppleWin.log", "a+t");	// Open log file (append & text g_nAppMode)
			CHAR aDateStr[80], aTimeStr[80];
			GetDateFormat(LOCALE_SYSTEM_DEFAULT, 0, NULL, NULL, (LPTSTR)aDateStr, sizeof(aDateStr));
			GetTimeFormat(LOCALE_SYSTEM_DEFAULT, 0, NULL, NULL, (LPTSTR)aTimeStr, sizeof(aTimeStr));
			fprintf(g_fh,"*** Logging started: %s %s\n",aDateStr,aTimeStr);
		}
		else if(strcmp(lpCmdLine, "-m") == 0)
		{
			g_bDisableDirectSound = true;
		}
#ifdef RAMWORKS
		else if(strcmp(lpCmdLine, "-r") == 0)		// RamWorks size [1..127]
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
		else if(strcmp(lpCmdLine, "-f8rom") == 0)		// Use custom 2K ROM at [$F800..$FFFF]
		{
			lpCmdLine = GetCurrArg(lpNextArg);
			lpNextArg = GetNextArg(lpNextArg);
			g_hCustomRomF8 = CreateFile(lpCmdLine, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_READONLY, NULL);
			if ((g_hCustomRomF8 == INVALID_HANDLE_VALUE) || (GetFileSize(g_hCustomRomF8, NULL) != 0x800))
				g_bCustomRomF8Failed = true;
		}

		lpCmdLine = lpNextArg;
	}

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

    if(0 == GetModuleFileName(NULL, szPath, sizeof(szPath)))
    {
        strcpy(szPath, __argv[0]);
    }

    // Extract application version and store in a global variable
    DWORD dwHandle, dwVerInfoSize;

    dwVerInfoSize = GetFileVersionInfoSize(szPath, &dwHandle);

    if(dwVerInfoSize > 0)
    {
        char* pVerInfoBlock = new char[dwVerInfoSize];

        if(GetFileVersionInfo(szPath, NULL, dwVerInfoSize, pVerInfoBlock))
        {
            VS_FIXEDFILEINFO* pFixedFileInfo;
            UINT pFixedFileInfoLen;

            VerQueryValue(pVerInfoBlock, TEXT("\\"), (LPVOID*) &pFixedFileInfo, (PUINT) &pFixedFileInfoLen);

            // Construct version string from fixed file info block

            unsigned long major     = pFixedFileInfo->dwFileVersionMS >> 16;
            unsigned long minor     = pFixedFileInfo->dwFileVersionMS & 0xffff;
            unsigned long fix       = pFixedFileInfo->dwFileVersionLS >> 16;
			unsigned long fix_minor = pFixedFileInfo->dwFileVersionLS & 0xffff;

            sprintf(VERSIONSTRING, "%d.%d.%d.%d", major, minor, fix, fix_minor);
        }
    }

#if DBG_CALC_FREQ
	QueryPerformanceFrequency((LARGE_INTEGER*)&g_nPerfFreq);
	if(g_fh) fprintf(g_fh, "Performance frequency = %d\n",g_nPerfFreq);
#endif

	//-----

	// Initialize COM - so we can use CoCreateInstance
	// . NB. DSInit() & DirectInputInit are done when g_hFrameWindow is created (WM_CREATE)
	CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
	bool bSysClkOK = SysClk_InitTimer();

	// DO ONE-TIME INITIALIZATION
	g_hInstance = passinstance;
	MemPreInitialize();		// Call before any of the slot devices are initialized
	GdiSetBatchLimit(512);
	GetProgramDirectory();
	RegisterExtensions();
	FrameRegisterClass();
	ImageInitialize();
	DiskInitialize();
	CreateColorMixMap();	// For tv emulation g_nAppMode

	//

	int nError = 0;
	if(szImageName_drive1)
	{
		nError = DoDiskInsert(0, szImageName_drive1);
		bBoot = true;
	}
	if(szImageName_drive2)
	{
		nError |= DoDiskInsert(1, szImageName_drive2);
	}

	//
	
	do
	{
		// DO INITIALIZATION THAT MUST BE REPEATED FOR A RESTART
		restart = 0;
		g_nAppMode = MODE_LOGO;
		LoadConfiguration();
		DebugInitialize();
		JoyInitialize();
		MemInitialize();
		VideoInitialize();
		FrameCreateWindow();
		// Need to test if it's safe to call ResetMachineState(). In the meantime, just call DiskReset():
		DiskReset();	// Switch from a booting A][+ to a non-autostart A][, so need to turn off floppy motor

		if (!bSysClkOK)
		{
			MessageBox(g_hFrameWindow, "DirectX failed to create SystemClock instance", TEXT("AppleWin Error"), MB_OK);
			PostMessage(g_hFrameWindow, WM_DESTROY, 0, 0);	// Close everything down
		}

		if (g_bCustomRomF8Failed)
		{
			MessageBox(g_hFrameWindow, "Failed to load custom F8 rom (not found or not exactly 2KB)", TEXT("AppleWin Error"), MB_OK);
			PostMessage(g_hFrameWindow, WM_DESTROY, 0, 0);	// Close everything down
		}

		tfe_init();
        Snapshot_Startup();		// Do this after everything has been init'ed
    
		if(bSetFullScreen)
		{
			PostMessage(g_hFrameWindow, WM_KEYDOWN, VK_F1+BTN_FULLSCR, 0);
			PostMessage(g_hFrameWindow, WM_KEYUP,   VK_F1+BTN_FULLSCR, 0);
			bSetFullScreen = false;
		}

		if(bBoot)
		{
			PostMessage(g_hFrameWindow, WM_KEYDOWN, VK_F1+BTN_RUN, 0);
			PostMessage(g_hFrameWindow, WM_KEYUP,   VK_F1+BTN_RUN, 0);
			bBoot = false;
		}

		// ENTER THE MAIN MESSAGE LOOP
		EnterMessageLoop();

		MB_Reset();
		sg_Mouse.Uninitialize();	// Maybe restarting due to switching slot-4 card from mouse to MB
	}
	while (restart);
	
	// Release COM
	DSUninit();
	SysClk_UninitTimer();
	CoUninitialize();
	
	tfe_shutdown();
	
	if	(g_fh)
	{
		fprintf(g_fh,"*** Logging ended\n\n");
		fclose(g_fh);
	}

	RiffFinishWriteFile();

	if (g_hCustomRomF8 != INVALID_HANDLE_VALUE)
		CloseHandle(g_hCustomRomF8);

	return 0;
}
