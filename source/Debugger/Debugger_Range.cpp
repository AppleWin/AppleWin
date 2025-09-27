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

#include "../CardManager.h"

// Util - Range _______________________________________________________________


//===========================================================================
RangeType_t Range_GetPrefix(const int iArg, AddressPrefix_t* pAP)
{
	if ((iArg + 1) >= MAX_ARGS)
		return RANGE_NO_PREFIX;

	if (g_aArgs[iArg + 1].eToken != TOKEN_FSLASH)
		return RANGE_NO_PREFIX;

	// Spec: (GH#451)
	// [sN/][nn/][L<1|2>/][ROM/]<nnnn>

	if (tolower(g_aArgs[iArg].sArg[0]) == 's')	// slot
	{
		size_t len = strlen(g_aArgs[iArg].sArg);
		pAP->nSlot = g_aArgs[iArg].sArg[len - 1] - '0';	// eg. s1 or sl1 or slot1
		if (pAP->nSlot > 7)
		{
			ConsoleDisplayError("Address prefix out-of-range. Use slots 0-7.");
			return RANGE_PREFIX_BAD;
		}
	}
	else if (tolower(g_aArgs[iArg].sArg[0]) == 'l')	// LC
	{
		size_t len = strlen(g_aArgs[iArg].sArg);
		pAP->nLangCard = g_aArgs[iArg].sArg[len - 1] - '0';	// eg. l1 or lc1
		if (pAP->nLangCard < 1 || pAP->nLangCard > 2)
		{
			ConsoleDisplayError("Address prefix out-of-range. Use lc 1 or 2.");
			return RANGE_PREFIX_BAD;
		}
	}
	else if (tolower(g_aArgs[iArg].sArg[0]) == 'r')	// ROM
	{
		pAP->bIsROM = true;
	}
	else // bank (RamWorks or Saturn)
	{
		if (g_aArgs[iArg].nValue > 0x100)	// Permit 00-100
		{
			ConsoleDisplayError("Address prefix out-of-range. Use bank 0-100 (or 0-7 for Saturn).");
			return RANGE_PREFIX_BAD;
		}
		pAP->nBank = g_aArgs[iArg].nValue;
	}

	return RANGE_PREFIX_OK;
}


//===========================================================================
bool Range_GetAllPrefixes(int& iArg, const int nArg, int& dArgPrefix, AddressPrefix_t* pAP, bool slotCheck/*=true*/)
{
	const int kArgsPerPrefix = 2;
	for (int i = iArg; i < nArg; i += kArgsPerPrefix)
	{
		RangeType_t prefix = Range_GetPrefix(i, pAP);
		if (prefix == RANGE_NO_PREFIX)
			break;
		if (prefix == RANGE_PREFIX_BAD)
			return false;
		dArgPrefix += kArgsPerPrefix;
		iArg += kArgsPerPrefix;	// done 1 prefix (2 args)
	}

	// Got all prefixes, so do some checks:

	if (pAP->bIsROM &&
		(pAP->nSlot != AddressPrefix_t::kSlotInvalid || pAP->nBank != AddressPrefix_t::kBankInvalid || pAP->nLangCard != AddressPrefix_t::kLangCardInvalid))
	{
		ConsoleDisplayError("Address prefix bad: 'r/' not permitted with other prefixes.");
		return false;
	}

	if (pAP->nSlot != AddressPrefix_t::kSlotInvalid)	// Currently setting a slot# means Saturn card
	{
		if (slotCheck)
		{
			if (pAP->nBank != AddressPrefix_t::kBankInvalid && pAP->nBank > 7)
			{
				ConsoleDisplayError("Address prefix bad: Saturn only supports banks 0-7.");
				return false;
			}

			if (GetCardMgr().QuerySlot(pAP->nSlot) != CT_Saturn128K)
			{
				ConsoleDisplayError("Address prefix bad: No Saturn in slot.");
				return false;
			}
		}
	}
	else // No slot# specified, so aux slot (for Extended 80Col or RamWorks card)
	{
		if (pAP->nBank != AddressPrefix_t::kBankInvalid)
		{
			if (!IsAppleIIeOrAbove(GetApple2Type()))
			{
				ConsoleDisplayError("Address prefix bad: Aux slot requires //e or above.");
				return false;
			}
		}
	}

	return true;
}

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
RangeType_t Range_Get( WORD & nAddress1_, WORD & nAddress2_, const int iArg/* =1 */)
{
	nAddress1_ = g_aArgs[ iArg ].nValue;
	if (nAddress1_ > _6502_MEM_END)
		nAddress1_ = _6502_MEM_END;

	nAddress2_ = 0;
	int nTemp  = 0;

	RangeType_t eRange = RANGE_MISSING_ARG_2;

	if ((iArg + 2) >= MAX_ARGS)
		return eRange;

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
