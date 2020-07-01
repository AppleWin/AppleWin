#include "linux/windows/dsound.h"
#include "linux/windows/winerror.h"

#include <cstring>

HRESULT IDirectSoundNotify::SetNotificationPositions(DWORD cPositionNotifies, LPCDSBPOSITIONNOTIFY lpcPositionNotifies)
{
  return DS_OK;
}

IDirectSoundBuffer::IDirectSoundBuffer(const size_t bufferSize, const size_t channels, const size_t sampleRate, const size_t bitsPerSample, const size_t flags)
  : myBufferSize(bufferSize)
  , myChannels(channels)
  , mySampleRate(sampleRate)
  , myBitsPerSample(bitsPerSample)
  , myFlags(flags)
  , mySoundNotify(new IDirectSoundNotify)
  , mySoundBuffer(bufferSize)
{
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

HRESULT IDirectSoundBuffer::GetCurrentPosition( LPDWORD lpdwCurrentPlayCursor, LPDWORD lpdwCurrentWriteCursor )
{
  *lpdwCurrentPlayCursor = this->myWritePosition;
  *lpdwCurrentWriteCursor = this->myWritePosition;
  return DS_OK;
}

HRESULT IDirectSoundBuffer::GetVolume( LONG * lplVolume )
{
  *lplVolume = this->myVolume;
  return DS_OK;
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
