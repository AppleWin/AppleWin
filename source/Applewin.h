#pragma once

#include "Card.h"
#include "SaveState_Structs_common.h"
#include "Common.h"
#include "RemoteControl/RemoteControlManager.h"

void LogFileTimeUntilFirstKeyReadReset(void);
void LogFileTimeUntilFirstKeyRead(void);

bool SetCurrentImageDir(const std::string & pszImageDir);

extern const UINT16* GetOldAppleWinVersion(void);
extern TCHAR VERSIONSTRING[];	// Constructed in WinMain()

extern std::string g_pAppTitle;
extern std::string g_pProgramName;						// RIK -- Name of the emulated program
extern std::string g_pProgramSig;						// RIK -- Canonical signature of the emulated program using format: page-cnc32
extern RemoteControlManager g_RemoteControlMgr;			// RIK -- Necessary to have a single instance only

extern eApple2Type g_Apple2Type;
eApple2Type GetApple2Type(void);
void SetApple2Type(eApple2Type type);

double Get6502BaseClock(void);
void SetCurrentCLK6502(void);

void SingleStep(bool bReinit);

extern bool       g_bFullSpeed;

//===========================================

// Win32
extern HINSTANCE  g_hInstance;

extern AppMode_e g_nAppMode;
bool GetLoadedSaveStateFlag(void);
void SetLoadedSaveStateFlag(const bool bFlag);
bool GetHookAltGrControl(void);

extern std::string g_sProgramDir;
extern std::string g_sCurrentDir;

extern bool       g_bRestart;
extern bool       g_bRestartFullScreen;

extern DWORD      g_dwSpeed;
extern double     g_fCurrentCLK6502;

extern int        g_nCpuCyclesFeedback;
extern DWORD      g_dwCyclesThisFrame;

extern bool       g_bDisableDirectInput;				// Cmd line switch: don't init DI (so no DIMouse support)
extern bool       g_bDisableDirectSound;				// Cmd line switch: don't init DS (so no MB/Speaker support)
extern bool       g_bDisableDirectSoundMockingboard;	// Cmd line switch: don't init MB support
extern int        g_nMemoryClearType;					// Cmd line switch: use specific MIP (Memory Initialization Pattern)

extern class CardManager& GetCardMgr(void);
extern class SynchronousEventManager g_SynchronousEventMgr;

extern HANDLE	g_hCustomRomF8;		// INVALID_HANDLE_VALUE if no custom F8 rom
extern HANDLE	g_hCustomRom;		// INVALID_HANDLE_VALUE if no custom rom

#ifdef USE_SPEECH_API
class CSpeech;
extern CSpeech g_Speech;
#endif

extern __interface IPropertySheet& sg_PropertySheet;

//

//#define LOG_PERF_TIMINGS
#ifdef LOG_PERF_TIMINGS
class PerfMarker
{
public:
	PerfMarker(UINT64& globalCounter)
		: counter(globalCounter)
	{
		QueryPerformanceCounter(&timeStart);
	}
	~PerfMarker()
	{
		QueryPerformanceCounter(&timeEnd);
		counter += (UINT64)timeEnd.QuadPart - (UINT64)timeStart.QuadPart;
	}
private:
	UINT64& counter;
	LARGE_INTEGER timeStart;
	LARGE_INTEGER timeEnd;
};
#endif
