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

/* Description: Debugger
 *
 * Author: Copyright (C) 2006-2010 Michael Pohoreski
 */

#include "StdAfx.h"


#define DEBUG_COLOR_CONSOLE 0


// Utility ________________________________________________________________________________________

/*
	String types:

	http://www.codeproject.com/cpp/unicode.asp

				TEXT()       _tcsrev
	_UNICODE    Unicode      _wcsrev
	_MBCS       Multi-byte   _mbsrev
	n/a         ASCII        strrev

*/

// tests if pSrc fits into pDst
// returns true if pSrc safely fits into pDst, else false (pSrc would of overflowed pDst)
//===========================================================================
bool TestStringCat ( TCHAR * pDst, LPCSTR pSrc, const int nDstSize )
{
	int nLenDst = _tcslen( pDst );
	int nLenSrc = _tcslen( pSrc );
	int nSpcDst = nDstSize - nLenDst;
	int nChars  = MIN( nLenSrc, nSpcDst );

	bool bOverflow = (nSpcDst <= nLenSrc); // 2.5.6.25 BUGFIX
	if (bOverflow)
	{
		return false;
	}
	return true;
}


// tests if pSrc fits into pDst
// returns true if pSrc safely fits into pDst, else false (pSrc would of overflowed pDst)
//===========================================================================
bool TryStringCat ( TCHAR * pDst, LPCSTR pSrc, const int nDstSize )
{
	int nLenDst = _tcslen( pDst );
	int nLenSrc = _tcslen( pSrc );
	int nSpcDst = nDstSize - nLenDst;
	int nChars  = MIN( nLenSrc, nSpcDst );

	bool bOverflow = (nSpcDst < nLenSrc);
	if (bOverflow)
	{
		return false;
	}
	
	_tcsncat( pDst, pSrc, nChars );
	return true;
}

// cats string as much as possible
// returns true if pSrc safely fits into pDst, else false (pSrc would of overflowed pDst)
//===========================================================================
int StringCat ( TCHAR * pDst, LPCSTR pSrc, const int nDstSize )
{
	int nLenDst = _tcslen( pDst );
	int nLenSrc = _tcslen( pSrc );
	int nSpcDst = nDstSize - nLenDst;
	int nChars  = MIN( nLenSrc, nSpcDst );

	_tcsncat( pDst, pSrc, nChars );

	bool bOverflow = (nSpcDst < nLenSrc);
	if (bOverflow)
		return 0;
		
	return nChars;
}



// Help ___________________________________________________________________________________________


//===========================================================================
Update_t HelpLastCommand()
{
	return Help_Arg_1( g_iCommand );
}


// Loads the arguments with the command to get help on and call display help.
//===========================================================================
Update_t Help_Arg_1( int iCommandHelp )
{
	_Arg_1( iCommandHelp );

	wsprintf( g_aArgs[ 1 ].sArg, g_aCommands[ iCommandHelp ].m_sName ); // .3 Fixed: Help_Arg_1() now copies command name into arg.name

	return CmdHelpSpecific( 1 );
}


//===========================================================================
void Help_Categories()
{
	const int nBuf = CONSOLE_WIDTH * 2;

	char sText[ nBuf ] = "";
	int  nLen = 0;

		// TODO/FIXME: Colorize( sText, ... )
		// Colorize("Usage:")
		nLen += StringCat( sText, CHC_USAGE , nBuf );
		nLen += StringCat( sText, "Usage", nBuf );

		nLen += StringCat( sText, CHC_DEFAULT, nBuf );
		nLen += StringCat( sText, ": " , nBuf );

		nLen += StringCat( sText, CHC_ARG_OPT, nBuf );
		nLen += StringCat( sText, "[ ", nBuf );

		nLen += StringCat( sText, CHC_ARG_MAND, nBuf );
		nLen += StringCat( sText, "< ", nBuf );


		for (int iCategory = _PARAM_HELPCATEGORIES_BEGIN ; iCategory < _PARAM_HELPCATEGORIES_END; iCategory++)
		{
			char *pName = g_aParameters[ iCategory ].m_sName; 

			if (nLen + strlen( pName ) >= (CONSOLE_WIDTH - 1))
			{
				ConsolePrint( sText );
				sText[ 0 ] = 0;
				nLen = StringCat( sText, "    ", nBuf ); // indent
			}

			        StringCat( sText, CHC_COMMAND, nBuf );
			nLen += StringCat( sText, pName      , nBuf );

			if (iCategory < (_PARAM_HELPCATEGORIES_END - 1))
			{
				char sSep[] = " | ";

				if (nLen + strlen( sSep ) >= (CONSOLE_WIDTH - 1))
				{
					ConsolePrint( sText );
					sText[ 0 ] = 0;
					nLen = StringCat( sText, "    ", nBuf ); // indent
				}
				        StringCat( sText, CHC_ARG_SEP, nBuf );
				nLen += StringCat( sText, sSep, nBuf );
			}
		}
		StringCat( sText, CHC_ARG_MAND, nBuf );
		StringCat( sText, " >", nBuf);

		StringCat( sText, CHC_ARG_OPT, nBuf );
		StringCat( sText, " ]", nBuf);

//		ConsoleBufferPush( sText );
		ConsolePrint( sText );  // Transcode colored text to native console color text
		
		sprintf( sText, "%sNotes%s: %s<>%s = mandatory, %s[]%s = optional, %s|%s argument option"
			, CHC_USAGE
			, CHC_DEFAULT
			, CHC_ARG_MAND
			, CHC_DEFAULT
			, CHC_ARG_OPT
			, CHC_DEFAULT
			, CHC_ARG_SEP
			, CHC_DEFAULT
		);
		ConsolePrint( sText );  // Transcode colored text to native console color text
//		ConsoleBufferPush( sText );
}

void Help_Examples()
{
	char sText[ CONSOLE_WIDTH ];
	sprintf( sText, " %sExamples%s:"
		, CHC_USAGE
		, CHC_DEFAULT
	);
	ConsolePrint( sText );
}

//===========================================================================
void Help_Range()
{
	ConsoleBufferPush( "  Where <range> is of the form:"                 );
	ConsoleBufferPush( "    address , length   [address,address+length)" );
	ConsoleBufferPush( "    address : end      [address,end]"            );
}

//===========================================================================
void Help_Operators()
{
	char sText[ CONSOLE_WIDTH ];
	
//	sprintf( sText," %sOperators%s:"                                 , CHC_USAGE, CHC_DEFAULT ); ConsolePrint( sText );
//	sprintf( sText,"  Operators: (Math)"                             );
	sprintf( sText,"  Operators: (%sMath%s)"                         , CHC_USAGE, CHC_DEFAULT ); ConsolePrint( sText );
	sprintf( sText,"    %s+%s   Addition"                            , CHC_USAGE, CHC_DEFAULT ); ConsolePrint( sText );
	sprintf( sText,"    %s-%s   Subtraction"                         , CHC_USAGE, CHC_DEFAULT ); ConsolePrint( sText );
	sprintf( sText,"    %s*%s   Multiplication"                      , CHC_USAGE, CHC_DEFAULT ); ConsolePrint( sText );
	sprintf( sText,"    %s/%s   Division"                            , CHC_USAGE, CHC_DEFAULT ); ConsolePrint( sText );
	sprintf( sText,"    %s%%%s   Modulas or Remainder"               , CHC_USAGE, CHC_DEFAULT ); ConsolePrint( sText );
//ConsoleBufferPush( "  Operators: (Bit Wise)"                         );
	sprintf( sText,"  Operators: (%sBit Wise%s)"                     , CHC_USAGE, CHC_DEFAULT ); ConsolePrint( sText );
	sprintf( sText,"    %s&%s   Bit-wise and (AND)"                  , CHC_USAGE, CHC_DEFAULT ); ConsolePrint( sText );
	sprintf( sText,"    %s|%s   Bit-wise or  (OR )"                  , CHC_USAGE, CHC_DEFAULT ); ConsolePrint( sText );
	sprintf( sText,"    %s^%s   Bit-wise exclusive-or (EOR/XOR)"     , CHC_USAGE, CHC_DEFAULT ); ConsolePrint( sText );
	sprintf( sText,"    %s!%s   Bit-wise negation (NOT)"             , CHC_USAGE, CHC_DEFAULT ); ConsolePrint( sText );
//ConsoleBufferPush( "  Operators: (Input)"                            );
	sprintf( sText,"  Operators: (%sInput%s)"                        , CHC_USAGE, CHC_DEFAULT ); ConsolePrint( sText );
	sprintf( sText,"    %s@%s   next number refers to search results", CHC_USAGE, CHC_DEFAULT ); ConsolePrint( sText );
	sprintf( sText,"    %s\"%s   Designate string in ASCII format"   , CHC_USAGE, CHC_DEFAULT ); ConsolePrint( sText );
	sprintf( sText,"    %s\'%s   Desginate string in High-Bit apple format", CHC_USAGE, CHC_DEFAULT ); ConsolePrint( sText );
	sprintf( sText,"    %s$%s   Designate number/symbol"             , CHC_USAGE, CHC_DEFAULT ); ConsolePrint( sText );
	sprintf( sText,"    %s#%s   Designate number in hex"             , CHC_USAGE, CHC_DEFAULT ); ConsolePrint( sText );
//ConsoleBufferPush( "  Operators: (Range)"                            );
	sprintf( sText,"  Operators: (%sRange%s)"                        , CHC_USAGE, CHC_DEFAULT ); ConsolePrint( sText );
	sprintf( sText,"    %s,%s   range seperator (2nd address is relative)", CHC_USAGE, CHC_DEFAULT ); ConsolePrint( sText );
	sprintf( sText,"    %s:%s   range seperator (2nd address is absolute)", CHC_USAGE, CHC_DEFAULT ); ConsolePrint( sText );
//	sprintf( sText,"  Operators: (Misc)"                             );
	sprintf( sText,"  Operators: (%sMisc%s)"                         , CHC_USAGE, CHC_DEFAULT ); ConsolePrint( sText );
	sprintf( sText,"    %s//%s  comment until end of line"           , CHC_USAGE, CHC_DEFAULT ); ConsolePrint( sText );
//ConsoleBufferPush( "  Operators: (Breakpoint)"                       );
	sprintf( sText,"  Operators: (%sBreakpoint%s)"                   , CHC_USAGE, CHC_DEFAULT ); ConsolePrint( sText );

	_tcscpy( sText, "    " );
	_tcscat( sText, CHC_USAGE );
	int iBreakOp = 0;
	for( iBreakOp = 0; iBreakOp < NUM_BREAKPOINT_OPERATORS; iBreakOp++ )
	{
		if ((iBreakOp >= PARAM_BP_LESS_EQUAL) &&
			(iBreakOp <= PARAM_BP_GREATER_EQUAL))
		{
			_tcscat( sText, g_aBreakpointSymbols[ iBreakOp ] );
			_tcscat( sText, " " );
		}
	}	
	_tcscat( sText, CHC_DEFAULT );
	ConsolePrint( sText );
}

