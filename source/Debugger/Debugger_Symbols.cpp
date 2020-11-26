/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 1994-1996, Michael O'Brien
Copyright (C) 1999-2001, Oliver Schmidt
Copyright (C) 2002-2005, Tom Charlesworth
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

/* Description: Debugger Symbol Tables
 *
 * Author: Copyright (C) 2006-2010 Michael Pohoreski
 */

#include "StdAfx.h"

#include "Debug.h"

#include "../Windows/AppleWin.h"
#include "../Core.h"

	// 2.6.2.13 Added: Can now enable/disable selected symbol table(s) !
	// Allow the user to disable/enable symbol tables
	// xxx1xxx symbol table is active (are displayed in disassembly window, etc.)
	// xxx1xxx symbol table is disabled (not displayed in disassembly window, etc.)
	// See: CmdSymbolsListTable(), g_bDisplaySymbolTables
	int g_bDisplaySymbolTables = ((1 << NUM_SYMBOL_TABLES) - 1) & (~(int)SYMBOL_TABLE_PRODOS);// default to all symbol tables displayed/active

// Symbols ________________________________________________________________________________________

	char*     g_sFileNameSymbols[ NUM_SYMBOL_TABLES ] = {
		 "APPLE2E.SYM"
		,"A2_BASIC.SYM"
		,"A2_ASM.SYM"
		,"A2_USER1.SYM" // "A2_USER.SYM",
		,"A2_USER2.SYM"
		,"A2_SRC1.SYM" // "A2_SRC.SYM",
		,"A2_SRC2.SYM"
		,"A2_DOS33.SYM"
		,"A2_PRODOS.SYM"
	};
	std::string  g_sFileNameSymbolsUser;

	char * g_aSymbolTableNames[ NUM_SYMBOL_TABLES ] =
	{
		 "Main"
		,"Basic"
		,"Asm" // "Assembly",
		,"User1" // User
		,"User2"
		,"Src1"
		,"Src2"
		,"DOS33"
		,"ProDOS"
	};

	bool g_bSymbolsDisplayMissingFile = true;

	SymbolTable_t g_aSymbols[ NUM_SYMBOL_TABLES ];
	int           g_nSymbolsLoaded = 0;  // on Last Load

// Utils _ ________________________________________________________________________________________

	void      _CmdSymbolsInfoHeader( int iTable, char * pText, int nDisplaySize = 0 );
	void      _PrintCurrentPath();
	Update_t  _PrintSymbolInvalidTable();


// Private ________________________________________________________________________________________

//===========================================================================
void _PrintCurrentPath()
{
	ConsoleDisplayError( g_sProgramDir.c_str() );
}

Update_t _PrintSymbolInvalidTable()
{
	char sText[ CONSOLE_WIDTH * 2 ];
	char sTemp[ CONSOLE_WIDTH * 2 ];

	// TODO: display the user specified file name
	ConsoleBufferPush( "Invalid symbol table." );

	ConsolePrintFormat( sText, "Only %s%d%s symbol tables are supported:"
		, CHC_NUM_DEC, NUM_SYMBOL_TABLES
		, CHC_DEFAULT
	);

	// Similar to _CmdSymbolsInfoHeader()
	sText[0] = 0;
	for( int iTable = 0; iTable < NUM_SYMBOL_TABLES; iTable++ )
	{
		sprintf( sTemp, "%s%s%s%c " // %s"
			, CHC_USAGE, g_aSymbolTableNames[ iTable ]
			, CHC_ARG_SEP
			, (iTable != (NUM_SYMBOL_TABLES-1))
				? ','
				: '.'
		);
		strcat( sText, sTemp );
	}

//	return ConsoleDisplayError( sText );
	ConsolePrint( sText );
	return ConsoleUpdate();
}


// Public _________________________________________________________________________________________


//===========================================================================
const char* GetSymbol (WORD nAddress, int nBytes)
{
	const char* pSymbol = FindSymbolFromAddress( nAddress );
	if (pSymbol)
		return pSymbol;

	return FormatAddress( nAddress, nBytes );
}

//===========================================================================
int GetSymbolTableFromCommand()
{
	return (g_iCommand - CMD_SYMBOLS_ROM);
}

