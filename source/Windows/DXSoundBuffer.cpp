/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 1994-1996, Michael O'Brien
Copyright (C) 1999-2001, Oliver Schmidt
Copyright (C) 2002-2005, Tom Charlesworth
Copyright (C) 2006-2007, Tom Charlesworth, Michael Pohoreski

AppleWin is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

AppleWin is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with AppleWin; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/* Description: DirectX implementation of SoundBuffer
 *
 * Author: Tom Charlesworth
 */

#include "StdAfx.h"

#include "DXSoundBuffer.h"

#include "Core.h" // for g_fh
#include "Interface.h"
#include "SoundCore.h"

//-----------------------------------------------------------------------------

#define MAX_SOUND_DEVICES 10

static std::string sound_devices[MAX_SOUND_DEVICES];
static GUID sound_device_guid[MAX_SOUND_DEVICES];
static int num_sound_devices = 0;

static LPDIRECTSOUND g_lpDS = NULL;
static bool g_bDSAvailable = false;
static UINT g_uDSInitRefCount = 0;

//-----------------------------------------------------------------------------

HRESULT DXSoundBuffer::Init(DWORD dwFlags, DWORD dwBufferSize, DWORD nSampleRate, int nChannels, LPCSTR pDevName)
{
	if (!g_lpDS)
		return E_FAIL;

	WAVEFORMATEX wavfmt;
	DSBUFFERDESC dsbdesc;

	wavfmt.wFormatTag = WAVE_FORMAT_PCM;
	wavfmt.nChannels = nChannels;
	wavfmt.nSamplesPerSec = nSampleRate;
	wavfmt.wBitsPerSample = 16;
	wavfmt.nBlockAlign = wavfmt.nChannels == 1 ? 2 : 4;
	wavfmt.nAvgBytesPerSec = wavfmt.nBlockAlign * wavfmt.nSamplesPerSec;

	memset(&dsbdesc, 0, sizeof(dsbdesc));
	dsbdesc.dwSize = sizeof(dsbdesc);
	dsbdesc.dwBufferBytes = dwBufferSize;
	dsbdesc.lpwfxFormat = &wavfmt;
	dsbdesc.dwFlags = dwFlags | DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_STICKYFOCUS;

	// Are buffers released when g_lpDS OR m_pBuffer is released?
	// . From DirectX doc:
	//   "Buffer objects are owned by the device object that created them. When the
	//    device object is released, all buffers created by that object are also released..."
	return g_lpDS->CreateSoundBuffer(&dsbdesc, &m_pBuffer, NULL);
}

HRESULT DXSoundBuffer::Release()
{
	if (!m_pBuffer)
		return DS_OK;

	HRESULT hr = m_pBuffer->Release();
	m_pBuffer = NULL;
	return hr;
}

HRESULT DXSoundBuffer::SetCurrentPosition(DWORD dwNewPosition)
{
	return m_pBuffer->SetCurrentPosition(dwNewPosition);
}

HRESULT DXSoundBuffer::GetCurrentPosition(LPDWORD lpdwCurrentPlayCursor, LPDWORD lpdwCurrentWriteCursor)
{
	return m_pBuffer->GetCurrentPosition(lpdwCurrentPlayCursor, lpdwCurrentWriteCursor);
}

HRESULT DXSoundBuffer::Lock(DWORD dwWriteCursor, DWORD dwWriteBytes, LPVOID* lplpvAudioPtr1, DWORD* lpdwAudioBytes1, LPVOID* lplpvAudioPtr2, DWORD* lpdwAudioBytes2, DWORD dwFlags)
{
	return m_pBuffer->Lock(dwWriteCursor, dwWriteBytes, lplpvAudioPtr1, lpdwAudioBytes1, lplpvAudioPtr2, lpdwAudioBytes2, dwFlags);
}

HRESULT DXSoundBuffer::Unlock(LPVOID lpvAudioPtr1, DWORD dwAudioBytes1, LPVOID lpvAudioPtr2, DWORD dwAudioBytes2)
{
	return m_pBuffer->Unlock(lpvAudioPtr1, dwAudioBytes1, lpvAudioPtr2, dwAudioBytes2);
}

HRESULT DXSoundBuffer::Stop()
{
	return m_pBuffer->Stop();
}

