#include "dsound.h"
#include "linux/linuxinterface.h"

#include <cstring>

HRESULT IDirectSoundNotify::SetNotificationPositions(DWORD cPositionNotifies, LPCDSBPOSITIONNOTIFY lpcPositionNotifies)
{
  return DS_OK;
}

IDirectSoundBuffer::IDirectSoundBuffer(const size_t aBufferSize, const size_t aChannels, const size_t aSampleRate, const size_t aBitsPerSample, const size_t aFlags)
  : mySoundNotify(new IDirectSoundNotify)
  , mySoundBuffer(aBufferSize)
  , bufferSize(aBufferSize)
  , sampleRate(aSampleRate)
  , channels(aChannels)
  , bitsPerSample(aBitsPerSample)
  , flags(aFlags)
{
}

HRESULT IDirectSoundBuffer::Release()
{
  // unregister *before* the destructor is called (in Release below)
  // makes things a little bit more linear
  unregisterSoundBuffer(this);

  // do not call any more methods after the next function returns
  return IUnknown::Release();
}

HRESULT IDirectSoundBuffer::QueryInterface(int riid, void **ppvObject)
{
  if (riid == IID_IDirectSoundNotify)
  {
    *ppvObject = mySoundNotify.get();
    return S_OK;
  }

  return E_NOINTERFACE;
}

HRESULT IDirectSoundBuffer::Unlock( LPVOID lpvAudioPtr1, DWORD dwAudioBytes1, LPVOID lpvAudioPtr2, DWORD dwAudioBytes2 )
{
  const size_t totalWrittenBytes = dwAudioBytes1 + dwAudioBytes2;
  this->myWritePosition = (this->myWritePosition + totalWrittenBytes) % this->mySoundBuffer.size();
  mutex.unlock();
  return DS_OK;
}

HRESULT IDirectSoundBuffer::Stop()
{
  const DWORD mask = DSBSTATUS_PLAYING | DSBSTATUS_LOOPING;
  this->myStatus &= ~mask;
  return DS_OK;
}

HRESULT IDirectSoundBuffer::SetCurrentPosition( DWORD dwNewPosition )
{
  return DS_OK;
}

HRESULT IDirectSoundBuffer::Play( DWORD dwReserved1, DWORD dwReserved2, DWORD dwFlags )
{
  this->myStatus |= DSBSTATUS_PLAYING;
  if (dwFlags & DSBPLAY_LOOPING)
  {
    this->myStatus |= DSBSTATUS_LOOPING;
  }
  return S_OK;
}

HRESULT IDirectSoundBuffer::SetVolume( LONG lVolume )
{
  this->myVolume = lVolume;
  return DS_OK;
}

HRESULT IDirectSoundBuffer::GetStatus( LPDWORD lpdwStatus )
{
  *lpdwStatus = this->myStatus;
  return DS_OK;
}

HRESULT IDirectSoundBuffer::Restore()
{
  return DS_OK;
}

HRESULT IDirectSoundBuffer::Lock( DWORD dwWriteCursor, DWORD dwWriteBytes, LPVOID * lplpvAudioPtr1, DWORD * lpdwAudioBytes1, LPVOID * lplpvAudioPtr2, DWORD * lpdwAudioBytes2, DWORD dwFlags )
{
  mutex.lock();
  // No attempt is made at restricting write buffer not to overtake play cursor
  if (dwFlags & DSBLOCK_ENTIREBUFFER)
  {
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

HRESULT IDirectSoundBuffer::Read( DWORD dwReadBytes, LPVOID * lplpvAudioPtr1, DWORD * lpdwAudioBytes1, LPVOID * lplpvAudioPtr2, DWORD * lpdwAudioBytes2)
{
  const std::lock_guard<std::mutex> guard(mutex);

  // Read up to dwReadBytes, never going past the write cursor
  // Positions are updated immediately
  const DWORD available = (this->myWritePosition - this->myPlayPosition) % this->bufferSize;
  dwReadBytes = std::min(dwReadBytes, available);

  const DWORD availableInFirstPart = this->mySoundBuffer.size() - this->myPlayPosition;

  *lplpvAudioPtr1 = this->mySoundBuffer.data() + this->myPlayPosition;
  *lpdwAudioBytes1 = std::min(availableInFirstPart, dwReadBytes);

  if (lplpvAudioPtr2 && lpdwAudioBytes2)
  {
    if (*lpdwAudioBytes1 < dwReadBytes)
    {
      *lplpvAudioPtr2 = this->mySoundBuffer.data();
      *lpdwAudioBytes2 = dwReadBytes - *lpdwAudioBytes1;
    }
    else
    {
      *lplpvAudioPtr2 = nullptr;
      *lpdwAudioBytes2 = 0;
    }
  }
  this->myPlayPosition = (this->myPlayPosition + dwReadBytes) % this->mySoundBuffer.size();
  return DS_OK;
}

DWORD IDirectSoundBuffer::GetBytesInBuffer()
{
  const std::lock_guard<std::mutex> guard(mutex);
  const DWORD available = (this->myWritePosition - this->myPlayPosition) % this->bufferSize;
  return available;
}

HRESULT IDirectSoundBuffer::GetCurrentPosition( LPDWORD lpdwCurrentPlayCursor, LPDWORD lpdwCurrentWriteCursor )
{
  *lpdwCurrentPlayCursor = this->myPlayPosition;
  *lpdwCurrentWriteCursor = this->myWritePosition;
  return DS_OK;
}

HRESULT IDirectSoundBuffer::GetVolume( LONG * lplVolume )
{
  *lplVolume = this->myVolume;
  return DS_OK;
}

double IDirectSoundBuffer::GetLogarithmicVolume() const
{
  const double volume = (double(myVolume) - DSBVOLUME_MIN) / (0.0 - DSBVOLUME_MIN);
  return volume;
}

HRESULT WINAPI DirectSoundCreate(LPGUID lpGuid, LPDIRECTSOUND* ppDS, LPUNKNOWN pUnkOuter)
{
  *ppDS = new IDirectSound();
  return DS_OK;
}

HRESULT DirectSoundEnumerate(LPDSENUMCALLBACK lpDSEnumCallback, LPVOID lpContext)
{
  GUID guid = 123;
  lpDSEnumCallback(&guid, "audio", "linux", lpContext);
  return DS_OK;
}

HRESULT IDirectSound::CreateSoundBuffer( LPCDSBUFFERDESC lpcDSBufferDesc, IDirectSoundBuffer **lplpDirectSoundBuffer, IUnknown FAR* pUnkOuter )
{
  const size_t bufferSize = lpcDSBufferDesc->dwBufferBytes;
  const size_t channels = lpcDSBufferDesc->lpwfxFormat->nChannels;
  const size_t sampleRate = lpcDSBufferDesc->lpwfxFormat->nSamplesPerSec;
  const size_t bitsPerSample = lpcDSBufferDesc->lpwfxFormat->wBitsPerSample;
  const size_t flags = lpcDSBufferDesc->dwFlags;
  IDirectSoundBuffer * dsb = new IDirectSoundBuffer(bufferSize, channels, sampleRate, bitsPerSample, flags);

  registerSoundBuffer(dsb);

  *lplpDirectSoundBuffer = dsb;
  return DS_OK;
}

HRESULT IDirectSound::SetCooperativeLevel( HWND hwnd, DWORD dwLevel )
{
  return DS_OK;
}

HRESULT IDirectSound::GetCaps(LPDSCCAPS pDSCCaps)
{
  memset(pDSCCaps, 0, sizeof(*pDSCCaps));
  return DS_OK;
}
