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

/* Description: Joystick emulation via keyboard, PC joystick or mouse
 *
 * Author: Various
 */

// TC: Extended for 2nd joystick:
// Apple joystick #0 can be emulated with: NONE, JOYSTICKID1, KEYBOARD, MOUSE
// Apple joystick #1 can be emulated with: NONE, JOYSTICKID2, KEYBOARD, MOUSE
// If Apple joystick #0 is using {KEYBOARD | MOUSE} then joystick #1 can't use it.
// If Apple joystick #1 is using KEYBOARD, then disable the standard keys that control Apple switches #0/#1.
// - So that in a 2 player game, player 2 can't cheat by controlling player 1's buttons.
// If Apple joystick #1 is not NONE, then Apple joystick #0 only gets the use of Apple switch #0.
// - When using 2 joysticks, button #1 is used by joystick #1 (Archon).
// Apple joystick #1's button now controls Apple switches #1 and #2.
// - This is because the 2-joystick version of Mario Bros expects the 2nd joystick to control Apple switch #2.

#include "StdAfx.h"

#include "Joystick.h"
#include "Windows/AppleWin.h"
#include "CPU.h"
#include "Memory.h"
#include "YamlHelper.h"
#include "Interface.h"

#include "Configuration/PropertySheet.h"

#define BUTTONTIME	5000	// This is the latch (debounce) time in usecs for the joystick buttons

enum {DEVICE_NONE=0, DEVICE_JOYSTICK, DEVICE_KEYBOARD, DEVICE_MOUSE, DEVICE_JOYSTICK_THUMBSTICK2};

// Indexed by joytype[n]
static const DWORD joyinfo[6] =	{	DEVICE_NONE,
									DEVICE_JOYSTICK,
									DEVICE_KEYBOARD,	// Cursors (prev: Numpad-Standard)
									DEVICE_KEYBOARD,	// Numpad (prev: Numpad-Centering)
									DEVICE_MOUSE,
									DEVICE_JOYSTICK_THUMBSTICK2 };

// Key pad [1..9]; Key pad 0,Key pad '.'; Left ALT,Right ALT
enum JOYKEY {	JK_DOWNLEFT=0,
				JK_DOWN,
				JK_DOWNRIGHT,
				JK_LEFT,
				JK_CENTRE,
				JK_RIGHT,
				JK_UPLEFT,
				JK_UP,
				JK_UPRIGHT,
				JK_BUTTON0,
				JK_BUTTON1,
				JK_OPENAPPLE,
				JK_CLOSEDAPPLE,
				JK_MAX
			};

const UINT PDL_MIN = 0;
const UINT PDL_CENTRAL = 127;
const UINT PDL_MAX = 255;

static BOOL  keydown[JK_MAX] = {FALSE};
static POINT keyvalue[9] = {{PDL_MIN,PDL_MAX},    {PDL_CENTRAL,PDL_MAX},    {PDL_MAX,PDL_MAX},
                            {PDL_MIN,PDL_CENTRAL},{PDL_CENTRAL,PDL_CENTRAL},{PDL_MAX,PDL_CENTRAL},
                            {PDL_MIN,PDL_MIN},    {PDL_CENTRAL,PDL_MIN},    {PDL_MAX,PDL_MIN}};

static int   buttonlatch[3] = {0,0,0};
static BOOL  joybutton[3]   = {0,0,0};

static int   joyshrx[2]     = {8,8};
static int   joyshry[2]     = {8,8};
static int   joysubx[2]     = {0,0};
static int   joysuby[2]     = {0,0};

// Value persisted to Registry for REGVALUE_JOYSTICK0_EMU_TYPE
static DWORD joytype[2]            = {J0C_JOYSTICK1, J1C_DISABLED};	// Emulation Type for joysticks #0 & #1

static BOOL  setbutton[3]   = {0,0,0};	// Used when a mouse button is pressed/released

static int   xpos[2]        = {PDL_CENTRAL,PDL_CENTRAL};
static int   ypos[2]        = {PDL_CENTRAL,PDL_CENTRAL};

static unsigned __int64 g_nJoyCntrResetCycle = 0;	// Abs cycle that joystick counters were reset

static short g_nPdlTrimX = 0;
static short g_nPdlTrimY = 0;

enum {JOYPORT_LEFTRIGHT=0, JOYPORT_UPDOWN=1};

static UINT g_bJoyportEnabled = 0;	// Set to use Joyport to drive the 3 button inputs
static UINT g_uJoyportActiveStick = 0;
static UINT g_uJoyportReadMode = JOYPORT_LEFTRIGHT;

static bool g_bHookAltKeys = true;

//===========================================================================

void JoySetHookAltKeys(bool hook)
{
	g_bHookAltKeys = hook;
}

