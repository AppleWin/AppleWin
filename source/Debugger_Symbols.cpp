#include "StdAfx.h"
#pragma  hdrstop

// Symbols ________________________________________________________________________________________

	char*     g_sFileNameSymbols[ NUM_SYMBOL_TABLES ] = {
		"APPLE2E.SYM",
		"A2_BASIC.SYM",
		"A2_ASM.SYM",
		"A2_USER1.SYM",
		"A2_USER2.SYM",
		"A2_SRC1.SYM",
		"A2_SRC2.SYM"
	};
	char      g_sFileNameSymbolsUser [ MAX_PATH ] = "";

	char * g_aSymbolTableNames[ NUM_SYMBOL_TABLES ] =
	{
		"Main",
		"Basic",
		"Assembly",
		"User1",
		"User2",
		"Src1",
		"Src2"
	};

	bool g_bSymbolsDisplayMissingFile = true;

	SymbolTable_t g_aSymbols[ NUM_SYMBOL_TABLES ];
	int           g_nSymbolsLoaded = 0;  // on Last Load
	bool          g_aConfigSymbolsDisplayed[ NUM_SYMBOL_TABLES ] =
	{
		true,
		true,
		true
	};


// Utils _ ________________________________________________________________________________________

//===========================================================================
LPCTSTR GetSymbol (WORD nAddress, int nBytes)
{
	LPCSTR pSymbol = FindSymbolFromAddress( nAddress );
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
LPCTSTR FindSymbolFromAddress (WORD nAddress, int * iTable_ )
{
	// Bugfix/User feature: User symbols should be searched first
	int iTable = NUM_SYMBOL_TABLES;
	while (iTable-- > 0)
	{
		if (g_aSymbols[iTable].size())
		{
			map<WORD, string>::iterator iSymbols = g_aSymbols[iTable].find(nAddress);
			if(g_aSymbols[iTable].find(nAddress) != g_aSymbols[iTable].end())
			{
				if (iTable_)
				{
					*iTable_ = iTable;
				}
				return iSymbols->second.c_str();
			}
		}
	}	
	return NULL;	
}

//===========================================================================
bool FindAddressFromSymbol ( LPCTSTR pSymbol, WORD * pAddress_, int * iTable_ )
{
	// Bugfix/User feature: User symbols should be searched first
	for (int iTable = NUM_SYMBOL_TABLES; iTable-- > 0; )
	{
		if (! g_aSymbols[iTable].size())
			continue;

//		map<WORD, string>::iterator iSymbol = g_aSymbols[iTable].begin();
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
WORD GetAddressFromSymbol (LPCTSTR pSymbol)
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

void _CmdSymbolsInfoHeader( int iTable, char * pText )
{
	int nSymbols  = g_aSymbols[ iTable ].size();
	sprintf( pText, "  %s: %s%d%s"
		, g_aSymbolTableNames[ iTable ]
		, CHC_NUM_DEC
		, nSymbols
		, CHC_DEFAULT
	);
}

//===========================================================================
Update_t CmdSymbolsInfo (int nArgs)
{
	char sText[ CONSOLE_WIDTH * 2 ] = "";
	char sTemp[ CONSOLE_WIDTH ] = "";

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
			sprintf( sText, "Only %s%d%s symbol tables supported!"
				, CHC_NUM_DEC
				, NUM_SYMBOL_TABLES
				, CHC_DEFAULT );
			return ConsoleDisplayError( sText );
		}

		bDisplaySymbolTables = (1 << iWhichTable);
	}

	//sprintf( sText, "  Symbols  Main: %s%d%s  User: %s%d%s   Source: %s%d%s"

	int bTable = 1;
	int iTable = 0;
	for( ; bTable <= bDisplaySymbolTables; iTable++, bTable <<= 1 ) {
		if( bDisplaySymbolTables & bTable ) {
			_CmdSymbolsInfoHeader( iTable, sTemp );
			strcat( sText, sTemp );
		}
	}
	ConsolePrint( sText );

	return ConsoleUpdate();
}

//===========================================================================
void _CmdPrintSymbol( LPCTSTR pSymbol, WORD nAddress, int iTable )
{
	char   sText[ CONSOLE_WIDTH ];
	sprintf( sText, "  %s$%s%04X%s (%s%s%s) %s%s"
		, CHC_ARG_SEP
		, CHC_ADDRESS
		, nAddress
		, CHC_DEFAULT
		, CHC_STRING
		, g_aSymbolTableNames[ iTable ]
		, CHC_DEFAULT
		, CHC_SYMBOL
		, pSymbol );
	// ConsoleBufferPush( sText );
	ConsolePrint( sText );
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
	LPCTSTR pSymbol = FindSymbolFromAddress( nAddress, &iTable );

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
				//		map<WORD, string>::iterator iSymbol = g_aSymbols[iTable].begin();
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
					wsprintf( sText
						, TEXT(" Address not found: %s$%s%04X%s" )
						, CHC_ARG_SEP
						, CHC_ADDRESS, nAddress, CHC_DEFAULT );
					ConsolePrint( sText );
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
						wsprintf( sText
							, TEXT(" Symbol not found: %s%s%s")
							, CHC_SYMBOL, pSymbol, CHC_DEFAULT
						);
						ConsolePrint( sText );
					}
				}
				else
				{
					wsprintf( sText
						, TEXT(" Symbol not found: %s%s%s")
						, CHC_SYMBOL, pSymbol, CHC_DEFAULT
					);
					ConsolePrint( sText );
				}
			}
		}
	}
	return ConsoleUpdate();
}


