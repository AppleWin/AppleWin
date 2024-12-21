#pragma once

// Registry soundtype:
#define  REG_SOUNDTYPE_NONE    0
#define  REG_SOUNDTYPE_DIRECT  1	// Not supported from 1.26
#define  REG_SOUNDTYPE_SMART   2	// Not supported from 1.26
#define  REG_SOUNDTYPE_WAVE    3

enum SoundType_e
{
	SOUND_NONE = 0,
	SOUND_WAVE
};

extern SoundType_e soundtype;
extern double     g_fClksPerSpkrSample;
extern bool       g_bQuieterSpeaker;
extern short      g_nSpeakerData;

void    SpkrDestroy ();
void    SpkrInitialize ();
void    SpkrReinitialize ();
void    SpkrReset();
void    SpkrSetEmulationType (SoundType_e newSoundType);
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
