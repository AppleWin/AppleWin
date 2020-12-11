/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 1994-1996, Michael O'Brien
Copyright (C) 1999-2001, Oliver Schmidt
Copyright (C) 2002-2005, Tom Charlesworth
Copyright (C) 2006-2010, Tom Charlesworth, Michael Pohoreski

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

/* Description: Disk Image
 *
 * Author: Various
 */

#include "StdAfx.h"

//=============================================================================
// DirectInput interface
//=============================================================================

//#define STRICT

#include "Windows/DirectInput.h"
#include "SoundCore.h"	// SAFE_RELEASE()
#include "Log.h"
#include "Common.h"

#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>

extern bool g_bDisableDirectInput;	// currently in AppleWin.h

namespace DIMouse
{

	static LPDIRECTINPUT8       g_pDI    = NULL;         
	static LPDIRECTINPUTDEVICE8 g_pMouse = NULL;     
	#define SAMPLE_BUFFER_SIZE  16      // arbitrary number of buffer elements

	static UINT_PTR g_TimerIDEvent = 0;

	//-----------------------------------------------------------------------------
	// Name: OnCreateDevice()
	// Desc: Sets up the mouse device using the flags from the dialog.
	//-----------------------------------------------------------------------------
	HRESULT DirectInputInit( HWND hDlg )
	{
		LogFileOutput("DirectInputInit: g_bDisableDirectInput=%d\n", g_bDisableDirectInput);
		if (g_bDisableDirectInput)
			return S_OK;

#ifdef NO_DIRECT_X

		return E_FAIL;

#else // NO_DIRECT_X

		HRESULT hr;
		BOOL    bExclusive;
		BOOL    bForeground;
		BOOL    bImmediate;
		DWORD   dwCoopFlags;

		DirectInputUninit(hDlg);
		LogFileOutput("DirectInputInit: DirectInputUninit()\n");

		// Determine where the buffer would like to be allocated 
		bExclusive         = FALSE;
		bForeground        = FALSE;	// Otherwise get DIERR_OTHERAPPHASPRIO (== E_ACCESSDENIED) on Acquire()
		bImmediate         = TRUE;

		if( bExclusive )
			dwCoopFlags = DISCL_EXCLUSIVE;
		else
			dwCoopFlags = DISCL_NONEXCLUSIVE;

		if( bForeground )
			dwCoopFlags |= DISCL_FOREGROUND;
		else
			dwCoopFlags |= DISCL_BACKGROUND;

		// Create a DInput object
		hr = DirectInput8Create( GetModuleHandle(NULL), DIRECTINPUT_VERSION, IID_IDirectInput8, (VOID**)&g_pDI, NULL );
		LogFileOutput("DirectInputInit: DirectInput8Create(), hr=0x%08X\n", hr);
		if (FAILED(hr))
			return hr;

		// Obtain an interface to the system mouse device.
		hr = g_pDI->CreateDevice( GUID_SysMouse, &g_pMouse, NULL );
		LogFileOutput("DirectInputInit: CreateDevice(), hr=0x%08X\n", hr);
		if (FAILED(hr))
			return hr;

		// Set the data format to "mouse format" - a predefined data format 
		//
		// A data format specifies which controls on a device we
		// are interested in, and how they should be reported.
		//
		// This tells DirectInput that we will be passing a
		// DIMOUSESTATE2 structure to IDirectInputDevice::GetDeviceState.
		hr = g_pMouse->SetDataFormat( &c_dfDIMouse2 );
		LogFileOutput("DirectInputInit: SetDataFormat(), hr=0x%08X\n", hr);
		if (FAILED(hr))
			return hr;

		// Set the cooperativity level to let DirectInput know how
		// this device should interact with the system and with other
		// DirectInput applications.
		hr = g_pMouse->SetCooperativeLevel( hDlg, dwCoopFlags );
		LogFileOutput("DirectInputInit: SetCooperativeLevel(), hr=0x%08X\n", hr);
		if( hr == DIERR_UNSUPPORTED && !bForeground && bExclusive )
		{
			DirectInputUninit(hDlg);
			LogFileOutput("DirectInputInit: DirectInputUninit()n");
			//MessageBox( hDlg, _T("SetCooperativeLevel() returned DIERR_UNSUPPORTED.\n")
			//                  _T("For security reasons, background exclusive mouse\n")
			//                  _T("access is not allowed."), 
			//                  _T("Mouse"), MB_OK );
			return S_OK;
		}

		if( FAILED(hr) )
			return hr;

		if( !bImmediate )
		{
			// IMPORTANT STEP TO USE BUFFERED DEVICE DATA!
			//
			// DirectInput uses unbuffered I/O (buffer size = 0) by default.
			// If you want to read buffered data, you need to set a nonzero
			// buffer size.
			//
			// Set the buffer size to SAMPLE_BUFFER_SIZE (defined above) elements.
			//
			// The buffer size is a DWORD property associated with the device.
			DIPROPDWORD dipdw;
			dipdw.diph.dwSize       = sizeof(DIPROPDWORD);
			dipdw.diph.dwHeaderSize = sizeof(DIPROPHEADER);
			dipdw.diph.dwObj        = 0;
			dipdw.diph.dwHow        = DIPH_DEVICE;
			dipdw.dwData            = SAMPLE_BUFFER_SIZE; // Arbitary buffer size

			hr = g_pMouse->SetProperty( DIPROP_BUFFERSIZE, &dipdw.diph );
			LogFileOutput("DirectInputInit: SetProperty(), hr=0x%08X\n", hr);
			if (FAILED(hr))
				return hr;
		}

		// Acquire the newly created device
		hr = g_pMouse->Acquire();
		LogFileOutput("DirectInputInit: Acquire(), hr=0x%08X\n", hr);
		if (FAILED(hr))
			return hr;

		// Setup timer to read mouse position
		_ASSERT(g_TimerIDEvent == 0);
		g_TimerIDEvent = SetTimer(hDlg, IDEVENT_TIMER_MOUSE, 8, NULL);	// 120Hz timer
		LogFileOutput("DirectInputInit: SetTimer(), id=0x%08X\n", g_TimerIDEvent);
		if (g_TimerIDEvent == 0)
			return E_FAIL;

		return S_OK;

#endif // NO_DIRECT_X
	}

