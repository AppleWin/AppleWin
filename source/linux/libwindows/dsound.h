#pragma once

#include "winerror.h"
#include "mmreg.h"
#include "guiddef.h"

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
  LPCSTR  szName;  // only in the linux version to differentiate the channels
} DSBUFFERDESC,*LPDSBUFFERDESC;
typedef const DSBUFFERDESC *LPCDSBUFFERDESC;
