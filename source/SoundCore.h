#pragma once

#define MAX_SAMPLES (16*1024)

#define SAFE_RELEASE(p)      { if(p) { (p)->Release(); (p)=NULL; } }

// Define max 1 of these:
//#define RIFF_SPKR
//#define RIFF_MB

struct VOICE
{
	LPDIRECTSOUNDBUFFER lpDSBvoice;
	LPDIRECTSOUNDNOTIFY lpDSNotify;
	bool bActive;			// Playback is active
	bool bMute;
	LONG nVolume;			// Current volume (as used by DirectSound)
	LONG nFadeVolume;		// Current fade volume (as used by DirectSound)
	DWORD dwUserVolume;		// Volume from slider on Property Sheet (0=Max)
	bool bIsSpeaker;
	bool bRecentlyActive;	// (Speaker only) false after 0.2s of speaker inactivity
	std::string name;

	VOICE(void)
	{
		lpDSBvoice = NULL;
		lpDSNotify = NULL;
		bActive = false;
		bMute = false;
		nVolume = 0;
		nFadeVolume = 0;
		dwUserVolume = 0;
		bIsSpeaker = false;
		bRecentlyActive = false;
		name = "";
	}

	~VOICE(void);
};

typedef VOICE* PVOICE;

// PHASOR_MODE: Circular dependency for Mockingboard.h & SSI263.h - so put it here for now
enum PHASOR_MODE { PH_Mockingboard = 0, PH_UNDEF1, PH_UNDEF2, PH_UNDEF3, PH_UNDEF4, PH_Phasor/*=5*/, PH_UNDEF6, PH_EchoPlus/*=7*/ };

HRESULT DSGetLock(LPDIRECTSOUNDBUFFER pVoice, DWORD dwOffset, DWORD dwBytes,
					  SHORT** ppDSLockedBuffer0, DWORD* pdwDSLockedBufferSize0,
					  SHORT** ppDSLockedBuffer1, DWORD* pdwDSLockedBufferSize1);

HRESULT DSGetSoundBuffer(VOICE* pVoice, DWORD dwFlags, DWORD dwBufferSize, DWORD nSampleRate, int nChannels, const char* pszDevName);
void DSReleaseSoundBuffer(VOICE* pVoice);

bool DSVoiceStop(PVOICE Voice);
bool DSZeroVoiceBuffer(PVOICE Voice, DWORD dwBufferSize);
bool DSZeroVoiceWritableBuffer(PVOICE Voice, DWORD dwBufferSize);

enum eFADE {FADE_NONE, FADE_IN, FADE_OUT};
void SoundCore_SetFade(eFADE FadeType);
bool SoundCore_GetTimerState();
void SoundCore_TweakVolumes();

int SoundCore_GetErrorInc();
void SoundCore_SetErrorInc(const int nErrorInc);
int SoundCore_GetErrorMax();
void SoundCore_SetErrorMax(const int nErrorMax);

bool DSInit();
void DSUninit();

LONG NewVolume(DWORD dwVolume, DWORD dwVolumeMax);

void SysClk_WaitTimer();
bool SysClk_InitTimer();
void SysClk_UninitTimer();
void SysClk_StartTimerUsec(DWORD dwUsecPeriod);
void SysClk_StopTimer();

//

extern bool g_bDSAvailable;
