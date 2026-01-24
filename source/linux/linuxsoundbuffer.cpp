#include <StdAfx.h>

#include "linux/linuxsoundbuffer.h"

namespace
{

    DWORD RingDistance(size_t from, size_t to, size_t size)
    {
        if (to >= from)
            return to - from;
        return size - (from - to);
    }

} // namespace

LinuxSoundBuffer::LinuxSoundBuffer(DWORD dwBufferSize, DWORD nSampleRate, int nChannels, LPCSTR pszVoiceName)
    : mySoundBuffer(dwBufferSize)
    , myNumberOfUnderruns(0)
    , myBufferSize(dwBufferSize)
    , mySampleRate(nSampleRate)
    , myChannels(nChannels)
    , myVoiceName(pszVoiceName)
{
}

HRESULT LinuxSoundBuffer::Unlock(LPVOID lpvAudioPtr1, DWORD dwAudioBytes1, LPVOID lpvAudioPtr2, DWORD dwAudioBytes2)
{
    const size_t totalWrittenBytes = dwAudioBytes1 + dwAudioBytes2;
    this->myWritePosition = (this->myWritePosition + totalWrittenBytes) % this->mySoundBuffer.size();
    myMutex.unlock();
    return DS_OK;
}

HRESULT LinuxSoundBuffer::Stop()
{
    const DWORD mask = DSBSTATUS_PLAYING | DSBSTATUS_LOOPING;
    this->myStatus &= ~mask;
    return DS_OK;
}

HRESULT LinuxSoundBuffer::SetCurrentPosition(DWORD dwNewPosition)
{
    return DS_OK;
}

HRESULT LinuxSoundBuffer::Play(DWORD dwReserved1, DWORD dwReserved2, DWORD dwFlags)
{
    this->myStatus |= DSBSTATUS_PLAYING;
    if (dwFlags & DSBPLAY_LOOPING)
    {
        this->myStatus |= DSBSTATUS_LOOPING;
    }
    return DS_OK;
}

HRESULT LinuxSoundBuffer::SetVolume(LONG lVolume)
{
    this->myVolume = lVolume;
    return DS_OK;
}

HRESULT LinuxSoundBuffer::GetStatus(LPDWORD lpdwStatus)
{
    *lpdwStatus = this->myStatus;
    return DS_OK;
}

HRESULT LinuxSoundBuffer::Restore()
{
    return DS_OK;
}

HRESULT LinuxSoundBuffer::Lock(
    DWORD dwWriteCursor, DWORD dwWriteBytes, LPVOID *lplpvAudioPtr1, DWORD *lpdwAudioBytes1, LPVOID *lplpvAudioPtr2,
    DWORD *lpdwAudioBytes2, DWORD dwFlags)
{
    myMutex.lock();

    // No attempt is made at restricting write buffer not to overtake play cursor
    if (dwFlags & DSBLOCK_ENTIREBUFFER)
    {
        myLastLockCursor = 0;
        *lplpvAudioPtr1 = this->mySoundBuffer.data();
        *lpdwAudioBytes1 = this->mySoundBuffer.size();
        if (lplpvAudioPtr2 && lpdwAudioBytes2)
        {
            *lplpvAudioPtr2 = nullptr;
            *lpdwAudioBytes2 = 0;
        }
    }
    else
    {
        dwWriteCursor %= myBufferSize;
        myLastLockCursor = dwWriteCursor;

        const DWORD availableInFirstPart = this->mySoundBuffer.size() - dwWriteCursor;

        *lplpvAudioPtr1 = this->mySoundBuffer.data() + dwWriteCursor;
        *lpdwAudioBytes1 = std::min(availableInFirstPart, dwWriteBytes);

        if (lplpvAudioPtr2 && lpdwAudioBytes2)
        {
            if (*lpdwAudioBytes1 < dwWriteBytes)
            {
                *lplpvAudioPtr2 = this->mySoundBuffer.data();
                *lpdwAudioBytes2 = std::min(dwWriteCursor, dwWriteBytes - *lpdwAudioBytes1);
            }
            else
            {
                *lplpvAudioPtr2 = nullptr;
                *lpdwAudioBytes2 = 0;
            }
        }
    }

    return DS_OK;
}

DWORD LinuxSoundBuffer::Read(
    DWORD dwReadBytes, LPVOID *lplpvAudioPtr1, DWORD *lpdwAudioBytes1, LPVOID *lplpvAudioPtr2, DWORD *lpdwAudioBytes2)
{
    const std::lock_guard<std::mutex> guard(myMutex);

    // Only advance play cursor if playing
    if (!(myStatus & DSBSTATUS_PLAYING))
    {
        *lplpvAudioPtr1 = nullptr;
        *lpdwAudioBytes1 = 0;
        *lplpvAudioPtr2 = nullptr;
        *lpdwAudioBytes2 = 0;
        return 0;
    }

    // Available bytes in the buffer
    DWORD available = RingDistance(myPlayPosition, myWritePosition, myBufferSize);

    // Count underruns if requested bytes exceed available
    if (available < dwReadBytes)
    {
        dwReadBytes = available;
        ++myNumberOfUnderruns;
    }

    // First part before wrap
    DWORD firstPart = myBufferSize - myPlayPosition;
    *lplpvAudioPtr1 = mySoundBuffer.data() + myPlayPosition;
    *lpdwAudioBytes1 = std::min(firstPart, dwReadBytes);

    // Second part after wrap
    if (lplpvAudioPtr2 && lpdwAudioBytes2)
    {
        if (*lpdwAudioBytes1 < dwReadBytes)
        {
            *lplpvAudioPtr2 = mySoundBuffer.data();
            *lpdwAudioBytes2 = dwReadBytes - *lpdwAudioBytes1;
        }
        else
        {
            *lplpvAudioPtr2 = nullptr;
            *lpdwAudioBytes2 = 0;
        }
    }

    // Advance play cursor
    myPlayPosition = (myPlayPosition + dwReadBytes) % myBufferSize;

    return dwReadBytes;
}

DWORD LinuxSoundBuffer::GetBytesInBuffer() const
{
    const std::lock_guard<std::mutex> guard(myMutex);
    const DWORD available = RingDistance(this->myPlayPosition, this->myWritePosition, this->myBufferSize);
    return available;
}

HRESULT LinuxSoundBuffer::GetCurrentPosition(LPDWORD lpdwCurrentPlayCursor, LPDWORD lpdwCurrentWriteCursor)
{
    *lpdwCurrentPlayCursor = this->myPlayPosition;
    *lpdwCurrentWriteCursor = this->myWritePosition;
    return DS_OK;
}

HRESULT LinuxSoundBuffer::GetVolume(LONG *lplVolume)
{
    *lplVolume = this->myVolume;
    return DS_OK;
}

double LinuxSoundBuffer::GetLogarithmicVolume() const
{
    const double volume = (double(myVolume) - DSBVOLUME_MIN) / (0.0 - DSBVOLUME_MIN);
    return volume;
}

size_t LinuxSoundBuffer::GetBufferUnderruns() const
{
    return myNumberOfUnderruns;
}

void LinuxSoundBuffer::ResetUnderruns()
{
    myNumberOfUnderruns = 0;
}

bool DSAvailable()
{
    return true;
}
