#pragma once

#include "winhandles.h"
#include "winerror.h"
#include "mmreg.h"
#include "guiddef.h"

#include <atomic>
#include <vector>
#include <memory>
#include <mutex>

#define DS_OK				0

#define DSBCAPS_CTRLVOLUME          0x00000080
#define DSBCAPS_LOCSOFTWARE         0x00000008
#define DSBCAPS_CTRLPOSITIONNOTIFY  0x00000100
#define DSBCAPS_STICKYFOCUS         0x00004000
#define DSBCAPS_GETCURRENTPOSITION2 0x00010000

#define DSBVOLUME_MIN               -10000
#define DSBVOLUME_MAX               0

#define DSBPLAY_LOOPING             0x00000001

#define DSBSTATUS_PLAYING           0x00000001
#define DSBSTATUS_BUFFERLOST        0x00000002
#define DSBSTATUS_LOOPING           0x00000004

#define DSBLOCK_ENTIREBUFFER        0x00000002

#define _FACDS                      0x878
#define MAKE_DSHRESULT(code)        MAKE_HRESULT(1,_FACDS,code)

#define DSERR_ALLOCATED             MAKE_DSHRESULT(10)
#define DSERR_CONTROLUNAVAIL        MAKE_DSHRESULT(30)
#define DSERR_INVALIDPARAM          E_INVALIDARG
#define DSERR_INVALIDCALL           MAKE_DSHRESULT(50)
#define DSERR_GENERIC               E_FAIL
#define DSERR_PRIOLEVELNEEDED       MAKE_DSHRESULT(70)
#define DSERR_OUTOFMEMORY           E_OUTOFMEMORY
#define DSERR_BADFORMAT             MAKE_DSHRESULT(100)
#define DSERR_UNSUPPORTED           E_NOTIMPL
#define DSERR_NODRIVER              MAKE_DSHRESULT(120)
#define DSERR_ALREADYINITIALIZED    MAKE_DSHRESULT(130)
#define DSERR_NOAGGREGATION         CLASS_E_NOAGGREGATION
#define DSERR_BUFFERLOST            MAKE_DSHRESULT(150)
#define DSERR_OTHERAPPHASPRIO       MAKE_DSHRESULT(160)
#define DSERR_UNINITIALIZED         MAKE_DSHRESULT(170)
#define DSERR_NOINTERFACE           E_NOINTERFACE

#define	DSSCL_NORMAL                1

typedef BOOL (CALLBACK *LPDSENUMCALLBACK)(LPGUID,LPCSTR,LPCSTR,LPVOID);

HRESULT DirectSoundEnumerate(LPDSENUMCALLBACK lpDSEnumCallback, LPVOID lpContext);

typedef struct {
  DWORD  dwSize;
  DWORD  dwFlags;
  DWORD  dwFormats;
  DWORD  dwChannels;
} DSCCAPS, DSCAPS, *LPDSCCAPS;

typedef const DSCCAPS *LPCDSCCAPS;

typedef struct _DSBUFFERDESC
{
  DWORD		dwSize;
  DWORD		dwFlags;
  DWORD		dwBufferBytes;
  DWORD		dwReserved;
  LPWAVEFORMATEX	lpwfxFormat;
  GUID		guid3DAlgorithm;
} DSBUFFERDESC,*LPDSBUFFERDESC;
typedef const DSBUFFERDESC *LPCDSBUFFERDESC;

typedef struct _DSBPOSITIONNOTIFY
{
  DWORD	dwOffset;
  HANDLE	hEventNotify;
} DSBPOSITIONNOTIFY,*LPDSBPOSITIONNOTIFY;
typedef const DSBPOSITIONNOTIFY *LPCDSBPOSITIONNOTIFY;

struct IDirectSoundNotify
{
};
typedef struct IDirectSoundNotify *LPDIRECTSOUNDNOTIFY,**LPLPDIRECTSOUNDNOTIFY;

class IDirectSoundBuffer : public IUnknown
{
  std::vector<char> mySoundBuffer;

  size_t myPlayPosition = 0;
  size_t myWritePosition = 0;
  WORD myStatus = 0;
  LONG myVolume = DSBVOLUME_MAX;

  // updated by the callback
  std::atomic_size_t myNumberOfUnderruns;
  std::mutex myMutex;

 public:
  const size_t bufferSize;
  const size_t sampleRate;
  const size_t channels;
  const size_t bitsPerSample;
  const size_t flags;

  IDirectSoundBuffer(const size_t bufferSize, const size_t channels, const size_t sampleRate, const size_t bitsPerSample, const size_t flags);
  virtual HRESULT Release() override;

  HRESULT SetCurrentPosition( DWORD dwNewPosition );
  HRESULT GetCurrentPosition( LPDWORD lpdwCurrentPlayCursor, LPDWORD lpdwCurrentWriteCursor );

  HRESULT Lock( DWORD dwWriteCursor, DWORD dwWriteBytes, LPVOID * lplpvAudioPtr1, DWORD * lpdwAudioBytes1, LPVOID * lplpvAudioPtr2, DWORD * lpdwAudioBytes2, DWORD dwFlags );
  HRESULT Unlock( LPVOID lpvAudioPtr1, DWORD dwAudioBytes1, LPVOID lpvAudioPtr2, DWORD dwAudioBytes2 );

  HRESULT Stop();
  HRESULT Play( DWORD dwReserved1, DWORD dwReserved2, DWORD dwFlags );

  HRESULT SetVolume( LONG lVolume );
  HRESULT GetVolume( LONG * lplVolume );

  HRESULT GetStatus( LPDWORD lpdwStatus );
  HRESULT Restore();

  // NOT part of Windows API
  DWORD Read( DWORD dwReadBytes, LPVOID * lplpvAudioPtr1, DWORD * lpdwAudioBytes1, LPVOID * lplpvAudioPtr2, DWORD * lpdwAudioBytes2);
  DWORD GetBytesInBuffer();
  size_t GetBufferUnderruns() const;
  void ResetUnderrruns();
  double GetLogarithmicVolume() const;  // in [0, 1]
};
typedef class IDirectSoundBuffer *LPDIRECTSOUNDBUFFER,**LPLPDIRECTSOUNDBUFFER;

struct IDirectSound : public IUnknown
{
  HRESULT CreateSoundBuffer( LPCDSBUFFERDESC lpcDSBufferDesc, IDirectSoundBuffer **lplpDirectSoundBuffer, IUnknown FAR* pUnkOuter );
  HRESULT SetCooperativeLevel( HWND hwnd, DWORD dwLevel );
  HRESULT GetCaps(LPDSCCAPS pDSCCaps);
};
typedef struct IDirectSound *LPDIRECTSOUND;

HRESULT WINAPI DirectSoundCreate(LPGUID lpGuid, LPDIRECTSOUND* ppDS, LPUNKNOWN pUnkOuter);
