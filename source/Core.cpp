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

#include "Core.h"
#include "CardManager.h"
#include "CPU.h"
#include "Interface.h"
#include "Log.h"
#include "Memory.h"
#include "Mockingboard.h"
#include "Speaker.h"
#include "Registry.h"
#include "SynchronousEventManager.h"

#ifdef USE_SPEECH_API
#include "Speech.h"
#endif

static const UINT VERSIONSTRING_SIZE = 16;
static UINT16 g_OldAppleWinVersion[4] = {0};
UINT16 g_AppleWinVersion[4] = { 0 };
TCHAR VERSIONSTRING[VERSIONSTRING_SIZE] = "xx.yy.zz.ww";

std::string g_pAppTitle;

eApple2Type	g_Apple2Type = A2TYPE_APPLE2EENHANCED;

bool      g_bFullSpeed      = false;

//=================================================

AppMode_e	g_nAppMode = MODE_LOGO;

std::string g_sStartDir;	// NB. AppleWin.exe maybe relative to this! (GH#663)
std::string g_sProgramDir;	// Directory of where AppleWin executable resides
std::string g_sCurrentDir;	// Also Starting Dir.  Debugger uses this when load/save

bool      g_bRestart = false;

bool		g_bDisableDirectInput = false;
bool		g_bDisableDirectSound = false;
bool		g_bDisableDirectSoundMockingboard = false;

DWORD		g_dwSpeed		= SPEED_NORMAL;	// Affected by Config dialog's speed slider bar
double		g_fCurrentCLK6502 = CLK_6502_NTSC;	// Affected by Config dialog's speed slider bar
static double g_fMHz		= 1.0;			// Affected by Config dialog's speed slider bar

int			g_nCpuCyclesFeedback = 0;
DWORD       g_dwCyclesThisFrame = 0;

int			g_nMemoryClearType = MIP_FF_FF_00_00; // Note: -1 = random MIP in Memory.cpp MemReset()

SynchronousEventManager g_SynchronousEventMgr;

HANDLE		g_hCustomRomF8 = INVALID_HANDLE_VALUE;	// Cmd-line specified custom F8 ROM at $F800..$FFFF
bool	    g_bCustomRomF8Failed = false;			// Set if custom F8 ROM file failed
HANDLE		g_hCustomRom = INVALID_HANDLE_VALUE;	// Cmd-line specified custom ROM at $C000..$FFFF(16KiB) or $D000..$FFFF(12KiB)
bool	    g_bCustomRomFailed = false;				// Set if custom ROM file failed

bool	g_bEnableSpeech = false;
#ifdef USE_SPEECH_API
CSpeech		g_Speech;
#endif

//===========================================================================

#ifdef LOG_PERF_TIMINGS
static UINT64 g_timeTotal = 0;
UINT64 g_timeCpu = 0;
UINT64 g_timeVideo = 0;			// part of timeCpu
UINT64 g_timeMB_Timer = 0;		// part of timeCpu
UINT64 g_timeMB_NoTimer = 0;
UINT64 g_timeSpeaker = 0;
static UINT64 g_timeVideoRefresh = 0;

