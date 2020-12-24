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

/* Description: Keyboard emulation
 *
 * Author: Various
 */

#include "StdAfx.h"

#include "Keyboard.h"
#include "Windows/AppleWin.h"
#include "Core.h"
#include "Interface.h"
#include "Utilities.h"
#include "Pravets.h"
#include "Tape.h"
#include "YamlHelper.h"
#include "Windows/WinVideo.h" // Needed by TK3000 //e, to refresh the frame at each |Mode| change
#include "Log.h"

static BYTE asciicode[2][10] = {
	// VK_LEFT/UP/RIGHT/DOWN/SELECT, VK_PRINT/EXECUTE/SNAPSHOT/INSERT/DELETE
	{0x08,0x00,0x15,0x00,0x00, 0x00,0x00,0x00,0x00,0x00},	// Apple II
	{0x08,0x0B,0x15,0x0A,0x00, 0x00,0x00,0x00,0x00,0x7F}	// Apple //e
};	// Convert PC arrow keys to Apple keycodes

bool  g_bShiftKey = false;
bool  g_bCtrlKey  = false;
bool  g_bAltKey   = false;

static bool  g_bTK3KModeKey   = false; //TK3000 //e |Mode| key

static bool  g_bCapsLock = true; //Caps lock key for Apple2 and Lat/Cyr lock for Pravets8
static bool  g_bP8CapsLock = true; //Caps lock key of Pravets 8A/C
static BYTE  keycode         = 0;	// Current Apple keycode
static BOOL  keywaiting      = 0;
static bool  g_bAltGrSendsWM_CHAR = false;

//
// ----- ALL GLOBALLY ACCESSIBLE FUNCTIONS ARE BELOW THIS LINE -----
//

void KeybSetAltGrSendsWM_CHAR(bool state)
{
	g_bAltGrSendsWM_CHAR = state;
}

//===========================================================================

void KeybReset()
{
	keycode = 0;
	keywaiting = 0;
}

//===========================================================================
bool KeybGetCapsStatus()
{
	return g_bCapsLock;
}

//===========================================================================
bool KeybGetP8CapsStatus()
{
	return g_bP8CapsLock;
}

//===========================================================================
bool KeybGetAltStatus()
{
	return g_bAltKey;
}

//===========================================================================
bool KeybGetCtrlStatus()
{
	return g_bCtrlKey;
}

//===========================================================================
bool KeybGetShiftStatus()
{
	return g_bShiftKey;
}

//===========================================================================
void KeybUpdateCtrlShiftStatus()
{
	g_bAltKey   = (GetKeyState( VK_MENU   ) < 0) ? true : false;	//  L or R alt
	g_bCtrlKey  = (GetKeyState( VK_CONTROL) < 0) ? true : false;	//  L or R ctrl
	g_bShiftKey = (GetKeyState( VK_SHIFT  ) < 0) ? true : false;	//  L or R shift
}

//===========================================================================
BYTE KeybGetKeycode ()		// Used by IORead_C01x() and TapeRead() for Pravets8A
{
	return keycode;
}

//===========================================================================

static bool IsVirtualKeyAnAppleIIKey(WPARAM wparam);

