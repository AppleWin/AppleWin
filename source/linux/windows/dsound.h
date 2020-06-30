#pragma once

#include "linux/windows/winbase.h"
#include "linux/windows/winerror.h"

#include <vector>
#include <memory>

#define IID_IDirectSoundNotify 1234
#define DS_OK				0
#define DSBPN_OFFSETSTOP		-1

#define DSBCAPS_CTRLVOLUME          0x00000080
#define DSBCAPS_CTRLPOSITIONNOTIFY  0x00000100
#define DSBCAPS_LOCSOFTWARE         0x00000008

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

struct IDirectSoundBuffer
{
  std::unique_ptr<IDirectSoundNotify> soundNotify;
  std::vector<SHORT> soundBuffer;

  size_t playPosition = 0;
  size_t writePosition = 0;

  LONG volume = DSBVOLUME_MIN;
  DWORD flags = 0;
  DWORD bufferSize = 0;
  DWORD sampleRate = 0;
  WORD status = 0;
  int channels = 0;

  IDirectSoundBuffer();

  HRESULT QueryInterface(int riid, void **ppvObject);

  HRESULT SetCurrentPosition( DWORD dwNewPosition );
  HRESULT GetCurrentPosition( LPDWORD lpdwCurrentPlayCursor, LPDWORD lpdwCurrentWriteCursor );

  HRESULT Lock( DWORD dwWriteCursor, DWORD dwWriteBytes, LPVOID * lplpvAudioPtr1, DWORD * lpdwAudioBytes1, LPVOID * lplpvAudioPtr2, DWORD * lpdwAudioBytes2, DWORD dwFlags );
  HRESULT Unlock( LPVOID lpvAudioPtr1, DWORD dwAudioBytes1, LPVOID lpvAudioPtr2, DWORD dwAudioBytes2 );

  HRESULT Stop();
  HRESULT Play( DWORD dwReserved1, DWORD dwReserved2, DWORD dwFlags );

  HRESULT SetVolume( LONG lVolume );

  HRESULT GetStatus( LPDWORD lpdwStatus );
  HRESULT Restore();
};
typedef struct IDirectSoundBuffer *LPDIRECTSOUNDBUFFER,**LPLPDIRECTSOUNDBUFFER;
