/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 1994-1996, Michael O'Brien
Copyright (C) 1999-2001, Oliver Schmidt
Copyright (C) 2002-2005, Tom Charlesworth
Copyright (C) 2006-2015, Tom Charlesworth, Michael Pohoreski

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

/* Description: Pravets - Apple II clone
 *
 * Author: Various
 */

#include "StdAfx.h"

#include "Pravets.h"
#include "Core.h"
#include "Interface.h"
#include "Keyboard.h"
#include "Tape.h"

Pravets::Pravets(void)
{
	// Pravets 8A
	g_CapsLockAllowed = false;

	// Pravets 8A/8C
	P8CAPS_ON = false;
	P8Shift = false;
}

void Pravets::Reset(void)
{
	if (GetApple2Type() == A2TYPE_PRAVETS8A)
	{
		P8CAPS_ON = false; 
		g_CapsLockAllowed = false;
		GetFrame().FrameRefreshStatus(DRAW_LEDS | DRAW_DISK_STATUS);
	}
}

//In Pravets8A/C if SETMODE (8bit character encoding) is enabled, bit6 in $C060 is 0; Else it is 1
//If (CAPS lOCK of Pravets8A/C is on or Shift is pressed) and (MODE is enabled), bit7 in $C000 is 1; Else it is 0
//Writing into $C060 sets MODE on and off. If bit 0 is 0 the the MODE is set 0, if bit 0 is 1 then MODE is set to 1 (8-bit)

BYTE Pravets::GetKeycode(BYTE floatingBus)	// Read $C060
{
	const BYTE uCurrentKeystroke = KeybGetKeycode();
	BYTE C060 = floatingBus;

	if (GetApple2Type() == A2TYPE_PRAVETS8A && g_CapsLockAllowed) //8bit keyboard mode
	{
		if ((!P8CAPS_ON && !P8Shift) || (P8CAPS_ON && P8Shift)) //LowerCase
		{
			if ((uCurrentKeystroke<65) //|| ((uCurrentKeystroke>90) && (uCurrentKeystroke<96))
				|| ((uCurrentKeystroke>126) && (uCurrentKeystroke<193)))
					C060 |= 1 << 7; //Sets bit 7 to 1 for special keys (arrows, return, etc) and for control+ key combinations.
			else
				C060 &= 0x7F; //sets bit 7 to 0
		}
		else //UpperCase
		{
			C060 |= 1 << 7;
		}
	}
	else //7bit keyboard mode
	{
		C060 &= 0xBF; //Sets bit 6 to 0; I am not sure if this shall be done, because its value is disregarded in this case.
		C060 |= 1 << 7; //Sets bit 7 to 1
	}

	return C060;
}

BYTE Pravets::SetCapsLockAllowed(BYTE value)	// Write $C060
{
	if (GetApple2Type() == A2TYPE_PRAVETS8A) 				
	{
		//If bit0 of the input byte is 0, it will forbid 8-bit characters (Default)
		//If bit0 of the input byte is 1, it will allow 8-bit characters

		if (value & 1) 
			g_CapsLockAllowed = true;
		else 
			g_CapsLockAllowed = false;
	}

	return 0;
}

BYTE Pravets::ConvertToKeycode(WPARAM key, BYTE keycode)
{
	if (KeybGetCapsStatus() && (key >= 'a') && (key <='z'))
		P8Shift = true;
	else
		P8Shift = false;

	//The latter line should be applied for Pravtes 8A/C only, but not for Pravets 82/M !!!			
	if ((KeybGetCapsStatus() == false) && (key >= 'A') && (key <='Z'))
	{
		P8Shift = true;
		if (GetApple2Type() == A2TYPE_PRAVETS8A)
			keycode = key + 32;
	}

	//Remap some keys for Pravets82/M
	if (GetApple2Type() == A2TYPE_PRAVETS82)
	{
		if (key == 64) 
			keycode = 96;
		if (key == '^') 
			keycode = '~';

		if (KeybGetCapsStatus() == false) //cyrillic letters
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

	if (GetApple2Type() == A2TYPE_PRAVETS8M)  //Pravets 8M charset is still uncertain
	{
		if (KeybGetCapsStatus() == false) //cyrillic letters
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
	if (GetApple2Type() == A2TYPE_PRAVETS8A) //&& (g_bCapsLock == false))
	{
		if (KeybGetCapsStatus() == false) //i.e. cyrillic letters
	    {
			if (key == '[') keycode = '{';
			if (key == ']') keycode = '}';
			if (key == '`') keycode = '~';
			if (key == 92) keycode = 96;
			if (g_CapsLockAllowed == true)
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
			if (g_CapsLockAllowed == false)
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
					P8Shift = true;
				if (key == 96)	 //This line shall generate sth. else i.e. ` In fact. this character is not generateable by the pravets keyboard.
				{
					keycode = '^';
					P8Shift = true;
				}
				if (key == 126)	keycode = '^';
			}
		}
	}

	return keycode;
}

BYTE Pravets::ConvertToPrinterChar(BYTE value)
{
	char Lat8A[]= "abwgdevzijklmnoprstufhc~{}yx`q|]";
	char Lat82[]= "abwgdevzijklmnoprstufhc^[]yx@q{}~`";
	char Kir82[]= "ÀÁÂÃÄÅÆÇÈÉÊËÌÍÎÏÐÑÒÓÔÕÖ×ØÙÚÜÞß[]^@";
	char Kir8ACapital[]= "ÀÁÂÃÄÅÆÇÈÉÊËÌÍÎÏÐÑÒÓÔÕÖ×ØÙÚÜÞßÝ";
	char Kir8ALowerCase[]= "àáâãäåæçèéêëìíîïðñòóôõö÷øùúüþÿý";

	BYTE c = 0;

	if (GetApple2Type() == A2TYPE_PRAVETS8A)  //This is print conversion for Pravets 8A/C. Print conversion for Pravets82/M is still to be done.
	{
		if ((value > 90) && (value < 128)) //This range shall be set more precisely
		{
			c = value;
			int loop = 0;
			while (loop < 31)
			{
				if (c == Lat8A[loop])
					c = Kir8ALowerCase[loop];
				loop++;
			}
		}
		else if ((value > 64) && (value < 91))
		{
			c = value + 32;
		}
		else
		{
			c = value & 0x7F;
			int loop = 0;
			while (loop < 31)
			{
				if (c == Lat8A[loop])
					c = Kir8ACapital[loop];
				loop++;
			}
		}
	}
	else if (GetApple2Type() == A2TYPE_PRAVETS82 || GetApple2Type() == A2TYPE_PRAVETS8M)
	{
		c = value & 0x7F;
		int loop = 0;
		while (loop < 34)
		{
			if (c == Lat82[loop])
				c = Kir82[loop];
			loop++;
		}
	}

	return c;
}