//===========================================================================
void CheckJoystick0()
{
  static DWORD lastcheck = 0;
  DWORD currtime = GetTickCount();
  if ((currtime-lastcheck >= 10) || joybutton[0] || joybutton[1])
  {
    lastcheck = currtime;
    JOYINFO info;
    if (joyGetPos(JOYSTICKID1,&info) == JOYERR_NOERROR)
	{
      if ((info.wButtons & JOY_BUTTON1) && !joybutton[0])
        buttonlatch[0] = BUTTONTIME;
      if ((info.wButtons & JOY_BUTTON2) && !joybutton[1] &&
          (joyinfo[joytype[1]] == DEVICE_NONE)	// Only consider 2nd button if NOT emulating a 2nd Apple joystick
         )
		   buttonlatch[1] = BUTTONTIME;
      joybutton[0] = ((info.wButtons & JOY_BUTTON1) != 0);
      if (joyinfo[joytype[1]] == DEVICE_NONE)	// Only consider 2nd button if NOT emulating a 2nd Apple joystick
        joybutton[1] = ((info.wButtons & JOY_BUTTON2) != 0);

      xpos[0] = (info.wXpos-joysubx[0]) >> joyshrx[0];
      ypos[0] = (info.wYpos-joysuby[0]) >> joyshry[0];

	  // NB. This does not work for analogue joysticks (not self-centreing) - except if Trim=0
	  if(xpos[0] == 127 || xpos[0] == 128) xpos[0] += g_nPdlTrimX;
	  if(ypos[0] == 127 || ypos[0] == 128) ypos[0] += g_nPdlTrimY;
    }
  }
}

void CheckJoystick1()
{
  static DWORD lastcheck = 0;
  DWORD currtime = GetTickCount();
  if ((currtime-lastcheck >= 10) || joybutton[2])
  {
    lastcheck = currtime;
    JOYINFO info;
    MMRESULT result = 0;
    if (joyinfo[joytype[1]] == DEVICE_JOYSTICK_THUMBSTICK2)
    {
      // Use results of joystick 1 thumbstick 2 and button 2 for joystick 1 and button 1
      JOYINFOEX infoEx;
      infoEx.dwSize = sizeof(infoEx);
      infoEx.dwFlags = JOY_RETURNBUTTONS | JOY_RETURNZ | JOY_RETURNR;
      result = joyGetPosEx(JOYSTICKID1, &infoEx);
      if (result == JOYERR_NOERROR)
      {
        info.wButtons = (infoEx.dwButtons & JOY_BUTTON2) ? JOY_BUTTON1 : 0;
        info.wXpos = infoEx.dwZpos;
        info.wYpos = infoEx.dwRpos;
      }
    }
    else
      result = joyGetPos(JOYSTICKID2, &info);
    if (result == JOYERR_NOERROR)
	{
      if ((info.wButtons & JOY_BUTTON1) && !joybutton[2])
	  {
        buttonlatch[2] = BUTTONTIME;
        if(joyinfo[joytype[1]] != DEVICE_NONE)
          buttonlatch[1] = BUTTONTIME;	// Re-map this button when emulating a 2nd Apple joystick
	  }

      joybutton[2] = ((info.wButtons & JOY_BUTTON1) != 0);
      if(joyinfo[joytype[1]] != DEVICE_NONE)
        joybutton[1] = ((info.wButtons & JOY_BUTTON1) != 0);	// Re-map this button when emulating a 2nd Apple joystick

      xpos[1] = (info.wXpos-joysubx[1]) >> joyshrx[1];
      ypos[1] = (info.wYpos-joysuby[1]) >> joyshry[1];

	  // NB. This does not work for analogue joysticks (not self-centreing) - except if Trim=0
	  if(xpos[1] == 127 || xpos[1] == 128) xpos[1] += g_nPdlTrimX;
	  if(ypos[1] == 127 || ypos[1] == 128) ypos[1] += g_nPdlTrimY;
    }
  }
}

//
// ----- ALL GLOBALLY ACCESSIBLE FUNCTIONS ARE BELOW THIS LINE -----
//