void Print_Current_Path()
{
	ConsoleDisplayError( g_sProgramDir );
}

//===========================================================================
int ParseSymbolTable( TCHAR *pFileName, SymbolTable_Index_e eSymbolTableWrite, int nSymbolOffset )
{
	int nSymbolsLoaded = 0;

	if (! pFileName)
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

	FILE *hFile = fopen(pFileName,"rt");

	if( !hFile && g_bSymbolsDisplayMissingFile )
	{
		ConsoleDisplayError( "Symbol File not found:" );
		Print_Current_Path();
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
				int nLen = p - szLine;
				if (nLen > MAX_SYMBOLS_LEN)
				{
					memset(&szLine[MAX_SYMBOLS_LEN], ' ', nLen-MAX_SYMBOLS_LEN);	// sscanf fails for nAddress if string too long
				}
				sscanf(szLine, sFormat2, sName, &nAddress);
			}

			// SymbolOffset
			nAddress += nSymbolOffset;

			if( (nAddress > _6502_MEM_END) || (sName[0] == 0) )
				continue;

	#if 1 // _DEBUG
			// If updating symbol, print duplicate symbols
			WORD nAddressPrev;
			int  iTable;
			bool bExists = FindAddressFromSymbol( sName, &nAddressPrev, &iTable );
			if( bExists )
			{
				char sText[ CONSOLE_WIDTH * 3 ];
				if( !bDupSymbolHeader )
				{
					bDupSymbolHeader = true;
					sprintf( sText, " %sDup Symbol Name%s (%s%s%s) %s"
						, CHC_ERROR
						, CHC_DEFAULT
						, CHC_STRING
						, g_aSymbolTableNames[ iTable ]
						, CHC_DEFAULT
						, pFileName
					);
					ConsolePrint( sText );
				}

				sprintf( sText, "  %s$%s%04X %s%-31s%s"
					, CHC_ARG_SEP
					, CHC_ADDRESS
					, nAddress
					, CHC_SYMBOL
					, sName
					, CHC_DEFAULT
				);
				ConsolePrint( sText );
			}
	#endif
			g_aSymbols[ eSymbolTableWrite ] [ (WORD) nAddress ] = sName;
			nSymbolsLoaded++;
		}
		fclose(hFile);
	}

	return nSymbolsLoaded;
}


