#include "StdAfx.h"

#include "Common.h"
#include "SoundCore.h"
#include "Log.h"


bool g_bDSAvailable = false;
static int g_nErrorInc = 20;	// Old: 1

bool DSZeroVoiceWritableBuffer(PVOICE Voice, const char* pszDevName, DWORD dwBufferSize)
{
  return true;
}

void DSReleaseSoundBuffer(VOICE* pVoice)
{
  delete pVoice->lpDSBvoice;
  pVoice->lpDSBvoice = nullptr;
}

int SoundCore_GetErrorInc()
{
  return g_nErrorInc;
}

LONG NewVolume(DWORD dwVolume, DWORD dwVolumeMax)
{
  float fVol = (float) dwVolume / (float) dwVolumeMax;	// 0.0=Max, 1.0=Min

  return (LONG) ((float) DSBVOLUME_MIN * fVol);
}

HRESULT DSGetSoundBuffer(VOICE* pVoice, DWORD dwFlags, DWORD dwBufferSize, DWORD nSampleRate, int nChannels)
{
  IDirectSoundBuffer * dsVoice = new IDirectSoundBuffer;

  dsVoice->flags = dwFlags;
  dsVoice->bufferSize = dwBufferSize;
  dsVoice->sampleRate = nSampleRate;
  dsVoice->channels = nChannels;
  dsVoice->soundBuffer.resize(dwBufferSize);

  pVoice->lpDSBvoice = dsVoice;

  return S_OK;
}

bool DSInit()
{
  g_bDSAvailable = true;
  return true;
}

//-----------------------------------------------------------------------------

void DSUninit()
{
  g_bDSAvailable = false;
}

bool DSZeroVoiceBuffer(PVOICE Voice, const char* pszDevName, DWORD dwBufferSize)
{
#ifdef NO_DIRECT_X

  return false;

#else

  DWORD dwDSLockedBufferSize = 0;    // Size of the locked DirectSound buffer
  SHORT* pDSLockedBuffer;

  _ASSERT(Voice->lpDSBvoice);
  HRESULT hr = Voice->lpDSBvoice->Stop();
  if(FAILED(hr))
  {
    if(g_fh) fprintf(g_fh, "%s: DSStop failed (%08X)\n",pszDevName,hr);
    return false;
  }

  hr = DSGetLock(Voice->lpDSBvoice, 0, 0, &pDSLockedBuffer, &dwDSLockedBufferSize, NULL, 0);
  if(FAILED(hr))
  {
    if(g_fh) fprintf(g_fh, "%s: DSGetLock failed (%08X)\n",pszDevName,hr);
    return false;
  }

  _ASSERT(dwDSLockedBufferSize == dwBufferSize);
  memset(pDSLockedBuffer, 0x00, dwDSLockedBufferSize);

  hr = Voice->lpDSBvoice->Unlock((void*)pDSLockedBuffer, dwDSLockedBufferSize, NULL, 0);
  if(FAILED(hr))
  {
    if(g_fh) fprintf(g_fh, "%s: DSUnlock failed (%08X)\n",pszDevName,hr);
    return false;
  }

  hr = Voice->lpDSBvoice->Play(0,0,DSBPLAY_LOOPING);
  if(FAILED(hr))
  {
    if(g_fh) fprintf(g_fh, "%s: DSPlay failed (%08X)\n",pszDevName,hr);
    return false;
  }

  return true;
#endif // NO_DIRECT_X
}

bool DSGetLock(LPDIRECTSOUNDBUFFER pVoice, DWORD dwOffset, DWORD dwBytes,
	       SHORT** ppDSLockedBuffer0, DWORD* pdwDSLockedBufferSize0,
	       SHORT** ppDSLockedBuffer1, DWORD* pdwDSLockedBufferSize1)
{
  DWORD nStatus;
  HRESULT hr = pVoice->GetStatus(&nStatus);
  if(hr != DS_OK)
    return false;

  if(nStatus & DSBSTATUS_BUFFERLOST)
  {
    do
    {
      hr = pVoice->Restore();
      if(hr == DSERR_BUFFERLOST)
	Sleep(10);
    }
    while(hr != DS_OK);
  }

  // Get write only pointer(s) to sound buffer
  if(dwBytes == 0)
  {
    if(FAILED(hr = pVoice->Lock(0, 0,
				(void**)ppDSLockedBuffer0, pdwDSLockedBufferSize0,
				(void**)ppDSLockedBuffer1, pdwDSLockedBufferSize1,
				DSBLOCK_ENTIREBUFFER)))
      return false;
  }
  else
  {
    if(FAILED(hr = pVoice->Lock(dwOffset, dwBytes,
				(void**)ppDSLockedBuffer0, pdwDSLockedBufferSize0,
				(void**)ppDSLockedBuffer1, pdwDSLockedBufferSize1,
				0)))
      return false;
  }

  return true;
}