//===========================================================================
void JoyInitialize()
{
  // Emulated joystick #0 can only use JOYSTICKID1 (if no joystick, then use keyboard)
  // Emulated joystick #1 can only use JOYSTICKID2 (if no joystick, then disable)

  //
  // Init for emulated joystick #0:
  //

  if (joyinfo[joytype[0]] == DEVICE_JOYSTICK)
  {
    JOYCAPS caps;
    if (joyGetDevCaps(JOYSTICKID1,&caps,sizeof(JOYCAPS)) == JOYERR_NOERROR)
	{
      joyshrx[0] = 0;
      joyshry[0] = 0;
      joysubx[0] = (int)caps.wXmin;
      joysuby[0] = (int)caps.wYmin;
      UINT xrange  = caps.wXmax-caps.wXmin;
      UINT yrange  = caps.wYmax-caps.wYmin;
      while (xrange > 256)
	  {
        xrange >>= 1;
        ++joyshrx[0];
      }
      while (yrange > 256)
	  {
        yrange >>= 1;
        ++joyshry[0];
      }
    }
    else
	{
      joytype[0] = J0C_KEYBD_NUMPAD;
	}
  }

  //
  // Init for emulated joystick #1:
  //

  if (joyinfo[joytype[1]] == DEVICE_JOYSTICK)
  {
    JOYCAPS caps;
    if (joyGetDevCaps(JOYSTICKID2,&caps,sizeof(JOYCAPS)) == JOYERR_NOERROR)
	{
      joyshrx[1] = 0;
      joyshry[1] = 0;
      joysubx[1] = (int)caps.wXmin;
      joysuby[1] = (int)caps.wYmin;
      UINT xrange  = caps.wXmax-caps.wXmin;
      UINT yrange  = caps.wYmax-caps.wYmin;
      while (xrange > 256)
	  {
        xrange >>= 1;
        ++joyshrx[1];
      }
      while (yrange > 256)
	  {
        yrange >>= 1;
        ++joyshry[1];
      }
    }
    else
	{
      joytype[1] = J1C_DISABLED;
	}
  }
  else if (joyinfo[joytype[1]] == DEVICE_JOYSTICK_THUMBSTICK2)
  {
    JOYCAPS caps;
    if (joyGetDevCaps(JOYSTICKID1, &caps, sizeof(JOYCAPS)) == JOYERR_NOERROR)
    {
      joyshrx[1] = 0;
      joyshry[1] = 0;
      joysubx[1] = (int)caps.wZmin;
      joysuby[1] = (int)caps.wRmin;
      UINT xrange = caps.wZmax - caps.wZmin;
      UINT yrange = caps.wRmax - caps.wRmin;
      while (xrange > 256)
      {
        xrange >>= 1;
        ++joyshrx[1];
      }
      while (yrange > 256)
      {
        yrange >>= 1;
        ++joyshry[1];
      }
    }
    else
    {
      joytype[1] = J1C_DISABLED;
    }
  }
}

//===========================================================================

static UINT g_buttonVirtKey[2] = { VK_MENU, VK_MENU | KF_EXTENDED };	// VK_MENU == ALT Key

void JoySetButtonVirtualKey(UINT button, UINT virtKey)
{
	_ASSERT(button < 2);
	if (button >= 2) return;
	g_buttonVirtKey[button] = virtKey;
}

//===========================================================================

#define SUPPORT_CURSOR_KEYS