void Help_KeyboardShortcuts()
{
	ConsoleBufferPush("  Scrolling:"                                         );
	ConsoleBufferPush("    Up Arrow"                                         );
	ConsoleBufferPush("    Down Arrow"                                       );
	ConsoleBufferPush("    Shift + Up Arrow"                                 );
	ConsoleBufferPush("    Shift + Down Arrow"                               );
	ConsoleBufferPush("    Page Up"                                          );
	ConsoleBufferPush("    Page Down"                                        );
	ConsoleBufferPush("    Shift + Page Up"                                  );
	ConsoleBufferPush("    Shift + Page Down"                                );
	
	ConsoleBufferPush("  Bookmarks:"                                         );
	ConsoleBufferPush("    Ctrl-Shift-#"                                     );
	ConsoleBufferPush("    Ctrl-#      "                                     );
}


void _ColorizeHeader(
	char * & pDst,const char * & pSrc,
	const char * pHeader, const int nHeaderLen )
{
	int nLen;
	
	nLen = strlen( CHC_USAGE );
	strcpy( pDst, CHC_USAGE );
	pDst += nLen;

	nLen = nHeaderLen - 1;
	strncpy( pDst, pHeader, nLen );
	pDst += nLen;

	pSrc += nHeaderLen;

	nLen = strlen( CHC_ARG_SEP );
	strcpy( pDst, CHC_ARG_SEP );
	pDst += nLen;

	*pDst = ':';
	pDst++;

	nLen = strlen( CHC_DEFAULT );
	strcpy( pDst, CHC_DEFAULT );
	pDst += nLen;
}

void _ColorizeOperator(
	char * & pDst, const char * & pSrc,
	char * pOperator )
{
	int nLen;
	
	nLen = strlen( pOperator );
	strcpy( pDst, pOperator );
	pDst += nLen;

	*pDst = *pSrc;
	pDst++;

	nLen = strlen( CHC_DEFAULT );
	strcpy( pDst, CHC_DEFAULT );
	pDst += nLen;

	pSrc++;
}


bool Colorize( char * pDst, const char * pSrc )
{
	if (! pSrc)
		return false;
		
	if (! pDst)
		return false;

	const char sNote [] = "Note:";
	const int  nNote    = sizeof( sNote ) - 1;

	const char sSeeAlso[] = "See also:";
	const char nSeeAlso   = sizeof( sSeeAlso ) - 1;

	const char sUsage[] = "Usage:";
	const int  nUsage   = sizeof( sUsage ) - 1;

	const char sTotal[] = "Total:";
	const int  nTotal = sizeof( sTotal ) - 1;

	int nLen = 0;
	while (*pSrc)
	{
		if (strncmp( sUsage, pSrc, nUsage) == 0)
		{
			_ColorizeHeader( pDst, pSrc, sUsage, nUsage );
		}
		else
		if (strncmp( sSeeAlso, pSrc, nSeeAlso) == 0)
		{
			_ColorizeHeader( pDst, pSrc, sSeeAlso, nSeeAlso );
		}
		else
		if (strncmp( sNote, pSrc, nNote) == 0)
		{
			_ColorizeHeader( pDst, pSrc, sNote, nNote );
		}
		else
		if (strncmp( sTotal, pSrc, nNote) == 0)
		{
			_ColorizeHeader( pDst, pSrc, sTotal, nTotal );
		}
		else
		if (*pSrc == '[')
		{
			_ColorizeOperator( pDst, pSrc, CHC_ARG_OPT );
		}
		else
		if (*pSrc == ']')
		{
			_ColorizeOperator( pDst, pSrc, CHC_ARG_OPT );
		}
		else
		if (*pSrc == '<')
		{
			_ColorizeOperator( pDst, pSrc, CHC_ARG_MAND );
		}
		else
		if (*pSrc == '>')
		{
			_ColorizeOperator( pDst, pSrc, CHC_ARG_MAND );
		}
		else
		if (*pSrc == '|')
		{
			_ColorizeOperator( pDst, pSrc, CHC_ARG_SEP );
		}
		else
		if (*pSrc == '\'')
		{
			_ColorizeOperator( pDst, pSrc, CHC_ARG_SEP );
		}
		else
		{
			*pDst = *pSrc;
			pDst++;
			pSrc++;	
		}
	}
	*pDst = 0;
	return true;
}

//===========================================================================
Update_t CmdMOTD( int nArgs )
{
	char sText[ CONSOLE_WIDTH*2 ];
	char sTemp[ CONSOLE_WIDTH*2 ];

#if DEBUG_COLOR_CONSOLE
	ConsolePrint( "`" );
	ConsolePrint( "`A" );
	ConsolePrint( "`2`A" );
#endif

	sprintf( sText, "`9`A`7 Apple `9][ ][+ //e `7Emulator for Windows (TM) `9`@" );
	ConsolePrint( sText );

	CmdVersion(0);
	CmdSymbols(0);
	sprintf( sTemp, "  '%sCtrl ~'%s console, '%s%s'%s (specific), '%s%s'%s (all)"
		, CHC_KEY
		, CHC_DEFAULT
		, CHC_COMMAND
		, g_aCommands[ CMD_HELP_SPECIFIC ].m_sName
		, CHC_DEFAULT
//		, g_aCommands[ CMD_HELP_SPECIFIC ].pHelpSummary
		, CHC_COMMAND
		, g_aCommands[ CMD_HELP_LIST     ].m_sName
		, CHC_DEFAULT
//		, g_aCommands[ CMD_HELP_LIST     ].pHelpSummary
	);
	Colorize( sText, sTemp );
	ConsolePrint( sText );

	ConsoleUpdate();

	return UPDATE_ALL;
}