HRESULT DXSoundBuffer::Play(DWORD dwReserved1, DWORD dwReserved2, DWORD dwFlags)
{
	return m_pBuffer->Play(dwReserved1, dwReserved2, dwFlags);
}

HRESULT DXSoundBuffer::SetVolume(LONG lVolume)
{
	return m_pBuffer->SetVolume(lVolume);
}

HRESULT DXSoundBuffer::GetVolume(LONG* lplVolume)
{
	return m_pBuffer->GetVolume(lplVolume);
}

HRESULT DXSoundBuffer::GetStatus(LPDWORD lpdwStatus)
{
	return m_pBuffer->GetStatus(lpdwStatus);
}

HRESULT DXSoundBuffer::Restore()
{
	return m_pBuffer->Restore();
}

//-----------------------------------------------------------------------------

bool DSAvailable()
{
	return g_bDSAvailable;
}

static BOOL CALLBACK DSEnumProc(LPGUID lpGUID, LPCTSTR lpszDesc, LPCTSTR lpszDrvName, LPVOID lpContext)
{
	int i = num_sound_devices;
	if (i == MAX_SOUND_DEVICES)
		return TRUE;
	if (lpGUID != NULL)
		memcpy(&sound_device_guid[i], lpGUID, sizeof(GUID));
	else
		memset(&sound_device_guid[i], 0, sizeof(GUID));
	sound_devices[i] = lpszDesc;

	if (g_fh) fprintf(g_fh, "%d: %s - %s\n", i, lpszDesc, lpszDrvName);

	num_sound_devices++;
	return TRUE;
}

bool DSInit()
{
	if (g_bDSAvailable)
	{
		g_uDSInitRefCount++;
		return true;		// Already initialised successfully
	}

	num_sound_devices = 0;
	HRESULT hr = DirectSoundEnumerate((LPDSENUMCALLBACK)DSEnumProc, NULL);
	if (FAILED(hr))
	{
		if (g_fh) fprintf(g_fh, "DSEnumerate failed (%08X)\n", (unsigned)hr);
		return false;
	}

	if (g_fh)
	{
		fprintf(g_fh, "Number of sound devices = %d\n", num_sound_devices);
	}

	bool bCreatedOK = false;
	for (int x = 0; x < num_sound_devices; x++)
	{
		hr = DirectSoundCreate(&sound_device_guid[x], &g_lpDS, NULL);
		if (SUCCEEDED(hr))
		{
			if (g_fh) fprintf(g_fh, "DSCreate succeeded for sound device #%d\n", x);
			bCreatedOK = true;
			break;
		}

		if (g_fh) fprintf(g_fh, "DSCreate failed for sound device #%d (%08X)\n", x, (unsigned)hr);
	}
	if (!bCreatedOK)
	{
		if (g_fh) fprintf(g_fh, "DSCreate failed for all sound devices\n");
		return false;
	}

	HWND hwnd = GetFrame().g_hFrameWindow;
	_ASSERT(hwnd);
	hr = g_lpDS->SetCooperativeLevel(hwnd, DSSCL_NORMAL);
	if (FAILED(hr))
	{
		if (g_fh) fprintf(g_fh, "SetCooperativeLevel failed (%08X)\n", (unsigned)hr);
		return false;
	}

	DSCAPS DSCaps;
	memset(&DSCaps, 0, sizeof(DSCAPS));
	DSCaps.dwSize = sizeof(DSCAPS);
	hr = g_lpDS->GetCaps(&DSCaps);
	if (FAILED(hr))
	{
		if (g_fh) fprintf(g_fh, "GetCaps failed (%08X)\n", (unsigned)hr);
		// Not fatal: so continue...
	}

	g_bDSAvailable = true;

	g_uDSInitRefCount = 1;

	return true;
}

void DSUninit()
{
	if (!g_bDSAvailable)
		return;

	_ASSERT(g_uDSInitRefCount);

	if (g_uDSInitRefCount == 0)
		return;

	g_uDSInitRefCount--;

	if (g_uDSInitRefCount)
		return;

	//

	_ASSERT(g_uNumVoices == 0);

	SAFE_RELEASE(g_lpDS);
	g_bDSAvailable = false;

	SoundCore_StopTimer();
}