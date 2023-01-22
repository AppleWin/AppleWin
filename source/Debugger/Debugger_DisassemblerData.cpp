/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 2006-2014, Tom Charlesworth, Michael Pohoreski

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

/* Description: Data Blocks shown in Disassembler
 *
 * Author: Copyright (C) 2009 - 2010 Michael Pohoreski
 */

#include "StdAfx.h"

#include "Debug.h"


// Disassembler Data ______________________________________________________________________________

// __ Debugger Interface ____________________________________________________________________________

std::string _GetAutoSymbolName(const Nopcode_e& nopcode, const WORD nStartAddress)
{
	switch (nopcode)
	{
		case NOP_ADDRESS:
			return StrFormat( "A_%04X", nStartAddress ); // DA range

		case NOP_FAC:
			return StrFormat( "F_%04X", nStartAddress ); // DF range

		case NOP_STRING_ASCII:
		case NOP_STRING_APPLE:
			return StrFormat( "T_%04X", nStartAddress ); // ASC range

		case NOP_WORD_1:
		case NOP_WORD_2:
		case NOP_WORD_4:
			return StrFormat( "W_%04X", nStartAddress ); // DW range

		case NOP_BYTE_1:
		case NOP_BYTE_2:
		case NOP_BYTE_4:
		case NOP_BYTE_8:
		default:
			return StrFormat( "B_%04X", nStartAddress ); // DB range
	}
}

// @param tData_ Filled out with range data
//===========================================================================
WORD _GetDataRange (int nArgs, int iArg, DisasmData_t& tData_)
{
	WORD nAddress  = 0;
	WORD nAddress2 = 0;
	WORD nEnd      = 0;
	int  nLen      = 0;

	memset( (void*) &tData_, 0, sizeof(tData_) );

	// DB address
	// DB symbol
	// bool bisAddress ...

	if (nArgs < 1)
	{
		nAddress = g_nDisasmCurAddress;
	}
	else
	{
		// DB foo = 300 // nArgs == 3

		RangeType_t eRange = Range_Get( nAddress, nAddress2, iArg);
		if ((eRange == RANGE_HAS_END) ||
			(eRange == RANGE_HAS_LEN))
		{
			Range_CalcEndLen( eRange, nAddress, nAddress2, nEnd, nLen );
			nLen--; // Disassembly_IsDataAddress() is *inclusive* // KEEP IN SYNC: _CmdDefineByteRange() CmdDisasmDataList() _6502_GetOpmodeOpbyte() FormatNopcodeBytes()
		}
		else
		{
			if (nArgs > 1)
			{
				// 2.9.1.1 Add: Support for equal sign, also make it optional for command DB
				// DB FOO   300
				// DB FOO = 300
				if (g_aArgs[2].bType == TOKEN_EQUAL)
					nAddress = g_aArgs[ 3 ].nValue;
				else
					nAddress = g_aArgs[ 2 ].nValue;
			}
			else
				nAddress = g_aArgs[ 1 ].nValue;
		}
	}

	// 2.7.0.35 DW address -- round the length up to even number for convenience.
	// Example: 'DW 6062' is equivalent to: 'DW 6062:6063'
	if ( g_iCommand == CMD_DEFINE_DATA_WORD1 )
	{
		if ( ~nLen & 1 )
			nLen++;
	}

	tData_.nStartAddress = nAddress;
	tData_.nEndAddress = nAddress + nLen;

	return nAddress;
}

