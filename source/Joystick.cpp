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
#pragma  hdrstop
#include "MouseInterface.h"

#define  BUTTONTIME       5000

#define  DEVICE_NONE      0
#define  DEVICE_JOYSTICK  1
#define  DEVICE_KEYBOARD  2
#define  DEVICE_MOUSE     3

#define  MODE_NONE        0
#define  MODE_STANDARD    1
#define  MODE_CENTERING   2
#define  MODE_SMOOTH      3

typedef struct
{
    int device;
    int mode;
} joyinforec;

static const joyinforec joyinfo[5] = {{DEVICE_NONE,MODE_NONE},
                           {DEVICE_JOYSTICK,MODE_STANDARD},
                           {DEVICE_KEYBOARD,MODE_STANDARD},
                           {DEVICE_KEYBOARD,MODE_CENTERING},
                           {DEVICE_MOUSE,MODE_STANDARD}};

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

static DWORD buttonlatch[3] = {0,0,0};
static BOOL  joybutton[3]   = {0,0,0};

static int   joyshrx[2]     = {8,8};
static int   joyshry[2]     = {8,8};
static int   joysubx[2]     = {0,0};
static int   joysuby[2]     = {0,0};

DWORD joytype[2]            = {DEVICE_JOYSTICK,DEVICE_NONE};	// Emulation Type for joysticks #0 & #1

static BOOL  setbutton[3]   = {0,0,0};	// Used when a mouse button is pressed/released

static int   xpos[2]        = {PDL_CENTRAL,PDL_CENTRAL};
static int   ypos[2]        = {PDL_CENTRAL,PDL_CENTRAL};

static unsigned __int64 g_nJoyCntrResetCycle = 0;	// Abs cycle that joystick counters were reset

static short g_nPdlTrimX = 0;
static short g_nPdlTrimY = 0;

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
          (joyinfo[joytype[1]].device == DEVICE_NONE)	// Only consider 2nd button if NOT emulating a 2nd Apple joystick
         )
		   buttonlatch[1] = BUTTONTIME;
      joybutton[0] = ((info.wButtons & JOY_BUTTON1) != 0);
      if (joyinfo[joytype[1]].device == DEVICE_NONE)	// Only consider 2nd button if NOT emulating a 2nd Apple joystick
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
    if (joyGetPos(JOYSTICKID2,&info) == JOYERR_NOERROR)
	{
      if ((info.wButtons & JOY_BUTTON1) && !joybutton[2])
	  {
        buttonlatch[2] = BUTTONTIME;
        if(joyinfo[joytype[1]].device != DEVICE_NONE)
          buttonlatch[1] = BUTTONTIME;	// Re-map this button when emulating a 2nd Apple joystick
	  }

      joybutton[2] = ((info.wButtons & JOY_BUTTON1) != 0);
      if(joyinfo[joytype[1]].device != DEVICE_NONE)
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
  // Emulated joystick #0 can only use JOYSTICKID1 (if no joystick, then use mouse)
  // Emulated joystick #1 can only use JOYSTICKID2 (if no joystick, then disable)

  //
  // Init for emulated joystick #0:
  //

  if (joyinfo[joytype[0]].device == DEVICE_JOYSTICK)
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
      joytype[0] = DEVICE_MOUSE;
	}
  }

  //
  // Init for emulated joystick #1:
  //

  if (joyinfo[joytype[1]].device == DEVICE_JOYSTICK)
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
      joytype[1] = DEVICE_NONE;
	}
  }
}

//===========================================================================

