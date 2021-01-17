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

/* Description: Tape interface.
 *
 * Author: Various
 *
 * In comments, UTAIIe is an abbreviation for a reference to "Understanding the Apple //e" by James Sather
 */

#include "StdAfx.h"

#include "Core.h"
#include "Tape.h"
#include "Memory.h"
#include "Pravets.h"

//---------------------------------------------------------------------------

BYTE __stdcall TapeRead(WORD, WORD address, BYTE, BYTE, ULONG nExecutedCycles)	// $C060 TAPEIN
{
	if (g_Apple2Type == A2TYPE_PRAVETS8A)
		return GetPravets().GetKeycode( MemReadFloatingBus(nExecutedCycles) );

	return MemReadFloatingBus(1, nExecutedCycles); // TAPEIN has high bit 1 when input is low or not connected (UTAIIe page 7-5, 7-6)
}

BYTE __stdcall TapeWrite(WORD, WORD address, BYTE, BYTE, ULONG nExecutedCycles)	// $C020 TAPEOUT
{
	return 0;
}
