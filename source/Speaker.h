#pragma once

extern DWORD      soundtype;
extern double     g_fClksPerSpkrSample;

void    SpkrDestroy ();
void    SpkrInitialize ();
void    SpkrReinitialize ();
void    SpkrReset();
BOOL    SpkrSetEmulationType (HWND,DWORD);
void    SpkrUpdate (DWORD);
void    SpkrUpdate_Timer();
DWORD   SpkrGetVolume();
void    SpkrSetVolume(DWORD dwVolume, DWORD dwVolumeMax);
void    Spkr_Mute();
void    Spkr_Demute();
bool    Spkr_IsActive();
bool    Spkr_DSInit();
void    Spkr_DSUninit();
DWORD   SpkrGetSnapshot(SS_IO_Speaker* pSS);
DWORD   SpkrSetSnapshot(SS_IO_Speaker* pSS);

BYTE __stdcall SpkrToggle (WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft);