void LogPerfTimings(void)
{
	if (g_timeTotal)
	{
		UINT64 cpu = g_timeCpu - g_timeVideo - g_timeMB_Timer;
		UINT64 video = g_timeVideo + g_timeVideoRefresh;
		UINT64 spkr = g_timeSpeaker;
		UINT64 mb = g_timeMB_Timer + g_timeMB_NoTimer;
		UINT64 audio = spkr + mb;
		UINT64 other = g_timeTotal - g_timeCpu - g_timeSpeaker - g_timeMB_NoTimer - g_timeVideoRefresh;

		LogOutput("Perf breakdown:\n");
		LogOutput(". CPU %%        = %6.2f\n", (double)cpu / (double)g_timeTotal * 100.0);
		LogOutput(". Video %%      = %6.2f\n", (double)video / (double)g_timeTotal * 100.0);
		LogOutput("... NTSC %%     = %6.2f\n", (double)g_timeVideo / (double)g_timeTotal * 100.0);
		LogOutput("... refresh %%  = %6.2f\n", (double)g_timeVideoRefresh / (double)g_timeTotal * 100.0);
		LogOutput(". Audio %%      = %6.2f\n", (double)audio / (double)g_timeTotal * 100.0);
		LogOutput("... Speaker %%  = %6.2f\n", (double)spkr / (double)g_timeTotal * 100.0);
		LogOutput("... MB %%       = %6.2f\n", (double)mb / (double)g_timeTotal * 100.0);
		LogOutput(". Other %%      = %6.2f\n", (double)other / (double)g_timeTotal * 100.0);
		LogOutput(". TOTAL %%      = %6.2f\n", (double)(cpu+video+audio+other) / (double)g_timeTotal * 100.0);
	}
}
#endif

//===========================================================================

static DWORD dwLogKeyReadTickStart;
static bool bLogKeyReadDone = false;

void LogFileTimeUntilFirstKeyReadReset(void)
{
#ifdef LOG_PERF_TIMINGS
	LogPerfTimings();
#endif

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

CardManager& GetCardMgr(void)
{
	static CardManager g_CardMgr;	// singleton
	return g_CardMgr;
}

//===========================================================================

double Get6502BaseClock(void)
{
	return (GetVideo().GetVideoRefreshRate() == VR_50HZ) ? CLK_6502_PAL : CLK_6502_NTSC;
}

void SetCurrentCLK6502(void)
{
	static DWORD dwPrevSpeed = (DWORD) -1;
	static VideoRefreshRate_e prevVideoRefreshRate = VR_NONE;

	if (dwPrevSpeed == g_dwSpeed && GetVideo().GetVideoRefreshRate() == prevVideoRefreshRate)
		return;

	dwPrevSpeed = g_dwSpeed;
	prevVideoRefreshRate = GetVideo().GetVideoRefreshRate();

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

void SetAppleWinVersion(UINT16 major, UINT16 minor, UINT16 fix, UINT16 fix_minor)
{
	g_AppleWinVersion[0] = major;
	g_AppleWinVersion[1] = minor;
	g_AppleWinVersion[2] = fix;
	g_AppleWinVersion[3] = fix_minor;
	StringCbPrintf(VERSIONSTRING, VERSIONSTRING_SIZE, "%d.%d.%d.%d", major, minor, fix, fix_minor);
}

bool CheckOldAppleWinVersion(void)
{
	TCHAR szOldAppleWinVersion[VERSIONSTRING_SIZE + 1];
	RegLoadString(TEXT(REG_CONFIG), TEXT(REGVALUE_VERSION), 1, szOldAppleWinVersion, VERSIONSTRING_SIZE, TEXT(""));
	const bool bShowAboutDlg = strcmp(szOldAppleWinVersion, VERSIONSTRING) != 0;

	// version: xx.yy.zz.ww
	char* p0 = szOldAppleWinVersion;
	int len = strlen(szOldAppleWinVersion);
	szOldAppleWinVersion[len] = '.';	// append a null terminator
	szOldAppleWinVersion[len + 1] = '\0';
	for (UINT i = 0; i < 4; i++)
	{
		char* p1 = strstr(p0, ".");
		if (!p1)
			break;
		*p1 = 0;
		g_OldAppleWinVersion[i] = atoi(p0);
		p0 = p1 + 1;
	}

	return bShowAboutDlg;
}

bool SetCurrentImageDir(const std::string& pszImageDir)
{
	g_sCurrentDir = pszImageDir;

	if (!g_sCurrentDir.empty() && *g_sCurrentDir.rbegin() != '\\')
		g_sCurrentDir += '\\';

	if (SetCurrentDirectory(g_sCurrentDir.c_str()))
		return true;

	return false;
}