BOOL JoyProcessKey(int virtkey, bool extended, bool down, bool autorep)
{
	static struct
	{
		UINT32 Left:1;
		UINT32 Up:1;
		UINT32 Right:1;
		UINT32 Down:1;
	} CursorKeys = {0};

	const UINT virtKeyWithExtended = ((UINT)virtkey) | (extended ? KF_EXTENDED : 0);

	if ( (joyinfo[joytype[0]] != DEVICE_KEYBOARD) &&
		 (joyinfo[joytype[1]] != DEVICE_KEYBOARD) &&
		 (virtKeyWithExtended != g_buttonVirtKey[0]) &&
		 (virtKeyWithExtended != g_buttonVirtKey[1]) )
	{
		return 0;
	}

	if (!g_bHookAltKeys && virtkey == VK_MENU)				// GH#583
		return 0;

	//

	BOOL keychange = 0;
	bool bIsCursorKey = false;
	const bool swapButtons0and1 = GetPropertySheet().GetButtonsSwapState();

	if (virtKeyWithExtended == g_buttonVirtKey[!swapButtons0and1 ? 0 : 1])
	{
		keychange = 1;
		keydown[JK_OPENAPPLE] = down;
	}
	else if (virtKeyWithExtended == g_buttonVirtKey[!swapButtons0and1 ? 1 : 0])
	{
		keychange = 1;
		keydown[JK_CLOSEDAPPLE] = down;
	}
	else if (!extended)
	{
		if (JoyUsingKeyboardNumpad())
		{
			keychange = 1;

			if ((virtkey >= VK_NUMPAD1) && (virtkey <= VK_NUMPAD9))		// NumLock on
			{
				keydown[virtkey-VK_NUMPAD1] = down;
			}
			else														// NumLock off
			{
				switch (virtkey)
				{
				case VK_END:     keydown[JK_DOWNLEFT] = down;	break;
				case VK_DOWN:    keydown[JK_DOWN] = down;		break;
				case VK_NEXT:    keydown[JK_DOWNRIGHT] = down;	break;
				case VK_LEFT:    keydown[JK_LEFT] = down;		break;
				case VK_CLEAR:   keydown[JK_CENTRE] = down;		break;
				case VK_RIGHT:   keydown[JK_RIGHT] = down;		break;
				case VK_HOME:    keydown[JK_UPLEFT] = down;		break;
				case VK_UP:      keydown[JK_UP] = down;			break;
				case VK_PRIOR:   keydown[JK_UPRIGHT] = down;	break;
				case VK_NUMPAD0: keydown[JK_BUTTON0] = down;	break;
				case VK_DECIMAL: keydown[JK_BUTTON1] = down;	break;
				default:         keychange = 0;					break;
				}
			}
		}
	}
#ifdef SUPPORT_CURSOR_KEYS
	else if (extended)
	{
		if (JoyUsingKeyboardCursors() && (virtkey == VK_LEFT || virtkey == VK_UP || virtkey == VK_RIGHT || virtkey == VK_DOWN))
		{
			keychange = 1;	// This prevents cursors keys being available to the Apple II (eg. Lode Runner uses cursor left/right for game speed & Ctrl-J/K for joystick/keyboard)
			bIsCursorKey = true;

			switch (virtkey)
			{
			case VK_LEFT:	CursorKeys.Left = down;		break;
			case VK_UP:		CursorKeys.Up = down;		break;
			case VK_RIGHT:	CursorKeys.Right = down;	break;
			case VK_DOWN:	CursorKeys.Down = down;		break;
			}
		}
	}
#endif

	if (!keychange)
		return 0;

	//

	if (virtkey == VK_NUMPAD0)
	{
		if(down)
		{
			if(joyinfo[joytype[1]] != DEVICE_KEYBOARD)
			{
				buttonlatch[0] = BUTTONTIME;
			}
			else if(joyinfo[joytype[1]] != DEVICE_NONE)
			{
				buttonlatch[2] = BUTTONTIME;
				buttonlatch[1] = BUTTONTIME;	// Re-map this button when emulating a 2nd Apple joystick
			}
		}
	}
	else if (virtkey == VK_DECIMAL)
	{
		if(down)
		{
			if(joyinfo[joytype[1]] != DEVICE_KEYBOARD)
				buttonlatch[1] = BUTTONTIME;
		}
	}
	else if ((down && !autorep) || (GetPropertySheet().GetJoystickCenteringControl() == JOYSTICK_MODE_CENTERING))
	{
		int xkeys  = 0;
		int ykeys  = 0;
		int xtotal = 0;
		int ytotal = 0;

		for (int keynum = JK_DOWNLEFT; keynum <= JK_UPRIGHT; keynum++)
		{
			if (keydown[keynum])
			{
				if ((keynum % 3) != 1)	// Not middle col (ie. not VK_DOWN, VK_CLEAR, VK_UP)
				{
					xkeys++;
					xtotal += keyvalue[keynum].x;
				}
				if ((keynum / 3) != 1)	// Not middle row (ie. not VK_LEFT, VK_CLEAR, VK_RIGHT)
				{
					ykeys++;
					ytotal += keyvalue[keynum].y;
				}
			}
		}

		if (CursorKeys.Left)
		{
			xkeys++; xtotal += keyvalue[JK_LEFT].x;
		}
		if (CursorKeys.Right)
		{
			xkeys++; xtotal += keyvalue[JK_RIGHT].x;
		}
		if (CursorKeys.Up)
		{
			ykeys++; ytotal += keyvalue[JK_UP].y;
		}
		if (CursorKeys.Down)
		{
			ykeys++; ytotal += keyvalue[JK_DOWN].y;
		}

		// Joystick # which is being emulated by keyboard
		int nJoyNum = (joyinfo[joytype[0]] == DEVICE_KEYBOARD) ? 0 : 1;

		if (xkeys)
			xpos[nJoyNum] = xtotal / xkeys;
		else
			xpos[nJoyNum] = PDL_CENTRAL + g_nPdlTrimX;

		if (ykeys)
			ypos[nJoyNum] = ytotal / ykeys;
		else
			ypos[nJoyNum] = PDL_CENTRAL + g_nPdlTrimY;
	}

	if (bIsCursorKey && GetPropertySheet().GetJoystickCursorControl())
	{
		// Allow AppleII keyboard to see this cursor keypress too
		return 0;
	}

	return 1;
}

//===========================================================================

