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

	const char*     g_sFileNameSymbols[ NUM_SYMBOL_TABLES ] = {
		 "APPLE2E.SYM"
		,"A2_BASIC.SYM"
		,"A2_ASM.SYM"
		,"A2_USER1.SYM" // "A2_USER.SYM",
		,"A2_USER2.SYM"
		,"A2_SRC1.SYM" // "A2_SRC.SYM",
		,"A2_SRC2.SYM"
		,"A2_DOS33.SYM2"
		,"A2_PRODOS.SYM"
	};
	std::string  g_sFileNameSymbolsUser;

	const char * g_aSymbolTableNames[ NUM_SYMBOL_TABLES ] =
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

	std::string _CmdSymbolsInfoHeader( int iTable, int nDisplaySize = 0 );
	void        _PrintCurrentPath();
	Update_t    _PrintSymbolInvalidTable();


// Private ________________________________________________________________________________________

//===========================================================================
void _PrintCurrentPath()
{
	ConsoleDisplayError( g_sProgramDir.c_str() );
}

Update_t _PrintSymbolInvalidTable()
{
	// TODO: display the user specified file name
	ConsoleBufferPush( "Invalid symbol table." );

	ConsolePrintFormat( "Only " CHC_NUM_DEC "%d" CHC_DEFAULT " symbol tables are supported:"
		, NUM_SYMBOL_TABLES
	);

	std::string sText;

	// Similar to _CmdSymbolsInfoHeader()
	for ( int iTable = 0; iTable < NUM_SYMBOL_TABLES; iTable++ )
	{
		sText += StrFormat( CHC_USAGE "%s" CHC_ARG_SEP "%c "
			, g_aSymbolTableNames[ iTable ]
			, (iTable != (NUM_SYMBOL_TABLES-1))
				? ','
				: '.'
		);
	}

//	return ConsoleDisplayError( sText.c_str() );
	ConsolePrint( sText.c_str() );
	return ConsoleUpdate();
}


// Public _________________________________________________________________________________________


//===========================================================================
std::string const& GetSymbol (WORD nAddress, int nBytes, std::string& sAddressBuf)
{
	std::string const* pSymbol = FindSymbolFromAddress( nAddress );
	if (pSymbol)
		return *pSymbol;

	return sAddressBuf = FormatAddress( nAddress, nBytes );
}

//===========================================================================
int GetSymbolTableFromCommand()
{
	return (g_iCommand - CMD_SYMBOLS_ROM);
}