//===========================================================================
WORD _CmdDefineByteRange(int nArgs,int iArg,DisasmData_t & tData_)
{
	WORD nAddress  = 0;
	WORD nAddress2 = 0;
	WORD nEnd      = 0;
	int  nLen      = 0;

	nAddress = _GetDataRange(nArgs,iArg,tData_);

	std::string sSymbolName;

	SymbolTable_Index_e eSymbolTable = SYMBOLS_ASSEMBLY;
	bool bAutoDefineName = false; // 2.7.0.34

	if ( nArgs > 1 )
	{
		if ( g_aArgs[ 2 ].eToken == TOKEN_COLON ) // 2.7.0.31 Bug fix: DB range, i.e. DB 174E:174F
		{
			bAutoDefineName = true;
		}
		else
		{
			g_aArgs[ 1 ].sArg[MAX_SYMBOLS_LEN] = 0;	// truncate to max symbol length
			sSymbolName = g_aArgs[1].sArg;
		}
	}
	else
	{
		bAutoDefineName = true;
	}

	// 2.7.0.34 Unified auto-defined name: B_, W_, T_ for byte, word, or text respectively
	// Old name: auto define D_# DB $XX
	// Example 'DB' or 'DW' with 1 arg
	//   DB 801
	Nopcode_e nopcode = NOP_BYTE_1;

	bool isFloat = (g_iCommand == CMD_DEFINE_DATA_FLOAT);
	if ( isFloat )
		nopcode = NOP_FAC;

	bool isString = (g_iCommand == CMD_DEFINE_DATA_STR);
	if ( isString )
		nopcode = NOP_STRING_ASCII;

	bool isWord1 = (g_iCommand == CMD_DEFINE_DATA_WORD1);
	if ( isWord1 )
		nopcode = NOP_WORD_1;

	bool isAddr = (g_iCommand == CMD_DEFINE_ADDR_WORD);
	if ( isAddr )
		nopcode = NOP_ADDRESS;

	if ( bAutoDefineName )
	{
		sSymbolName = _GetAutoSymbolName( nopcode, tData_.nStartAddress );
	}

	// bRemoveSymbol = false // use arg[2]
	// bUpdateSymbol = true // add the symbol to the table
	SymbolUpdate( eSymbolTable, sSymbolName.c_str(), nAddress, false, true );

	// TODO: Note: need to call ConsoleUpdate(), as may print symbol has been updated
	
	// NOTE: We don't set the type here
	//    tData_.eElementType = nopcode;
	// As that is done by the caller.

	strncpy_s( tData_.sSymbol, sSymbolName.c_str(), _TRUNCATE );

	return nAddress;
}

// Undefine Data
//
//     X
//     X addr
//     X addr:addr
//
// Example:
//     DB hgr 2000:3FFF
//     U 1FFF
//     X 1FFF:2001
//===========================================================================
Update_t CmdDisasmDataDefCode (int nArgs)
{
	// treat memory (bytes) as code
	if (! ((nArgs <= 2) || (nArgs == 4)) && (nArgs != 3))
	{
		return Help_Arg_1( CMD_DISASM_CODE );
	}

	DisasmData_t tData; // X 1FFF:2001
	WORD nAddress  = 0; // 8191
	int  iArg      = 0;

	if (nArgs == 3)
	{
		iArg = 1; // #### : ####
		nAddress  = _GetDataRange( nArgs, iArg, tData );
	}
	else
	{
		iArg      = 2;
		nAddress  = _GetDataRange( nArgs, iArg, tData );
	}

	// Need to iterate through all blocks
	// DB TEST1 300:320
	// DB TEST2 310:330
	// DB TEST3 320:340
	// X  TEST1

	/*
        // Edge cases:
        U 1FFF
        DB 2000:2005
        X 1FFF:2001 // Chop 2 head
        X 2004:2006 // Chop 2 tail
        X 1FFF:2006 // Chop entire

        U 1FFF
        DB 2000:2000
        X  2000:2000 // Chop entire

        U 1FFF
        DB 2000:2002
        X  2001:2001 // Chop middle

        U 1FFF
        DB 2000:2005
        X  2002:2003 // Chop middle
	*/
	while (nAddress <= tData.nEndAddress)
	{
		DisasmData_t *pData = Disassembly_IsDataAddress( nAddress );
		if ( pData )
		{
			if ((      nAddress    <= pData->nStartAddress)
			&&  (tData.nEndAddress >= pData->nEndAddress  ))
			{
				// remove entire
				Disassembly_DelData( *pData );
			}
			else
			if ((      nAddress      <= pData->nStartAddress)
			&&  (tData.nStartAddress <  pData->nEndAddress  ))
			{
				// head
				pData->nStartAddress = nAddress+1;
			}
			else
			if ((      nAddress    >  pData->nStartAddress)
			&&  (tData.nEndAddress >= pData->nEndAddress  ))
			{
				// tail
				pData->nEndAddress = nAddress-1;
			}
			else
			{
				// middle
				SymbolTable_Index_e eSymbolTable = SYMBOLS_ASSEMBLY;

				DisasmData_t tSplit = *pData;
				pData->nEndAddress = nAddress - 1;

				tSplit.nStartAddress = tData.nEndAddress + 1; // nAddress + 1;
				std::string sSymbolName = _GetAutoSymbolName( pData->eElementType, tSplit.nStartAddress );
				strncpy_s(tSplit.sSymbol, sSymbolName.c_str(), _TRUNCATE);

				SymbolUpdate( eSymbolTable, tSplit.sSymbol, tSplit.nStartAddress, false, true );
				Disassembly_AddData( tSplit );
			}

			// TODO: Remove symbol 'D_FA62' from symbol table!
		}

		nAddress++;
	}

	return UPDATE_DISASM | ConsoleUpdate();
}

