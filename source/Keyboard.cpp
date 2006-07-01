/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 1994-1996, Michael O'Brien
Copyright (C) 1999-2001, Oliver Schmidt
Copyright (C) 2002-2005, Tom Charlesworth
Copyright (C) 2006, Tom Charlesworth, Michael Pohoreski

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

/* Description: Keyboard emulation
 *
 * Author: Various
 */

#include "StdAfx.h"
#pragma  hdrstop

static bool g_bKeybBufferEnable = false;

#define KEY_OLD

static BYTE asciicode[2][10] = {
	{0x08,0x0D,0x15,0x2F,0x00,0x00,0x00,0x00,0x00,0x00},
	{0x08,0x0B,0x15,0x0A,0x00,0x00,0x00,0x00,0x00,0x7F}
};	// Convert PC arrow keys to Apple keycodes

static bool  g_bShiftKey = false;
static bool  g_bCtrlKey  = false;
static bool  g_bAltKey   = false;
static bool  g_bCapsLock = true;
static int   lastvirtkey     = 0;	// Current PC keycode
static BYTE  keycode         = 0;	// Current Apple keycode
static DWORD keyboardqueries = 0;

#ifdef KEY_OLD
// Original
static BOOL  keywaiting      = 0;
#else
// Buffered key input:
// - Needed on faster PCs where aliasing occurs during short/fast bursts of 6502 code.
// - Keyboard only sampled during 6502 execution, so if it's run too fast then key presses will be missed.
const int KEY_BUFFER_MIN_SIZE = 1;
const int KEY_BUFFER_MAX_SIZE = 2;
static int g_nKeyBufferSize = KEY_BUFFER_MAX_SIZE;	// Circ key buffer size
static int g_nNextInIdx = 0;
static int g_nNextOutIdx = 0;
static int g_nKeyBufferCnt = 0;

static struct
{
	int nVirtKey;
	BYTE nAppleKey;
} g_nKeyBuffer[KEY_BUFFER_MAX_SIZE];
#endif

static BYTE g_nLastKey = 0x00;

//
// ----- ALL GLOBALLY ACCESSIBLE FUNCTIONS ARE BELOW THIS LINE -----
//

//===========================================================================

void KeybReset()
{
#ifdef KEY_OLD
	keywaiting = 0;
#else
	g_nNextInIdx = 0;
	g_nNextOutIdx = 0;
	g_nKeyBufferCnt = 0;
	g_nLastKey = 0x00;

	g_nKeyBufferSize = g_bKeybBufferEnable ? KEY_BUFFER_MAX_SIZE : KEY_BUFFER_MIN_SIZE;
#endif
}

//===========================================================================

//void KeybSetBufferMode(bool bNewKeybBufferEnable)
//{
//	if(g_bKeybBufferEnable == bNewKeybBufferEnable)
//		return;
//
//	g_bKeybBufferEnable = bNewKeybBufferEnable;
//	KeybReset();
//}
//
//bool KeybGetBufferMode()
//{
//	return g_bKeybBufferEnable;
//}

//===========================================================================
bool KeybGetAltStatus ()
{
	return g_bAltKey;
}

//===========================================================================
bool KeybGetCapsStatus ()
{
	return g_bCapsLock;
}

//===========================================================================
bool KeybGetCtrlStatus ()
{
	return g_bCtrlKey;
}

//===========================================================================
bool KeybGetShiftStatus ()
{
	return g_bShiftKey;
}

//===========================================================================
void KeybUpdateCtrlShiftStatus()
{
	g_bShiftKey = (GetKeyState( VK_SHIFT  ) & KF_UP) ? true : false; // 0x8000 KF_UP
	g_bCtrlKey  = (GetKeyState( VK_CONTROL) & KF_UP) ? true : false;
	g_bAltKey   = (GetKeyState( VK_MENU   ) & KF_UP) ? true : false;
}

//===========================================================================
BYTE KeybGetKeycode ()		// Used by MemCheckPaging() & VideoCheckMode()
{
	return keycode;
}

//===========================================================================
DWORD KeybGetNumQueries ()	// Used in determining 'idleness' of Apple system
{
	DWORD result = keyboardqueries;
	keyboardqueries = 0;
	return result;
}

