#pragma once

#include "Card.h"

enum PHASOR_MODE {PH_Mockingboard=0, PH_UNDEF1, PH_UNDEF2, PH_UNDEF3, PH_UNDEF4, PH_Phasor/*=5*/, PH_UNDEF6, PH_EchoPlus/*=7*/};

void	MB_Initialize();
void	MB_Reinitialize();
void	MB_Destroy();
void    MB_Reset(const bool powerCycle);
void	MB_InitializeForLoadingSnapshot(void);
void    MB_InitializeIO(LPBYTE pCxRomPeripheral, UINT uSlot4, UINT uSlot5);
void    MB_Mute();
void    MB_Unmute();
#ifdef _DEBUG
void    MB_CheckCumulativeCycles();	// DEBUG
#endif
void    MB_SetCumulativeCycles();
void    MB_PeriodicUpdate(UINT executedCycles);
void    MB_CheckIRQ();
void    MB_UpdateCycles(ULONG uExecutedCycles);
SS_CARDTYPE MB_GetSoundcardType();
bool    MB_IsActive();
DWORD   MB_GetVolume();
void    MB_SetVolume(DWORD dwVolume, DWORD dwVolumeMax);
void MB_Get6522IrqDescription(std::string& desc);

void MB_UpdateIRQ(void);
UINT64 MB_GetLastCumulativeCycles(void);
void MB_UpdateIFR(BYTE nDevice, BYTE clr_mask, BYTE set_mask);
BYTE MB_GetPCR(BYTE nDevice);

void    MB_GetSnapshot_v1(struct SS_CARD_MOCKINGBOARD_v1* const pSS, const DWORD dwSlot);	// For debugger
const std::string& MB_GetSnapshotCardName(void);
void    MB_SaveSnapshot(class YamlSaveHelper& yamlSaveHelper, const UINT uSlot);
bool    MB_LoadSnapshot(class YamlLoadHelper& yamlLoadHelper, UINT slot, UINT version);

const std::string& Phasor_GetSnapshotCardName(void);
void Phasor_SaveSnapshot(class YamlSaveHelper& yamlSaveHelper, const UINT uSlot);
bool Phasor_LoadSnapshot(class YamlLoadHelper& yamlLoadHelper, UINT slot, UINT version);