static void DoAutofire(UINT uButton, BOOL& pressed)
{
	static BOOL toggle[3] = {0};
	static BOOL lastPressed[3] = {0};

	BOOL nowPressed = pressed;
	if (GetPropertySheet().GetAutofire(uButton) && pressed)
	{
		toggle[uButton] = (!lastPressed[uButton]) ? TRUE : !toggle[uButton];
		pressed = pressed && toggle[uButton];
	}
	lastPressed[uButton] = nowPressed;
}

BYTE __stdcall JoyportReadButton(WORD address, ULONG nExecutedCycles)
{
	BOOL pressed = 0;

	if (g_uJoyportActiveStick == 0)
	{
		switch (address)
		{
			case 0x61:
				pressed = (buttonlatch[0] || joybutton[0] || setbutton[0] /*|| keydown[JK_OPENAPPLE]*/);
				if(joyinfo[joytype[1]] != DEVICE_KEYBOARD)	// BUG? joytype[1] should be [0] ?
					pressed = (pressed || keydown[JK_BUTTON0]);
				buttonlatch[0] = 0;
				break;

			case 0x62:	// Left or Up
				if (g_uJoyportReadMode == JOYPORT_LEFTRIGHT)	// LEFT
				{
					if (xpos[0] == 0)	// TODO: More range for mouse control?
						pressed = 1;
				}
				else	// UP
				{
					if (ypos[0] == 0)	// TODO: More range for mouse control?
						pressed = 1;
				}
				break;

			case 0x63:	// Right or Down
				if (g_uJoyportReadMode == JOYPORT_LEFTRIGHT)	// RIGHT
				{
					if (xpos[0] >= 255)	// TODO: More range for mouse control?
						pressed = 1;
				}
				else	// DOWN
				{
					if (ypos[0] >= 255)	// TODO: More range for mouse control?
						pressed = 1;
				}
				break;
		}
	}
	else	// TODO: stick #1
	{
	}

	pressed = pressed ? 0 : 1;	// Invert as Joyport signals are active low

	return MemReadFloatingBus(pressed, nExecutedCycles);
}

BYTE __stdcall JoyReadButton(WORD pc, WORD address, BYTE, BYTE, ULONG nExecutedCycles)
{
	address &= 0xFF;

	if(joyinfo[joytype[0]] == DEVICE_JOYSTICK)
		CheckJoystick0();
	if((joyinfo[joytype[1]] == DEVICE_JOYSTICK) || (joyinfo[joytype[1]] == DEVICE_JOYSTICK_THUMBSTICK2))
		CheckJoystick1();

	if (g_bJoyportEnabled)
	{
		// Some extra logic to stop the Joyport forcing a self-test at CTRL+RESET
		if ((address != 0x62) || (address == 0x62 && pc != 0xC242 && pc != 0xC2BE))	// Original //e ($C242), Enhanced //e ($C2BE) 
			return JoyportReadButton(address, nExecutedCycles);
	}

	BOOL pressed = 0;
	switch (address)
	{
		case 0x61:
			pressed = (buttonlatch[0] || joybutton[0] || setbutton[0] || keydown[JK_OPENAPPLE]);
			if(joyinfo[joytype[1]] != DEVICE_KEYBOARD)	// BUG? joytype[1] should be [0] ?
				pressed = (pressed || keydown[JK_BUTTON0]);
			buttonlatch[0] = 0;
			DoAutofire(0, pressed);
			break;

		case 0x62:
			pressed = (buttonlatch[1] || joybutton[1] || setbutton[1] || keydown[JK_CLOSEDAPPLE]);
			if(joyinfo[joytype[1]] != DEVICE_KEYBOARD)
				pressed = (pressed || keydown[JK_BUTTON1]);
			buttonlatch[1] = 0;
			DoAutofire(1, pressed);
			break;

		case 0x63:
			if (IS_APPLE2 && (joyinfo[joytype[1]] == DEVICE_NONE))
			{
				// Apple II/II+ with no joystick has the "SHIFT key mod"
				// See Sather's Understanding The Apple II p7-36
				pressed = !(GetKeyState(VK_SHIFT) < 0);
			}
			else
			{
				pressed = (buttonlatch[2] || joybutton[2] || setbutton[2]);
				DoAutofire(2, pressed);
			}
			buttonlatch[2] = 0;
			break;
	}

	return MemReadFloatingBus(pressed, nExecutedCycles);
}

//===========================================================================

// PREAD:		; $FB1E
//  AD 70 C0 : (4) LDA $C070
//  A0 00    : (2) LDA #$00
//  EA       : (2) NOP
//  EA       : (2) NOP
// Lbl1:						; 11 cycles is the normal duration of the loop
//  BD 64 C0 : (4) LDA $C064,X
//  10 04    : (2) BPL Lbl2		; NB. 3 cycles if branch taken (not likely)
//  C8       : (2) INY
//  D0 F8    : (3) BNE Lbl1		; NB. 2 cycles if branch not taken (not likely)
//  88       : (2) DEY
// Lbl2:
//  60       : (6) RTS
//