const char* g_aNopcodeTypes[ NUM_NOPCODE_TYPES ] =
{
	 "-n/a-"
	,"byte1"
	,"byte2"
	,"byte4"
	,"byte8"
	,"word1"
	,"word2"
	,"word4"
	,"addr "
	,"hex  "
	,"char "
	,"ascii"
	,"apple"
	,"mixed"
	,"FAC  "
	,"bmp  "
};

// Command: B
// no args
// List the data blocks
//===========================================================================
Update_t CmdDisasmDataList (int nArgs)
{

	// Need to iterate through all blocks
	DisasmData_t* pData = NULL;

	while ( (pData = Disassembly_Enumerate( pData )) )
	{
		if (pData->iDirective != _NOP_REMOVED)
		{
			bool hasSymbol = pData->sSymbol[0] != '\0';

			// <symbol> <type> <start>:<end>
			// `TEST `300`:`320
			ConsolePrintFormat( CHC_CATEGORY "%s %s%*s " CHC_ADDRESS "%04X" CHC_ARG_SEP ":" CHC_ADDRESS "%04X"
				, g_aNopcodeTypes[ pData->eElementType ] 
				, hasSymbol ? CHC_SYMBOL     : CHC_DEFAULT
				, MAX_SYMBOLS_LEN
				, hasSymbol ? pData->sSymbol : "???"
				, pData->nStartAddress
				, pData->nEndAddress // Disassembly_IsDataAddress() is *inclusive* // KEEP IN SYNC:  _CmdDefineByteRange() CmdDisasmDataList() _6502_GetOpmodeOpbyte() FormatNopcodeBytes()
			);
		}
	}

	return UPDATE_DISASM | ConsoleUpdate();
}

// Common code


// TODO: merge _CmdDisasmDataDefByteX() and _CmdDisasmDataDefWordX
//       add params( iDirective, iOpcode ) to allow ASM_DEFINE_FLOAT, NOP_FAC

//===========================================================================
Update_t _CmdDisasmDataDefByteX (int nArgs)
{
	// DB
	// DB symbol // use current instruction pointer
	// DB symbol address
	// DB symbol range:range
	// DB address
	// To "return to code" use ."X" 
	int iCmd = g_aArgs[0].nValue - NOP_BYTE_1;

	if (nArgs > 4) // 2.7.0.31 Bug fix: DB range, i.e. DB 174E:174F
	{
		return Help_Arg_1( CMD_DEFINE_DATA_BYTE1 + iCmd );
	}

	DisasmData_t tData;
	int iArg = 2;

	if (nArgs == 3 ) // 2.7.0.31 Bug fix: DB range, i.e. DB 174E:175F
	{
		if ( g_aArgs[ 2 ].eToken == TOKEN_COLON )
			iArg = 1;
	}

	WORD nAddress = _CmdDefineByteRange( nArgs, iArg, tData );

	// TODO: Allow user to select which assembler to use for displaying directives!
//	tData.iDirective = FIRST_M_DIRECTIVE + ASM_M_DEFINE_BYTE;
	tData.iDirective = g_aAssemblerFirstDirective[ g_iAssemblerSyntax ] + ASM_DEFINE_BYTE;

	tData.eElementType = (Nopcode_e)( NOP_BYTE_1 + iCmd );
	tData.bSymbolLookup = false;
	tData.nTargetAddress = 0;

	// Already exists, so update
	DisasmData_t *pData = Disassembly_IsDataAddress( nAddress );
	if ( pData )
	{
		*pData = tData;
	}
	else
		Disassembly_AddData( tData );

	return UPDATE_DISASM | ConsoleUpdate();
}

/*
	Usage:
		DW
		DW symbol
		DW symbol address
		DW symbol range:range
		DW address  Auto-define W_#### where # is the address
		DW range    Auto-define W_#### where # is the address
	Examples:
		DW 3F2:3F3
*/
//===========================================================================
Update_t _CmdDisasmDataDefWordX (int nArgs)
{
	int iCmd = g_aArgs[0].nValue - NOP_WORD_1;

	if (nArgs > 4) // 2.7.0.31 Bug fix: DB range, i.e. DB 174E:174F
	{
		return Help_Arg_1( CMD_DEFINE_DATA_WORD1 + iCmd );
	}

	DisasmData_t tData;
	int iArg = 2;

	if (nArgs == 3 ) // 2.7.0.33 Bug fix: DW range, i.e. DW 3F2:3F3
	{
		if ( g_aArgs[ 2 ].eToken == TOKEN_COLON )
			iArg = 1;
	}

	WORD nAddress = _CmdDefineByteRange( nArgs, iArg, tData );

//	tData.iDirective = FIRST_M_DIRECTIVE + ASM_M_DEFINE_WORD;
	tData.iDirective = g_aAssemblerFirstDirective[ g_iAssemblerSyntax ] + ASM_DEFINE_WORD;

	tData.eElementType = (Nopcode_e)( NOP_WORD_1 + iCmd );
	tData.bSymbolLookup = false;
	tData.nTargetAddress = 0;

	// Already exists, so update
	DisasmData_t *pData = Disassembly_IsDataAddress( nAddress );
	if ( pData )
	{
		*pData = tData;
	}
	else
		Disassembly_AddData( tData );

	return UPDATE_DISASM | ConsoleUpdate();
}