//===========================================================================
const char* FindSymbolFromAddress (WORD nAddress, int * iTable_ )
{
	// Bugfix/User feature: User symbols should be searched first
	int iTable = NUM_SYMBOL_TABLES;
	while (iTable-- > 0)
	{
		if (! g_aSymbols[iTable].size())
			continue;

		if (! (g_bDisplaySymbolTables & (1 << iTable)))
			continue;

		std::map<WORD, std::string>::iterator iSymbols = g_aSymbols[iTable].find(nAddress);
		if(g_aSymbols[iTable].find(nAddress) != g_aSymbols[iTable].end())
		{
			if (iTable_)
			{
				*iTable_ = iTable;
			}
			return iSymbols->second.c_str();
		}
	}	
	return NULL;	
}

//===========================================================================
bool FindAddressFromSymbol ( const char* pSymbol, WORD * pAddress_, int * iTable_ )
{
	// Bugfix/User feature: User symbols should be searched first
	for (int iTable = NUM_SYMBOL_TABLES; iTable-- > 0; )
	{
		if (! g_aSymbols[iTable].size())
			continue;

		if (! (g_bDisplaySymbolTables & (1 << iTable)))
			continue;

		SymbolTable_t :: iterator  iSymbol = g_aSymbols[iTable].begin();
		while (iSymbol != g_aSymbols[iTable].end())
		{
			if (!_tcsicmp( iSymbol->second.c_str(), pSymbol))
			{
				if (pAddress_)
				{
					*pAddress_ = iSymbol->first;
				}
				if (iTable_)
				{
					*iTable_ = iTable;
				}
				return true;
			}
			iSymbol++;
		}
	}
	return false;
}



// Symbols ________________________________________________________________________________________

//===========================================================================
WORD GetAddressFromSymbol (const char* pSymbol)
{
	WORD nAddress;
	bool bFoundSymbol = FindAddressFromSymbol( pSymbol, & nAddress );
	if (! bFoundSymbol)
	{
		nAddress = 0;
	}
	return nAddress;
}



//===========================================================================
bool String2Address( LPCTSTR pText, WORD & nAddress_ )
{
	TCHAR sHexApple[ CONSOLE_WIDTH ];

	if (pText[0] == '$')
	{
		if (!TextIsHexString( pText+1))
			return false;

		_tcscpy( sHexApple, "0x" );
		_tcsncpy( sHexApple+2, pText+1, MAX_SYMBOLS_LEN - 3 );
		sHexApple[2 + (MAX_SYMBOLS_LEN - 3) - 1] = 0;
		pText = sHexApple;
	}

	if (pText[0] == TEXT('0'))
	{
		if ((pText[1] == TEXT('X')) || pText[1] == TEXT('x'))
		{
			if (!TextIsHexString( pText+2))
				return false;

			TCHAR *pEnd;
			nAddress_ = (WORD) _tcstol( pText, &pEnd, 16 );
			return true;
		}
		if (TextIsHexString( pText ))
		{
			TCHAR *pEnd;
			nAddress_ = (WORD) _tcstol( pText, &pEnd, 16 );
			return true;
		}
	}

	return false;
}		


//===========================================================================
Update_t CmdSymbols (int nArgs)
{
	if (! nArgs)
		return CmdSymbolsInfo( 0 );

	Update_t iUpdate = _CmdSymbolsUpdate( nArgs, SYMBOL_TABLE_USER_1 );
	if (iUpdate != UPDATE_NOTHING)
		return iUpdate;

	int bSymbolTables = (1 << NUM_SYMBOL_TABLES) - 1;
	return _CmdSymbolsListTables( nArgs, bSymbolTables );
}

//===========================================================================
Update_t CmdSymbolsClear (int nArgs)
{
	SymbolTable_Index_e eSymbolTable = SYMBOLS_USER_1;	
	_CmdSymbolsClear( eSymbolTable );
	return (UPDATE_DISASM | UPDATE_SYMBOLS);
}

// Format the summary of the specified symbol table
//===========================================================================
void _CmdSymbolsInfoHeader( int iTable, char * pText, int nDisplaySize /* = 0 */ )
{
	// Common case is to use/calc the table size
	bool bActive = (g_bDisplaySymbolTables & (1 << iTable)) ? true : false;
	int nSymbols  = nDisplaySize ? nDisplaySize : g_aSymbols[ iTable ].size();

	// Short Desc: `MAIN`: `1000`
	// // 2.6.2.19 Color for name of symbol table: _CmdPrintSymbol() "SYM HOME" _CmdSymbolsInfoHeader "SYM"
	// CHC_STRING and CHC_NUM_DEC are both cyan, using CHC_USAGE instead of CHC_STRING
	sprintf( pText, "%s%s%s:%s%d " // %s"
		, CHC_USAGE, g_aSymbolTableNames[ iTable ]
		, CHC_ARG_SEP
		, bActive ? CHC_NUM_DEC : CHC_WARNING, nSymbols
//		, CHC_DEFAULT
	);
}

