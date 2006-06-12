#pragma once

extern char VERSIONSTRING[];	// Contructed in WinMain()

extern bool       apple2e;
extern bool       apple2plus;

extern BOOL       behind;
extern DWORD      cumulativecycles;
extern DWORD      cyclenum;
extern DWORD      emulmsec;
extern bool       g_bFullSpeed;
extern HINSTANCE  instance;

extern AppMode_e g_nAppMode;

extern DWORD      needsprecision;
extern TCHAR      progdir[MAX_PATH];
extern BOOL       resettiming;
extern BOOL       restart;

extern DWORD      g_dwSpeed;
extern double     g_fCurrentCLK6502;

extern int        g_nCpuCyclesFeedback;
extern DWORD      g_dwCyclesThisFrame;

extern FILE*      g_fh;				// Filehandle for log file
extern bool       g_bDisableDirectSound;	// Cmd line switch: don't init DS (so no MB support)

void    SetCurrentCLK6502();