// @param iTable_ Which symbol table the symbol is in if any.  If none will be NUM_SYMBOL_TABLES
//===========================================================================
std::string const* FindSymbolFromAddress (WORD nAddress, int * iTable_ )
{
	// Bugfix/User feature: User symbols should be searched first
	int iTable = NUM_SYMBOL_TABLES;
	if (iTable_)
	{
		*iTable_ = iTable;
	}

	while (iTable-- > 0)
	{
		if (! g_aSymbols[iTable].size())
			continue;

		if (! (g_bDisplaySymbolTables & (1 << iTable)))
			continue;

		std::map<WORD, std::string>::iterator iSymbols = g_aSymbols[iTable].find(nAddress);
		if (g_aSymbols[iTable].find(nAddress) != g_aSymbols[iTable].end())
		{
			if (iTable_)
			{
				*iTable_ = iTable;
			}
			return &iSymbols->second;
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
			if (!_stricmp( iSymbol->second.c_str(), pSymbol))
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
	char sHexApple[ CONSOLE_WIDTH ];

	if (pText[0] == '$')
	{
		if (!TextIsHexString( pText+1))
			return false;

		strcpy( sHexApple, "0x" );
		strncpy( sHexApple+2, pText+1, MAX_SYMBOLS_LEN - 3 );
		sHexApple[2 + (MAX_SYMBOLS_LEN - 3) - 1] = 0;
		pText = sHexApple;
	}

	if (pText[0] == '0')
	{
		if ((pText[1] == 'X') || pText[1] == 'x')
		{
			if (!TextIsHexString( pText+2))
				return false;

			char *pEnd;
			nAddress_ = (WORD) strtol( pText, &pEnd, 16 );
			return true;
		}
		if (TextIsHexString( pText ))
		{
			char *pEnd;
			nAddress_ = (WORD) strtol( pText, &pEnd, 16 );
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
std::string _CmdSymbolsInfoHeader ( int iTable, int nDisplaySize /* = 0 */ )
{
	// Common case is to use/calc the table size
	bool bActive = (g_bDisplaySymbolTables & (1 << iTable)) ? true : false;
	int nSymbols  = nDisplaySize ? nDisplaySize : g_aSymbols[ iTable ].size();

	// Short Desc: `MAIN`: `1000`
	// // 2.6.2.19 Color for name of symbol table: _CmdPrintSymbol() "SYM HOME" _CmdSymbolsInfoHeader "SYM"
	// CHC_STRING and CHC_NUM_DEC are both cyan, using CHC_USAGE instead of CHC_STRING
	return StrFormat(CHC_USAGE "%s" CHC_ARG_SEP ":%s%d " // CHC_DEFAULT
		, g_aSymbolTableNames[ iTable ]
		, bActive ? CHC_NUM_DEC : CHC_WARNING, nSymbols
	);
}

//===========================================================================
std::string _CmdSymbolsSummaryStatus ( int iTable )
{
	bool bActive = (g_bDisplaySymbolTables & (1 << iTable)) ? true : false;
	int iParam  = bActive
	            ? PARAM_ON
	            : PARAM_OFF
	            ;
	std::string sSymbolSummary = _CmdSymbolsInfoHeader( iTable );
	sSymbolSummary += StrFormat( "%s(%s%s%s)", CHC_ARG_SEP, CHC_COMMAND, g_aParameters[ iParam ].m_sName,  CHC_ARG_SEP );

	return sSymbolSummary;
}

//===========================================================================
Update_t CmdSymbolsInfo (int nArgs)
{
	int bDisplaySymbolTables = 0;

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

	std::string const sIndent = "  ";
	std::string       sText   = sIndent; // Indent new line

	for ( int iTable = 0, bTable = 1; bTable <= bDisplaySymbolTables; iTable++, bTable <<= 1 )
	{
		if ( !!(bDisplaySymbolTables & bTable) )
		{
			std::string hdr = _CmdSymbolsInfoHeader( iTable ); // 15 chars per table

			// 2.8.0.4 BUGFIX: Check for buffer overflow and wrap text
			int nLen = ConsoleColor_StringLength( hdr.c_str() );
			int nDst = ConsoleColor_StringLength( sText.c_str() );
			if ( (nDst + nLen) > CONSOLE_WIDTH )
			{
				ConsolePrint( sText.c_str() );
				sText = sIndent; // Indent new line
			}
			sText += hdr;
		}
	}
	ConsolePrint( sText.c_str() );

	return ConsoleUpdate();
}

//===========================================================================
void _CmdPrintSymbol( LPCTSTR pSymbol, WORD nAddress, int iTable )
{
	// 2.6.2.19 Color for name of symbol table: _CmdPrintSymbol() "SYM HOME" _CmdSymbolsInfoHeader "SYM"
	// CHC_STRING and CHC_NUM_DEC are both cyan, using CHC_USAGE instead of CHC_STRING

	// 2.6.2.20 Changed: Output of found symbol more table friendly.  Symbol table name displayed first.
	ConsolePrintFormat( "  %s%s%s: $%s%04X %s%s"
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

	if ( bSymbolTables & (1 << iTable) )
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

	for ( ; bTable <= bSymbolTables; iTable++, bTable <<= 1 )
	{
		if ( bTable & bSymbolTables )
			break;
	}

	return iTable;
}


/**
	@param bSymbolTables Bit Flags of which symbol tables to search
//=========================================================================== */
bool _CmdSymbolList_Address2Symbol( int nAddress, int bSymbolTables )
{
	int iTable = 0;
	std::string const* pSymbol = FindSymbolFromAddress( nAddress, &iTable );

	if (pSymbol)
	{				
		if (_FindSymbolTable( bSymbolTables, iTable ))
		{
			_CmdPrintSymbol( pSymbol->c_str(), nAddress, iTable);
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

	for ( int iArgs = 1; iArgs <= nArgs; iArgs++ )
	{
		WORD nAddress = g_aArgs[iArgs].nValue;
		LPCTSTR pSymbol = g_aArgs[iArgs].sArg;

		// Dump all symbols for this table
		if ( g_aArgRaw[iArgs].eToken == TOKEN_STAR)
		{
	//		int iWhichTable = (g_iCommand - CMD_SYMBOLS_MAIN);
	//		bDisplaySymbolTables = (1 << iWhichTable);

			int iTable = 0;
			int bTable = 1;
			for ( ; bTable <= bSymbolTables; iTable++, bTable <<= 1 )
			{
				if ( bTable & bSymbolTables )
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
					ConsolePrint(_CmdSymbolsInfoHeader(iTable).c_str() );
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
					ConsolePrintFormat( " Address not found: %s$%s%04X%s"
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
						ConsolePrintFormat( " %sSymbol not found: %s%s%s"
							, CHC_ERROR, CHC_SYMBOL, pSymbol, CHC_DEFAULT
						);
					}
				}
				else
				{
					ConsolePrintFormat( " %sSymbol not found: %s%s%s"
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
	bool bFileDisplayed = false;

	const int nMaxLen = std::min<int>(DISASM_DISPLAY_MAX_TARGET_LEN, MAX_SYMBOLS_LEN);

	int nSymbolsLoaded = 0;

	if (pPathFileName.empty())
		return nSymbolsLoaded;

	std::string sFormat1 = StrFormat( "%%x %%%ds", MAX_SYMBOLS_LEN ); // i.e. "%x %51s"
	std::string sFormat2 = StrFormat( "%%%ds %%x", MAX_SYMBOLS_LEN ); // i.e. "%51s %x"

	FILE *hFile = fopen( pPathFileName.c_str(), "rt" );

	if ( !hFile && g_bSymbolsDisplayMissingFile )
	{
		// TODO: print filename! Bug #242 Help file (.chm) description for "Symbols" #242
		ConsoleDisplayError( "Symbol File not found:" );
		_PrintCurrentPath();
		nSymbolsLoaded = -1; // HACK: ERROR: FILE NOT EXIST
	}
	
	bool bDupSymbolHeader = false;
	if ( hFile )
	{
		while ( !feof(hFile) )
		{
			// Support 2 types of symbols files:
			// 1) AppleWin:
			//    . 0000 SYMBOL
			//    . FFFF SYMBOL
			// 2) ACME:
			//    . SYMBOL  =$0000; Comment
			//    . SYMBOL  =$FFFF; Comment
			//
			uint32_t nAddress = _6502_MEM_END + 1; // default to invalid address
			char  sName[ MAX_SYMBOLS_LEN+1 ]  = "";

			const int MAX_LINE = 256;
			char  szLine[ MAX_LINE ] = "";

			if ( !fgets(szLine, MAX_LINE-1, hFile) )	// Get next line
			{
				//ConsolePrint("<<EOF");
				break;
			}

			if (strstr(szLine, "$") == NULL)
			{
				sscanf(szLine, sFormat1.c_str(), &nAddress, sName);
			}
			else
			{
				char* p = strstr(szLine, "=");	// Optional
				if (p) *p = ' ';
				p = strstr(szLine, "$");
				if (p) *p = ' ';
				p = strstr(szLine, ";");		// Optional
				if (p) *p = 0;
				p = strstr(szLine, " ");		// 1st space between name & value
				if (p)
				{
					int nLen = p - szLine;
					if (nLen > MAX_SYMBOLS_LEN)
					{
						memset(&szLine[MAX_SYMBOLS_LEN], ' ', nLen - MAX_SYMBOLS_LEN);	// sscanf fails for nAddress if string too long
					}
				}
				sscanf(szLine, sFormat2.c_str(), sName, &nAddress);
			}

			// SymbolOffset
			nAddress += nSymbolOffset;

			if ( (nAddress > _6502_MEM_END) || (sName[0] == 0) )
				continue;

			// 2.9.0.11 Bug #479
			int nLen = strlen( sName );
			if (nLen > nMaxLen)
			{
				ConsolePrintFormat( " %sWarn.: %s%s %s(%s%d %s> %s%d%s)"
					, CHC_WARNING
					, CHC_SYMBOL
					, sName
					, CHC_ARG_SEP
					, CHC_NUM_DEC
					, nLen
					, CHC_ARG_SEP
					, CHC_NUM_DEC
					, nMaxLen
					, CHC_ARG_SEP
				);
				ConsoleUpdate(); // Flush buffered output so we don't ask the user to pause
			}

			int iTable = 0;

			// 2.8.0.5 Bug #244 (Debugger) Duplicate symbols for identical memory addresses in APPLE2E.SYM
			std::string const* pSymbolPrev = FindSymbolFromAddress( (WORD)nAddress, &iTable ); // don't care which table it is in
			if ( pSymbolPrev )
			{
				if ( !bFileDisplayed )
				{
					bFileDisplayed = true;

					ConsolePrintFormat( "%s%s"
						, CHC_PATH
						, pPathFileName.c_str()
					);
				}

				ConsolePrintFormat( " %sInfo.: %s%-16s %saliases %s$%s%04X %s%-12s%s (%s%s%s)" // MAGIC NUMBER: -MAX_SYMBOLS_LEN
					, CHC_INFO // 2.9.0.10 was CHC_WARNING, see #479
					, CHC_SYMBOL
					, sName
					, CHC_INFO
					, CHC_ARG_SEP
					, CHC_ADDRESS
					, nAddress
					, CHC_SYMBOL
					, pSymbolPrev->c_str()
					, CHC_DEFAULT
					, CHC_STRING
					, g_aSymbolTableNames[ iTable ]
					, CHC_DEFAULT
				);

				ConsoleUpdate(); // Flush buffered output so we don't ask the user to pause
/*
				ConsolePrintFormat( " %sWarning: %sAddress already has symbol Name%s (%s%s%s): %s%s"
					, CHC_WARNING
					, CHC_INFO
					, CHC_ARG_SEP
					, CHC_STRING
					, g_aSymbolTableNames[ iTable ]
					, CHC_DEFAULT
					, CHC_SYMBOL
					, pSymbolPrev
				);

				ConsolePrintFormat( "  %s$%s%04X %s%-31s%s"
					, CHC_ARG_SEP
					, CHC_ADDRESS
					, nAddress
					, CHC_SYMBOL
					, sName
					, CHC_DEFAULT
				);
*/
			}

			// If updating symbol, print duplicate symbols
			WORD nAddressPrev = 0;

			bool bExists  = FindAddressFromSymbol( sName, &nAddressPrev, &iTable );
			if ( bExists )
			{
				if ( !bDupSymbolHeader )
				{
					bDupSymbolHeader = true;
					ConsolePrintFormat( " %sDup Symbol Name%s (%s%s%s) %s"
						, CHC_ERROR
						, CHC_DEFAULT
						, CHC_STRING
						, g_aSymbolTableNames[ iTable ]
						, CHC_DEFAULT
						, pPathFileName.c_str()
					);
				}

				ConsolePrintFormat( "  %s$%s%04X %s%-31s%s"
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
		sFileName = g_sProgramDir + g_sFileNameSymbols[ iSymbolTable ];

		// if ".sym2" extension then RUN since we need support for DB, DA, etc.
		// TODO: Use Util_GetFileNameExtension()
		const size_t nLength    = sFileName.length();
		const size_t iExtension = sFileName.rfind( '.', nLength );
		const std::string         sExt = (iExtension != std::string::npos)
		                               ? sFileName.substr( iExtension, nLength )
		                               : std::string("")
		                               ;

		bool bSymbolFormat2 = (sExt == std::string( ".SYM2"));
		if (bSymbolFormat2)
		{
			// We could hijack:
			//     CmdOutputRun( -1 );
			// But we would need to futz around with arguments
			//     strncpy(g_aArgs[0].sArg, sFileName.c_str(), sizeof(g_aArgs[0].sArg));

			MemoryTextFile_t script;
			if (script.Read( sFileName ))
			{
				int nLine = script.GetNumLines();
				Update_t bUpdateDisplay = UPDATE_NOTHING; // not used

				for ( int iLine = 0; iLine < nLine; iLine++ )
				{
					script.GetLine( iLine, g_pConsoleInput, CONSOLE_WIDTH-2 );
					g_nConsoleInputChars = strlen( g_pConsoleInput );
					bUpdateDisplay |= DebuggerProcessCommand( false );
				}
			}

		}
		else
		{
			nSymbols = ParseSymbolTable( sFileName, (SymbolTable_Index_e) iSymbolTable );
		}

		// Try optional alternate location
		if ((nSymbols == 0) && !g_sBuiltinSymbolsDir.empty())
		{
			sFileName = g_sBuiltinSymbolsDir + g_sFileNameSymbols[ iSymbolTable ];
			nSymbols = ParseSymbolTable( sFileName, (SymbolTable_Index_e) iSymbolTable );
		}
	}

	int iArg = 1;
	if (iArg <= nArgs)
	{
		std::string pFileName;
		
		if ( g_aArgs[ iArg ].bType & TYPE_QUOTED_2 )
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
		if ( iArg <= nArgs)
		{
			if (g_aArgs[ iArg ].eToken == TOKEN_COMMA)
			{
				iArg++;
				if ( iArg <= nArgs )
				{
					nOffsetAddr = g_aArgs[ iArg ].nValue;
					if ( (nOffsetAddr < _6502_MEM_BEGIN) || (nOffsetAddr > _6502_MEM_END) )
					{
						nOffsetAddr = 0;
					}
				}
			}
		}

		if ( !pFileName.empty() )
		{
			nSymbols = ParseSymbolTable( sFileName, (SymbolTable_Index_e) iSymbolTable, nOffsetAddr );
		}
	}

	if ( nSymbols > 0 )
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
void SymbolUpdate ( SymbolTable_Index_e eSymbolTable, const char *pSymbolName, WORD nAddress, bool bRemoveSymbol, bool bUpdateSymbol )
{
	if (bRemoveSymbol)
		pSymbolName = g_aArgs[2].sArg;

	size_t nSymLen = strlen( pSymbolName );
	if (nSymLen < MAX_SYMBOLS_LEN)
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
					ConsoleBufferPush( " Removing symbol." );
				}

				g_aSymbols[ eSymbolTable ].erase( nAddressPrev );

				if (bUpdateSymbol)
				{
					ConsolePrintFormat( " Updating %s%s%s from %s$%s%04X%s to %s$%s%04X%s"
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
				ConsoleBufferPush( " Symbol not in table." );
			}
		}

		if (bUpdateSymbol)
		{
#if _DEBUG
			std::string const* pSymbol = FindSymbolFromAddress( nAddress, &iTable );
			{
				// Found another symbol for this address.  Harmless.
				// TODO: Probably should check if same name?
			}
#endif
			g_aSymbols[ eSymbolTable ][ nAddress ] = pSymbolName;

			// 2.9.1.26: When adding symbols list the address first then the name for readability
			// Tell user symbol was added
			ConsolePrintFormat(
				/*CHC_DEFAULT*/ " Added: "
				  CHC_ARG_SEP   "$"
				  CHC_ADDRESS   "%04X"
				  CHC_DEFAULT   " "
				  CHC_SYMBOL    "%s"
				, nAddress
				, pSymbolName
			);
		}
	}
	else
		ConsolePrintFormat(
			CHC_ERROR   "Error: "
			CHC_DEFAULT "Symbol length "
			CHC_NUM_DEC "%d "
			CHC_ARG_SEP "> "
			CHC_NUM_DEC "%d "
			, (int) nSymLen
			, MAX_SYMBOLS_LEN
		);
}

// Syntax:
//     sym ! <symbol>
//     sym ~ <symbol>
//     sym   <symbol> =
//     sym @ = <addr>
// NOTE: Listing of the symbols is handled via returning UPDATE_NOTHING which is triggered by:
//     sym *
//===========================================================================
Update_t _CmdSymbolsUpdate( int nArgs, int bSymbolTables )
{
	bool bRemoveSymbol = false;
	bool bUpdateSymbol = false;

	char *pSymbolName = g_aArgs[1].sArg;
	WORD   nAddress    = g_aArgs[3].nValue;

	if ((nArgs == 2)
	 && ((g_aArgs[ 1 ].eToken == TOKEN_EXCLAMATION) || (g_aArgs[1].eToken == TOKEN_TILDE)) )
		bRemoveSymbol = true;

	if ((nArgs == 3) && (g_aArgs[ 2 ].eToken == TOKEN_EQUAL      ))
		bUpdateSymbol = true;

	// 2.9.1.7 Added: QoL for automatic symbol names
	if ((nArgs == 2)
	&& (g_aArgRaw[ 1 ].eToken == TOKEN_AT   )   // NOTE: @ is parsed and evaluated and NOT in the cooked args
	&& (g_aArgs  [ 1 ].eToken == TOKEN_EQUAL))
	{
		if (bSymbolTables == SYMBOL_TABLE_USER_1)
			bSymbolTables =  SYMBOL_TABLE_USER_2; // Autogenerated symbol names go in table 2 for organization when reverse engineering.  Table 1 = known, Table 2 = unknown.

		nAddress = g_aArgs[2].nValue;
		strncpy_s( g_aArgs[1].sArg, StrFormat("_%04X", nAddress).c_str(), _TRUNCATE ); // Autogenerated symbol name

		bUpdateSymbol = true;
	}

	if (bRemoveSymbol || bUpdateSymbol)
	{
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
					ConsolePrintFormat( " Cleared symbol table: %s%s"
						, CHC_STRING, g_aSymbolTableNames[ iTable ]
					 );
					iUpdate |= ConsoleUpdate();
					return iUpdate;
				}
				else
				{
					// Shouldn't have multiple symbol tables selected
//					nArgs = _Arg_1( eSymbolsTable );
					ConsoleBufferPush( " Error: Unknown Symbol Table Type" );
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
					if ( bUpdate & UPDATE_SYMBOLS )
					{
						ConsolePrint( _CmdSymbolsInfoHeader( iTable, g_nSymbolsLoaded ).c_str() );
					}
				}
				else
				{
					ConsoleBufferPush( " Error: Unknown Symbol Table Type" );
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
					ConsolePrint( _CmdSymbolsSummaryStatus( iTable ).c_str() );
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
					ConsolePrint( _CmdSymbolsSummaryStatus( iTable ).c_str() );
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
