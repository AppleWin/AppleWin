#pragma once

#include "winbase.h"
#include "winerror.h"
#include "mmreg.h"
#include "guiddef.h"

#include <vector>
#include <memory>
#include <mutex>

#define IID_IDirectSoundNotify 1234
#define DS_OK				0
#define DSBPN_OFFSETSTOP		-1

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

#define DSERR_BUFFERLOST	    MAKE_DSHRESULT(150)

#define DSBLOCK_ENTIREBUFFER        0x00000002

#define	_FACDS		0x878
#define	MAKE_DSHRESULT(code)		MAKE_HRESULT(1,_FACDS,code)

#define	DSSCL_NORMAL		1

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
  HRESULT SetNotificationPositions(DWORD cPositionNotifies, LPCDSBPOSITIONNOTIFY lpcPositionNotifies);
};
typedef struct IDirectSoundNotify *LPDIRECTSOUNDNOTIFY,**LPLPDIRECTSOUNDNOTIFY;

class IDirectSoundBuffer : public IUnknown
{
  const std::unique_ptr<IDirectSoundNotify> mySoundNotify;
  std::vector<char> mySoundBuffer;

  size_t myPlayPosition = 0;
  size_t myWritePosition = 0;
  WORD myStatus = 0;
  LONG myVolume = DSBVOLUME_MAX;

 public:
  const size_t bufferSize;
  const size_t sampleRate;
  const size_t channels;
  const size_t bitsPerSample;
  const size_t flags;

  std::mutex mutex;

  IDirectSoundBuffer(const size_t bufferSize, const size_t channels, const size_t sampleRate, const size_t bitsPerSample, const size_t flags);
  HRESULT Release() override;

  HRESULT QueryInterface(int riid, void **ppvObject);

  HRESULT SetCurrentPosition( DWORD dwNewPosition );
  HRESULT GetCurrentPosition( LPDWORD lpdwCurrentPlayCursor, LPDWORD lpdwCurrentWriteCursor );

  // Read is NOT part of Windows API
  HRESULT Read( DWORD dwReadBytes, LPVOID * lplpvAudioPtr1, DWORD * lpdwAudioBytes1, LPVOID * lplpvAudioPtr2, DWORD * lpdwAudioBytes2);
  DWORD GetBytesInBuffer();

  HRESULT Lock( DWORD dwWriteCursor, DWORD dwWriteBytes, LPVOID * lplpvAudioPtr1, DWORD * lpdwAudioBytes1, LPVOID * lplpvAudioPtr2, DWORD * lpdwAudioBytes2, DWORD dwFlags );
  HRESULT Unlock( LPVOID lpvAudioPtr1, DWORD dwAudioBytes1, LPVOID lpvAudioPtr2, DWORD dwAudioBytes2 );

  HRESULT Stop();
  HRESULT Play( DWORD dwReserved1, DWORD dwReserved2, DWORD dwFlags );

  HRESULT SetVolume( LONG lVolume );
  HRESULT GetVolume( LONG * lplVolume );

  double GetLogarithmicVolume() const;  // in [0, 1]

  HRESULT GetStatus( LPDWORD lpdwStatus );
  HRESULT Restore();
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