//===========================================================================
Update_t CmdDisasmDataDefAddress8H (int nArgs)
{
	return UPDATE_DISASM;
}

//===========================================================================
Update_t CmdDisasmDataDefAddress8L (int nArgs)
{
	return UPDATE_DISASM;
}

// Command: DA
// Description: Markup data as an address (i.e. table of function pointers)
// Usage:
//     DA <addr>
// Example:
//     DA D000
//     DA D000:D0B1
//===========================================================================
Update_t CmdDisasmDataDefAddress16 (int nArgs)
{
	int iCmd = NOP_WORD_1 - g_aArgs[0].nValue;

	if (! ((nArgs <= 2) || (nArgs == 4)))
	{
		return Help_Arg_1( CMD_DEFINE_DATA_WORD1 + iCmd );
	}

	DisasmData_t tData;
	int iArg = 2;
	WORD nAddress = _CmdDefineByteRange( nArgs, iArg, tData );

//	tData.iDirective = FIRST_M_DIRECTIVE + ASM_M_DEFINE_WORD;
	tData.iDirective = g_aAssemblerFirstDirective[ g_iAssemblerSyntax ] + ASM_DEFINE_ADDRESS_16;

	tData.eElementType = NOP_ADDRESS;
	tData.bSymbolLookup = true;
	tData.nTargetAddress = 0; // dynamic -- will be filled in ...

	// Already exists, so update
	DisasmData_t *pData = Disassembly_IsDataAddress( nAddress );
	if ( pData )
	{
		*pData = tData;
	}
	else
		Disassembly_AddData( tData );

	return UPDATE_DISASM | ConsoleUpdate();
}

// Command: DB
// Usage:
//     DB <addr>
Update_t CmdDisasmDataDefByte1 ( int nArgs )
{
	g_aArgs[0].nValue = NOP_BYTE_1;
	return _CmdDisasmDataDefByteX( nArgs );	
}

// Command: DB2
// Usage:
//     DB2 <addr>
Update_t CmdDisasmDataDefByte2 ( int nArgs )
{
	g_aArgs[0].nValue = NOP_BYTE_2;
	return _CmdDisasmDataDefByteX( nArgs );	
}

// Command: DB4
// Usage:
//     DB4 <addr>
Update_t CmdDisasmDataDefByte4 ( int nArgs )
{
	g_aArgs[0].nValue = NOP_BYTE_4;
	return _CmdDisasmDataDefByteX( nArgs );	
}

// Command DB8
// Usage:
//     DB8 <addr>
Update_t CmdDisasmDataDefByte8 ( int nArgs )
{
	g_aArgs[0].nValue = NOP_BYTE_8;
	return _CmdDisasmDataDefByteX( nArgs );	
}

// Command: DF
// Usage:
//     DF <addr>
Update_t CmdDisasmDataDefFloat(int nArgs)
{
	g_aArgs[0].nValue = NOP_FAC;
	return _CmdDisasmDataDefByteX( nArgs );
}

// Command: DW
// Usage:
//     DW  <addr>
Update_t CmdDisasmDataDefWord1 ( int nArgs )
{
	g_aArgs[0].nValue = NOP_WORD_1;
	return _CmdDisasmDataDefWordX( nArgs );	
}

// Command: DW2
// Usage:
//     DW2  <addr>
Update_t CmdDisasmDataDefWord2 ( int nArgs )
{
	g_aArgs[0].nValue = NOP_WORD_2;
	return _CmdDisasmDataDefWordX( nArgs );	
}

// Command: DW4
// Usage:
//     DW4  <addr>
Update_t CmdDisasmDataDefWord4 ( int nArgs )
{
	g_aArgs[0].nValue = NOP_WORD_4;
	return _CmdDisasmDataDefWordX( nArgs );	
}