//===========================================================================
Update_t CmdSymbolsInfo (int nArgs)
{
	const char sIndent[] = "  ";
	char sText[ CONSOLE_WIDTH * 4 ] = "";
	char sTemp[ CONSOLE_WIDTH * 2 ] = "";

	int bDisplaySymbolTables = 0;

	strcpy( sText, sIndent ); // Indent new line

	if (! nArgs)
	{
		// default to all tables
		bDisplaySymbolTables = (1 << NUM_SYMBOL_TABLES) - 1;
	}
	else
	{	// Convert Command Index to parameter
		int iWhichTable = GetSymbolTableFromCommand();
		if ((iWhichTable < 0) || (iWhichTable >= NUM_SYMBOL_TABLES))
		{
			return _PrintSymbolInvalidTable();
		}

		bDisplaySymbolTables = (1 << iWhichTable);
	}

	//sprintf( sText, "  Symbols  Main: %s%d%s  User: %s%d%s   Source: %s%d%s"
	// "Main:# Basic:# Asm:# User1:# User2:# Src1:# Src2:# Dos:# Prodos:#

	int bTable = 1;
	int iTable = 0;
	for( ; bTable <= bDisplaySymbolTables; iTable++, bTable <<= 1 )
	{
		if( bDisplaySymbolTables & bTable )
		{
			_CmdSymbolsInfoHeader( iTable, sTemp ); // 15 chars per table

			// 2.8.0.4 BUGFIX: Check for buffer overflow and wrap text
			int nLen = ConsoleColor_StringLength( sTemp );
			int nDst = ConsoleColor_StringLength( sText );
			if((nDst + nLen) > CONSOLE_WIDTH )
			{
				ConsolePrint( sText );
				strcpy( sText, sIndent ); // Indent new line
			}
			strcat( sText, sTemp );
		}
	}
	ConsolePrint( sText );

	return ConsoleUpdate();
}

//===========================================================================
void _CmdPrintSymbol( LPCTSTR pSymbol, WORD nAddress, int iTable )
{
	char   sText[ CONSOLE_WIDTH * 2 ];

	// 2.6.2.19 Color for name of symbol table: _CmdPrintSymbol() "SYM HOME" _CmdSymbolsInfoHeader "SYM"
	// CHC_STRING and CHC_NUM_DEC are both cyan, using CHC_USAGE instead of CHC_STRING

	// 2.6.2.20 Changed: Output of found symbol more table friendly.  Symbol table name displayed first.
	ConsolePrintFormat( sText, "  %s%s%s: $%s%04X %s%s"
		, CHC_USAGE, g_aSymbolTableNames[ iTable ]
		, CHC_ARG_SEP
		, CHC_ADDRESS, nAddress
		, CHC_SYMBOL, pSymbol );

	// ConsoleBufferPush( sText );
}


// Test if bit-mask to index (equal to number of bit-shifs required to reach table)
//=========================================================================== */
bool _FindSymbolTable( int bSymbolTables, int iTable )
{
	// iTable is enumeration
	// bSymbolTables is bit-flags of enabled tables to search

	if( bSymbolTables & (1 << iTable) )
	{
		return true;
	}

	return false;
}

// Convert bit-mask to index
//=========================================================================== */
int _GetSymbolTableFromFlag( int bSymbolTables )
{
	int iTable = 0;
	int bTable = 1;

	for( ; bTable <= bSymbolTables; iTable++, bTable <<= 1 )
	{
		if( bTable & bSymbolTables )
			break;
	}

	return iTable;
}


/**
	@param bSymbolTables Bit Flags of which symbol tables to search
//=========================================================================== */
bool _CmdSymbolList_Address2Symbol( int nAddress, int bSymbolTables )
{
	int  iTable;
	const char* pSymbol = FindSymbolFromAddress( nAddress, &iTable );

	if (pSymbol)
	{				
		if (_FindSymbolTable( bSymbolTables, iTable ))
		{
			_CmdPrintSymbol( pSymbol, nAddress, iTable );
			return true;
		}
	}

	return false;
}