//===========================================================================
Update_t CmdSymbolsLoad (int nArgs)
{
	TCHAR sFileName[MAX_PATH];
	_tcscpy(sFileName,g_sProgramDir);

	int iSymbolTable = GetSymbolTableFromCommand();
	if ((iSymbolTable < 0) || (iSymbolTable >= NUM_SYMBOL_TABLES))
	{
		wsprintf( sFileName, "Only %d symbol tables supported!", NUM_SYMBOL_TABLES );
		return ConsoleDisplayError( sFileName );
	}

	int nSymbols = 0;

	if (! nArgs)
	{
		// Default to main table
//		if (g_iCommand == CMD_SYMBOLS_MAIN)
//			_tcscat(sFileName, g_sFileNameSymbolsMain );
//		else
//		{
//			if (! _tcslen( g_sFileNameSymbolsUser ))
//			{
//				return ConsoleDisplayError(TEXT("No user symbol file to reload."));
//			}
//			// load user symbols
//			_tcscat( sFileName, g_sFileNameSymbolsUser );
//		}
		_tcscat(sFileName, g_sFileNameSymbols[ iSymbolTable ]);
		nSymbols = ParseSymbolTable( sFileName, (SymbolTable_Index_e) iSymbolTable );
	}

	int iArg = 1;
	if (iArg <= nArgs)
	{
		TCHAR *pFileName = NULL;
		
		if( g_aArgs[ iArg ].bType & TYPE_QUOTED_2 )
		{
			pFileName = g_aArgs[ iArg ].sArg;

			_tcscpy(sFileName,g_sProgramDir);
			_tcscat(sFileName, pFileName);

			// Remember File Name of last symbols loaded
			_tcscpy( g_sFileNameSymbolsUser, pFileName );
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

		if( pFileName )
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
					sprintf( sText, " Updating %s%s%s from %s$%s%04X%s to %s$%s%04X%s"
						, CHC_SYMBOL, pSymbolName, CHC_DEFAULT
						, CHC_ARG_SEP					
						, CHC_ADDRESS, nAddressPrev, CHC_DEFAULT
						, CHC_ARG_SEP					
						, CHC_ADDRESS, nAddress, CHC_DEFAULT
					);
					ConsolePrint( sText );
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
			LPCTSTR pSymbol = FindSymbolFromAddress( nAddress, &iTable );
			{
				// Found another symbol for this address.  Harmless.
				// TODO: Probably should check if same name?
			}
#endif
			g_aSymbols[ eSymbolTable ][ nAddress ] = pSymbolName;
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
					wsprintf( sText, TEXT(" Cleared symbol table: %s"),
						g_aSymbolTableNames[ iTable ]
					 );
					ConsoleBufferPush( sText );
					iUpdate |= ConsoleUpdate();
					return iUpdate;
				}
				else
				{
					ConsoleBufferPush( TEXT(" Error: Unknown Symbol Table Type") );
					return ConsoleUpdate();
				}
//				if (bSymbolTable & SYMBOL_TABLE_MAIN)
//					return _CmdSymbolsClear( SYMBOLS_MAIN );
//				else
//				if (bSymbolsTable & SYMBOL_TABLE_USER)
//					return _CmdSymbolsClear( SYMBOLS_USER );
//				else
					// Shouldn't have multiple symbol tables selected
//					nArgs = _Arg_1( eSymbolsTable );
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
						wsprintf( sText, "  Symbol Table: %s, loaded symbols: %d",
						g_aSymbolTableNames[ iTable ], g_nSymbolsLoaded );
						ConsoleBufferPush( sText );
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
		}
		else
		{
			return _CmdSymbolsListTables( nArgs, bSymbolTables ); // bSymbolTables
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
	return _CmdSymbolsCommon( nArgs, SYMBOL_TABLE_MAIN );
}

//===========================================================================
Update_t CmdSymbolsSave (int nArgs)
{
	return UPDATE_CONSOLE_DISPLAY;
}
