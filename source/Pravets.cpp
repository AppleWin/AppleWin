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

//Pravets 8A/C variables
bool P8CAPS_ON = false;
bool P8Shift = false;

void PravetsReset(void)
{
	if (g_Apple2Type == A2TYPE_PRAVETS8A)
	{
		P8CAPS_ON = false; 
		TapeWrite(0, 0, 0, 0 ,0);
		GetFrame().FrameRefreshStatus(DRAW_LEDS);
	}
}