//===========================================================================
bool _CmdSymbolList_Symbol2Address( LPCTSTR pSymbol, int bSymbolTables )
{
	int  iTable;
	WORD nAddress;
	

	bool bFoundSymbol = FindAddressFromSymbol( pSymbol, &nAddress, &iTable );
	if (bFoundSymbol)
	{
		if (_FindSymbolTable( bSymbolTables, iTable ))
		{
			_CmdPrintSymbol( pSymbol, nAddress, iTable );
		}
	}
	return bFoundSymbol;
}

// LIST is normally an implicit "LIST *", but due to the numbers of symbols
// only look up symbols the user specifies
//===========================================================================
Update_t CmdSymbolsList (int nArgs )
{
	int bSymbolTables = (1 << NUM_SYMBOL_TABLES) - 1; // default to all
	return _CmdSymbolsListTables( nArgs, bSymbolTables );
}


//===========================================================================
Update_t _CmdSymbolsListTables (int nArgs, int bSymbolTables )
{
	if (! nArgs)
	{
		return Help_Arg_1( CMD_SYMBOLS_LIST );
	}

	/*
		Test Cases

		SYM 0 RESET FA6F $FA59
			$0000 LOC0
			$FA6F RESET
			$FA6F INITAN
			$FA59 OLDBRK
		SYM B
		
		SYMBOL B = $2000
		SYM B
	*/
		
	TCHAR sText[ CONSOLE_WIDTH ] = "";

	for( int iArgs = 1; iArgs <= nArgs; iArgs++ )
	{
		WORD nAddress = g_aArgs[iArgs].nValue;
		LPCTSTR pSymbol = g_aArgs[iArgs].sArg;

		// Dump all symbols for this table
		if( g_aArgRaw[iArgs].eToken == TOKEN_STAR)
		{
	//		int iWhichTable = (g_iCommand - CMD_SYMBOLS_MAIN);
	//		bDisplaySymbolTables = (1 << iWhichTable);

			int iTable = 0;
			int bTable = 1;
			for( ; bTable <= bSymbolTables; iTable++, bTable <<= 1 )
			{
				if( bTable & bSymbolTables )
				{
					int nSymbols = g_aSymbols[iTable].size();
					if (nSymbols)
					{
						SymbolTable_t :: iterator  iSymbol = g_aSymbols[iTable].begin();
						while (iSymbol != g_aSymbols[iTable].end())
						{
							const char *pSymbol = iSymbol->second.c_str();
							unsigned short nAddress = iSymbol->first;
							_CmdPrintSymbol( pSymbol, nAddress, iTable );
							++iSymbol;
						}
					}
					_CmdSymbolsInfoHeader( iTable, sText );
					ConsolePrint( sText );
				}
			}
		}
		else
		if (nAddress)
		{	// Have address, do symbol lookup first
			if (! _CmdSymbolList_Symbol2Address( pSymbol, bSymbolTables ))
			{
				// nope, ok, try as address
				if (! _CmdSymbolList_Address2Symbol( nAddress, bSymbolTables))
				{
					ConsolePrintFormat( sText
						, TEXT(" Address not found: %s$%s%04X%s" )
						, CHC_ARG_SEP
						, CHC_ADDRESS, nAddress, CHC_DEFAULT );
				}
			}
		}
		else
		{	// Have symbol, do address lookup
			if (! _CmdSymbolList_Symbol2Address( pSymbol, bSymbolTables ))
			{	// nope, ok, try as address
				if (String2Address( pSymbol, nAddress ))
				{
					if (! _CmdSymbolList_Address2Symbol( nAddress, bSymbolTables ))
					{
						ConsolePrintFormat( sText
							, TEXT(" %sSymbol not found: %s%s%s")
							, CHC_ERROR, CHC_SYMBOL, pSymbol, CHC_DEFAULT
						);
					}
				}
				else
				{
					ConsolePrintFormat( sText
						, TEXT(" %sSymbol not found: %s%s%s")
						, CHC_ERROR, CHC_SYMBOL, pSymbol, CHC_DEFAULT
					);
				}
			}
		}
	}
	return ConsoleUpdate();
}