BOOL JoyProcessKey(int virtkey, BOOL extended, BOOL down, BOOL autorep)
{
  if( (joyinfo[joytype[0]].device != DEVICE_KEYBOARD) &&
	  (joyinfo[joytype[1]].device != DEVICE_KEYBOARD) &&
	  (virtkey != VK_MENU)								// VK_MENU == ALT Key
	)
    return 0;

  // Joystick # which is being emulated by keyboard
  int nJoyNum = (joyinfo[joytype[0]].device == DEVICE_KEYBOARD) ? 0 : 1;
  int nCenteringType = joyinfo[joytype[nJoyNum]].mode;	// MODE_STANDARD or MODE_CENTERING

  //

  BOOL keychange = !extended;

  if (virtkey == VK_MENU)	// VK_MENU == ALT Key (Button #0 or #1)
  {
    keychange = 1;
    keydown[JK_OPENAPPLE+(extended != 0)] = down;
  }
  else if (!extended)
  {
    if ((virtkey >= VK_NUMPAD1) && (virtkey <= VK_NUMPAD9))
	{
      keydown[virtkey-VK_NUMPAD1] = down;
	}
    else
	{
      switch (virtkey)
	  {
        case VK_END:     keydown[ 0] = down;  break;
        case VK_DOWN:    keydown[ 1] = down;  break;
        case VK_NEXT:    keydown[ 2] = down;  break;
        case VK_LEFT:    keydown[ 3] = down;  break;
        case VK_CLEAR:   keydown[ 4] = down;  break;
        case VK_RIGHT:   keydown[ 5] = down;  break;
        case VK_HOME:    keydown[ 6] = down;  break;
        case VK_UP:      keydown[ 7] = down;  break;
        case VK_PRIOR:   keydown[ 8] = down;  break;
        case VK_NUMPAD0: keydown[ 9] = down;  break;	// Button #0
        case VK_INSERT:  keydown[ 9] = down;  break;	// Button #0
        case VK_DECIMAL: keydown[10] = down;  break;	// Button #1
        case VK_DELETE:  keydown[10] = down;  break;	// Button #1
        default:         keychange = 0;       break;
      }
	}
  }

  //

  if (keychange)
  {
    if ((virtkey == VK_NUMPAD0) || (virtkey == VK_INSERT))
	{
	  if(down)
	  {
	    if(joyinfo[joytype[1]].device != DEVICE_KEYBOARD)
		{
          buttonlatch[0] = BUTTONTIME;
		}
		else if(joyinfo[joytype[1]].device != DEVICE_NONE)
		{
          buttonlatch[2] = BUTTONTIME;
          buttonlatch[1] = BUTTONTIME;	// Re-map this button when emulating a 2nd Apple joystick
		}
	  }
    }
    else if ((virtkey == VK_DECIMAL) || (virtkey == VK_DELETE))
	{
      if(down)
	  {
		if(joyinfo[joytype[1]].device != DEVICE_KEYBOARD)
          buttonlatch[1] = BUTTONTIME;
	  }
    }
    else if ((down && !autorep) || (nCenteringType == MODE_CENTERING))
	{
      int xkeys  = 0;
      int ykeys  = 0;
      int xtotal = 0;
      int ytotal = 0;
      int keynum = 0;
      while (keynum < 9)
	  {
        if (keydown[keynum])
		{
          if ((keynum % 3) != 1)
		  {
            xkeys++;
            xtotal += keyvalue[keynum].x;
          }
          if ((keynum / 3) != 1)
		  {
            ykeys++;
            ytotal += keyvalue[keynum].y;
          }
        }
        keynum++;
      }

      if (xkeys)
        xpos[nJoyNum] = xtotal / xkeys;
      else
        xpos[nJoyNum] = PDL_CENTRAL+g_nPdlTrimX;
      if (ykeys)
        ypos[nJoyNum] = ytotal / ykeys;
      else
        ypos[nJoyNum] = PDL_CENTRAL+g_nPdlTrimY;
    }
  }

  return keychange;
}

//===========================================================================

