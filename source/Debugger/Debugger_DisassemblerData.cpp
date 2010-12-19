/*
AppleWin : An Apple //e emulator for Windows

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

/* Description: Data Blocks shown in Disassembler
 *
 * Author: Copyright (C) 2009 - 2010 Michael Pohoreski
 */

#include "StdAfx.h"


// Disassembler Data ______________________________________________________________________________

// __ Debugger Interaface ____________________________________________________________________________

//===========================================================================
WORD _CmdDefineByteRange(int nArgs,int iArg,DisasmData_t & tData_)
{
	WORD nAddress  = 0;
	WORD nAddress2 = 0;
	WORD nEnd      = 0;
	int  nLen      = 0;

	memset( (void*) &tData_, 0, sizeof(tData_) );

	if( nArgs < 2 )
	{
		nAddress = g_nDisasmCurAddress;
	}
	else
	{
		RangeType_t eRange = Range_Get( nAddress, nAddress2, iArg);
		if ((eRange == RANGE_HAS_END) ||
			(eRange == RANGE_HAS_LEN))
		{
			Range_CalcEndLen( eRange, nAddress, nAddress2, nEnd, nLen );
			//dArg = 2;
		}
		else
		{
				nAddress = g_aArgs[ 2 ].nValue;
		}
	}

	if (!nLen)
	{
		nLen = 1;
	}

	tData_.nStartAddress = nAddress;
	tData_.nEndAddress = nAddress + nLen;
//	tData_.nArraySize = 0;
	char *pSymbolName = "";
	if( nArgs )
	{
		pSymbolName = g_aArgs[ 1 ].sArg;
		SymbolTable_Index_e eSymbolTable = SYMBOLS_ASSEMBLY;
		// bRemoveSymbol = false // use arg[2]
		// bUpdateSymbol = true // add the symbol to the table
		SymbolUpdate( eSymbolTable, pSymbolName, nAddress, false, true ); 
		// Note: need to call ConsoleUpdate(), as may print symbol has been updated
	}
	else
	{
		// TODO: 'DB' with no args, should define D_# DB $XX
	}

	strcpy( tData_.sSymbol, pSymbolName );

	return nAddress;
}

// Undefine Data
//===========================================================================
Update_t CmdDisasmDataDefCode (int nArgs)
{
	// treat memory (bytes) as code
	if (! ((nArgs <= 2) || (nArgs == 4)))
	{
		return Help_Arg_1( CMD_DISASM_CODE );
	}

	DisasmData_t tData;
	int iArg = 2;
	WORD nAddress = _CmdDefineByteRange( nArgs, iArg, tData );

	// Need to iterate through all blocks
	// DB TEST1 300:320
	// DB TEST2 310:330
	// DB TEST3 320:340
	// !DB 300

	DisasmData_t *pData = Disassembly_IsDataAddress( nAddress );
	if( pData )
	{
		// TODO: Do we need to split the data !?
		//Disassembly_DelData( tData );
		pData->iDirective = _NOP_REMOVED;
	}
	else
	{
		Disassembly_DelData( tData );
	}

	return UPDATE_DISASM | ConsoleUpdate();
}

char* g_aNopcodeTypes[ NUM_NOPCODE_TYPES ] =
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

// List the data blocks
//===========================================================================
Update_t CmdDisasmDataList (int nArgs)
{
	// Need to iterate through all blocks
	DisasmData_t* pData = NULL;
	char sText[ CONSOLE_WIDTH * 2 ];

	while( pData = Disassembly_Enumerate( pData ) )
	{
		if (pData->iDirective != _NOP_REMOVED)
		{
			int nLen = strlen( pData->sSymbol );

			// <smbol> <type> <start>:<end>
			// `TEST `300`:`320
			sprintf( sText, "%s%s %s%*s %s%04X%s:%s%04X"
				, CHC_CATEGORY
				, g_aNopcodeTypes[ pData->eElementType ] 
				, (nLen > 0) ? CHC_SYMBOL     : CHC_DEFAULT
				, MAX_SYMBOLS_LEN
				, (nLen > 0) ? pData->sSymbol : "???"
				, CHC_ADDRESS
				, pData->nStartAddress
				, CHC_ARG_SEP
				, CHC_ADDRESS
				, pData->nEndAddress - 1
			);
			ConsolePrint( sText );
		}
	}

	return UPDATE_DISASM | ConsoleUpdate();
}

// Common code
//===========================================================================
Update_t _CmdDisasmDataDefByteX (int nArgs)
{
	// DB
	// DB symbol
	// DB symbol address
	// DB symbol range:range
	int iCmd = g_aArgs[0].nValue - NOP_BYTE_1;

//	if ((!nArgs) || (nArgs > 2))
	if (! ((nArgs <= 2) || (nArgs == 4)))
	{
		return Help_Arg_1( CMD_DEFINE_DATA_BYTE1 + iCmd );
	}

	DisasmData_t tData;
	int iArg = 2;
	WORD nAddress = _CmdDefineByteRange( nArgs, iArg, tData );

	// TODO: Allow user to select which assembler to use for displaying directives!
//	tData.iDirective = FIRST_M_DIRECTIVE + ASM_M_DEFINE_BYTE;
	tData.iDirective = g_aAssemblerFirstDirective[ g_iAssemblerSyntax ] + ASM_DEFINE_BYTE;

	tData.eElementType = (Nopcode_e)( NOP_BYTE_1 + iCmd );
	tData.bSymbolLookup = false;
	tData.nTargetAddress = 0;

	// Already exists, so update
	DisasmData_t *pData = Disassembly_IsDataAddress( nAddress );
	if( pData )
	{
		*pData = tData;
	}
	else
		Disassembly_AddData( tData );

	return UPDATE_DISASM | ConsoleUpdate();
}