//===========================================================================
int ParseSymbolTable(const std::string & pPathFileName, SymbolTable_Index_e eSymbolTableWrite, int nSymbolOffset )
{
	char sText[ CONSOLE_WIDTH * 3 ];
	bool bFileDisplayed = false;

	const int nMaxLen = MIN(MAX_TARGET_LEN,MAX_SYMBOLS_LEN);

	int nSymbolsLoaded = 0;

	if (pPathFileName.empty())
		return nSymbolsLoaded;

//#if _UNICODE
//	TCHAR sFormat1[ MAX_SYMBOLS_LEN ];
//	TCHAR sFormat2[ MAX_SYMBOLS_LEN ];
//	wsprintf( sFormat1, "%%x %%%ds", MAX_SYMBOLS_LEN ); // i.e. "%x %13s"
//	wsprintf( sFormat2, "%%%ds %%x", MAX_SYMBOLS_LEN ); // i.e. "%13s %x"
// ascii
	char sFormat1[ MAX_SYMBOLS_LEN ];
	char sFormat2[ MAX_SYMBOLS_LEN ];
	sprintf( sFormat1, "%%x %%%ds", MAX_SYMBOLS_LEN ); // i.e. "%x %13s"
	sprintf( sFormat2, "%%%ds %%x", MAX_SYMBOLS_LEN ); // i.e. "%13s %x"

	FILE *hFile = fopen( pPathFileName.c_str(), "rt" );

	if( !hFile && g_bSymbolsDisplayMissingFile )
	{
		// TODO: print filename! Bug #242 Help file (.chm) description for "Symbols" #242
		ConsoleDisplayError( "Symbol File not found:" );
		_PrintCurrentPath();
		nSymbolsLoaded = -1; // HACK: ERROR: FILE NOT EXIST
	}
	
	bool bDupSymbolHeader = false;
	if( hFile )
	{
		while( !feof(hFile) )
		{
			// Support 2 types of symbols files:
			// 1) AppleWin:
			//    . 0000 SYMBOL
			//    . FFFF SYMBOL
			// 2) ACME:
			//    . SYMBOL  =$0000; Comment
			//    . SYMBOL  =$FFFF; Comment
			//
			DWORD nAddress = _6502_MEM_END + 1; // default to invalid address
			char  sName[ MAX_SYMBOLS_LEN+1 ]  = "";

			const int MAX_LINE = 256;
			char  szLine[ MAX_LINE ] = "";

			if( !fgets(szLine, MAX_LINE-1, hFile) )	// Get next line
			{
				//ConsolePrint("<<EOF");
				break;
			}

			if(strstr(szLine, "$") == NULL)
			{
				sscanf(szLine, sFormat1, &nAddress, sName);
			}
			else
			{
				char* p = strstr(szLine, "=");	// Optional
				if(p) *p = ' ';
				p = strstr(szLine, "$");
				if(p) *p = ' ';
				p = strstr(szLine, ";");		// Optional
				if(p) *p = 0;
				p = strstr(szLine, " ");		// 1st space between name & value
				if (p)
				{
					int nLen = p - szLine;
					if (nLen > MAX_SYMBOLS_LEN)
					{
						memset(&szLine[MAX_SYMBOLS_LEN], ' ', nLen - MAX_SYMBOLS_LEN);	// sscanf fails for nAddress if string too long
					}
				}
				sscanf(szLine, sFormat2, sName, &nAddress);
			}

			// SymbolOffset
			nAddress += nSymbolOffset;

			if( (nAddress > _6502_MEM_END) || (sName[0] == 0) )
				continue;

			// If updating symbol, print duplicate symbols
			WORD nAddressPrev;
			int  iTable;

			// 2.9.0.11 Bug #479
			int nLen = strlen( sName );
			if (nLen > nMaxLen)
			{
				ConsolePrintFormat( sText, " %sWarn.: %s%s (%d > %d)"
					, CHC_WARNING
					, CHC_SYMBOL
					, sName
					, nLen
					, nMaxLen
				);
				ConsoleUpdate(); // Flush buffered output so we don't ask the user to pause
			}

			// 2.8.0.5 Bug #244 (Debugger) Duplicate symbols for identical memory addresses in APPLE2E.SYM
			const char *pSymbolPrev = FindSymbolFromAddress( (WORD)nAddress, &iTable ); // don't care which table it is in
			if( pSymbolPrev )
			{
				if( !bFileDisplayed )
				{
					bFileDisplayed = true;

					// TODO: Must check for buffer overflow !
					ConsolePrintFormat( sText, "%s%s"
						, CHC_PATH
						, pPathFileName.c_str()
					);
				}

				ConsolePrintFormat( sText, " %sInfo.: %s%-16s %saliases %s$%s%04X %s%-12s%s (%s%s%s)" // MAGIC NUMBER: -MAX_SYMBOLS_LEN
					, CHC_INFO // 2.9.0.10 was CHC_WARNING, see #479
					, CHC_SYMBOL
					, sName
					, CHC_INFO
					, CHC_ARG_SEP
					, CHC_ADDRESS
					, nAddress
					, CHC_SYMBOL
					, pSymbolPrev
					, CHC_DEFAULT
					, CHC_STRING
					, g_aSymbolTableNames[ iTable ]
					, CHC_DEFAULT
				);

				ConsoleUpdate(); // Flush buffered output so we don't ask the user to pause
/*
				ConsolePrintFormat( sText, " %sWarning: %sAddress already has symbol Name%s (%s%s%s): %s%s"
					, CHC_WARNING
					, CHC_INFO
					, CHC_ARG_SEP
					, CHC_STRING
					, g_aSymbolTableNames[ iTable ]
					, CHC_DEFAULT
					, CHC_SYMBOL
					, pSymbolPrev
				);

				ConsolePrintFormat( sText, "  %s$%s%04X %s%-31s%s"
					, CHC_ARG_SEP
					, CHC_ADDRESS
					, nAddress
					, CHC_SYMBOL
					, sName
					, CHC_DEFAULT
				);
*/
			}

			bool bExists  = FindAddressFromSymbol( sName, &nAddressPrev, &iTable );
			if( bExists )
			{
				if( !bDupSymbolHeader )
				{
					bDupSymbolHeader = true;
					ConsolePrintFormat( sText, " %sDup Symbol Name%s (%s%s%s) %s"
						, CHC_ERROR
						, CHC_DEFAULT
						, CHC_STRING
						, g_aSymbolTableNames[ iTable ]
						, CHC_DEFAULT
						, pPathFileName.c_str()
					);
				}

				ConsolePrintFormat( sText, "  %s$%s%04X %s%-31s%s"
					, CHC_ARG_SEP
					, CHC_ADDRESS
					, nAddress
					, CHC_SYMBOL
					, sName
					, CHC_DEFAULT
				);
			}
	
			// else // It is not a bug to have duplicate addresses by different names

			g_aSymbols[ eSymbolTableWrite ] [ (WORD) nAddress ] = sName;
			nSymbolsLoaded++; // TODO: FIXME: BUG: This is the total symbols read, not added
		}
		fclose(hFile);
	}

	return nSymbolsLoaded;
}