static const double PDL_CNTR_INTERVAL = 2816.0 / 255.0;	// 11.04 (From KEGS)

BYTE __stdcall JoyReadPosition(WORD programcounter, WORD address, BYTE, BYTE, ULONG nExecutedCycles)
{
	int nJoyNum = (address & 2) ? 1 : 0;	// $C064..$C067

	CpuCalcCycles(nExecutedCycles);

	ULONG nPdlPos = (address & 1) ? ypos[nJoyNum] : xpos[nJoyNum];

	// This is from KEGS. It helps games like Championship Lode Runner & Boulderdash
	if(nPdlPos >= 255)
		nPdlPos = 280;

	BOOL nPdlCntrActive = g_nCumulativeCycles <= (g_nJoyCntrResetCycle + (unsigned __int64) ((double)nPdlPos * PDL_CNTR_INTERVAL));

	// If no joystick connected, then this is always active (GH#778)
	if (joyinfo[joytype[nJoyNum]] == DEVICE_NONE)
		nPdlCntrActive = TRUE;

	return MemReadFloatingBus(nPdlCntrActive, nExecutedCycles);
}

//===========================================================================
void JoyReset()
{
  int loop = 0;
  while (loop < JK_MAX)
    keydown[loop++] = FALSE;
}

//===========================================================================
void JoyResetPosition(ULONG nExecutedCycles)
{
	CpuCalcCycles(nExecutedCycles);
	g_nJoyCntrResetCycle = g_nCumulativeCycles;

	if(joyinfo[joytype[0]] == DEVICE_JOYSTICK)
		CheckJoystick0();
	if((joyinfo[joytype[1]] == DEVICE_JOYSTICK) || (joyinfo[joytype[1]] == DEVICE_JOYSTICK_THUMBSTICK2))
		CheckJoystick1();
}

//===========================================================================

// Called when mouse is being used as a joystick && mouse button changes
void JoySetButton(eBUTTON number, eBUTTONSTATE down)
{
  if (number > 1)	// Sanity check on mouse button #
    return;

  // If 2nd joystick is enabled, then both joysticks only have 1 button
  if((joyinfo[joytype[1]] != DEVICE_NONE) && (number != 0))
	  return;

  // If it is 2nd joystick that is being emulated with mouse, then re-map button #
  if(joyinfo[joytype[1]] == DEVICE_MOUSE)
  {
	number = BUTTON1;	// 2nd joystick controls Apple button #1
  }

  setbutton[number] = down;

  if (down)
    buttonlatch[number] = BUTTONTIME;
}

