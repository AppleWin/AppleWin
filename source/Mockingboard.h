#pragma once

extern bool       g_bMBTimerIrqActive;
#ifdef _DEBUG
extern UINT32	g_uTimer1IrqCount;	// DEBUG
#endif

void	MB_Initialize();
void	MB_Reinitialize();
void	MB_Destroy();
void    MB_Reset();
void    MB_InitializeIO(LPBYTE pCxRomPeripheral, UINT uSlot4, UINT uSlot5);
void    MB_Mute();
void    MB_Demute();
void    MB_StartOfCpuExecute();
void    MB_EndOfVideoFrame();
void    MB_CheckIRQ();
void    MB_UpdateCycles(ULONG uExecutedCycles);
SS_CARDTYPE MB_GetSoundcardType();
void    MB_SetSoundcardType(SS_CARDTYPE NewSoundcardType);
double  MB_GetFramePeriod();
bool    MB_IsActive();
DWORD   MB_GetVolume();
void    MB_SetVolume(DWORD dwVolume, DWORD dwVolumeMax);

void    MB_GetSnapshot_v1(struct SS_CARD_MOCKINGBOARD_v1* const pSS, const DWORD dwSlot);	// For debugger
int     MB_SetSnapshot_v1(const struct SS_CARD_MOCKINGBOARD_v1* const pSS, const DWORD dwSlot);
void    MB_GetSnapshot(const HANDLE hFile, const UINT uSlot);
void    MB_SetSnapshot(const HANDLE hFile);

void Phasor_GetSnapshot(const HANDLE hFile);
void Phasor_SetSnapshot(const HANDLE hFile);