//===========================================================================
Update_t CmdSymbolsLoad (int nArgs)
{
	std::string sFileName = g_sProgramDir;

	int iSymbolTable = GetSymbolTableFromCommand();
	if ((iSymbolTable < 0) || (iSymbolTable >= NUM_SYMBOL_TABLES))
	{
		return _PrintSymbolInvalidTable();
	}

	int nSymbols = 0;

	// Debugger will call us with 0 args on startup as a way to pre-load symbol tables
	if (! nArgs)
	{
		sFileName += g_sFileNameSymbols[ iSymbolTable ];
		nSymbols = ParseSymbolTable( sFileName, (SymbolTable_Index_e) iSymbolTable );
	}

	int iArg = 1;
	if (iArg <= nArgs)
	{
		std::string pFileName;
		
		if( g_aArgs[ iArg ].bType & TYPE_QUOTED_2 )
		{
			pFileName = g_aArgs[ iArg ].sArg;

			sFileName = g_sProgramDir + pFileName;

			// Remember File Name of last symbols loaded
			g_sFileNameSymbolsUser = pFileName;
		}

		// SymbolOffset
		// sym load "filename" [,symbol_offset]
		unsigned int nOffsetAddr = 0;

		iArg++;
		if( iArg <= nArgs)
		{
			if (g_aArgs[ iArg ].eToken == TOKEN_COMMA)
			{
				iArg++;
				if( iArg <= nArgs )
				{
					nOffsetAddr = g_aArgs[ iArg ].nValue;
					if( (nOffsetAddr < _6502_MEM_BEGIN) || (nOffsetAddr > _6502_MEM_END) )
					{
						nOffsetAddr = 0;
					}
				}
			}
		}

		if( !pFileName.empty() )
		{
			nSymbols = ParseSymbolTable( sFileName, (SymbolTable_Index_e) iSymbolTable, nOffsetAddr );
		}
	}

	if( nSymbols > 0 )
	{
		g_nSymbolsLoaded = nSymbols;
	}

	Update_t bUpdateDisplay = UPDATE_DISASM;
	bUpdateDisplay |= (nSymbols > 0) ? UPDATE_SYMBOLS : 0;

	return bUpdateDisplay;
}