BYTE __stdcall JoyReadButton(WORD, WORD address, BYTE, BYTE, ULONG nCyclesLeft)
{
  address &= 0xFF;

  if(joyinfo[joytype[0]].device == DEVICE_JOYSTICK)
    CheckJoystick0();
  if(joyinfo[joytype[1]].device == DEVICE_JOYSTICK)
    CheckJoystick1();

  BOOL pressed = 0;
  switch (address) {

    case 0x61:
      pressed = (buttonlatch[0] || joybutton[0] || setbutton[0] || keydown[JK_OPENAPPLE]);
	  if(joyinfo[joytype[1]].device != DEVICE_KEYBOARD)
	      pressed = (pressed || keydown[JK_BUTTON0]);
      buttonlatch[0] = 0;
      break;

    case 0x62:
      pressed = (buttonlatch[1] || joybutton[1] || setbutton[1] || keydown[JK_CLOSEDAPPLE]);
	  if(joyinfo[joytype[1]].device != DEVICE_KEYBOARD)
	      pressed = (pressed || keydown[JK_BUTTON1]);
      buttonlatch[1] = 0;
      break;

    case 0x63:
      pressed = (buttonlatch[2] || joybutton[2] || setbutton[2] || (GetKeyState(VK_SHIFT) < 0));
      buttonlatch[2] = 0;
      break;

  }
  return MemReadFloatingBus(pressed, nCyclesLeft);
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
//  D0 F8    : (3) BNE Lbl1		; NB. 2 cycles if branck not taken (not likely)
//  88       : (2) DEY
// Lbl2:
//  60       : (6) RTS
//

static const double PDL_CNTR_INTERVAL = 2816.0 / 255.0;	// 11.04 (From KEGS)

BYTE __stdcall JoyReadPosition(WORD programcounter, WORD address, BYTE, BYTE, ULONG nCyclesLeft)
{
	int nJoyNum = (address & 2) ? 1 : 0;	// $C064..$C067

	CpuCalcCycles(nCyclesLeft);

	ULONG nPdlPos = (address & 1) ? ypos[nJoyNum] : xpos[nJoyNum];

	// This is from KEGS. It helps games like Championship Lode Runner & Boulderdash
	if(nPdlPos >= 255)
		nPdlPos = 280;

	BOOL nPdlCntrActive = g_nCumulativeCycles <= (g_nJoyCntrResetCycle + (unsigned __int64) ((double)nPdlPos * PDL_CNTR_INTERVAL));

	return MemReadFloatingBus(nPdlCntrActive, nCyclesLeft);
}

//===========================================================================
void JoyReset()
{
  int loop = 0;
  while (loop < JK_MAX)
    keydown[loop++] = FALSE;
}

//===========================================================================
BYTE __stdcall JoyResetPosition(WORD, WORD, BYTE, BYTE, ULONG nCyclesLeft)
{
	CpuCalcCycles(nCyclesLeft);
	g_nJoyCntrResetCycle = g_nCumulativeCycles;

	if(joyinfo[joytype[0]].device == DEVICE_JOYSTICK)
		CheckJoystick0();
	if(joyinfo[joytype[1]].device == DEVICE_JOYSTICK)
		CheckJoystick1();

	return MemReadFloatingBus(nCyclesLeft);
}

//===========================================================================

// Called when mouse is being used as a joystick && mouse button changes
void JoySetButton(eBUTTON number, eBUTTONSTATE down)
{
  if (number > 1)	// Sanity check on mouse button #
    return;

  // If 2nd joystick is enabled, then both joysticks only have 1 button
  if((joyinfo[joytype[1]].device != DEVICE_NONE) && (number != 0))
	  return;

  // If it is 2nd joystick that is being emulated with mouse, then re-map button #
  if(joyinfo[joytype[1]].device == DEVICE_MOUSE)
  {
	number = BUTTON1;	// 2nd joystick controls Apple button #1
  }

  setbutton[number] = down;

  if (down)
    buttonlatch[number] = BUTTONTIME;
}

//===========================================================================
BOOL JoySetEmulationType(HWND window, DWORD newtype, int nJoystickNumber)
{
  if(joytype[nJoystickNumber] == newtype)
	  return 1;	// Already set to this type. Return OK.

  if (joyinfo[newtype].device == DEVICE_JOYSTICK)
  {
    JOYCAPS caps;
	unsigned int nJoyID = nJoystickNumber == JN_JOYSTICK0 ? JOYSTICKID1 : JOYSTICKID2;
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
  }
  else if ((joyinfo[newtype].device == DEVICE_MOUSE) &&
           (joyinfo[joytype[nJoystickNumber]].device != DEVICE_MOUSE))
  {
	if (sg_Mouse.IsActive())
	{
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
  joytype[nJoystickNumber] = newtype;
  JoyInitialize();
  JoyReset();
  return 1;
}


//===========================================================================

// Called when mouse is being used as a joystick && mouse position changes
void JoySetPosition(int xvalue, int xrange, int yvalue, int yrange)
{
  int nJoyNum = (joyinfo[joytype[0]].device == DEVICE_MOUSE) ? 0 : 1;
  xpos[nJoyNum] = (xvalue*255)/xrange;
  ypos[nJoyNum] = (yvalue*255)/yrange;
}
 
//===========================================================================
void JoyUpdatePosition()
{
	if (buttonlatch[0]) --buttonlatch[0];
	if (buttonlatch[1]) --buttonlatch[1];
	if (buttonlatch[2]) --buttonlatch[2];
}

//===========================================================================
BOOL JoyUsingMouse()
{
  return (joyinfo[joytype[0]].device == DEVICE_MOUSE) || (joyinfo[joytype[1]].device == DEVICE_MOUSE);
}

//===========================================================================

void JoyDisableUsingMouse()
{
	if (joyinfo[joytype[0]].device == DEVICE_MOUSE)
		joytype[0] = DEVICE_NONE;

	if (joyinfo[joytype[1]].device == DEVICE_MOUSE)
		joytype[1] = DEVICE_NONE;
}

//===========================================================================

void JoySetTrim(short nValue, bool bAxisX)
{
	if(bAxisX)
		g_nPdlTrimX = nValue;
	else
		g_nPdlTrimY = nValue;

	int nJoyNum = -1;

	if(joyinfo[joytype[0]].device == DEVICE_KEYBOARD)
		nJoyNum = 0;
	else if(joyinfo[joytype[1]].device == DEVICE_KEYBOARD)
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

DWORD JoyGetSnapshot(SS_IO_Joystick* pSS)
{
	pSS->g_nJoyCntrResetCycle = g_nJoyCntrResetCycle;
	return 0;
}

DWORD JoySetSnapshot(SS_IO_Joystick* pSS)
{
	g_nJoyCntrResetCycle = pSS->g_nJoyCntrResetCycle;
	return 0;
}