//===========================================================================
Update_t _CmdDisasmDataDefWordX (int nArgs)
{
	// DW
	// DW symbol
	// DW symbol address
	// DW symbol range:range
	int iCmd = g_aArgs[0].nValue - NOP_WORD_1;

	if (! ((nArgs <= 2) || (nArgs == 4)))
	{
		return Help_Arg_1( CMD_DEFINE_DATA_WORD1 + iCmd );
	}

	DisasmData_t tData;
	int iArg = 2;
	WORD nAddress = _CmdDefineByteRange( nArgs, iArg, tData );

//	tData.iDirective = FIRST_M_DIRECTIVE + ASM_M_DEFINE_WORD;
	tData.iDirective = g_aAssemblerFirstDirective[ g_iAssemblerSyntax ] + ASM_DEFINE_WORD;

	tData.eElementType = (Nopcode_e)( NOP_WORD_1 + iCmd );
	tData.bSymbolLookup = false;
	tData.nTargetAddress = 0;

	// Already exists, so update
	DisasmData_t *pData = Disassembly_IsDataAddress( nAddress );
	if( pData )
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
	if( pData )
	{
		*pData = tData;
	}
	else
		Disassembly_AddData( tData );

	return UPDATE_DISASM | ConsoleUpdate();
}

Update_t CmdDisasmDataDefByte1 ( int nArgs )
{
	g_aArgs[0].nValue = NOP_BYTE_1;
	return _CmdDisasmDataDefByteX( nArgs );	
}

Update_t CmdDisasmDataDefByte2 ( int nArgs )
{
	g_aArgs[0].nValue = NOP_BYTE_2;
	return _CmdDisasmDataDefByteX( nArgs );	
}

Update_t CmdDisasmDataDefByte4 ( int nArgs )
{
	g_aArgs[0].nValue = NOP_BYTE_4;
	return _CmdDisasmDataDefByteX( nArgs );	
}

Update_t CmdDisasmDataDefByte8 ( int nArgs )
{
	g_aArgs[0].nValue = NOP_BYTE_8;
	return _CmdDisasmDataDefByteX( nArgs );	
}

Update_t CmdDisasmDataDefWord1 ( int nArgs )
{
	g_aArgs[0].nValue = NOP_WORD_1;
	return _CmdDisasmDataDefWordX( nArgs );	
}

Update_t CmdDisasmDataDefWord2 ( int nArgs )
{
	g_aArgs[0].nValue = NOP_WORD_2;
	return _CmdDisasmDataDefWordX( nArgs );	
}

Update_t CmdDisasmDataDefWord4 ( int nArgs )
{
	g_aArgs[0].nValue = NOP_WORD_4;
	return _CmdDisasmDataDefWordX( nArgs );	
}

Update_t CmdDisasmDataDefString ( int nArgs )
{
	return UPDATE_DISASM;
}

// __ Disassembler View Interface ____________________________________________________________________

/// @param pCurrent NULL start a new serch, or continue enumerating
//===========================================================================
DisasmData_t* Disassembly_Enumerate( DisasmData_t *pCurrent )
{
	DisasmData_t *pData = NULL; // bIsNopcode = false
	int nDataTargets = g_aDisassemblerData.size();

	if( nDataTargets )
	{
		DisasmData_t *pBegin = & g_aDisassemblerData[ 0 ];
		DisasmData_t *pEnd   = & g_aDisassemblerData[ nDataTargets - 1 ];

		if( pCurrent )
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
	int nDataTargets = g_aDisassemblerData.size();

	if( nDataTargets )
	{
		// TODO: Replace with binary search -- should store data in sorted order, via start address
		pData = & g_aDisassemblerData[ 0 ];
		for( int iTarget = 0; iTarget < nDataTargets; iTarget++ )
		{
			if( pData->iDirective != _NOP_REMOVED )
			{
				if( (nAddress >= pData->nStartAddress) && (nAddress < pData->nEndAddress) )
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
	if( !pData )
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
	int nDataTargets = g_aDisassemblerData.size();

	if( nDataTargets )
	{
		// TODO: Replace with binary search -- should store data in sorted order, via start address
		pData = & g_aDisassemblerData[ 0 ];
		for( int iTarget = 0; iTarget < nDataTargets; iTarget++ )
		{
			if (pData->iDirective != _NOP_REMOVED)
			{
				if ((nAddress >= pData->nStartAddress) && (nAddress < pData->nEndAddress))
				{
					pData->iDirective = _NOP_REMOVED;
				}
			}
			pData++;
		}
		pData = NULL; // bIsNopCode = false
	}
}

