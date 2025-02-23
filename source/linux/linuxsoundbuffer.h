#pragma once

#include "SoundBuffer.h"

#include <vector>
#include <mutex>
#include <atomic>
#include <string>

class LinuxSoundBuffer : public SoundBuffer
{
private:
    std::vector<uint8_t> mySoundBuffer;

    size_t myPlayPosition = 0;
    size_t myWritePosition = 0;
    WORD myStatus = 0;
    LONG myVolume = 0;

    // updated by the callback
    std::atomic_size_t myNumberOfUnderruns;
    mutable std::mutex myMutex;

protected:
    LinuxSoundBuffer(DWORD dwBufferSize, DWORD nSampleRate, int nChannels, LPCSTR pszVoiceName);

public:
    const size_t myBufferSize;
    const size_t mySampleRate;
    const size_t myChannels;
    const std::string myVoiceName;

    virtual HRESULT SetCurrentPosition(DWORD dwNewPosition) override;
    virtual HRESULT GetCurrentPosition(LPDWORD lpdwCurrentPlayCursor, LPDWORD lpdwCurrentWriteCursor) override;

    virtual HRESULT Lock(
        DWORD dwWriteCursor, DWORD dwWriteBytes, LPVOID *lplpvAudioPtr1, DWORD *lpdwAudioBytes1, LPVOID *lplpvAudioPtr2,
        DWORD *lpdwAudioBytes2, DWORD dwFlags) override;
    virtual HRESULT Unlock(LPVOID lpvAudioPtr1, DWORD dwAudioBytes1, LPVOID lpvAudioPtr2, DWORD dwAudioBytes2) override;

    virtual HRESULT Stop() override;
    virtual HRESULT Play(DWORD dwReserved1, DWORD dwReserved2, DWORD dwFlags) override;

    virtual HRESULT SetVolume(LONG lVolume) override;
    virtual HRESULT GetVolume(LONG *lplVolume) override;

    virtual HRESULT GetStatus(LPDWORD lpdwStatus) override;
    virtual HRESULT Restore() override;

    DWORD Read(
        DWORD dwReadBytes, LPVOID *lplpvAudioPtr1, DWORD *lpdwAudioBytes1, LPVOID *lplpvAudioPtr2,
        DWORD *lpdwAudioBytes2);
    DWORD GetBytesInBuffer() const;
    size_t GetBufferUnderruns() const;
    void ResetUnderruns();
    double GetLogarithmicVolume() const; // in [0, 1]
};

bool DSAvailable();