// Help on specific command
//===========================================================================
Update_t CmdHelpSpecific (int nArgs)
{
	int iArg;
	char sText[ CONSOLE_WIDTH * 2 ];
	char sTemp[ CONSOLE_WIDTH * 2 ];
	ZeroMemory( sText, CONSOLE_WIDTH*2 );
	ZeroMemory( sTemp, CONSOLE_WIDTH*2 );

	if (! nArgs)
	{
		Help_Categories();
		return ConsoleUpdate();
	}

	CmdFuncPtr_t pFunction = NULL;
	bool bAllCommands = false;
	bool bCategory = false;
	bool bDisplayCategory = true;

	if ((! _tcscmp( g_aArgs[1].sArg, g_aParameters[ PARAM_WILDSTAR ].m_sName)) ||
		(! _tcscmp( g_aArgs[1].sArg, g_aParameters[ PARAM_MEM_SEARCH_WILD ].m_sName)) )
	{
		bAllCommands = true;
		nArgs = NUM_COMMANDS;
	}

	// If Help on category, push command name as arg
	// Mame has categories:
	//	General, Memory, Execution, Breakpoints, Watchpoints, Expressions, Comments
	int iParam = 0;

	int nNewArgs  = 0;
	int iCmdBegin = 0;
	int iCmdEnd   = 0;
	int nFound    = 0;
	int iCommand  = 0;

	if (! bAllCommands)
	{
		for (iArg = 1; iArg <= nArgs; iArg++ )
		{
	//		int nFoundCategory = FindParam( g_aArgs[ iArg ].sArg, MATCH_EXACT, iParam, _PARAM_HELPCATEGORIES_BEGIN, _PARAM_HELPCATEGORIES_END );
			int nFoundCategory = FindParam( g_aArgs[ iArg ].sArg, MATCH_FUZZY, iParam, _PARAM_HELPCATEGORIES_BEGIN, _PARAM_HELPCATEGORIES_END );
			if( nFoundCategory )
				bCategory = true;
			else
				bCategory = false;
			switch( iParam )
			{
				case PARAM_CAT_BOOKMARKS  : iCmdBegin = CMD_BOOKMARK        ; iCmdEnd = CMD_BOOKMARK_SAVE        ; break;
				case PARAM_CAT_BREAKPOINTS: iCmdBegin = CMD_BREAK_INVALID   ; iCmdEnd = CMD_BREAKPOINT_SAVE      ; break;
				case PARAM_CAT_CONFIG     : iCmdBegin = CMD_BENCHMARK       ; iCmdEnd = CMD_CONFIG_SAVE          ; break;
				case PARAM_CAT_CPU        : iCmdBegin = CMD_ASSEMBLE        ; iCmdEnd = CMD_UNASSEMBLE           ; break;
				case PARAM_CAT_FLAGS      :
					nFound = FindCommand( g_aArgs[iArg].sArg, pFunction, & iCommand ); // check if we have an exact command match first
					if ( nFound ) // && (iCommand != CMD_MEMORY_FILL))
						bCategory = false; 
					else if ( nFoundCategory )
					{
						iCmdBegin = CMD_FLAG_CLEAR     ; iCmdEnd = CMD_FLAG_SET_N;
					}
					break;
				case PARAM_CAT_HELP       : iCmdBegin = CMD_HELP_LIST       ; iCmdEnd = CMD_MOTD                 ; break;
				case PARAM_CAT_KEYBOARD   :
					nFound = FindCommand( g_aArgs[iArg].sArg, pFunction, & iCommand ); // check if we have an exact command match first
					if ((!nFound) || (iCommand != CMD_INPUT_KEY))
					{
						nArgs = 0;
						Help_KeyboardShortcuts();
					}
					bCategory = false;
					break;
				case PARAM_CAT_MEMORY     :
					nFound = FindCommand( g_aArgs[iArg].sArg, pFunction, & iCommand );  // check if we have an exact command match first
					if ( nFound )// && (iCommand != CMD_MEMORY_MOVE))
						bCategory = false;
					else if ( nFoundCategory )
					{
						iCmdBegin = CMD_MEMORY_COMPARE                      ; iCmdEnd = CMD_MEMORY_FILL           ;
					}
					break;
				case PARAM_CAT_OUTPUT     :
					nFound = FindCommand( g_aArgs[iArg].sArg, pFunction, & iCommand );  // check if we have an exact command match first
					if ( nFound ) // && (iCommand != CMD_OUT))
						bCategory = false; 
					else if ( nFoundCategory )
					{
						iCmdBegin = CMD_OUTPUT_CALC                         ; iCmdEnd = CMD_OUTPUT_RUN           ;
					}
					break;
				case PARAM_CAT_SYMBOLS    :
					nFound = FindCommand( g_aArgs[iArg].sArg, pFunction, & iCommand );  // check if we have an exact command match first
					if ( nFound ) // && (iCommand != CMD_SYMBOLS_LOOKUP) && (iCommand != CMD_MEMORY_SEARCH))
						bCategory = false; 
					else if ( nFoundCategory )
					{
						iCmdBegin = CMD_SYMBOLS_LOOKUP                      ; iCmdEnd = CMD_SYMBOLS_LIST         ;
					}
					break;
				case PARAM_CAT_VIEW       :
					{
						iCmdBegin = CMD_VIEW_TEXT4X                         ; iCmdEnd = CMD_VIEW_DHGR2          ;
					}
					break;
				case PARAM_CAT_WATCHES    :
					nFound = FindCommand( g_aArgs[iArg].sArg, pFunction, & iCommand );  // check if we have an exact command match first
					if (nFound) {
						bCategory = false; 
					} else  // 2.7.0.17: HELP <category> wasn't displaying when category was one of: FLAGS, OUTPUT, WATCHES
						if( nFoundCategory )
						{
							iCmdBegin = CMD_WATCH_ADD       ; iCmdEnd = CMD_WATCH_LIST           ;
						}
					break;
				case PARAM_CAT_WINDOW     : iCmdBegin = CMD_WINDOW          ; iCmdEnd = CMD_WINDOW_OUTPUT        ; break;
				case PARAM_CAT_ZEROPAGE   : iCmdBegin = CMD_ZEROPAGE_POINTER; iCmdEnd = CMD_ZEROPAGE_POINTER_SAVE; break;
//				case PARAM_CAT_EXPRESSION : // fall-through
				case PARAM_CAT_OPERATORS  : nArgs = 0; Help_Operators(); break;
				case PARAM_CAT_RANGE      :
					// HACK: check if we have an exact command match first
					nFound = FindCommand( g_aArgs[iArg].sArg, pFunction, & iCommand );
					if ((!nFound) || (iCommand != CMD_REGISTER_SET))
					{
						nArgs = 0;
						Help_Range();
					}
					bCategory = false;
					break;
				default:
					bCategory = false;
					break;
			}
			if (iCmdEnd)
				iCmdEnd++;
			nNewArgs = (iCmdEnd - iCmdBegin);
			if (nNewArgs > 0)
				break;
		}
	}

	if (nNewArgs > 0)
	{
		nArgs = nNewArgs;
		for (iArg = 1; iArg <= nArgs; iArg++ )
		{
#if DEBUG_VAL_2
			g_aArgs[ iArg ].nVal2 = iCmdBegin + iArg - 1;
#endif
			g_aArgs[ iArg ].nValue = iCmdBegin + iArg - 1;
		}
	}

	for (iArg = 1; iArg <= nArgs; iArg++ )
	{	
		iCommand = 0;
		nFound = 0;

		if (bCategory)
		{
#if DEBUG_VAL_2
			iCommand = g_aArgs[iArg].nVal2;
#endif
			iCommand = g_aArgs[ iArg  ].nValue;
			nFound = 1;
		}
		else
		if (bAllCommands)
		{
			iCommand = iArg;
			if (iCommand == NUM_COMMANDS) // skip: Internal Consistency Check __COMMANDS_VERIFY_TXT__
				continue;
			nFound = 1;
		}
		else
			nFound = FindCommand( g_aArgs[iArg].sArg, pFunction, & iCommand );

		if (nFound > 1)
		{
			DisplayAmbigiousCommands( nFound );
		}

		if (iCommand > NUM_COMMANDS)
			continue;

		if ((nArgs == 1) && (! nFound))
			iCommand = g_aArgs[iArg].nValue;

		Command_t *pCommand = & g_aCommands[ iCommand ];

		if (! nFound)
		{
			iCommand = NUM_COMMANDS;
			pCommand = NULL;
		}
		
//		if (nFound && (! bAllCommands) && (! bCategory))
		if (nFound && (! bAllCommands) && bDisplayCategory)
		{
			char sCategory[ CONSOLE_WIDTH ];
			int iCmd = g_aCommands[ iCommand ].iCommand; // Unaliased command

			// HACK: Major kludge to display category!!!
			if (iCmd <= CMD_UNASSEMBLE)
				wsprintf( sCategory, g_aParameters[ PARAM_CAT_CPU ].m_sName );
			else
			if (iCmd <= CMD_BOOKMARK_SAVE)
				wsprintf( sCategory, g_aParameters[ PARAM_CAT_BOOKMARKS ].m_sName );
			else
			if (iCmd <= CMD_BREAKPOINT_SAVE)
				wsprintf( sCategory, g_aParameters[ PARAM_CAT_BREAKPOINTS ].m_sName );
			else
			if (iCmd <= CMD_CONFIG_SAVE)
				wsprintf( sCategory, g_aParameters[ PARAM_CAT_CONFIG ].m_sName );
			else
			if (iCmd <= CMD_CURSOR_PAGE_DOWN_4K)
				wsprintf( sCategory, "Scrolling" );
			else
			if (iCmd <= CMD_FLAG_SET_N)
				wsprintf( sCategory, g_aParameters[ PARAM_CAT_FLAGS ].m_sName );
			else
			if (iCmd <= CMD_MOTD)
				wsprintf( sCategory, g_aParameters[ PARAM_CAT_HELP ].m_sName );
			else
			if (iCmd <= CMD_MEMORY_FILL)
				wsprintf( sCategory, g_aParameters[ PARAM_CAT_MEMORY ].m_sName );
			else
			if (iCmd <= CMD_OUTPUT_RUN)
				wsprintf( sCategory, g_aParameters[ PARAM_CAT_OUTPUT ].m_sName );
			else
			if (iCmd <= CMD_SYNC)
				wsprintf( sCategory, "Source" );
			else
			if (iCmd <= CMD_SYMBOLS_LIST)
				wsprintf( sCategory, g_aParameters[ PARAM_CAT_SYMBOLS ].m_sName );
			else
			if (iCmd <= CMD_VIEW_DHGR2)
				wsprintf( sCategory, g_aParameters[ PARAM_CAT_VIEW ].m_sName );
			else
			if (iCmd <= CMD_WATCH_SAVE)
				wsprintf( sCategory, g_aParameters[ PARAM_CAT_WATCHES ].m_sName );
			else
			if (iCmd <= CMD_WINDOW_OUTPUT)
				wsprintf( sCategory, g_aParameters[ PARAM_CAT_WINDOW ].m_sName );
			else
			if (iCmd <= CMD_ZEROPAGE_POINTER_SAVE)
				wsprintf( sCategory, g_aParameters[ PARAM_CAT_ZEROPAGE ].m_sName );
			else
				wsprintf( sCategory, "Unknown!" );

			sprintf( sText, "%sCategory%s: %s%s"
				, CHC_USAGE
				, CHC_DEFAULT
				, CHC_CATEGORY
				, sCategory );
			ConsolePrint( sText );

			if (bCategory)
				if (bDisplayCategory)
					bDisplayCategory = false;
		}
		
		if (pCommand)
		{
			char *pHelp = pCommand->pHelpSummary;
			if (pHelp)
			{
				if (bCategory)
					sprintf( sText, "%s%8s%s, "
						, CHC_COMMAND
						, pCommand->m_sName
						, CHC_ARG_SEP
					);
				else
					sprintf( sText, "%s%s%s, "
						, CHC_COMMAND
						, pCommand->m_sName
						, CHC_ARG_SEP
					);

//				if (! TryStringCat( sText, pHelp, g_nConsoleDisplayWidth ))
//				{
//					if (! TryStringCat( sText, pHelp, CONSOLE_WIDTH-1 ))
//					{
						strncat( sText, CHC_DEFAULT, CONSOLE_WIDTH );
						strncat( sText, pHelp      , CONSOLE_WIDTH );
//						ConsoleBufferPush( sText );
//					}
//				}
				ConsolePrint( sText );
			}
			else
			{
#if _DEBUG
				wsprintf( sText, "%s  <-- Missing", pCommand->m_sName );
				ConsoleBufferPush( sText );
	#if DEBUG_COMMAND_HELP
				if (! bAllCommands) // Release version doesn't display message
				{			
					wsprintf( sText, "Missing Summary Help: %s", g_aCommands[ iCommand ].aName );
					ConsoleBufferPush( sText );
				}
	#endif
#endif
			}

			if (bCategory)
				continue;
		}		

		// MASTER HELP
		switch (iCommand)
		{	
	// CPU / General
		case CMD_ASSEMBLE:
			ConsoleBufferPush( " Built-in assember isn't functional yet." );
			break;
		case CMD_UNASSEMBLE:
			Colorize( sText, " Usage: [address | symbol]" );
			ConsolePrint( sText );
			ConsoleBufferPush( "  Disassembles memory." );
			break;
		case CMD_GO:
			Colorize( sText, " Usage: address | symbol [Skip,Length]" );
			ConsolePrint( sText );
			Colorize( sText, " Usage: address | symbol [Start:End]" );
			ConsolePrint( sText );
			ConsoleBufferPush( " Skip  : Start address to skip stepping"                     );
			ConsoleBufferPush( " Length: Range of bytes past start address to skip stepping" );
			ConsoleBufferPush( " End   : Inclusive end address to skip stepping"             );
			ConsoleBufferPush( "  If the Program Counter is outside the skip range, resumes single-stepping." );
			ConsoleBufferPush( "  Can be used to skip ROM/OS/user code." );
			Help_Examples();
			sprintf( sText, "%s  G C600 FA00,600" , CHC_EXAMPLE ); ConsolePrint( sText );
			sprintf( sText, "%s  G C600 F000:FFFF", CHC_EXAMPLE ); ConsolePrint( sText );
			break;
		case CMD_JSR:
			Colorize( sText, " %sUsage%s: %s[symbol | address]" );
			ConsolePrint( sText );
			ConsoleBufferPush( "  Pushes PC on stack; calls the named subroutine." );
			break;
		case CMD_NOP:
			ConsoleBufferPush( TEXT("  Puts a NOP opcode at current instruction") );
			break;
		case CMD_OUT:
			Colorize( sText, " Usage: [address8 | address16 | symbol] ## [##]" );
			ConsolePrint( sText );
			ConsoleBufferPush( TEXT("  Ouput a byte or word to the IO address $C0xx" ) );
			break;
		case CMD_PROFILE:
			sprintf( sTemp, " Usage: [%s | %s | %s]"
				, g_aParameters[ PARAM_RESET ].m_sName
				, g_aParameters[ PARAM_SAVE  ].m_sName
				, g_aParameters[ PARAM_LIST  ].m_sName
			);
			Colorize( sText, sTemp );
			ConsolePrint( sText );
			ConsoleBufferPush( " No arguments resets the profile." );
			break;
	// Registers
		case CMD_REGISTER_SET:
			Colorize( sText,    " Usage: <reg> <value | expression | symbol>" );
			ConsolePrint( sText );
			ConsoleBufferPush( "  Where <reg> is one of: A X Y PC SP " );
			sprintf( sTemp,    " See also: %s%s"
				, CHC_CATEGORY
				, g_aParameters[ PARAM_CAT_OPERATORS ].m_sName );
			Colorize( sText, sTemp );
			ConsolePrint( sText );
			Help_Examples();
			sprintf( sText, "%s  R PC RESET + 1", CHC_EXAMPLE ); ConsolePrint( sText );
			sprintf( sText, "%s  R PC $FC58"    , CHC_EXAMPLE ); ConsolePrint( sText );
			sprintf( sText, "%s  R A  A1"       , CHC_EXAMPLE ); ConsolePrint( sText );
			sprintf( sText, "%s  R A  $A1"      , CHC_EXAMPLE ); ConsolePrint( sText );
			sprintf( sText, "%s  R A  #A1"      , CHC_EXAMPLE ); ConsolePrint( sText );
			break;
		case CMD_SOURCE:
//			ConsoleBufferPush( TEXT(" Reads assembler source file." ) );
			sprintf( sTemp, " Usage: [ %s | %s ] \"filename\""             , g_aParameters[ PARAM_SRC_MEMORY  ].m_sName, g_aParameters[ PARAM_SRC_SYMBOLS ].m_sName );
			Colorize( sText, sTemp );
			ConsolePrint( sText );
			sprintf( sText, "   %s: read source bytes into memory."        , g_aParameters[ PARAM_SRC_MEMORY  ].m_sName ); ConsoleBufferPush( sText );
			sprintf( sText, "   %s: read symbols into Source symbol table.", g_aParameters[ PARAM_SRC_SYMBOLS ].m_sName ); ConsoleBufferPush( sText );
			sprintf( sText, " Supports: %s."                               , g_aParameters[ PARAM_SRC_MERLIN  ].m_sName ); ConsoleBufferPush( sText );
			break;
		case CMD_STEP_OUT: 
			ConsoleBufferPush( "  Steps out of current subroutine" );
			ConsoleBufferPush( "  Hotkey: Ctrl-Space" ); // TODO: FIXME
			break;
		case CMD_STEP_OVER: // Bad name? FIXME/TODO: do we need to rename?
			Colorize( sText, " Usage: [#]" );
			ConsolePrint( sText );
			ConsoleBufferPush( "  Steps, # times, thru current instruction" );
			ConsoleBufferPush( "  JSR will be stepped into AND out of." );
			ConsoleBufferPush( "  Hotkey: Ctrl-Space" ); // TODO: FIXME
			break;
		case CMD_TRACE:
			Colorize( sText, " Usage: [#]" ); ConsolePrint( sText );
			ConsoleBufferPush( "  Traces, # times, current instruction(s)" );
			ConsoleBufferPush( "  JSR will be stepped into" );
			ConsoleBufferPush( "  Hotkey: Shift-Space" );
		case CMD_TRACE_FILE:
			Colorize( sText, " Usage: \"[filename]\"" );
			ConsolePrint( sText );
			break;
		case CMD_TRACE_LINE:
			Colorize( sText, " Usage: [#]" );
			ConsolePrint( sText );
			ConsoleBufferPush( "  Traces into current instruction" );
			ConsoleBufferPush( "  with cycle counting." );
			break;
	// Bookmarks
		case CMD_BOOKMARK:
		case CMD_BOOKMARK_ADD:
			Colorize( sText, " Usage: [address | symbol]"   ); ConsolePrint( sText );
			Colorize( sText, " Usage: # <address | symbol>" ); ConsolePrint( sText );
			ConsoleBufferPush("  If no address or symbol is specified, lists the current bookmarks." );
			ConsoleBufferPush("  Updates the specified bookmark (#)" );
			Help_Examples();
			sprintf( sText,  "%s   %s RESET ", CHC_EXAMPLE, pCommand->m_sName ); ConsolePrint( sText );
			sprintf( sText,  "%s   %s 1 HOME", CHC_EXAMPLE, pCommand->m_sName ); ConsolePrint( sText );
			break;
		case CMD_BOOKMARK_CLEAR:
			Colorize( sText, " Usage: [# | *]" ); ConsolePrint( sText );
			ConsoleBufferPush( "  Clears specified bookmark, or all." );
			Help_Examples();
			sprintf( sText,  "%s   %s 1", CHC_EXAMPLE, pCommand->m_sName );
			ConsolePrint( sText );
			break;
		case CMD_BOOKMARK_LIST:
//		case CMD_BOOKMARK_LOAD:
		case CMD_BOOKMARK_SAVE:
			break;
	// Breakpoints
		case CMD_BREAK_INVALID:
			sprintf( sTemp, TEXT(" Usage: [%s%s | %s%s] | [ # | # %s%s | # %s%s ]")
				, CHC_COMMAND
				, g_aParameters[ PARAM_ON  ].m_sName
				, CHC_COMMAND
				, g_aParameters[ PARAM_OFF  ].m_sName
				, CHC_COMMAND
				, g_aParameters[ PARAM_ON  ].m_sName
				, CHC_COMMAND
				, g_aParameters[ PARAM_OFF  ].m_sName
			);
			Colorize( sText, sTemp );
			ConsolePrint( sText );
			sprintf( sTemp, TEXT("Where: # is 0=BRK, 1=Invalid Opcode_1, 2=Invalid Opcode_2, 3=Invalid Opcode_3"));
			Colorize( sText, sTemp );
			ConsolePrint( sText );
			break;
//		case CMD_BREAK_OPCODE:
		case CMD_BREAKPOINT:
			sprintf( sTemp, TEXT(" Usage: [%s%s | %s%s | %s%s]")
				, CHC_COMMAND
				, g_aParameters[ PARAM_LOAD  ].m_sName
				, CHC_COMMAND
				, g_aParameters[ PARAM_SAVE  ].m_sName
				, CHC_COMMAND
				, g_aParameters[ PARAM_RESET ].m_sName );
			Colorize( sText, sTemp );
			ConsolePrint( sText );
			sprintf( sText, " Maximum breakpoints: %s%d", CHC_NUM_DEC, MAX_BREAKPOINTS );
			ConsolePrint( sText );
			ConsoleBufferPush( "  Set breakpoint at PC if no args."    );
			ConsoleBufferPush( "  Loading/Saving not yet implemented." );
			break;
		case CMD_BREAKPOINT_ADD_REG:
			Colorize( sText, " Usage: [A|X|Y|PC|S] [op] <range | value>" );
			ConsolePrint( sText );
			ConsoleBufferPush( "  Set breakpoint when reg is [op] value"   );
			ConsoleBufferPush( "  Default operator is '='"                 );
			sprintf(  sTemp, " See also: %s%s"
				, CHC_CATEGORY
				, g_aParameters[ PARAM_CAT_OPERATORS ].m_sName );
			Colorize( sText, sTemp );
			ConsolePrint( sText );
			// ConsoleBufferPush( " Examples:"     );
			Help_Examples();
			sprintf( sText, "%s   %s PC < D000"                    , CHC_EXAMPLE, pCommand->m_sName ); ConsolePrint( sText );
			sprintf( sText, "%s   %s PC = F000:FFFF PC < D000,1000", CHC_EXAMPLE, pCommand->m_sName ); ConsolePrint( sText );
			sprintf( sText, "%s   %s A <= D5"                      , CHC_EXAMPLE, pCommand->m_sName ); ConsolePrint( sText );
			sprintf( sText, "%s   %s A != 01:FF"                   , CHC_EXAMPLE, pCommand->m_sName ); ConsolePrint( sText );
			sprintf( sText, "%s   %s X  = A5"                      , CHC_EXAMPLE, pCommand->m_sName ); ConsolePrint( sText );
			break;
		case CMD_BREAKPOINT_ADD_SMART:
			Colorize( sText, " Usage: [address | register]" );
			ConsolePrint( sText );
			ConsoleBufferPush( "  If address, sets two breakpoints" );
			ConsoleBufferPush( "    1. one memory access at address" );
			ConsoleBufferPush( "    2. if PC reaches address" );
//			"Sets a breakpoint at the current PC or specified address." );
			ConsoleBufferPush( "  If an IO address, sets breakpoint on IO access." );
			ConsoleBufferPush( "  If register, sets a breakpoint on memory access at address of register." );
			break;
		case CMD_BREAKPOINT_ADD_PC:
			Colorize( sText, " Usage: [address]" );
			ConsolePrint( sText );
			ConsoleBufferPush( "  Sets a breakpoint at the current PC or at the specified address." );
			break;
		case CMD_BREAKPOINT_CLEAR:
			Colorize( sText, " Usage: [# | *]" );
			ConsolePrint( sText );
			ConsoleBufferPush( "  Clears specified breakpoint, or all." );
			Help_Examples();
			sprintf( sText, "%s   %s 1", CHC_EXAMPLE, pCommand->m_sName );
			ConsolePrint( sText );
			break;
		case CMD_BREAKPOINT_DISABLE:
			Colorize( sText, " Usage: [# [,#] | *]" );
			ConsolePrint( sText );
			ConsoleBufferPush( "  Disable breakpoint previously set, or all." );
			Help_Examples();
			sprintf( sText,  "%s   %s 1", CHC_EXAMPLE, pCommand->m_sName );
			ConsolePrint( sText );
			break;
		case CMD_BREAKPOINT_ENABLE:
			Colorize( sText, " Usage: [# [,#] | *]" );
			ConsolePrint( sText );
			ConsoleBufferPush( "  Re-enables breakpoint previously set, or all." );
			Help_Examples();
			sprintf( sText,  "%s   %s 1", CHC_EXAMPLE, pCommand->m_sName );
			ConsolePrint( sText );
			break;
		case CMD_BREAKPOINT_LIST:
			break;
	// Config - Load / Save
		case CMD_CONFIG_LOAD:
			Colorize( sText, " Usage: [\"filename\"]" );
			ConsolePrint( sText );
			sprintf( sText, "  Load debugger configuration from '%s', or the specificed file.", g_sFileNameConfig ); ConsoleBufferPush( sText );
			break;
		case CMD_CONFIG_SAVE:
			Colorize( sText, " Usage: [\"filename\"]" );
			ConsolePrint( sText );
			sprintf( sText, "  Save debugger configuration to '%s', or the specificed file.", g_sFileNameConfig ); ConsoleBufferPush( sText );
			break;
	// Config - Color
		case CMD_CONFIG_COLOR:
			ConsoleBufferPush( TEXT(" Usage: [<#> | <# RR GG BB>]" ) );
			ConsoleBufferPush( TEXT("  0 params: switch to 'color' scheme" ) );
			ConsoleBufferPush( TEXT("  1 param : dumps R G B for scheme 'color'") );
			ConsoleBufferPush( TEXT("  4 params: sets  R G B for scheme 'color'" ) );
			break;
		case CMD_CONFIG_MONOCHROME:
			ConsoleBufferPush( TEXT(" Usage: [<#> | <# RR GG BB>]" ) );
			ConsoleBufferPush( TEXT("  0 params: switch to 'monochrome' scheme" ) );
			ConsoleBufferPush( TEXT("  1 param : dumps R G B for scheme 'monochrome'") );
			ConsoleBufferPush( TEXT("  4 params: sets  R G B for scheme 'monochrome'" ) );
			break;
	// Config - Diasm
		case CMD_CONFIG_DISASM:
		{
			Colorize( sText, " Note: All arguments effect the disassembly view" );
			ConsolePrint( sText );

			sprintf( sTemp, " Usage: [%s%s | %s%s | %s%s | %s%s | %s%s]"
				, CHC_COMMAND
				, g_aParameters[ PARAM_CONFIG_BRANCH ].m_sName
				, CHC_COMMAND
				, g_aParameters[ PARAM_CONFIG_COLON  ].m_sName
				, CHC_COMMAND
				, g_aParameters[ PARAM_CONFIG_OPCODE ].m_sName
				, CHC_COMMAND
				, g_aParameters[ PARAM_CONFIG_SPACES ].m_sName
				, CHC_COMMAND
				, g_aParameters[ PARAM_CONFIG_TARGET ].m_sName );
			Colorize( sText, sTemp );
			ConsolePrint( sText );
			ConsoleBufferPush( "  Display current settings if no args." );

			iParam = PARAM_CONFIG_BRANCH;
			sprintf( sTemp, " Usage: %s [#]", g_aParameters[ iParam ].m_sName );
			Colorize( sText, sTemp );
			ConsolePrint( sText );
			ConsoleBufferPush( "  Set the type of branch character:" );
			sprintf( sText, "  %d off, %d plain, %d fancy",
				 DISASM_BRANCH_OFF, DISASM_BRANCH_PLAIN, DISASM_BRANCH_FANCY );
			ConsoleBufferPush( sText );
			sprintf( sText, "  i.e. %s%s %s 1", CHC_EXAMPLE, pCommand->m_sName, g_aParameters[ iParam ].m_sName );
			ConsolePrint( sText );

			iParam = PARAM_CONFIG_COLON;
			sprintf( sTemp,   " Usage: %s [0|1]", g_aParameters[ iParam ].m_sName );
			Colorize( sText, sTemp );
			ConsolePrint( sText );
			ConsoleBufferPush( "  Display a colon after the address" );
			sprintf( sText,   "  i.e. %s%s %s 0", CHC_EXAMPLE, pCommand->m_sName, g_aParameters[ iParam ].m_sName );
			ConsolePrint( sText );

			iParam = PARAM_CONFIG_OPCODE;
			sprintf( sTemp, " Usage: %s [0|1]", g_aParameters[ iParam ].m_sName );
			Colorize( sText, sTemp );
			ConsolePrint( sText );
			ConsoleBufferPush( "  Display opcode(s) after colon" );
			sprintf( sText,   "  i.e. %s%s %s 1", CHC_EXAMPLE, pCommand->m_sName, g_aParameters[ iParam ].m_sName );
			ConsolePrint( sText );

			iParam = PARAM_CONFIG_SPACES;
			sprintf( sTemp, " Usage: %s [0|1]", g_aParameters[ iParam ].m_sName );
			Colorize( sText, sTemp );
			ConsolePrint( sText );
			ConsoleBufferPush( "  Display spaces between opcodes" );
			sprintf( sText, "  i.e. %s%s %s 0", CHC_EXAMPLE, pCommand->m_sName, g_aParameters[ iParam ].m_sName );
			ConsolePrint( sText );

			iParam = PARAM_CONFIG_TARGET;
			sprintf( sTemp, " Usage: %s [#]", g_aParameters[ iParam ].m_sName );
			Colorize( sText, sTemp );
			ConsolePrint( sText );
			ConsoleBufferPush( "  Set the type of target address/value displayed:" );
			sprintf( sText, "  %d off, %d value only, %d address only, %d both",
				DISASM_TARGET_OFF, DISASM_TARGET_VAL, DISASM_TARGET_ADDR, DISASM_TARGET_BOTH );
			ConsoleBufferPush( sText );
			sprintf( sText, "  i.e. %s%s %s %d", CHC_EXAMPLE, pCommand->m_sName, g_aParameters[ iParam ].m_sName, DISASM_TARGET_VAL );
			ConsolePrint( sText );
			break;
		}
	// Config - Font
		case CMD_CONFIG_FONT:
			sprintf( sText, " No longer applicable with new debugger font" );
			ConsolePrint( sText );
/*
			sprintf( sText, TEXT(" Usage: [%s | %s] \"FontName\" [Height]" ),
				g_aParameters[ PARAM_FONT_MODE ].m_sName, g_aParameters[ PARAM_DISASM ].m_sName );
			ConsoleBufferPush( sText );
			ConsoleBufferPush( TEXT(" i.e. FONT \"Courier\" 12" ) );
			ConsoleBufferPush( TEXT(" i.e. FONT \"Lucida Console\" 12" ) );
			wsprintf( sText, TEXT(" %s Controls line spacing."), g_aParameters[ PARAM_FONT_MODE ].m_sName );
			ConsoleBufferPush( sText );
			wsprintf( sText, TEXT(" Valid values are: %d, %d, %d." ),
				FONT_SPACING_CLASSIC, FONT_SPACING_CLEAN, FONT_SPACING_COMPRESSED );
			ConsoleBufferPush( sText );
*/
			break;

	// Memory
		case CMD_MEMORY_ENTER_BYTE:
			sprintf( sTemp, " Usage: <address | symbol> ## [## ... ##]" );
			Colorize( sText, sTemp );
			ConsolePrint( sText );
			ConsoleBufferPush( TEXT("  Sets memory to the specified 8-Bit Values (bytes)" ) );
			Help_Examples();
			sprintf( sText, "%s  %s 00 4C FF69", CHC_EXAMPLE, pCommand->m_sName ); ConsolePrint( sText );
			sprintf( sText, "%s  00:4C FF69", CHC_EXAMPLE ); ConsolePrint( sText );
			break;
		case CMD_MEMORY_ENTER_WORD:
			sprintf( sTemp, " Usage: <address | symbol> #### [#### ... ####]" );
			Colorize( sText, sTemp );
			ConsolePrint( sText );
			ConsoleBufferPush( TEXT("  Sets memory to the specified 16-Bit Values (words)" ) );
			break;
		case CMD_MEMORY_FILL:
			sprintf( sTemp, " Usage: <address | symbol> <address | symbol> ##" ); 
			Colorize( sText, sTemp );
			ConsolePrint( sText );
			ConsoleBufferPush( TEXT("  Fills the memory range with the specified byte" ) );
			sprintf( sTemp, " Note: Can't fill IO addresses %s$%sC0xx", CHC_ARG_SEP, CHC_ADDRESS );
			Colorize( sText, sTemp );
			ConsolePrint( sText );
			Help_Examples();
			sprintf( sText, "%s  %s 2000:3FFF 00   // Clear HGR page 1", CHC_EXAMPLE, pCommand->m_sName ); ConsolePrint( sText );
			sprintf( sText, "%s  %s 4000,2000 00   // Clear HGR page 2", CHC_EXAMPLE, pCommand->m_sName ); ConsolePrint( sText );
			sprintf( sText, "%s  %s 2000 3FFF 00   // Clear HGR page 1", CHC_EXAMPLE, pCommand->m_sName ); ConsolePrint( sText );
			break;
		case CMD_MEMORY_MOVE:
			sprintf( sTemp, " Usage: destination range" );
			Colorize( sText, sTemp );
			ConsolePrint( sText );
			ConsoleBufferPush( TEXT("  Copies bytes specified by the range to the destination starting address." ) );
			Help_Examples();
			sprintf( sText, "%s  %s 4000 2000:3FFF   // move HGR page 1 to page 2", CHC_EXAMPLE, pCommand->m_sName ); ConsolePrint( sText );
			sprintf( sText, "%s  %s 2001 2000:3FFF   // clear $2000-$3FFF with the byte at $2000", CHC_EXAMPLE, pCommand->m_sName ); ConsolePrint( sText );
			sprintf( sText, "%s  2001<2000:3FFFM", CHC_EXAMPLE ); ConsolePrint( sText );
			break;
//		case CMD_MEM_MINI_DUMP_ASC_1:
//		case CMD_MEM_MINI_DUMP_ASC_2:
		case CMD_MEM_MINI_DUMP_ASCII_1:
		case CMD_MEM_MINI_DUMP_ASCII_2:
			sprintf( sTemp, " Usage: <address | symbol>" ); 
			Colorize( sText, sTemp );
			ConsolePrint( sText );
			ConsoleBufferPush( TEXT("  Displays ASCII text in the Mini-Memory area") ); 
			ConsoleBufferPush( TEXT("  ASCII control chars are hilighted") );
			ConsoleBufferPush( TEXT("  ASCII hi-bit chars are normal") ); 
//			break;
//		case CMD_MEM_MINI_DUMP_TXT_LO_1:
//		case CMD_MEM_MINI_DUMP_TXT_LO_2:
		case CMD_MEM_MINI_DUMP_APPLE_1:
		case CMD_MEM_MINI_DUMP_APPLE_2:
			sprintf( sTemp, " Usage: <address | symbol>" ); 
			Colorize( sText, sTemp );
			ConsolePrint( sText );
			ConsoleBufferPush( "  Displays APPLE text in the Mini-Memory area" ); 
			ConsoleBufferPush( "  APPLE control chars are inverse" );
			ConsoleBufferPush( "  APPLE hi-bit chars are normal"   ); 
			break;
//		case CMD_MEM_MINI_DUMP_TXT_HI_1:
//		case CMD_MEM_MINI_DUMP_TXT_HI_2:
//			ConsoleBufferPush( TEXT(" Usage: <address | symbol>") ); 
//			ConsoleBufferPush( TEXT("  Displays text in the Memory Mini-Dump area") ); 
//			ConsoleBufferPush( TEXT("  ASCII chars with the hi-bit set, is inverse") ); 
			break;

		case CMD_MEMORY_LOAD:
		case CMD_MEMORY_SAVE:
			if (iCommand == CMD_MEMORY_LOAD)
			{
				sprintf( sTemp, " Usage: [\"Filename\"],address[,length]" );
				Colorize( sText, sTemp );
				ConsolePrint( sText );
				sprintf( sTemp, " Usage: [\"Filename\"],range"            );
				Colorize( sText, sTemp );
				ConsolePrint( sText );
				Help_Range();
				ConsoleBufferPush( "  Notes: If no filename specified, defaults to the last filename (if possible)" );
			}
			if (iCommand == CMD_MEMORY_SAVE)
			{			
				sprintf( sTemp, " Usage: [\"Filename\"],address,length" );
				Colorize( sText, sTemp );
				ConsolePrint( sText );
				sprintf( sTemp, " Usage: [\"Filename\"],range"          );
				Colorize( sText, sTemp );
				ConsolePrint( sText );
				Help_Range();
				ConsoleBufferPush( "  Notes: If no filename specified, defaults to: '####.####.bin'" );
				ConsoleBufferPush( "    Where the form is <address>.<length>.bin"                    );
			}
			
//			ConsoleBufferPush( TEXT(" Examples:" ) );
			Help_Examples();
			sprintf( sText, "%s   BSAVE \"test\",FF00,100"  , CHC_EXAMPLE ); ConsolePrint( sText );
			sprintf( sText, "%s   BLOAD \"test\",2000:2010" , CHC_EXAMPLE ); ConsolePrint( sText );
			sprintf( sText, "%s   BSAVE \"test\",F000:FFFF" , CHC_EXAMPLE ); ConsolePrint( sText );
			sprintf( sText, "%s   BLOAD \"test\",4000"      , CHC_EXAMPLE ); ConsolePrint( sText );
			break;
		case CMD_MEMORY_SEARCH:
			Colorize( sText, " Usage: range <\"ASCII text\" | 'apple text' | hex>" );
			ConsolePrint( sText );
			Help_Range();
			ConsoleBufferPush( "  Where text is of the form:"            );
			ConsoleBufferPush( "    \"...\" designate ASCII text"        );
			ConsoleBufferPush( "    '...' designate Apple High-Bit text" );
			Help_Examples();
			sprintf( sText, "%s  %s F000,1000 'Apple'   // search High-Bit", CHC_EXAMPLE, pCommand->m_sName ); ConsolePrint( sText );
			sprintf( sText, "%s  MT1 @2"                                   , CHC_EXAMPLE, pCommand->m_sName ); ConsolePrint( sText );
			sprintf( sText, "%s  %s D000:FFFF \"FLAS\"    // search ASCII ", CHC_EXAMPLE, pCommand->m_sName ); ConsolePrint( sText );
			sprintf( sText, "%s  MA1 @1"                                   , CHC_EXAMPLE, pCommand->m_sName ); ConsolePrint( sText );
			sprintf( sText, "%s  %s D000,4000 \"EN\" 'D'  // Mixed text"   , CHC_EXAMPLE, pCommand->m_sName ); ConsolePrint( sText );
			sprintf( sText, "%s  MT1 @1"                                   , CHC_EXAMPLE, pCommand->m_sName ); ConsolePrint( sText );
			sprintf( sText, "%s  %s D000,4000 'Apple' ? ']'"               , CHC_EXAMPLE, pCommand->m_sName ); ConsolePrint( sText );
			break;
		case CMD_MEMORY_SEARCH_HEX:
			Colorize( sText, " Usage: range [text | byte1 [byte2 ...]]" );
			ConsolePrint( sText );
			Help_Range();
			ConsoleBufferPush( "  Where <byte> is of the form:" );
			ConsoleBufferPush( "    ##   match specific byte"   );
			ConsoleBufferPush( "    #### match specific 16-bit value"  );
			ConsoleBufferPush( "    ?    match any byte"               );
			ConsoleBufferPush( "    ?#   match any high nibble, match low nibble to specific number" );
			ConsoleBufferPush( "    #?   match specific high nibble, match any low nibble"           );
			Help_Examples();
			sprintf( sText, "%s   %s F000,1000 AD ? C0", CHC_EXAMPLE, pCommand->m_sName ); ConsolePrint( sText );
			sprintf( sText, "%s   U @1"                , CHC_EXAMPLE, pCommand->m_sName ); ConsolePrint( sText );
			sprintf( sText, "%s   %s F000,1000 ?1 C0"  , CHC_EXAMPLE, pCommand->m_sName ); ConsolePrint( sText );
			sprintf( sText, "%s   %s F000,1000 5? C0"  , CHC_EXAMPLE, pCommand->m_sName ); ConsolePrint( sText );
			sprintf( sText, "%s   %s F000,1000 10 C?"  , CHC_EXAMPLE, pCommand->m_sName ); ConsolePrint( sText );
			sprintf( sText, "%s   U @2 - 1"            , CHC_EXAMPLE, pCommand->m_sName ); ConsolePrint( sText );
			sprintf( sText, "%s   %s F000:FFFF C030"   , CHC_EXAMPLE, pCommand->m_sName ); ConsolePrint( sText );
			sprintf( sText, "%s   U @1 - 1"            , CHC_EXAMPLE, pCommand->m_sName ); ConsolePrint( sText );
			break;
//		case CMD_MEMORY_SEARCH_APPLE:
//			wsprintf( sText,   TEXT("Deprecated.  Use: %s" ), g_aCommands[ CMD_MEMORY_SEARCH ].m_sName ); ConsoleBufferPush( sText );
//			break;
//		case CMD_MEMORY_SEARCH_ASCII:
//			wsprintf( sText,   TEXT("Deprecated.  Use: %s" ), g_aCommands[ CMD_MEMORY_SEARCH ].m_sName ); ConsoleBufferPush( sText );
//			break;
	// Output
		case CMD_OUTPUT_CALC:
			Colorize( sText, " Usage: <address | symbol | expression >" );
			ConsolePrint( sText );
			ConsoleBufferPush( "  Expression is one of: + - * / % ^ ~"  );
			ConsoleBufferPush( " Output order is: Hex Bin Dec Char"     );
			ConsoleBufferPush( "  Note: symbols take piority."          );
			Help_Examples();
			ConsoleBufferPush( "Note: #A (if you don't want the accumulator value)" );
			ConsoleBufferPush( "Note: #F (if you don't want the flags value)"  );
			break;
		case CMD_OUTPUT_ECHO:
			Colorize( sText, " Usage: string"    );
			ConsolePrint( sText );
//			ConsoleBufferPush( TEXT(" Examples:"        ) );
			Help_Examples();
			sprintf( sText,   "%s   %s Checkpoint", CHC_EXAMPLE, pCommand->m_sName ); ConsolePrint( sText );
			sprintf( sText,   "%s   %s PC"        , CHC_EXAMPLE, pCommand->m_sName ); ConsolePrint( sText );
//			ConsoleBufferPush( TEXT("  Echo the string to the console" ) );
			break;
		case CMD_OUTPUT_PRINT: 
			Colorize( sText, " Usage: <string | expression> [, string | expression]*"       );
			ConsolePrint( sText );
			Colorize( sText, "  Note: To print Register values, they must be in upper case" );
			ConsolePrint( sText );
			Help_Examples();
			sprintf( sText, "%s   %s \"A:\",A,\" X:\",X", CHC_EXAMPLE, pCommand->m_sName ); ConsolePrint( sText );
			sprintf( sText, "%s   %s A,\" \",X,\" \",Y" , CHC_EXAMPLE, pCommand->m_sName ); ConsolePrint( sText );
			sprintf( sText, "%s   %s PC"                , CHC_EXAMPLE, pCommand->m_sName ); ConsolePrint( sText );
			break;
		case CMD_OUTPUT_PRINTF:
			Colorize( sText,   " Usage: <string> [, expression, ...]" );
			ConsolePrint( sText );
			ConsoleBufferPush( "  The string may contain c-style printf formatting flags: %d %x %z %c" );
			ConsoleBufferPush( "  Where: %d decimal, %x hex, %z binary, %c char, %% percent"           );
			Help_Examples();
			sprintf( sText, "%s   %s \"Dec: %%d  Hex: %%x  Bin: %%z  Char: %c\",A,A,A,A", CHC_EXAMPLE, pCommand->m_sName ); ConsolePrint( sText );
			sprintf( sText, "%s   %s \"%%x %%x %%x\",A,X,Y"                             , CHC_EXAMPLE, pCommand->m_sName ); ConsolePrint( sText );

			break;
	// Symbols
		case CMD_SYMBOLS_LOOKUP:
			Colorize( sText, " Usage: symbol [= <address>]" );
			ConsolePrint( sText );
			Help_Examples();
			wsprintf( sText, "%s   %s HOME"       , CHC_EXAMPLE, pCommand->m_sName ); ConsolePrint( sText );
			wsprintf( sText, "%s   %s LIFE = 2000", CHC_EXAMPLE, pCommand->m_sName ); ConsolePrint( sText );
			wsprintf( sText, "%s   %s LIFE"       , CHC_EXAMPLE, pCommand->m_sName ); ConsolePrint( sText );
			break;
		case CMD_SYMBOLS_ROM:
		case CMD_SYMBOLS_APPLESOFT:
		case CMD_SYMBOLS_ASSEMBLY:
		case CMD_SYMBOLS_USER_1:
		case CMD_SYMBOLS_USER_2:
		case CMD_SYMBOLS_SRC_1:
		case CMD_SYMBOLS_SRC_2:
//			ConsoleBufferPush( TEXT(" Usage: [ ON | OFF | symbol | address ]" ) );
//			ConsoleBufferPush( TEXT(" Usage: [ LOAD [\"filename\"] | SAVE \"filename\"]" ) ); 
//			ConsoleBufferPush( TEXT("  ON  : Turns symbols on in the disasm window" ) );
//			ConsoleBufferPush( TEXT("  OFF : Turns symbols off in the disasm window" ) );
//			ConsoleBufferPush( TEXT("  LOAD: Loads symbols from last/default filename" ) );
//			ConsoleBufferPush( TEXT("  SAVE: Saves symbol table to file" ) );
//			ConsoleBufferPush( TEXT(" CLEAR: Clears the symbol table" ) );
			Colorize( sText, " Usage: [ <cmd> | symbol | address ]" );
			ConsolePrint( sText );
			ConsoleBufferPush( "  Where <cmd> is one of:" );
			sprintf( sText, "%s%-5s%s: Turns symbols on in the disasm window"       , CHC_STRING, g_aParameters[ PARAM_ON    ].m_sName, CHC_DEFAULT ); ConsolePrint( sText );
			sprintf( sText, "%s%-5s%s: Turns symbols off in the disasm window"      , CHC_STRING, g_aParameters[ PARAM_OFF   ].m_sName, CHC_DEFAULT ); ConsolePrint( sText );
			sprintf( sText, "%s%-5s%s: Loads symbols from last/default \"filename\"", CHC_STRING, g_aParameters[ PARAM_LOAD  ].m_sName, CHC_DEFAULT ); ConsolePrint( sText ); // 2.6.2.14 Fixed: Save/Load Param help was swapped.
			sprintf( sText, "%s%-5s%s: Saves symbol table to \"filename\""          , CHC_STRING, g_aParameters[ PARAM_SAVE  ].m_sName, CHC_DEFAULT ); ConsolePrint( sText ); // 2.6.2.14 Fixed: Save/Load Param help was swapped.
			sprintf( sText, "%s%-5s%s: Clears the symbol table"                     , CHC_STRING, g_aParameters[ PARAM_CLEAR ].m_sName, CHC_DEFAULT ); ConsolePrint( sText );
			sprintf( sText, "%s%-5s%s: Remove symbol"                               , CHC_STRING, g_aTokens[ TOKEN_EXCLAMATION ].sToken, CHC_DEFAULT ); ConsolePrint( sText );
			break;
		case CMD_SYMBOLS_LIST :
			Colorize( sText, " Usage: symbol" );
			ConsolePrint( sText );
			ConsoleBufferPush( "  Looks up symbol in all 3 symbol tables: main, user, source" );
			break;
	// View
		case CMD_VIEW_TEXT4X:
		case CMD_VIEW_TEXT41:
		case CMD_VIEW_TEXT42:
		case CMD_VIEW_TEXT8X:
		case CMD_VIEW_TEXT81:
		case CMD_VIEW_TEXT82:
		case CMD_VIEW_GRX   :
		case CMD_VIEW_GR1   :
		case CMD_VIEW_GR2   :
		case CMD_VIEW_DGRX  :
		case CMD_VIEW_DGR1  :
		case CMD_VIEW_DGR2  :
		case CMD_VIEW_HGRX  :
		case CMD_VIEW_HGR1  :
		case CMD_VIEW_HGR2  :
		case CMD_VIEW_DHGRX :
		case CMD_VIEW_DHGR1 :
		case CMD_VIEW_DHGR2 :
			ConsolePrint("Show the video output in the specified format.");
			break;
	// Watches
		case CMD_WATCH_ADD:
			Colorize( sText, " Usage: <address | symbol>" );
			ConsolePrint( sText );
			ConsoleBufferPush( "  Adds the specified memory location to the watch window." );
			break;
	// Window
		case CMD_WINDOW_CODE    : // summary is good enough
		case CMD_WINDOW_CODE_2  : // summary is good enough
		case CMD_WINDOW_SOURCE_2: // summary is good enough
			break;
	// Zero Page pointers
		case CMD_ZEROPAGE_POINTER:
		case CMD_ZEROPAGE_POINTER_ADD:
			Colorize( sText, " Usage: <address | symbol>" );
			ConsolePrint( sText );
			Colorize( sText, " Usage: # <address | symbol> [address...]" );
			ConsolePrint( sText );
			ConsoleBufferPush("  Adds the specified memory location to the zero page pointer window." );
			ConsoleBufferPush("  Update the specified zero page pointer (#) with the address." );
			ConsoleBufferPush(" Note: Displayed as symbol name (if possible) and the 16-bit target pointer" );
			Help_Examples();
			sprintf( sText, "%s   %s CH"     , CHC_EXAMPLE, pCommand->m_sName ); ConsolePrint( sText );
			sprintf( sText, "%s   %s 0 CV"   , CHC_EXAMPLE, pCommand->m_sName ); ConsolePrint( sText );
			sprintf( sText, "%s   %s 0 CV CH", CHC_EXAMPLE, pCommand->m_sName ); ConsolePrint( sText );
			break;
		case CMD_ZEROPAGE_POINTER_CLEAR:
			Colorize( sText, " Usage: [# | *]" );
			ConsoleBufferPush( "  Clears specified zero page pointer, or all." );
			sprintf( sText,  "  i.e. %s%s 1", CHC_EXAMPLE, pCommand->m_sName );
			ConsolePrint( sText );
			break;
		case CMD_ZEROPAGE_POINTER_0:
		case CMD_ZEROPAGE_POINTER_1:
		case CMD_ZEROPAGE_POINTER_2:
		case CMD_ZEROPAGE_POINTER_3:
		case CMD_ZEROPAGE_POINTER_4:
		case CMD_ZEROPAGE_POINTER_5:
		case CMD_ZEROPAGE_POINTER_6:
		case CMD_ZEROPAGE_POINTER_7:
			Colorize( sText, " Usage: [<address | symbol>]" );
			ConsolePrint( sText );
			ConsoleBufferPush( "  If no address specified, will remove watching the zero page pointer." );
			break;

	// Misc
		case CMD_VERSION:
			Colorize( sText, " Usage: [*]" );
			ConsolePrint( sText );
			ConsoleBufferPush( "  * Display extra internal stats" );
			break;

		default:
			if (bAllCommands)
				break;

			if ((! nFound) || (! pCommand))
			{
				wsprintf( sText, " Invalid command." );
				ConsoleBufferPush( sText );
			}
			else
			{
//#if DEBUG_COMMAND_HELP
#if _DEBUG
			wsprintf( sText, "Command help not done yet!: %s", g_aCommands[ iCommand ].m_sName );
			ConsoleBufferPush( sText );
#endif
			}

			break;
		}

	}
	
	return ConsoleUpdate();
}

