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

/* Description: This module is created for emulation of the 8bit character mode (mode 1) switch, 
 * which is located in $c060, and so far does not intend to emulate a tape device.
 *
 *
 * Author: Various
 */

#include "StdAfx.h"

static bool g_CapsLockAllowed = false;

//---------------------------------------------------------------------------

BYTE __stdcall TapeRead(WORD, WORD address, BYTE, BYTE, ULONG nCyclesLeft)
{
	/*
	If retrieving KeybGetKeycode(); causes problems uCurrentKeystroke shall be added 
	in the submission variables and it shall be added by the TapeRead caller
	i.e. BYTE __stdcall TapeRead (WORD, WORD address, BYTE, BYTE, ULONG nCyclesLeft) shall become
	     BYTE __stdcall TapeRead (WORD, WORD address, BYTE, BYTE, ULONG nCyclesLeft, BYTE uCurrentKeystroke)
	*/

	if (g_Apple2Type == A2TYPE_PRAVETS8A) 
	{
		const BYTE uCurrentKeystroke = KeybGetKeycode();
		BYTE C060 = MemReadFloatingBus(nCyclesLeft);

		if (g_CapsLockAllowed) //8bit keyboard mode
		{
			if (((P8CAPS_ON == false) && (P8Shift == false)) || ((P8CAPS_ON ) && (P8Shift ))) //LowerCase
			{
				if ((uCurrentKeystroke<65) //|| ((uCurrentKeystroke>90) && (uCurrentKeystroke<96))
					|| ((uCurrentKeystroke>126) && (uCurrentKeystroke<193)))
						C060 |= 1 << 7; //Sets bit 7 to 1 for special keys (arrows, return, etc) and for control+ key combinations.
				else
					C060 &= 127; //sets bit 7 to 0
			}
			else //UpperCase
			{
				C060 |= 1 << 7;
			}
		}
		else //7bit keyboard mode
		{
			C060 &= 191; //Sets bit 6 to 0; I am not sure if this shall be done, because its value is disregarded in this case.
			C060 |= 1 << 7; //Sets bit 7 to 1
		}

		return C060;
	}
	
	return (1<<7) | (MemReadFloatingBus(nCyclesLeft) & 0x7F);	// Keep high-bit fixed (since TAPEIN isn't supported)
}

/*
In case s.o. decides to develop tape device emulation, this function may be renamed,
because tape is not written in $C060
*/
BYTE __stdcall TapeWrite(WORD programcounter, WORD address, BYTE write, BYTE value, ULONG nCyclesLeft)
{
	if (g_Apple2Type == A2TYPE_PRAVETS8A) 				
	{
		if (value & 1) 
			g_CapsLockAllowed = true;
		else 
			g_CapsLockAllowed = false;

		//If bit0 of the input byte is 0, it will forbid 8-bit characters (Default)
		//If bit0 of the input byte is 1, it will allow 8-bit characters
		return 0;
	}

	return MemReadFloatingBus(nCyclesLeft);
}

bool GetCapsLockAllowed(void)
{
	return g_CapsLockAllowed;
}