//===========================================================================
BOOL JoySetEmulationType(HWND window, DWORD newtype, int nJoystickNumber, const bool bMousecardActive)
{
  if(joytype[nJoystickNumber] == newtype)
	  return 1;	// Already set to this type. Return OK.

  if (joyinfo[newtype] == DEVICE_JOYSTICK || joyinfo[newtype] == DEVICE_JOYSTICK_THUMBSTICK2)
  {
    JOYCAPS caps;
	unsigned int nJoy2ID = joyinfo[newtype] == DEVICE_JOYSTICK_THUMBSTICK2 ? JOYSTICKID1 : JOYSTICKID2;
	unsigned int nJoyID = nJoystickNumber == JN_JOYSTICK0 ? JOYSTICKID1 : nJoy2ID;
    if (joyGetDevCaps(nJoyID, &caps, sizeof(JOYCAPS)) != JOYERR_NOERROR)
    {
      MessageBox(window,
                 TEXT("The emulator is unable to read your PC joystick.  ")
                 TEXT("Ensure that your game port is configured properly, ")
                 TEXT("that the joystick is firmly plugged in, and that ")
                 TEXT("you have a joystick driver installed."),
                 TEXT("Configuration"),
                 MB_ICONEXCLAMATION | MB_SETFOREGROUND);
      return 0;
    }
    if ((joyinfo[newtype] == DEVICE_JOYSTICK_THUMBSTICK2) && (caps.wNumAxes < 4))
    {
      MessageBox(window,
                 TEXT("The emulator is unable to read thumbstick 2.  ")
                 TEXT("Ensure that your game port is configured properly, ")
                 TEXT("that the joystick is firmly plugged in, and that ")
                 TEXT("you have a joystick driver installed."),
                 TEXT("Configuration"),
                 MB_ICONEXCLAMATION | MB_SETFOREGROUND);
      return 0;
    }
  }
  else if ((joyinfo[newtype] == DEVICE_MOUSE) &&
           (joyinfo[joytype[nJoystickNumber]] != DEVICE_MOUSE))
  {
	if (bMousecardActive)
	{
		// Shouldn't be necessary, since Property Sheet's logic should prevent this option being given to the user.
	  MessageBox(window,
				 TEXT("Mouse interface card is enabled - unable to use mouse for joystick emulation."),
				 TEXT("Configuration"),
				 MB_ICONEXCLAMATION | MB_SETFOREGROUND);
	  return 0;
	}

    MessageBox(window,
               TEXT("To begin emulating a joystick with your mouse, move ")
               TEXT("the mouse cursor over the emulated screen of a running ")
               TEXT("program and click the left mouse button.  During the ")
               TEXT("time the mouse is emulating a joystick, you will not ")
               TEXT("be able to use it to perform mouse functions, and the ")
               TEXT("mouse cursor will not be visible.  To end joystick ")
               TEXT("emulation and regain the mouse cursor, click the left ")
               TEXT("mouse button while pressing Ctrl."),
               TEXT("Configuration"),
               MB_ICONINFORMATION | MB_SETFOREGROUND);
  }
  else if (joyinfo[newtype] == DEVICE_KEYBOARD)
  {
	  if (newtype == J0C_KEYBD_CURSORS || newtype == J1C_KEYBD_CURSORS)
	  {
			MessageBox(window,
						TEXT("Using cursor keys to emulate a joystick can cause conflicts.\n\n")
						TEXT("Be aware that 'cursor-up' = CTRL+K, and 'cursor-down' = CTRL+J.\n")
						TEXT("EG. Lode Runner uses CTRL+K/J to switch between keyboard/joystick modes ")
						TEXT("(and cursor-left/right to control speed).\n\n")
						TEXT("Also if cursor keys are blocked from being read from the Apple keyboard ")
						TEXT("then even simple AppleSoft command-line editing (cursor left/right) will not work."),
						TEXT("Configuration"),
						MB_ICONINFORMATION | MB_SETFOREGROUND);
	  }
  }

  joytype[nJoystickNumber] = newtype;
  JoyInitialize();
  JoyReset();
  return 1;
}


//===========================================================================

// Called when mouse is being used as a joystick && mouse position changes
void JoySetPosition(int xvalue, int xrange, int yvalue, int yrange)
{
  int nJoyNum = (joyinfo[joytype[0]] == DEVICE_MOUSE) ? 0 : 1;
  xpos[nJoyNum] = (xvalue*255)/xrange;
  ypos[nJoyNum] = (yvalue*255)/yrange;
}
 
//===========================================================================

// Update the latch (debounce) time for each button
void JoyUpdateButtonLatch(const UINT nExecutionPeriodUsec)
{
	for (UINT i=0; i<3; i++)
	{
		if (buttonlatch[i])
		{
			buttonlatch[i] -= nExecutionPeriodUsec;
			if (buttonlatch[i] < 0)
				buttonlatch[i] = 0;
		}
	}
}

//===========================================================================

BOOL JoyUsingMouse()
{
	return (joyinfo[joytype[0]] == DEVICE_MOUSE) || (joyinfo[joytype[1]] == DEVICE_MOUSE);
}

BOOL JoyUsingKeyboard()
{
	return (joyinfo[joytype[0]] == DEVICE_KEYBOARD) || (joyinfo[joytype[1]] == DEVICE_KEYBOARD);
}

BOOL JoyUsingKeyboardCursors()
{
	return (joytype[0] == J0C_KEYBD_CURSORS) || (joytype[1] == J1C_KEYBD_CURSORS);
}

BOOL JoyUsingKeyboardNumpad()
{
	return (joytype[0] == J0C_KEYBD_NUMPAD) || (joytype[1] == J1C_KEYBD_NUMPAD);
}

//===========================================================================

void JoyDisableUsingMouse()
{
	if (joyinfo[joytype[0]] == DEVICE_MOUSE)
		joytype[0] = J0C_DISABLED;

	if (joyinfo[joytype[1]] == DEVICE_MOUSE)
		joytype[1] = J1C_DISABLED;
}

//===========================================================================

void JoySetJoyType(UINT num, DWORD type)
{
	_ASSERT(num <= JN_JOYSTICK1);
	if (num > JN_JOYSTICK1)
		return;

	if (num == JN_JOYSTICK0)	// GH#434
	{
		_ASSERT(type < J0C_MAX);
		if (type >= J0C_MAX)
			return;
	}
	else
	{
		_ASSERT(type < J1C_MAX);
		if (type >= J1C_MAX)
			return;
	}

	joytype[num] = type;

	// Refresh centre positions whenever 'joytype' changes
	JoySetTrim(JoyGetTrim(true) , true);
	JoySetTrim(JoyGetTrim(false), false);
}