//===========================================================================
void KeybQueueKeypress (int key, BOOL bASCII)
{
	static bool bFreshReset;

	if (bASCII == ASCII)
	{
		if (bFreshReset && key == 0x03)
		{
			bFreshReset = 0;
			return; // Swallow spurious CTRL-C caused by CTRL-BREAK
		}
		bFreshReset = 0;
		if (key > 0x7F)
			return;

		if (g_bApple2e) 
		{
			if (g_bCapsLock && (key >= 'a') && (key <='z'))
				keycode = key - 32;
			else
				keycode = key;
		}
		else
		{
			if (key >= '`')
				keycode = key - 32;
			else
				keycode = key;
		}
		lastvirtkey = LOBYTE(VkKeyScan(key));
	}
	else
	{
		if ((key == VK_CANCEL) && (GetKeyState(VK_CONTROL) < 0))
		{
			// Ctrl+Reset
			if (g_bApple2e)
				MemResetPaging();

			DiskReset();
			KeybReset();
			if (g_bApple2e)
				VideoResetState();	// Switch Alternate char set off
			MB_Reset();

#ifndef KEY_OLD
			g_nNextInIdx = g_nNextOutIdx = g_nKeyBufferCnt = 0;
#endif

			CpuReset();
			bFreshReset = 1;
			return;
		}

		if ((key == VK_INSERT) && (GetKeyState(VK_SHIFT) < 0))
		{
			// Shift+Insert
			ClipboardInitiatePaste();
			return;
		}

		if (!((key >= VK_LEFT) && (key <= VK_DELETE) && asciicode[g_bApple2e][key - VK_LEFT]))
			return;

		keycode = asciicode[g_bApple2e][key - VK_LEFT];		// Convert to Apple arrow keycode
		lastvirtkey = key;
	}
#ifdef KEY_OLD
	keywaiting = 1;
#else
	bool bOverflow = false;

	if(g_nKeyBufferCnt < g_nKeyBufferSize)
		g_nKeyBufferCnt++;
	else
		bOverflow = true;

	g_nKeyBuffer[g_nNextInIdx].nVirtKey = lastvirtkey;
	g_nKeyBuffer[g_nNextInIdx].nAppleKey = keycode;
	g_nNextInIdx = (g_nNextInIdx + 1) % g_nKeyBufferSize;

	if(bOverflow)
		g_nNextOutIdx = (g_nNextOutIdx + 1) % g_nKeyBufferSize;
#endif
}

//===========================================================================

static HGLOBAL hglb = NULL;
static LPTSTR lptstr = NULL;
static bool g_bPasteFromClipboard = false;
static bool g_bClipboardActive = false;

void ClipboardInitiatePaste()
{
	if (g_bClipboardActive)
		return;

	g_bPasteFromClipboard = true;
}

static void ClipboardDone()
{
	if (g_bClipboardActive)
	{
		g_bClipboardActive = false;
		GlobalUnlock(hglb);
		CloseClipboard();
	}
}

static void ClipboardInit()
{
	ClipboardDone();

	if (!IsClipboardFormatAvailable(CF_TEXT))
		return;
	
	if (!OpenClipboard(g_hFrameWindow))
		return;
	
	hglb = GetClipboardData(CF_TEXT);
	if (hglb == NULL)
	{	
		CloseClipboard();
		return;
	}

	lptstr = (char*) GlobalLock(hglb);
	if (lptstr == NULL)
	{	
		CloseClipboard();
		return;
	}

	g_bPasteFromClipboard = false;
	g_bClipboardActive = true;
}

static char ClipboardCurrChar(bool bIncPtr)
{
	char nKey;
	int nInc = 1;

	if((lptstr[0] == 0x0D) && (lptstr[1] == 0x0A))
	{
		nKey = 0x0D;
		nInc = 2;
	}
	else
	{
		nKey = lptstr[0];
	}

	if(bIncPtr)
		lptstr += nInc;

	return nKey;
}

//===========================================================================

BYTE __stdcall KeybReadData (WORD, BYTE, BYTE, BYTE, ULONG)
{
	keyboardqueries++;

	//

	if(g_bPasteFromClipboard)
		ClipboardInit();

	if(g_bClipboardActive)
	{
		if(*lptstr == 0)
			ClipboardDone();
		else
			return 0x80 | ClipboardCurrChar(false);
	}

	//

#ifdef KEY_OLD
	return keycode | (keywaiting ? 0x80 : 0);
#else
	BYTE nKey = g_nKeyBufferCnt ? 0x80 : 0;
	if(g_nKeyBufferCnt)
	{
		nKey |= g_nKeyBuffer[g_nNextOutIdx].nAppleKey;
		g_nLastKey = g_nKeyBuffer[g_nNextOutIdx].nAppleKey;
	}
	else
	{
		nKey |= g_nLastKey;
	}
	return nKey;
#endif
}

//===========================================================================

BYTE __stdcall KeybReadFlag (WORD, BYTE, BYTE, BYTE, ULONG)
{
	keyboardqueries++;

	//

	if(g_bPasteFromClipboard)
		ClipboardInit();

	if(g_bClipboardActive)
	{
		if(*lptstr == 0)
			ClipboardDone();
		else
			return 0x80 | ClipboardCurrChar(true);
	}

	//

#ifdef KEY_OLD
	keywaiting = 0;
	return keycode | ((GetKeyState(lastvirtkey) < 0) ? 0x80 : 0);
#else
	BYTE nKey = (GetKeyState(g_nKeyBuffer[g_nNextOutIdx].nVirtKey) < 0) ? 0x80 : 0;
	nKey |= g_nKeyBuffer[g_nNextOutIdx].nAppleKey;
	if(g_nKeyBufferCnt)
	{
		g_nKeyBufferCnt--;
		g_nNextOutIdx = (g_nNextOutIdx + 1) % g_nKeyBufferSize;
	}
	return nKey;
#endif
}

//===========================================================================
void KeybToggleCapsLock ()
{
	if (g_bApple2e)
	{
		g_bCapsLock = (GetKeyState(VK_CAPITAL) & 1);
		FrameRefreshStatus(DRAW_LEDS);
	}
}

//===========================================================================

DWORD KeybGetSnapshot(SS_IO_Keyboard* pSS)
{
	pSS->keyboardqueries	= keyboardqueries;
	pSS->nLastKey			= g_nLastKey;
	return 0;
}

DWORD KeybSetSnapshot(SS_IO_Keyboard* pSS)
{
	keyboardqueries	= pSS->keyboardqueries;
	g_nLastKey		= pSS->nLastKey;
	return 0;
}
