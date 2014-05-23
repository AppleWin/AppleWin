#pragma once

void SetCurrentCLK6502();
void SetCurrentImageDir(const char* pszImageDir);


extern char VERSIONSTRING[];	// Contructed in WinMain()

extern TCHAR     *g_pAppTitle;

extern eApple2Type g_Apple2Type;

extern BOOL       behind;
extern DWORD      cumulativecycles;
extern DWORD      cyclenum;
extern DWORD      emulmsec;
extern bool       g_bFullSpeed;

//Pravets 8A/C only variables
extern bool     P8CAPS_ON;
extern bool		P8Shift; 
//===========================================

// Win32
extern HINSTANCE  g_hInstance;

extern AppMode_e g_nAppMode;
bool GetLoadedSaveStateFlag(void);
void SetLoadedSaveStateFlag(const bool bFlag);

extern DWORD      needsprecision;
extern TCHAR      g_sProgramDir[MAX_PATH];
extern TCHAR      g_sCurrentDir[MAX_PATH];

extern bool       g_bResetTiming;
extern BOOL       restart;

extern DWORD      g_dwSpeed;
extern double     g_fCurrentCLK6502;

extern int        g_nCpuCyclesFeedback;
extern DWORD      g_dwCyclesThisFrame;

extern FILE*      g_fh;				// Filehandle for log file
extern bool       g_bDisableDirectInput;				// Cmd line switch: don't init DI (so no DIMouse support)
extern bool       g_bDisableDirectSound;				// Cmd line switch: don't init DS (so no MB/Speaker support)
extern bool       g_bDisableDirectSoundMockingboard;	// Cmd line switch: don't init MB support

extern SS_CARDTYPE g_Slot4;	// Mockingboard, Z80, Mouse in slot4
extern SS_CARDTYPE g_Slot5;	// Mockingboard, Z80,       in slot5

extern HANDLE	g_hCustomRomF8;		// NULL if no custom rom

enum eCPU {CPU_6502=1, CPU_Z80};
extern eCPU		g_ActiveCPU;

#ifdef USE_SPEECH_API
class CSpeech;
extern CSpeech g_Speech;
#endif

extern __interface IPropertySheet& sg_PropertySheet;
