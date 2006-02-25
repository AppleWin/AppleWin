#pragma once

extern bool       g_bMBTimerIrqActive;

void	MB_Initialize();
void	MB_Reinitialize();
void	MB_Destroy();
void    MB_Reset();
BYTE	MB_Read(WORD nAddr);
void	MB_Write(WORD nAddr, BYTE nValue);
void    MB_Mute();
void    MB_Demute();
void    MB_EndOfFrame();
void    MB_CheckIRQ();
void    MB_UpdateCycles(USHORT nClocks);
eSOUNDCARDTYPE MB_GetSoundcardType();
void    MB_SetSoundcardType(eSOUNDCARDTYPE NewSoundcardType);
double  MB_GetFramePeriod();
bool    MB_IsActive();
DWORD   MB_GetVolume();
void    MB_SetVolume(DWORD dwVolume, DWORD dwVolumeMax);
DWORD   MB_GetSnapshot(SS_CARD_MOCKINGBOARD* pSS, DWORD dwSlot);
DWORD   MB_SetSnapshot(SS_CARD_MOCKINGBOARD* pSS, DWORD dwSlot);

BYTE __stdcall PhasorIO (WORD pc, BYTE addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft);