// Command: DS
// Usage:
//     DS <addr>
//		ASC range    Auto-define T_#### where # is the address
Update_t CmdDisasmDataDefString ( int nArgs )
{
	int iCmd = 0; // Define Ascii, AppleText, MixedText (DOS3.3)

	if (nArgs > 4) // 2.7.0.31 Bug fix: DB range, i.e. DB 174E:174F
	{
		return Help_Arg_1( CMD_DEFINE_DATA_STR + iCmd );
	}

	DisasmData_t tData;
	int iArg = 2;

	if (nArgs == 3 ) // 2.7.0.32 Bug fix: ASC range, i.e. ASC 174E:175F
	{
		if ( g_aArgs[ 2 ].eToken == TOKEN_COLON )
			iArg = 1;
	}

	WORD nAddress = _CmdDefineByteRange( nArgs, iArg, tData );

//	tData.iDirective = g_aAssemblerFirstDirective[ g_iAssemblerSyntax ] + ASM_DEFINE_APPLE_TEXT;
	tData.iDirective = FIRST_M_DIRECTIVE + ASM_M_ASCII; // ASM_MERLIN

	tData.eElementType = (Nopcode_e)( NOP_STRING_APPLE + iCmd );
	tData.bSymbolLookup = false;
	tData.nTargetAddress = 0;

	// Already exists, so update
	DisasmData_t *pData = Disassembly_IsDataAddress( nAddress );
	if ( pData )
	{
		*pData = tData;
	}
	else
		Disassembly_AddData( tData );

	return UPDATE_DISASM | ConsoleUpdate();
}

// __ Disassembler View Interface ____________________________________________________________________

/// @param pCurrent NULL start a new serch, or continue enumerating
//===========================================================================
DisasmData_t* Disassembly_Enumerate( DisasmData_t *pCurrent )
{
	DisasmData_t *pData = NULL; // bIsNopcode = false
	size_t nDataTargets = g_aDisassemblerData.size();

	if ( nDataTargets > 0 )
	{
		DisasmData_t *pBegin = & g_aDisassemblerData[ 0 ];
		DisasmData_t *pEnd   = & g_aDisassemblerData[ nDataTargets - 1 ];

		if ( pCurrent )
		{
			pCurrent++;
			if (pCurrent <= pEnd)
				pData = pCurrent;
		} else {
			pData = pBegin;
		}
	}
	return pData;
}

// returns NULL if address has no data associated with it
//===========================================================================
DisasmData_t* Disassembly_IsDataAddress ( WORD nAddress )
{
	DisasmData_t *pData = NULL; // bIsNopcode = false
	size_t nDataTargets = g_aDisassemblerData.size();

	if ( nDataTargets > 0 )
	{
		// TODO: Replace with binary search -- should store data in sorted order, via start address
		pData = & g_aDisassemblerData[ 0 ];
		for ( size_t iTarget = 0; iTarget < nDataTargets; iTarget++ )
		{
			if ( pData->iDirective != _NOP_REMOVED )
			{
				if ( (nAddress >= pData->nStartAddress) && (nAddress <= pData->nEndAddress) )
				{
					return pData;
				}
			}
			pData++;
		}
		pData = NULL; // bIsNopCode = false
	}
	return pData;
}

// Notes: tData.iDirective should not be _NOP_REMOVED !
//===========================================================================
void Disassembly_AddData( DisasmData_t tData)
{
	g_aDisassemblerData.push_back( tData );
}

// DEPRECATED ! Inlined in _6502_GetOpmodeOpbyte() !
//===========================================================================
void Disassembly_GetData ( WORD nBaseAddress, const DisasmData_t *pData, DisasmLine_t & line_ )
{
	if ( !pData )
	{
#if _DEBUG
		ConsoleDisplayError( "Disassembly_GetData() but we don't have a valid DisasmData_t *" );
#endif
		return;
	}
}

//===========================================================================
void Disassembly_DelData( DisasmData_t tData)
{
	// g_aDisassemblerData.erase( );
	WORD nAddress = tData.nStartAddress;

	DisasmData_t *pData = NULL; // bIsNopcode = false
	size_t nDataTargets = g_aDisassemblerData.size();

	if ( nDataTargets > 0 )
	{
		// TODO: Replace with binary search -- should store data in sorted order, via start address
		pData = & g_aDisassemblerData[ 0 ];
		for ( size_t iTarget = 0; iTarget < nDataTargets; iTarget++ )
		{
			if (pData->iDirective != _NOP_REMOVED)
			{
				if ((nAddress >= pData->nStartAddress) && (nAddress <= pData->nEndAddress))
				{
					pData->iDirective = _NOP_REMOVED;

					// TODO: delete from vector?
				}
			}
			pData++;
		}
		pData = NULL; // bIsNopCode = false
	}
}

