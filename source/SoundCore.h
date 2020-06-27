#pragma once

#define MAX_SAMPLES (16*1024)

#define SAFE_RELEASE(p)      { if(p) { (p)->Release(); (p)=NULL; } }

// Define max 1 of these:
//#define RIFF_SPKR
//#define RIFF_MB

typedef struct
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
} VOICE, *PVOICE;


bool DSGetLock(LPDIRECTSOUNDBUFFER pVoice, DWORD dwOffset, DWORD dwBytes,
					  SHORT** ppDSLockedBuffer0, DWORD* pdwDSLockedBufferSize0,
					  SHORT** ppDSLockedBuffer1, DWORD* pdwDSLockedBufferSize1);

HRESULT DSGetSoundBuffer(VOICE* pVoice, DWORD dwFlags, DWORD dwBufferSize, DWORD nSampleRate, int nChannels);
void DSReleaseSoundBuffer(VOICE* pVoice);

bool DSZeroVoiceBuffer(PVOICE Voice, const char* pszDevName, DWORD dwBufferSize);
bool DSZeroVoiceWritableBuffer(PVOICE Voice, const char* pszDevName, DWORD dwBufferSize);

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