	//-----------------------------------------------------------------------------
	// Name: FreeDirectInput()
	// Desc: Initialize the DirectInput variables.
	//-----------------------------------------------------------------------------
	void DirectInputUninit( HWND hDlg )
	{
		LogFileOutput("DirectInputUninit\n");

		// Unacquire the device one last time just in case 
		// the app tried to exit while the device is still acquired.
		if( g_pMouse ) 
		{
			HRESULT hr = g_pMouse->Unacquire();
			LogFileOutput("DirectInputUninit: Unacquire(), hr=0x%08X\n", hr);
		}

		// Release any DirectInput objects.
		SAFE_RELEASE( g_pMouse );
		SAFE_RELEASE( g_pDI );

		if (g_TimerIDEvent)
		{
			BOOL bRes = KillTimer(hDlg, g_TimerIDEvent);
			LogFileOutput("DirectInputUninit: KillTimer(), res=%d\n", bRes ? 1 : 0);
			g_TimerIDEvent = 0;
		}
	}

	//-----------------------------------------------------------------------------
	// Name: ReadImmediateData()
	// Desc: Read the input device's state when in immediate mode and display it.
	//-----------------------------------------------------------------------------
	HRESULT ReadImmediateData( long* pX, long* pY )
	{
		HRESULT       hr;
		DIMOUSESTATE2 dims2;      // DirectInput mouse state structure

		if (pX) *pX = 0;
		if (pY) *pY = 0;

		if( NULL == g_pMouse )
			return S_OK;

		// Get the input's device state, and put the state in dims
		memset( &dims2, 0, sizeof(dims2) );
		hr = g_pMouse->GetDeviceState( sizeof(DIMOUSESTATE2), &dims2 );
		if( FAILED(hr) ) 
		{
			// DirectInput may be telling us that the input stream has been
			// interrupted.  We aren't tracking any state between polls, so
			// we don't have any special reset that needs to be done.
			// We just re-acquire and try again.

			// If input is lost then acquire and keep trying 
			hr = g_pMouse->Acquire();
			while( hr == DIERR_INPUTLOST ) 
				hr = g_pMouse->Acquire();

			// Update the dialog text 
			if( hr == DIERR_OTHERAPPHASPRIO || 
				hr == DIERR_NOTACQUIRED ) 
			{
				//SetDlgItemText( hDlg, IDC_DATA, TEXT("Unacquired") );
			}

			// hr may be DIERR_OTHERAPPHASPRIO or other errors.  This
			// may occur when the app is minimized or in the process of 
			// switching, so just try again later 
			return S_OK; 
		}

		if (pX) *pX = dims2.lX;
		if (pY) *pY = dims2.lY;

		return S_OK;
	}

};	// namespace DIMouse
