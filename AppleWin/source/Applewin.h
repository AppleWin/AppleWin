#pragma once

extern char VERSIONSTRING[];	// Contructed in WinMain()

extern bool       g_bApple2e;
extern bool       g_bApple2plus;

extern BOOL       behind;
extern DWORD      cumulativecycles;
extern DWORD      cyclenum;
extern DWORD      emulmsec;
extern bool       g_bFullSpeed;
extern HINSTANCE  instance;

extern AppMode_e g_nAppMode;

extern DWORD      needsprecision;
extern TCHAR      g_sProgramDir[MAX_PATH];
extern TCHAR      g_sCurrentDir[MAX_PATH];

extern BOOL       resettiming;
extern BOOL       restart;

extern DWORD      g_dwSpeed;
extern double     g_fCurrentCLK6502;

extern int        g_nCpuCyclesFeedback;
extern DWORD      g_dwCyclesThisFrame;

extern FILE*      g_fh;				// Filehandle for log file
extern bool       g_bDisableDirectSound;	// Cmd line switch: don't init DS (so no MB support)

void    SetCurrentCLK6502();
