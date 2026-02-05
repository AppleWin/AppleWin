#pragma once

extern double     g_fClksPerSpkrSample;
extern bool       g_bQuieterSpeaker;
extern short      g_nSpeakerData;

void    SpkrDestroy ();
void    SpkrInitialize ();
void    SpkrReinitialize ();
void    SpkrReset();
void    SpkrUpdate (uint32_t);
void    SpkrUpdate_Timer();
uint32_t   SpkrGetVolume();
void    SpkrSetVolume(uint32_t dwVolume, uint32_t dwVolumeMax);
void    Spkr_Mute();
void    Spkr_Unmute();
bool    Spkr_IsActive();
bool    Spkr_DSInit();
void	Spkr_OutputToRiff(void);
UINT    Spkr_GetNumChannels(void);
void    SpkrSaveSnapshot(class YamlSaveHelper& yamlSaveHelper);
void    SpkrLoadSnapshot(class YamlLoadHelper& yamlLoadHelper);

BYTE __stdcall SpkrToggle (WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nExecutedCycles);
