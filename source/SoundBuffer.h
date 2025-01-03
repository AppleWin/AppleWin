#pragma once

class SoundBuffer
{
public:
	virtual ~SoundBuffer() = default;

	virtual HRESULT SetCurrentPosition(DWORD dwNewPosition) = 0;
	virtual HRESULT GetCurrentPosition(LPDWORD lpdwCurrentPlayCursor, LPDWORD lpdwCurrentWriteCursor) = 0;

	virtual HRESULT Lock(DWORD dwWriteCursor, DWORD dwWriteBytes, LPVOID* lplpvAudioPtr1, DWORD* lpdwAudioBytes1, LPVOID* lplpvAudioPtr2, DWORD* lpdwAudioBytes2, DWORD dwFlags) = 0;
	virtual HRESULT Unlock(LPVOID lpvAudioPtr1, DWORD dwAudioBytes1, LPVOID lpvAudioPtr2, DWORD dwAudioBytes2) = 0;

	virtual HRESULT Stop() = 0;
	virtual HRESULT Play(DWORD dwReserved1, DWORD dwReserved2, DWORD dwFlags) = 0;

	virtual HRESULT SetVolume(LONG lVolume) = 0;
	virtual HRESULT GetVolume(LONG* lplVolume) = 0;

	virtual HRESULT GetStatus(LPDWORD lpdwStatus) = 0;
	virtual HRESULT Restore() = 0;
};

// this must be reimplemented in each platform
bool DSAvailable();