DWORD JoyGetJoyType(UINT num)
{
	_ASSERT(num <= JN_JOYSTICK1);
	if (num > JN_JOYSTICK1)
		return J0C_DISABLED;

	return joytype[num];
}

//===========================================================================

void JoySetTrim(short nValue, bool bAxisX)
{
	if(bAxisX)
		g_nPdlTrimX = nValue;
	else
		g_nPdlTrimY = nValue;

	int nJoyNum = -1;

	if(joyinfo[joytype[0]] == DEVICE_KEYBOARD)
		nJoyNum = 0;
	else if(joyinfo[joytype[1]] == DEVICE_KEYBOARD)
		nJoyNum = 1;

	if(nJoyNum >= 0)
	{
        xpos[nJoyNum] = PDL_CENTRAL+g_nPdlTrimX;
        ypos[nJoyNum] = PDL_CENTRAL+g_nPdlTrimY;
	}
}

short JoyGetTrim(bool bAxisX)
{
	return bAxisX ? g_nPdlTrimX : g_nPdlTrimY;
}

//===========================================================================

// Joyport truth-table:
//
// AN0  AN1  /PB0       /PB1    /PB2
// ------------------------------------
//  0    0   Trigger-1  Left-1  Right-1
//  0    1   Trigger-1  Up-1    Down-1
//  1    0   Trigger-2  Left-2  Right-2
//  1    1   Trigger-2  Up-2    Down-2

#if 0
void JoyportEnable(const bool bEnable)
{
	if (IS_APPLE2C)
		g_bJoyportEnabled = false;
	else
		g_bJoyportEnabled = bEnable ? 1 : 0;
}
#endif

void JoyportControl(const UINT uControl)
{
	if (!g_bJoyportEnabled)
		return;

	switch (uControl)
	{
	case 0:	// AN0 clr
		g_uJoyportActiveStick = 0;
		break;
	case 1:	// AN0 set
		g_uJoyportActiveStick = 1;
		break;
	case 2:	// AN1 clr
		g_uJoyportReadMode = JOYPORT_LEFTRIGHT;
		break;
	case 3:	// AN1 set
		g_uJoyportReadMode = JOYPORT_UPDOWN;
		break;
	}
}

//===========================================================================

#define SS_YAML_KEY_COUNTERRESETCYCLE "Counter Reset Cycle"
#define SS_YAML_KEY_JOY0TRIMX "Joystick0 TrimX"
#define SS_YAML_KEY_JOY0TRIMY "Joystick0 TrimY"
#define SS_YAML_KEY_JOY1TRIMX "Joystick1 TrimX"
#define SS_YAML_KEY_JOY1TRIMY "Joystick1 TrimY"

static std::string JoyGetSnapshotStructName(void)
{
	static const std::string name("Joystick");
	return name;
}

void JoySaveSnapshot(YamlSaveHelper& yamlSaveHelper)
{
	YamlSaveHelper::Label state(yamlSaveHelper, "%s:\n", JoyGetSnapshotStructName().c_str());
	yamlSaveHelper.SaveHexUint64(SS_YAML_KEY_COUNTERRESETCYCLE, g_nJoyCntrResetCycle);
	yamlSaveHelper.SaveInt(SS_YAML_KEY_JOY0TRIMX, JoyGetTrim(true));
	yamlSaveHelper.SaveInt(SS_YAML_KEY_JOY0TRIMY, JoyGetTrim(false));
	yamlSaveHelper.Save("%s: %d # not implemented yet\n", SS_YAML_KEY_JOY1TRIMX, 0);	// not implemented yet
	yamlSaveHelper.Save("%s: %d # not implemented yet\n", SS_YAML_KEY_JOY1TRIMY, 0);	// not implemented yet
}

void JoyLoadSnapshot(YamlLoadHelper& yamlLoadHelper)
{
	if (!yamlLoadHelper.GetSubMap(JoyGetSnapshotStructName()))
		return;

	g_nJoyCntrResetCycle = yamlLoadHelper.LoadUint64(SS_YAML_KEY_COUNTERRESETCYCLE);
	JoySetTrim(yamlLoadHelper.LoadInt(SS_YAML_KEY_JOY0TRIMX), true);
	JoySetTrim(yamlLoadHelper.LoadInt(SS_YAML_KEY_JOY0TRIMY), false);
	yamlLoadHelper.LoadInt(SS_YAML_KEY_JOY1TRIMX);	// dump value
	yamlLoadHelper.LoadInt(SS_YAML_KEY_JOY1TRIMY);	// dump value

	yamlLoadHelper.PopMap();
}
