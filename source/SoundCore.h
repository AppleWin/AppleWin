#pragma once

#define MAX_SAMPLES (16*1024)

#define SAFE_RELEASE(p)      { if(p) { (p)->Release(); (p)=NULL; } }

#include "SoundBuffer.h"

struct VOICE
{
	std::shared_ptr<SoundBuffer> lpDSBvoice;
	bool bActive;			// Playback is active
	bool bMute;
	LONG nVolume;			// Current volume (as used by DirectSound)
	LONG nFadeVolume;		// Current fade volume (as used by DirectSound)
	uint32_t dwUserVolume;		// Volume from slider on Property Sheet (0=Max)
	bool bIsSpeaker;
	bool bRecentlyActive;	// (Speaker only) false after 0.2s of speaker inactivity
	std::string name;

	VOICE(void)
	{
		bActive = false;
		bMute = false;
		nVolume = DSBVOLUME_MAX;
		nFadeVolume = 0;
		dwUserVolume = 0;
		bIsSpeaker = false;
		bRecentlyActive = false;
		name = "";
	}

	~VOICE(void);
};

typedef VOICE* PVOICE;

HRESULT DSGetLock(const std::shared_ptr<SoundBuffer>& pVoice, uint32_t dwOffset, uint32_t dwBytes,
					  SHORT** ppDSLockedBuffer0, DWORD* pdwDSLockedBufferSize0,
					  SHORT** ppDSLockedBuffer1, DWORD* pdwDSLockedBufferSize1);

HRESULT DSGetSoundBuffer(VOICE* pVoice, uint32_t dwBufferSize, uint32_t nSampleRate, int nChannels, const char* pszVoiceName);
void DSReleaseSoundBuffer(VOICE* pVoice);

bool DSVoiceStop(PVOICE Voice);
bool DSZeroVoiceBuffer(PVOICE Voice, uint32_t dwBufferSize);
bool DSZeroVoiceWritableBuffer(PVOICE Voice, uint32_t dwBufferSize);

enum eFADE {FADE_NONE, FADE_IN, FADE_OUT};
void SoundCore_SetFade(eFADE FadeType);
bool SoundCore_GetTimerState();
void SoundCore_TweakVolumes();

int SoundCore_GetErrorInc();
void SoundCore_SetErrorInc(const int nErrorInc);
int SoundCore_GetErrorMax();
void SoundCore_SetErrorMax(const int nErrorMax);

void SoundCore_StopTimer();

LONG NewVolume(uint32_t dwVolume, uint32_t dwVolumeMax);

void SysClk_WaitTimer();
bool SysClk_InitTimer();
void SysClk_UninitTimer();
void SysClk_StartTimerUsec(uint32_t dwUsecPeriod);
void SysClk_StopTimer();

extern UINT g_uNumVoices;
