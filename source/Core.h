#pragma once

#include "Card.h"
#include "Common.h"

void LogFileTimeUntilFirstKeyReadReset(void);
void LogFileTimeUntilFirstKeyRead(void);

extern const UINT16* GetOldAppleWinVersion(void);
extern TCHAR VERSIONSTRING[];	// Constructed in WinMain()

void SetAppleWinVersion(UINT16 major, UINT16 minor, UINT16 fix, UINT16 fix_minor);
bool CheckOldAppleWinVersion(void);

extern std::string g_pAppTitle;

extern eApple2Type g_Apple2Type;
eApple2Type GetApple2Type(void);
void SetApple2Type(eApple2Type type);

double Get6502BaseClock(void);
void SetCurrentCLK6502(void);

// set g_dwSpeed =
// | clockMultiplier == 0  => unchanged
// | clockMultiplier < 1   => (max(0.5, clockMultiplier) - 0.5) * 20
// | else                  => min(SPEED_MAX - 1, clockMultiplier * 10)
void UseClockMultiplier(double clockMultiplier);

extern bool       g_bFullSpeed;

//===========================================

extern AppMode_e g_nAppMode;

extern std::string g_sStartDir;
extern std::string g_sProgramDir;
extern std::string g_sCurrentDir;

bool SetCurrentImageDir(const std::string& pszImageDir);

extern bool       g_bRestart;

extern DWORD      g_dwSpeed;
extern double     g_fCurrentCLK6502;

extern int        g_nCpuCyclesFeedback;
extern DWORD      g_dwCyclesThisFrame;

extern int        g_nMemoryClearType;					// Cmd line switch: use specific MIP (Memory Initialization Pattern)

extern class CardManager& GetCardMgr(void);
extern class SynchronousEventManager g_SynchronousEventMgr;

extern HANDLE	g_hCustomRomF8;			// INVALID_HANDLE_VALUE if no custom F8 rom
extern bool	    g_bCustomRomF8Failed;	// Set if custom F8 ROM file failed
extern HANDLE	g_hCustomRom;			// INVALID_HANDLE_VALUE if no custom rom
extern bool	    g_bCustomRomFailed;		// Set if custom ROM file failed

extern bool	g_bEnableSpeech;
#ifdef USE_SPEECH_API
class CSpeech;
extern CSpeech g_Speech;
#endif

extern bool       g_bDisableDirectInput;				// Cmd line switch: don't init DI (so no DIMouse support)
extern bool       g_bDisableDirectSound;				// Cmd line switch: don't init DS (so no MB/Speaker support)
extern bool       g_bDisableDirectSoundMockingboard;	// Cmd line switch: don't init MB support

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