//===========================================================================
Update_t _CmdSymbolsClear( SymbolTable_Index_e eSymbolTable )
{
	g_aSymbols[ eSymbolTable ].clear();
	
	return UPDATE_SYMBOLS;
}


//===========================================================================
void SymbolUpdate( SymbolTable_Index_e eSymbolTable, char *pSymbolName, WORD nAddress, bool bRemoveSymbol, bool bUpdateSymbol )
{
	if (bRemoveSymbol)
		pSymbolName = g_aArgs[2].sArg;

	if (_tcslen( pSymbolName ) < MAX_SYMBOLS_LEN)
	{
		WORD nAddressPrev;
		int  iTable;
		bool bExists = FindAddressFromSymbol( pSymbolName, &nAddressPrev, &iTable );

		if (bExists)
		{
			if (iTable == eSymbolTable)
			{
				if (bRemoveSymbol)
				{
					ConsoleBufferPush( TEXT(" Removing symbol." ) );
				}

				g_aSymbols[ eSymbolTable ].erase( nAddressPrev );

				if (bUpdateSymbol)
				{
					char sText[ CONSOLE_WIDTH * 2 ];
					ConsolePrintFormat( sText, " Updating %s%s%s from %s$%s%04X%s to %s$%s%04X%s"
						, CHC_SYMBOL, pSymbolName, CHC_DEFAULT
						, CHC_ARG_SEP					
						, CHC_ADDRESS, nAddressPrev, CHC_DEFAULT
						, CHC_ARG_SEP					
						, CHC_ADDRESS, nAddress, CHC_DEFAULT
					);
				}
			}
		}					
		else
		{
			if (bRemoveSymbol)
			{
				ConsoleBufferPush( TEXT(" Symbol not in table." ) );
			}
		}

		if (bUpdateSymbol)
		{
#if _DEBUG
			const char* pSymbol = FindSymbolFromAddress( nAddress, &iTable );
			{
				// Found another symbol for this address.  Harmless.
				// TODO: Probably should check if same name?
			}
#endif
			g_aSymbols[ eSymbolTable ][ nAddress ] = pSymbolName;

			// Tell user symbol was added
			char sText[ CONSOLE_WIDTH * 2 ];
			ConsolePrintFormat( sText, " Added symbol: %s%s%s %s$%s%04X%s"
				, CHC_SYMBOL, pSymbolName, CHC_DEFAULT
				, CHC_ARG_SEP					
				, CHC_ADDRESS, nAddress, CHC_DEFAULT
			);
		}
	}
}


//===========================================================================
Update_t _CmdSymbolsUpdate( int nArgs, int bSymbolTables )
{
	bool bRemoveSymbol = false;
	bool bUpdateSymbol = false;

	if ((nArgs == 2) && 
		((g_aArgs[ 1 ].eToken == TOKEN_EXCLAMATION) || (g_aArgs[1].eToken == TOKEN_TILDE)) )
		bRemoveSymbol = true;

	if ((nArgs == 3) && (g_aArgs[ 2 ].eToken == TOKEN_EQUAL      ))
		bUpdateSymbol = true;

	if (bRemoveSymbol || bUpdateSymbol)
	{
		TCHAR *pSymbolName = g_aArgs[1].sArg;
		WORD   nAddress    = g_aArgs[3].nValue;

		int iTable = _GetSymbolTableFromFlag( bSymbolTables );
		SymbolUpdate( (SymbolTable_Index_e) iTable, pSymbolName, nAddress, bRemoveSymbol, bUpdateSymbol );
		return ConsoleUpdate();
	}

	return UPDATE_NOTHING;
}