void KeybQueueKeypress (WPARAM key, Keystroke_e bASCII)
{
	if (bASCII == ASCII)	// WM_CHAR
	{
		if (GetFrame().g_bFreshReset && key == VK_CANCEL) // OLD HACK: 0x03
		{
			GetFrame().g_bFreshReset = false;
			return; // Swallow spurious CTRL-C caused by CTRL-BREAK
		}

		GetFrame().g_bFreshReset = false;
		if ((key > 0x7F) && !g_bTK3KModeKey) // When in TK3000 mode, we have special keys which need remapping
			return;

		if (!IS_APPLE2) 
		{
			P8Shift = false;
			if (g_bCapsLock && (key >= 'a') && (key <='z'))
			{
				P8Shift = true;
				keycode = key - 32;
			}
			else
			{
				keycode = key;
			}			

			//The latter line should be applied for Pravtes 8A/C only, but not for Pravets 82/M !!!			
			if ((g_bCapsLock == false) && (key >= 'A') && (key <='Z'))
			{
				P8Shift = true;
				if (g_Apple2Type == A2TYPE_PRAVETS8A) 
					keycode = key + 32;
			}

			//Remap some keys for Pravets82/M
			if (g_Apple2Type == A2TYPE_PRAVETS82)
			{
				if (key == 64) 
					keycode = 96;
				if (key == '^') 
					keycode = '~';

				if (g_bCapsLock == false) //cyrillic letters
				{
					if (key == '`') keycode = '^';
					if (key == 92) keycode = '@'; // \ to @	
					if (key == 124) keycode = 92;
				}
				else //(g_bCapsLock == true) //latin letters
				{
					if (key == 91) keycode = 123;
					if (key == 93) keycode = 125;
					if (key == 124) keycode = 92;
				}
			}
			if (g_Apple2Type == A2TYPE_PRAVETS8M)  //Pravets 8M charset is still uncertain
			{
				if (g_bCapsLock == false) //cyrillic letters
				{
					if (key == '[') keycode = '{';
					if (key == ']') keycode = '}';
					if (key == '`') keycode = '~'; //96= key `~
					if (key == 92) keycode = 96;
				}
				else //latin letters
				{
					if (key == '`') 
						keycode = '^'; //96= key `~
				}
			}
			//Remap some keys for Pravets8A/C, which has a different charset for Pravtes82/M, whose keys MUST NOT be remapped.
			if (g_Apple2Type == A2TYPE_PRAVETS8A) //&& (g_bCapsLock == false))
			{
				if (g_bCapsLock == false) //i.e. cyrillic letters
			    {
					if (key == '[') keycode = '{';
					if (key == ']') keycode = '}';
					if (key == '`') keycode = '~';
					if (key == 92) keycode = 96;
					if (GetCapsLockAllowed() == true)
					{
						if ((key == 92) || (key == 124)) keycode = 96; //Ý to Þ
						//This shall be rewriten, so that enabling CAPS_LOCK (i.e. F10) will not invert these keys values)
						//The same for latin letters.
						if ((key == '{') || (key == '}') || (key == '~') || (key == 124) || (key == '^') ||  (key == 95))
							P8Shift = true;					
					}
				}
				else //i.e. latin letters
				{
					if (GetCapsLockAllowed() == false)
					{
						if (key == '{') keycode = '[';
						if (key == '}') keycode = ']';
						if (key == 124) 
							keycode = 92;
						/*if (key == 92) 
							keycode = 124;*/
					//Characters ` and ~ cannot be generated in 7bit character mode, so they are replaced with
					}
					else
					{
						if (key == '{') keycode = 91;
						if (key == '}')	keycode = 93;
						if (key == 124)	keycode = 92;					
						if ((key == '[') || (key == ']') || (key == 92) || (key == '^') || (key == 95))
							P8Shift= true; 
						if (key == 96)	 //This line shall generate sth. else i.e. ` In fact. this character is not generateable by the pravets keyboard.
						{
							keycode = '^';
							P8Shift= true;
						}
						if (key == 126)	keycode = '^';
					}
				}
			}
			// Remap for the TK3000 //e, which had a special |Mode| key for displaying accented chars on screen
			// Borrowed from Fábio Belavenuto's TK3000e emulator (Copyright (C) 2004) - http://code.google.com/p/tk3000e/
			if (g_bTK3KModeKey)	// We already switch this on only if the the TK3000 is currently being emulated
			{
				if ((key >= 0xC0) && (key <= 0xDA)) key += 0x20; // Convert uppercase to lowercase
				switch (key)
				{
					case 0xE0: key = '_';  break; // à
					case 0xE1: key = '@';  break; // á
					case 0xE2: key = '\\'; break; // â
					case 0xE3: key = '[';  break; // ã
					case 0xE7: key = ']';  break; // ç
					case 0xE9: key = '`';  break; // é
					case 0xEA: key = '&';  break; // ê
					case 0xED: key = '{';  break; // í
					case 0xF3: key = '~';  break; // ó
					case 0xF4: key = '}';  break; // ô
					case 0xF5: key = '#';  break; // õ
					case 0xFA: key = '|';  break; // ú
				}
				if (key > 0x7F) return;	// Get out
				if ((key >= 'a') && (key <= 'z') && (g_bCapsLock))
					keycode = key - ('a'-'A');
				else
					keycode = key;
			}
		}
		else
		{
			if (g_Apple2Type == A2TYPE_PRAVETS8A)
			{
			}
			else
			{
				if (key >= '`')
					keycode = key - 32;
				else
					keycode = key;
			}
		}
	} 
	else //(bASCII != ASCII)	// WM_KEYDOWN
	{
		// Note: VK_CANCEL is Control-Break
		if ((key == VK_CANCEL) && (GetKeyState(VK_CONTROL) < 0))
		{
			GetFrame().g_bFreshReset = true;
			CtrlReset();
			return;
		}

		if ((key == VK_INSERT) && (GetKeyState(VK_SHIFT) < 0))
		{
			// Shift+Insert
			ClipboardInitiatePaste();
			return;
		}

		if (key == VK_SCROLL)
		{	// For the TK3000 //e we use Scroll Lock to switch between Apple ][ and accented chars modes
			if (g_Apple2Type == A2TYPE_TK30002E)
			{
				g_bTK3KModeKey = (GetKeyState(VK_SCROLL) & 1) ? true : false;	// Sync with the Scroll Lock status
				GetFrame().FrameRefreshStatus(DRAW_LEDS);	// TODO: Implement |Mode| LED in the UI; make it appear only when in TK3000 mode
				GetFrame().VideoRedrawScreen();	// TODO: Still need to implement page mode switching and 'whatnot'
			}
			return;
		}

		if (key >= VK_LEFT && key <= VK_DELETE)
		{
			BYTE n = asciicode[IS_APPLE2 ? 0 : 1][key - VK_LEFT];		// Convert to Apple arrow keycode
			if (!n)
				return;
			keycode = n;
		}
		else if (g_bAltGrSendsWM_CHAR && (GetKeyState(VK_RMENU) < 0))	// Right Alt (aka Alt Gr) - GH#558, GH#625
		{
			if (IsVirtualKeyAnAppleIIKey(key))
			{
				// When Alt Gr is down, then WM_CHAR is not posted - so fix this.
				// NB. Still get WM_KEYDOWN/WM_KEYUP for the virtual key, so AKD works.
				WPARAM newKey = key;

				// Translate if shift or ctrl is down
				if (key >= 'A' && key <= 'Z')
				{
					if ( (GetKeyState(VK_SHIFT) >= 0) && !g_bCapsLock )
						newKey += 'a' - 'A';	// convert to lowercase key
					else if (GetHookAltGrControl() && GetKeyState(VK_CONTROL) < 0)
						newKey -= 'A' - 1;		// convert to control-key
				}

				PostMessage(GetFrame().g_hFrameWindow, WM_CHAR, newKey, 0);
			}

			return;
		}
		else
		{
			return;
		}
	}

	keywaiting = 1;
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
	
	if (!OpenClipboard(GetFrame().g_hFrameWindow))
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

const UINT kAKDNumElements = 256/64;
static uint64_t g_AKDFlags[2][kAKDNumElements] = { {0,0,0,0},	// normal
												   {0,0,0,0}};	// extended

static bool IsVirtualKeyAnAppleIIKey(WPARAM wparam)
{
	if (wparam == VK_BACK ||
		wparam == VK_TAB ||
		wparam == VK_RETURN ||
		wparam == VK_ESCAPE ||
		wparam == VK_SPACE ||
		(wparam >= VK_LEFT && wparam <= VK_DOWN) ||
		wparam == VK_DELETE ||
		(wparam >= '0' && wparam <= '9') ||
		(wparam >= 'A' && wparam <= 'Z') ||
		(wparam >= VK_NUMPAD0 && wparam <= VK_NUMPAD9) ||
		(wparam >= VK_MULTIPLY && wparam <= VK_DIVIDE) ||
		(wparam >= VK_OEM_1 && wparam <= VK_OEM_3) ||	// 7 in total
		(wparam >= VK_OEM_4 && wparam <= VK_OEM_8) ||	// 5 in total
		(wparam == VK_OEM_102))
	{
		return true;
	}

	return false;
}

// NB. Don't need to be concerned about if numpad/cursors are used for joystick,
// since parent calls JoyProcessKey() just before this.
void KeybAnyKeyDown(UINT message, WPARAM wparam, bool bIsExtended)
{
	if (wparam > 255)
	{
		_ASSERT(0);
		return;
	}

	if (IsVirtualKeyAnAppleIIKey(wparam))
	{
		UINT offset = wparam >> 6;
		UINT bit    = wparam & 0x3f;
		UINT idx    = !bIsExtended ? 0 : 1;

		if (message == WM_KEYDOWN)
			g_AKDFlags[idx][offset] |= (1LL<<bit);
		else
			g_AKDFlags[idx][offset] &= ~(1LL<<bit);
	}
}

static bool IsAKD(void)
{
	uint64_t* p = &g_AKDFlags[0][0];

	for (UINT i=0; i<sizeof(g_AKDFlags)/sizeof(g_AKDFlags[0][0]); i++)
		if (p[i])
			return true;

	return false;
}

//===========================================================================

BYTE KeybReadData (void)
{
	LogFileTimeUntilFirstKeyRead();

	if (g_bPasteFromClipboard)
		ClipboardInit();

	if (g_bClipboardActive)
	{
		if(*lptstr == 0)
			ClipboardDone();
		else
			return 0x80 | ClipboardCurrChar(false);
	}

	//

	return keycode | (keywaiting ? 0x80 : 0);
}

//===========================================================================

BYTE KeybReadFlag (void)
{
	if (g_bPasteFromClipboard)
		ClipboardInit();

	if (g_bClipboardActive)
	{
		if(*lptstr == 0)
			ClipboardDone();
		else
			return 0x80 | ClipboardCurrChar(true);
	}

	//

	keywaiting = 0;

	if (IS_APPLE2)	// Include Pravets machines too?
		return keycode;

	// AKD

	return keycode | (IsAKD() ? 0x80 : 0);
}

//===========================================================================
void KeybToggleCapsLock ()
{
	if (!IS_APPLE2)
	{
		g_bCapsLock = (GetKeyState(VK_CAPITAL) & 1);
		GetFrame().FrameRefreshStatus(DRAW_LEDS);
	}
}

//===========================================================================
void KeybToggleP8ACapsLock ()
{
	_ASSERT(g_Apple2Type == A2TYPE_PRAVETS8A);
	P8CAPS_ON = !P8CAPS_ON;
	GetFrame().FrameRefreshStatus(DRAW_LEDS);
	// g_bP8CapsLock= g_bP8CapsLock?false:true; //The same as the upper, but slower
}

//===========================================================================

#define SS_YAML_KEY_LASTKEY "Last Key"
#define SS_YAML_KEY_KEYWAITING "Key Waiting"

static std::string KeybGetSnapshotStructName(void)
{
	static const std::string name("Keyboard");
	return name;
}

void KeybSaveSnapshot(YamlSaveHelper& yamlSaveHelper)
{
	YamlSaveHelper::Label state(yamlSaveHelper, "%s:\n", KeybGetSnapshotStructName().c_str());
	yamlSaveHelper.SaveHexUint8(SS_YAML_KEY_LASTKEY, keycode);
	yamlSaveHelper.SaveBool(SS_YAML_KEY_KEYWAITING, keywaiting ? true : false);
}

void KeybLoadSnapshot(YamlLoadHelper& yamlLoadHelper, UINT version)
{
	if (!yamlLoadHelper.GetSubMap(KeybGetSnapshotStructName()))
		return;

	keycode = (BYTE) yamlLoadHelper.LoadUint(SS_YAML_KEY_LASTKEY);

	if (version >= 2)
		keywaiting = (BOOL) yamlLoadHelper.LoadBool(SS_YAML_KEY_KEYWAITING);

	yamlLoadHelper.PopMap();
}
