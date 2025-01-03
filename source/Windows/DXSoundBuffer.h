#pragma once

#include "SoundBuffer.h"

class DXSoundBuffer : public SoundBuffer
{
public:
	static std::shared_ptr<SoundBuffer> create(uint32_t dwBufferSize, uint32_t nSampleRate, int nChannels);

	DXSoundBuffer(LPDIRECTSOUNDBUFFER pBuffer);
	virtual ~DXSoundBuffer();

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
	const LPDIRECTSOUNDBUFFER m_pBuffer;
};

bool DSInit();
void DSUninit();
