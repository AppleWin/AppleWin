#include "linux/windows/dsound.h"
#include "linux/windows/winerror.h"

HRESULT IDirectSoundNotify::SetNotificationPositions(DWORD cPositionNotifies, LPCDSBPOSITIONNOTIFY lpcPositionNotifies)
{
  return DS_OK;
}

IDirectSoundBuffer::IDirectSoundBuffer(): soundNotify(new IDirectSoundNotify)
{
}

HRESULT IDirectSoundBuffer::QueryInterface(int riid, void **ppvObject)
{
  if (riid == IID_IDirectSoundNotify)
  {
    *ppvObject = soundNotify.get();
    return S_OK;
  }

  return E_NOINTERFACE;
}

HRESULT IDirectSoundBuffer::Unlock( LPVOID lpvAudioPtr1, DWORD dwAudioBytes1, LPVOID lpvAudioPtr2, DWORD dwAudioBytes2 )
{
  const size_t totalWrittenBytes = dwAudioBytes1 + dwAudioBytes2;
  writePosition = (writePosition + totalWrittenBytes) % this->soundBuffer.size();
  return DS_OK;
}

HRESULT IDirectSoundBuffer::Stop()
{
  const DWORD mask = DSBSTATUS_PLAYING | DSBSTATUS_LOOPING;
  this->status &= ~mask;
  return DS_OK;
}

HRESULT IDirectSoundBuffer::SetCurrentPosition( DWORD dwNewPosition )
{
  return DS_OK;
}

HRESULT IDirectSoundBuffer::Play( DWORD dwReserved1, DWORD dwReserved2, DWORD dwFlags )
{
  this->status |= DSBSTATUS_PLAYING;
  if (dwFlags & DSBPLAY_LOOPING)
  {
    this->status |= DSBSTATUS_LOOPING;
  }
  return S_OK;
}

HRESULT IDirectSoundBuffer::SetVolume( LONG lVolume )
{
  this->volume = lVolume;
  return DS_OK;
}

HRESULT IDirectSoundBuffer::GetStatus( LPDWORD lpdwStatus )
{
  *lpdwStatus = this->status;
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
    *lplpvAudioPtr1 = this->soundBuffer.data();
    *lpdwAudioBytes1 = this->soundBuffer.size();
    if (lplpvAudioPtr2 && lpdwAudioBytes2)
    {
      *lplpvAudioPtr2 = nullptr;
      *lpdwAudioBytes2 = 0;
    }
  }
  else
  {
    const DWORD availableInFirstPart = this->soundBuffer.size() - dwWriteCursor;

    *lplpvAudioPtr1 = this->soundBuffer.data() + dwWriteCursor;
    *lpdwAudioBytes1 = std::min(availableInFirstPart, dwWriteBytes);

    if (lplpvAudioPtr2 && lpdwAudioBytes2)
    {
      if (*lpdwAudioBytes1 < dwWriteBytes)
      {
	*lplpvAudioPtr2 = this->soundBuffer.data();
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
  *lpdwCurrentPlayCursor = this->writePosition;
  *lpdwCurrentWriteCursor = this->writePosition;
  return DS_OK;
}
