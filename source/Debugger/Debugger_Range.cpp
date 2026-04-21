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

/* Description: Debugger Utility Range
 *
 * Author: Copyright (C) 2006-2010 Michael Pohoreski
 */

#include "StdAfx.h"

#include "Debug.h"

// Util - Range _______________________________________________________________


//===========================================================================
bool Range_CalcEndLen( const RangeType_t eRange
	, const WORD & nAddress1, const WORD & nAddress2
	, WORD & nAddressEnd_, int & nAddressLen_ )
{
	bool bValid = false;

	if (eRange == RANGE_HAS_LEN)
	{
		// BSAVE 2000,0  Len=0 End=n/a
		// BSAVE 2000,1  Len=1 End=2000
		// 0,FFFF [,)
		// End =  FFFE = Len-1
		// Len =  FFFF
		nAddressLen_ = nAddress2;
		unsigned int nTemp = nAddress1 + nAddressLen_ - 1;
		if (nTemp > _6502_MEM_END)
			nTemp = _6502_MEM_END;
		nAddressEnd_ = nTemp;
		bValid = true;
	}
	else
	if (eRange == RANGE_HAS_END)
	{
		// BSAVE 2000:2000 Len=0, End=n/a
		// BSAVE 2000:2001 Len=1, End=2000
		// 0:FFFF [,]
		// End =  FFFF
		// Len = 10000 = End+1
		nAddressEnd_ = nAddress2;
		nAddressLen_ = nAddress2 - nAddress1 + 1;
		bValid = true;
	}

	return bValid;
}


//===========================================================================
RangeType_t Range_Get( WORD & nAddress1_, WORD & nAddress2_, const int iArg ) // =1
{
	nAddress1_ = (unsigned) g_aArgs[ iArg ].nValue; 
	if (nAddress1_ > _6502_MEM_END)
		nAddress1_ = _6502_MEM_END;

	nAddress2_ = 0;
	int nTemp  = 0;

	RangeType_t eRange = RANGE_MISSING_ARG_2;

	if (g_aArgs[ iArg + 1 ].eToken == TOKEN_COMMA)
	{
		// 0,FFFF [,) // Note the mathematical range
		// End =  FFFE = Len-1
		// Len =  FFFF
		eRange = RANGE_HAS_LEN;
		nTemp  = g_aArgs[ iArg + 2 ].nValue;
		nAddress2_ = nTemp;
	}
	else
	if (g_aArgs[ iArg + 1 ].eToken == TOKEN_COLON)
	{
		// 0:FFFF [,] // Note the mathematical range
		// End =  FFFF
		// Len = 10000 = End+1
		eRange = RANGE_HAS_END;
		nTemp  = g_aArgs[ iArg + 2 ].nValue;

		// i.e.
		// FFFF:D000
		// 1    2    Temp
		// FFFF      D000
		//      FFFF
		// D000
		if (nAddress1_ > nTemp)
		{
			nAddress2_ = nAddress1_;
			nAddress1_ = nTemp;
		}
		else
			nAddress2_ = nTemp;
	}

	// .17 Bug Fix: D000,FFFF -> D000,CFFF (nothing searched!)
//	if (nTemp > _6502_MEM_END)
//		nTemp = _6502_MEM_END;

	return eRange;
}