//===========================================================================
Update_t _CmdSymbolsCommon ( int nArgs, int bSymbolTables )
{
	if (! nArgs)
	{
		return Help_Arg_1( g_iCommand );
	}

	Update_t iUpdate = _CmdSymbolsUpdate( nArgs, bSymbolTables );
	if (iUpdate != UPDATE_NOTHING)
		return iUpdate;

	TCHAR sText[ CONSOLE_WIDTH ];

	int iArg = 0;
	while (iArg++ <= nArgs)
	{
		int iParam;
		int nParams = FindParam( g_aArgs[iArg].sArg, MATCH_EXACT, iParam ); // MATCH_FUZZY
		if (nParams)
		{
			if (iParam == PARAM_CLEAR)
			{
				int iTable = _GetSymbolTableFromFlag( bSymbolTables );
				if (iTable != NUM_SYMBOL_TABLES)
				{
					Update_t iUpdate = _CmdSymbolsClear( (SymbolTable_Index_e) iTable );
					ConsolePrintFormat( sText, TEXT(" Cleared symbol table: %s%s")
						, CHC_STRING, g_aSymbolTableNames[ iTable ]
					 );
					iUpdate |= ConsoleUpdate();
					return iUpdate;
				}
				else
				{
					// Shouldn't have multiple symbol tables selected
//					nArgs = _Arg_1( eSymbolsTable );
					ConsoleBufferPush( TEXT(" Error: Unknown Symbol Table Type") );
					return ConsoleUpdate();
				}
			}
			else
			if (iParam == PARAM_LOAD)
			{
				nArgs = _Arg_Shift( iArg, nArgs);
				Update_t bUpdate = CmdSymbolsLoad( nArgs );

				int iTable = _GetSymbolTableFromFlag( bSymbolTables );
				if (iTable != NUM_SYMBOL_TABLES)
				{
					if( bUpdate & UPDATE_SYMBOLS )
					{
						//sprintf( sText, "  Symbol Table: %s%s%s, %sloaded symbols: %s%d"
						//	, CHC_STRING, g_aSymbolTableNames[ iTable ]
						//	, CHC_DEFAULT, CHC_DEFAULT
						//	, CHC_NUM_DEC, g_nSymbolsLoaded
						//);
						_CmdSymbolsInfoHeader( iTable, sText, g_nSymbolsLoaded );
						ConsolePrint( sText );
					}
				}
				else
				{
					ConsoleBufferPush( TEXT(" Error: Unknown Symbol Table Type") );
				}
				return ConsoleUpdate();
			}
			else
			if (iParam == PARAM_SAVE)
			{
				nArgs = _Arg_Shift( iArg, nArgs);
				return CmdSymbolsSave( nArgs );
			}
			else
			if (iParam == PARAM_ON)
			{
				g_bDisplaySymbolTables |= bSymbolTables;
				int iTable = _GetSymbolTableFromFlag( bSymbolTables );
				if (iTable != NUM_SYMBOL_TABLES)
				{
					_CmdSymbolsInfoHeader( iTable, sText );
					ConsolePrint( sText );
				}
				return ConsoleUpdate() | UPDATE_DISASM;
			}
			else
			if (iParam == PARAM_OFF)
			{
				g_bDisplaySymbolTables &= ~bSymbolTables;
				int iTable = _GetSymbolTableFromFlag( bSymbolTables );
				if (iTable != NUM_SYMBOL_TABLES)
				{
					_CmdSymbolsInfoHeader( iTable, sText );
					ConsolePrint( sText );
				}
				return ConsoleUpdate() | UPDATE_DISASM;
			}
		}
		else
		{
			return _CmdSymbolsListTables( nArgs, bSymbolTables );
		}

	}

	return ConsoleUpdate();
}

//===========================================================================
Update_t CmdSymbolsCommand (int nArgs)
{
	if (! nArgs)
	{
		return CmdSymbolsInfo( 1 );
	}

	int bSymbolTable = SYMBOL_TABLE_MAIN << GetSymbolTableFromCommand();
	return _CmdSymbolsCommon( nArgs, bSymbolTable ); // BUGFIX 2.6.2.12 Hard-coded to SYMMAIN
}

//===========================================================================
Update_t CmdSymbolsSave (int nArgs)
{
	return UPDATE_CONSOLE_DISPLAY;
}
