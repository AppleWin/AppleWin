#pragma once

#include "SoundBuffer.h"

class DXSoundBuffer : public SoundBuffer
{
public:
	~DXSoundBuffer() { Release(); }

	virtual HRESULT Init(DWORD dwFlags, DWORD dwBufferSize, DWORD nSampleRate, int nChannels, LPCSTR pDevName);
	virtual HRESULT Release();

	virtual HRESULT SetCurrentPosition(DWORD dwNewPosition);
	virtual HRESULT GetCurrentPosition(LPDWORD lpdwCurrentPlayCursor, LPDWORD lpdwCurrentWriteCursor);

	virtual HRESULT Lock(DWORD dwWriteCursor, DWORD dwWriteBytes, LPVOID* lplpvAudioPtr1, DWORD* lpdwAudioBytes1, LPVOID* lplpvAudioPtr2, DWORD* lpdwAudioBytes2, DWORD dwFlags);
	virtual HRESULT Unlock(LPVOID lpvAudioPtr1, DWORD dwAudioBytes1, LPVOID lpvAudioPtr2, DWORD dwAudioBytes2);

	virtual HRESULT Stop();
	virtual HRESULT Play(DWORD dwReserved1, DWORD dwReserved2, DWORD dwFlags);

	virtual HRESULT SetVolume(LONG lVolume);
	virtual HRESULT GetVolume(LONG* lplVolume);

	virtual HRESULT GetStatus(LPDWORD lpdwStatus);
	virtual HRESULT Restore();

private:
	LPDIRECTSOUNDBUFFER m_pBuffer = NULL;
};

bool DSInit();
void DSUninit();