//===========================================================================
Update_t CmdHelpList (int nArgs)
{
	const int nBuf = CONSOLE_WIDTH * 2;

	char sText[ nBuf ] = "";
	
	int nLenLine = strlen( sText );
	int y = 0;
	int nLinesScrolled = 0;

	int nMaxWidth = g_nConsoleDisplayWidth - 1;
	int iCommand;

	extern vector<Command_t> g_vSortedCommands;

	if (! g_vSortedCommands.size())
	{
		for (iCommand = 0; iCommand < NUM_COMMANDS_WITH_ALIASES; iCommand++ )
		{
			g_vSortedCommands.push_back( g_aCommands[ iCommand ] );
		}
		std::sort( g_vSortedCommands.begin(), g_vSortedCommands.end(), commands_functor_compare() );
	}
	int nCommands = g_vSortedCommands.size();

	int nLen = 0;
//		Colorize( sText, "Commands: " );
 		        StringCat( sText, CHC_USAGE , nBuf );
		nLen += StringCat( sText, "Commands", nBuf );

		        StringCat( sText, CHC_DEFAULT, nBuf );
		nLen += StringCat( sText, ": " , nBuf );

	for( iCommand = 0; iCommand < NUM_COMMANDS_WITH_ALIASES; iCommand++ ) // aliases are not printed
	{
		Command_t *pCommand = & g_vSortedCommands.at( iCommand );
//		Command_t *pCommand = & g_aCommands[ iCommand ];
		char      *pName = pCommand->m_sName;

		if (! pCommand->pFunction)
			continue; // not implemented function

		int nLenCmd = strlen( pName );
		if ((nLen + nLenCmd) < (nMaxWidth))
		{
			        StringCat( sText, CHC_COMMAND, nBuf );
			nLen += StringCat( sText, pName      , nBuf );
		}
		else
		{
			ConsolePrint( sText );
			nLen = 1;
			strcpy( sText, " " );
			        StringCat( sText, CHC_COMMAND, nBuf );
			nLen += StringCat( sText, pName, nBuf );
		}
		
		strcat( sText, " " );
		nLen++;
	}

	//ConsoleBufferPush( sText );
	ConsolePrint( sText );
	ConsoleUpdate();

	return UPDATE_CONSOLE_DISPLAY;
}

	
//===========================================================================
Update_t CmdVersion (int nArgs)
{
	TCHAR sText[ CONSOLE_WIDTH ];

	unsigned int nVersion = DEBUGGER_VERSION;
	int nMajor;
	int nMinor;
	int nFixMajor;
	int nFixMinor;
	UnpackVersion( nVersion, nMajor, nMinor, nFixMajor, nFixMinor );

	sprintf( sText, "  Emulator:  %s%s%s    Debugger: %s%d.%d.%d.%d%s"
		, CHC_SYMBOL
		, VERSIONSTRING
		, CHC_DEFAULT
		, CHC_SYMBOL
		, nMajor, nMinor, nFixMajor, nFixMinor
		, CHC_DEFAULT
	);
	ConsolePrint( sText );

	if (nArgs)
	{
		for (int iArg = 1; iArg <= g_nArgRaw; iArg++ )
		{
			// * PARAM_WILDSTAR -> ? PARAM_MEM_SEARCH_WILD
			if ((! _tcscmp( g_aArgs[ iArg ].sArg, g_aParameters[ PARAM_WILDSTAR        ].m_sName )) ||
				(! _tcscmp( g_aArgs[ iArg ].sArg, g_aParameters[ PARAM_MEM_SEARCH_WILD ].m_sName )) )
			{
				wsprintf( sText, "  Arg: %d bytes * %d = %d bytes",
					sizeof(Arg_t), MAX_ARGS, sizeof(g_aArgs) );
				ConsoleBufferPush( sText );

				wsprintf( sText, "  Console: %d bytes * %d height = %d bytes",
					sizeof( g_aConsoleDisplay[0] ), CONSOLE_HEIGHT, sizeof(g_aConsoleDisplay) );
				ConsoleBufferPush( sText );

				wsprintf( sText, "  Commands: %d   (Aliased: %d)   Params: %d",
					NUM_COMMANDS, NUM_COMMANDS_WITH_ALIASES, NUM_PARAMS );
				ConsoleBufferPush( sText );

				wsprintf( sText, "  Cursor(%d)  T: %04X  C: %04X  B: %04X %c D: %02X", // Top, Cur, Bot, Delta
					g_nDisasmCurLine, g_nDisasmTopAddress, g_nDisasmCurAddress, g_nDisasmBotAddress,
					g_bDisasmCurBad ? TEXT('*') : TEXT(' ')
					, g_nDisasmBotAddress - g_nDisasmTopAddress
				);
				ConsoleBufferPush( sText );

				CmdConfigGetFont( 0 );

				break;
			}
			else
				return Help_Arg_1( CMD_VERSION );
		}
	}

	return ConsoleUpdate();
}

