/*
AppleWin : An Apple //e emulator for Windows

/* Description: Data Blocks shown in Disassembler
 *
 * Author: Copyright (C) 2009 Michael Pohoreski
 */

#include "StdAfx.h"
#pragma  hdrstop

// Disassembler Data ______________________________________________________________________________

// __ Debugger Interaface ____________________________________________________________________________

//===========================================================================
WORD _CmdDefineByteRange(int nArgs,int iArg,DisasmData_t & tData_)
{
	WORD nAddress  = 0;
	WORD nAddress2 = 0;
	WORD nEnd      = 0;
	int  nLen      = 0;

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
		// Need to split the data
//		*pData = tData;
		Disassembly_AddData( tData );
	}
	else
	{
		Disassembly_DelData( tData );
	}

	return UPDATE_DISASM | ConsoleUpdate();
}

// List the data blocks
//===========================================================================
Update_t CmdDisasmDataList (int nArgs)
{
	// Need to iterate through all blocks
	DisasmData_t* pData = NULL;
	char sText[ CONSOLE_WIDTH ];
	while( pData = Disassembly_Enumerate( pData ) )
	{
		// `TEST `300`:`320
		sprintf( sText, "%s%s %s%04X%s:%s%04X\n"
			, CHC_SYMBOL
			, pData->sSymbol
			, CHC_ADDRESS
			, pData->nStartAddress
			, CHC_ARG_SEP
			, pData->nEndAddress
		);
		ConsolePrint( sText );
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
	// symbol range:range
	int iCmd = NOP_BYTE_1 - g_aArgs[0].nValue;

	if (! ((nArgs <= 2) || (nArgs == 4)))
	{
		return Help_Arg_1( CMD_DEFINE_DATA_BYTE1 + iCmd );
	}

	DisasmData_t tData;
	int iArg = 2;
	WORD nAddress = _CmdDefineByteRange( nArgs, iArg, tData );

	tData.iDirective = FIRST_M_DIRECTIVE + ASM_M_DEFINE_BYTE;
	tData.eElementType = NOP_BYTE_1 + iCmd;
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
	// symbol range:range
	int iCmd = NOP_WORD_1 - g_aArgs[0].nValue;

	if (! ((nArgs <= 2) || (nArgs == 4)))
	{
		return Help_Arg_1( CMD_DEFINE_DATA_WORD1 + iCmd );
	}

	DisasmData_t tData;
	int iArg = 2;
	WORD nAddress = _CmdDefineByteRange( nArgs, iArg, tData );

	tData.iDirective = FIRST_M_DIRECTIVE + ASM_M_DEFINE_WORD;
	tData.eElementType = NOP_WORD_1 + iCmd;
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
	return UPDATE_DISASM;
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

	if( pCurrent )
	{
		pCurrent++;
		pData = & g_aDisassemblerData[ nDataTargets ];
		if( pCurrent < pData )
			return pCurrent;
		else
			return NULL;
	}
	else
	{
		pData = & g_aDisassemblerData[ 0 ];
		if( nDataTargets )
			return pData;
		else
			return NULL;
	}
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
			if( (nAddress >= pData->nStartAddress) && (nAddress < pData->nEndAddress) )
			{
				return pData;
			}
			pData++;
		}
		pData = NULL; // bIsNopCode = false
	}
	return pData;
}

//===========================================================================
void Disassembly_AddData( DisasmData_t tData)
{
	g_aDisassemblerData.push_back( tData );
}

//===========================================================================
void Disassembly_GetData ( WORD nBaseAddress, const DisasmData_t *pData, DisasmLine_t & line_ )
{
	line_.ClearFlags();

	line_.iNoptype = pData->eElementType;
	switch( pData->eElementType )
	{
		case NOP_BYTE_1:
			line_.nOpbyte = 1;
			break;
		case NOP_BYTE_2:
			line_.nOpbyte = 2;
			break;
		case NOP_WORD_1:
			line_.nOpbyte= 2;
			break;
		case NOP_WORD_2:
			line_.nOpbyte= 4;
			break;
		case NOP_STRING_APPLESOFT:
			// scan memory for high byte
			line_.nOpbyte = 8;
			break;
		default:
			line_.nOpbyte = 1;
			break;
	}

	FormatOpcodeBytes( nBaseAddress, line_ );

	//pMnemonic = g_aOpcodes[ iOpcode ].sMnemonic;
	line_.iNopcode = pData->iDirective;
	strcpy( line_.sMnemonic, g_aAssemblerDirectives[ line_.iNopcode ].m_pMnemonic );

	FormatNopcodeBytes( nBaseAddress, line_ );
}

//===========================================================================
void Disassembly_DelData( DisasmData_t tData)
{
	// g_aDisassemblerData.erase( );
}

