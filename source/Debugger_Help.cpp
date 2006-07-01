/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 1994-1996, Michael O'Brien
Copyright (C) 1999-2001, Oliver Schmidt
Copyright (C) 2002-2005, Tom Charlesworth
Copyright (C) 2006, Tom Charlesworth, Michael Pohoreski

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
 * Author: Copyright (C) 2006, Michael Pohoreski
 */

#include "StdAfx.h"
#pragma  hdrstop


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
bool StringCat ( TCHAR * pDst, LPCSTR pSrc, const int nDstSize )
{
	int nLenDst = _tcslen( pDst );
	int nLenSrc = _tcslen( pSrc );
	int nSpcDst = nDstSize - nLenDst;
	int nChars  = MIN( nLenSrc, nSpcDst );

	_tcsncat( pDst, pSrc, nChars );

	bool bOverflow = (nSpcDst < nLenSrc);
	if (bOverflow)
		return false;
		
	return true;
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
void Help_Range()
{
	ConsoleBufferPush( TEXT("  Where <range> is of the form:"                ) );
	ConsoleBufferPush( TEXT("    address , length   [address,address+length)" ) );
	ConsoleBufferPush( TEXT("    address : end      [address,end]"            ) );
}

//===========================================================================
void Help_Operators()
{
	ConsoleBufferPush( TEXT("  Operators: (Math)"                         ) );
	ConsoleBufferPush( TEXT("    +   Addition"                            ) );
	ConsoleBufferPush( TEXT("    -   Subtraction"                         ) );
	ConsoleBufferPush( TEXT("    *   Multiplication"                      ) );
	ConsoleBufferPush( TEXT("    /   Division"                            ) );
	ConsoleBufferPush( TEXT("    %   Modulas / Remainder"                 ) );
	ConsoleBufferPush( TEXT("  Operators: (Bit Wise)"                     ) );
	ConsoleBufferPush( TEXT("    &   Bit-wise and (AND)"                  ) );
	ConsoleBufferPush( TEXT("    |   Bit-wise or  (OR )"                  ) );
	ConsoleBufferPush( TEXT("    ^   Bit-wise exclusive-or (EOR/XOR)"     ) );
	ConsoleBufferPush( TEXT("    !   Bit-wise negation (NOT)"             ) );
	ConsoleBufferPush( TEXT("  Operators: (Input)"                        ) );
	ConsoleBufferPush( TEXT("    @   next number refers to search results"       ) );
	ConsoleBufferPush( TEXT("    \"   Designate string in ASCII format"          ) );
	ConsoleBufferPush( TEXT("    \'   Desginate string in High-Bit apple format" ) );
	ConsoleBufferPush( TEXT("    $   Designate number/symbol"                    ) );
	ConsoleBufferPush( TEXT("    #   Designate number in hex"                    ) );
	ConsoleBufferPush( TEXT("  Operators: (Range)"                               ) );
	ConsoleBufferPush( TEXT("    ,   range seperator (2nd address is relative)"  ) );
	ConsoleBufferPush( TEXT("    :   range seperator (2nd address is absolute)"  ) );
	ConsoleBufferPush( TEXT("  Operators: (Misc)"                                ) );
	ConsoleBufferPush( TEXT("    //  comment until end of line"                  ) );
	ConsoleBufferPush( TEXT("  Operators: (Breakpoint)"                                ) );

	TCHAR sText[ CONSOLE_WIDTH ];

	_tcscpy( sText, "    " );
	int iBreakOp = 0;
	for( iBreakOp = 0; iBreakOp < NUM_BREAKPOINT_OPERATORS; iBreakOp++ )
	{
//		if (iBreakOp == PARAM_BP_LESS_EQUAL)
//			continue;
//		if (iBreakOp == PARAM_BP_GREATER_EQUAL)
//			continue;

		if ((iBreakOp >= PARAM_BP_LESS_EQUAL) &&
			(iBreakOp <= PARAM_BP_GREATER_EQUAL))
		{
			_tcscat( sText, g_aBreakpointSymbols[ iBreakOp ] );
			_tcscat( sText, " " );
		}
	}	
	ConsoleBufferPush( sText );
}

//===========================================================================
Update_t CmdMOTD( int nArgs )
{
	TCHAR sText[ CONSOLE_WIDTH ];

	ConsoleBufferPush( TEXT(" Apple ][ ][+ //e Emulator for Windows") );
	CmdVersion(0);
	CmdSymbols(0);
	wsprintf( sText, "  '~' console, '%s' (specific), '%s' (all)"
		 , g_aCommands[ CMD_HELP_SPECIFIC ].m_sName
//		 , g_aCommands[ CMD_HELP_SPECIFIC ].pHelpSummary
		 , g_aCommands[ CMD_HELP_LIST     ].m_sName
//		 , g_aCommands[ CMD_HELP_LIST     ].pHelpSummary
	);
	ConsoleBufferPush( sText );

	ConsoleUpdate();

	return UPDATE_ALL;
}


// Help on specific command
//===========================================================================
Update_t CmdHelpSpecific (int nArgs)
{
	int iArg;
	TCHAR sText[ CONSOLE_WIDTH ];
	ZeroMemory( sText, CONSOLE_WIDTH );

	if (! nArgs)
	{
//		ConsoleBufferPush( TEXT(" [] = optional, {} = mandatory.  Categories are: ") );

		_tcscpy( sText, TEXT("Usage: [< ") );
		for (int iCategory = _PARAM_HELPCATEGORIES_BEGIN ; iCategory < _PARAM_HELPCATEGORIES_END; iCategory++)
		{
#if _DEBUG
//			if (iCategory == (PARAM_CAT_ZEROPAGE - 1))
//			{
//				int  nLen = _tcslen( sText );
//				bool bStop = true;
//			}
#endif
			TCHAR *pName = g_aParameters[ iCategory ].m_sName; 
			if (! TestStringCat( sText, pName, CONSOLE_WIDTH - 4 )) // CONSOLE_WIDTH // g_nConsoleDisplayWidth - 3
			{
				ConsoleBufferPush( sText );
				_tcscpy( sText, TEXT("    ") );
			}

			StringCat( sText, pName, CONSOLE_WIDTH );
			if (iCategory < (_PARAM_HELPCATEGORIES_END - 1))
			{
				StringCat( sText, TEXT(" | "), CONSOLE_WIDTH - 1 );
			}
		}
		StringCat( sText, TEXT(" >]"), CONSOLE_WIDTH - 3 );
		ConsoleBufferPush( sText );

		wsprintf( sText, TEXT("Note: [] = optional, <> = mandatory"), CONSOLE_WIDTH );
		ConsoleBufferPush( sText );
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
			bCategory = true;
			switch( iParam )
			{
				case PARAM_CAT_BOOKMARKS  : iCmdBegin = CMD_BOOKMARK        ; iCmdEnd = CMD_BOOKMARK_SAVE        ; break;
				case PARAM_CAT_BREAKPOINTS: iCmdBegin = CMD_BREAKPOINT      ; iCmdEnd = CMD_BREAKPOINT_SAVE      ; break;
				case PARAM_CAT_CONFIG     : iCmdBegin = CMD_BENCHMARK       ; iCmdEnd = CMD_CONFIG_SAVE          ; break;
				case PARAM_CAT_CPU        : iCmdBegin = CMD_ASSEMBLE        ; iCmdEnd = CMD_UNASSEMBLE           ; break;
				case PARAM_CAT_FLAGS      : iCmdBegin = CMD_FLAG_CLEAR      ; iCmdEnd = CMD_FLAG_SET_N           ; break;
				case PARAM_CAT_HELP       : iCmdBegin = CMD_HELP_LIST       ; iCmdEnd = CMD_MOTD                 ; break;
				case PARAM_CAT_MEMORY     : iCmdBegin = CMD_MEMORY_COMPARE  ; iCmdEnd = CMD_MEMORY_FILL          ; break;
				case PARAM_CAT_OUTPUT     : iCmdBegin = CMD_OUTPUT_CALC     ; iCmdEnd = CMD_OUTPUT_RUN           ; break;
				case PARAM_CAT_SYMBOLS    :
					// HACK: check if we have an exact command match first
					nFound = FindCommand( g_aArgs[iArg].sArg, pFunction, & iCommand );
					if (nFound && (iCommand != CMD_SYMBOLS_LOOKUP) && (iCommand != CMD_MEMORY_SEARCH))
					{
						iCmdBegin = CMD_SYMBOLS_LOOKUP  ; iCmdEnd = CMD_SYMBOLS_LIST         ; break;
						bCategory = true;
					}
					else
						bCategory = false;
					break;
				case PARAM_CAT_WATCHES    : iCmdBegin = CMD_WATCH_ADD       ; iCmdEnd = CMD_WATCH_LIST           ; break;
				case PARAM_CAT_WINDOW     : iCmdBegin = CMD_WINDOW          ; iCmdEnd = CMD_WINDOW_OUTPUT        ; break;
				case PARAM_CAT_ZEROPAGE   : iCmdBegin = CMD_ZEROPAGE_POINTER; iCmdEnd = CMD_ZEROPAGE_POINTER_SAVE;break;

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
			TCHAR sCategory[ CONSOLE_WIDTH ];
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

			wsprintf( sText, "Category: %s", sCategory );
			ConsoleBufferPush( sText );

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
					wsprintf( sText, "%8s, ", pCommand->m_sName );
				else
					wsprintf( sText, "%s, ", pCommand->m_sName );
				if (! TryStringCat( sText, pHelp, g_nConsoleDisplayWidth ))
				{
					if (! TryStringCat( sText, pHelp, CONSOLE_WIDTH-1 ))
					{
						StringCat( sText, pHelp, CONSOLE_WIDTH );
						ConsoleBufferPush( sText );
					}
				}
				ConsoleBufferPush( sText );
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
			ConsoleBufferPush( TEXT(" Built-in assember isn't functional yet.") );
			break;
		case CMD_UNASSEMBLE:
			ConsoleBufferPush( TEXT(" Usage: {address | symbol}") );
			ConsoleBufferPush( TEXT("  Disassembles memory.") );
			break;
		case CMD_GO:
			ConsoleBufferPush( TEXT(" Usage: address | symbol [Skip,Length]]") );
			ConsoleBufferPush( TEXT("  addres | symbol [Start:End]") );
			ConsoleBufferPush( TEXT(" Skip  : Start address to skip stepping" ) );
			ConsoleBufferPush( TEXT(" Length: Range of bytes past start address to skip stepping" ) );
			ConsoleBufferPush( TEXT(" End   : Inclusive end address to skip stepping" ) );
			ConsoleBufferPush( TEXT("  If the Program Counter is outside the skip range, resumes single-stepping." ) );
			ConsoleBufferPush( TEXT("  Can be used to skip ROM/OS/user code." ));
			ConsoleBufferPush( TEXT(" Examples:" ) );
			ConsoleBufferPush( TEXT("  G C600 FA00,600" ) );
			ConsoleBufferPush( TEXT("  G C600 F000:FFFF" ) );
			break;
		case CMD_JSR:
			ConsoleBufferPush( TEXT(" Usage: {symbol | address}") );
			ConsoleBufferPush( TEXT("  Pushes PC on stack; calls the named subroutine.") );
			break;
		case CMD_NOP:
			ConsoleBufferPush( TEXT("  Puts a NOP opcode at current instruction") );
			break;
		case CMD_OUT:
			ConsoleBufferPush( TEXT(" Usage: {address8 | address16 | symbol} ## [##]") );
			ConsoleBufferPush( TEXT("  Ouput a byte or word to the IO address $C0xx" ) );
			break;
		case CMD_PROFILE:
			wsprintf( sText, TEXT(" Usage: [%s | %s | %s]")
				, g_aParameters[ PARAM_RESET ].m_sName
				, g_aParameters[ PARAM_SAVE  ].m_sName
				, g_aParameters[ PARAM_LIST  ].m_sName
			);
			ConsoleBufferPush( sText );
			ConsoleBufferPush( TEXT(" No arguments resets the profile.") );
			break;
	// Registers
		case CMD_REGISTER_SET:
			wsprintf( sText,   TEXT(" Usage: <reg> <value | expression | symbol>" ) ); ConsoleBufferPush( sText );
			ConsoleBufferPush( TEXT("  Where <reg> is one of: A X Y PC SP ") );
			wsprintf( sText,   TEXT(" See also: %s" ), g_aParameters[ PARAM_CAT_OPERATORS ].m_sName ); ConsoleBufferPush( sText );
			ConsoleBufferPush( TEXT(" Examples:") );
			ConsoleBufferPush( TEXT("  R PC RESET + 1") );
			ConsoleBufferPush( TEXT("  R PC $FC58") );
			ConsoleBufferPush( TEXT("  R A  A1") );
			ConsoleBufferPush( TEXT("  R A  $A1") );
			ConsoleBufferPush( TEXT("  R A  #A1") );
			break;
		case CMD_SOURCE:
//			ConsoleBufferPush( TEXT(" Reads assembler source file." ) );
			wsprintf( sText, TEXT(" Usage: [ %s | %s ] \"filename\""            ), g_aParameters[ PARAM_SRC_MEMORY  ].m_sName, g_aParameters[ PARAM_SRC_SYMBOLS ].m_sName ); ConsoleBufferPush( sText );
			wsprintf( sText, TEXT("   %s: read source bytes into memory."        ), g_aParameters[ PARAM_SRC_MEMORY  ].m_sName ); ConsoleBufferPush( sText );
			wsprintf( sText, TEXT("   %s: read symbols into Source symbol table."), g_aParameters[ PARAM_SRC_SYMBOLS ].m_sName ); ConsoleBufferPush( sText );
			wsprintf( sText, TEXT(" Supports: %s."                              ), g_aParameters[ PARAM_SRC_MERLIN  ].m_sName ); ConsoleBufferPush( sText );
			break;
		case CMD_STEP_OUT: 
			ConsoleBufferPush( TEXT("  Steps out of current subroutine") );
			ConsoleBufferPush( TEXT("  Hotkey: Ctrl-Space" ) ); // TODO: FIXME
			break;
		case CMD_STEP_OVER: // Bad name? FIXME/TODO: do we need to rename?
			ConsoleBufferPush( TEXT(" Usage: [#]") );
			ConsoleBufferPush( TEXT("  Steps, # times, thru current instruction") );
			ConsoleBufferPush( TEXT("  JSR will be stepped into AND out of.") );
			ConsoleBufferPush( TEXT("  Hotkey: Ctrl-Space" ) ); // TODO: FIXME
			break;
		case CMD_TRACE:
			ConsoleBufferPush( TEXT(" Usage: [#]") );
			ConsoleBufferPush( TEXT("  Traces, # times, current instruction(s)") );
			ConsoleBufferPush( TEXT("  JSR will be stepped into") );
			ConsoleBufferPush( TEXT("  Hotkey: Shift-Space" ) );
		case CMD_TRACE_FILE:
			ConsoleBufferPush( TEXT(" Usage: [filename]") );
			break;
		case CMD_TRACE_LINE:
			ConsoleBufferPush( TEXT(" Usage: [#]") );
			ConsoleBufferPush( TEXT("  Traces into current instruction") );
			ConsoleBufferPush( TEXT("  with cycle counting." ) );
			break;
	// Bookmarks
		case CMD_BOOKMARK:
		case CMD_BOOKMARK_ADD:
			ConsoleBufferPush(" Usage: [address | symbol]" );
			ConsoleBufferPush(" Usage: # <address | symbol>" );
			ConsoleBufferPush("  If no address or symbol is specified, lists the current bookmarks." );
			ConsoleBufferPush("  Updates the specified bookmark (#)" );
			wsprintf( sText,  TEXT("  i.e. %s RESET" ), pCommand->m_sName );
			ConsoleBufferPush( sText );
			wsprintf( sText,  TEXT("  i.e. %s 1 HOME" ), pCommand->m_sName );
			ConsoleBufferPush( sText );
			break;
		case CMD_BOOKMARK_CLEAR:
			ConsoleBufferPush( TEXT(" Usage: [# | *]") );
			ConsoleBufferPush( TEXT("  Clears specified bookmark, or all.") );
			wsprintf( sText,  TEXT("  i.e. %s 1" ), pCommand->m_sName );
			ConsoleBufferPush( sText );
			break;
		case CMD_BOOKMARK_LIST:
//		case CMD_BOOKMARK_LOAD:
		case CMD_BOOKMARK_SAVE:
			break;
	// Breakpoints
		case CMD_BREAKPOINT:
			wsprintf( sText, " Maximum breakpoints: %d", MAX_BREAKPOINTS );
			ConsoleBufferPush( sText );
			wsprintf( sText, TEXT(" Usage: [%s | %s | %s]")
				, g_aParameters[ PARAM_LOAD  ].m_sName
				, g_aParameters[ PARAM_SAVE  ].m_sName
				, g_aParameters[ PARAM_RESET ].m_sName );
			ConsoleBufferPush( sText );
			ConsoleBufferPush( TEXT("  Set breakpoint at PC if no args.") );
			ConsoleBufferPush( TEXT("  Loading/Saving not yet implemented.") );
			break;
		case CMD_BREAKPOINT_ADD_REG:
			ConsoleBufferPush( " Usage: [A|X|Y|PC|S] [op] <range | value>" );
			ConsoleBufferPush( "  Set breakpoint when reg is [op] value"   );
			ConsoleBufferPush( "  Default operator is '='"                 );
			wsprintf(   sText, " See also: %s", g_aParameters[ PARAM_CAT_OPERATORS ].m_sName ); ConsoleBufferPush( sText );
			ConsoleBufferPush( " Examples:"     );
			wsprintf(   sText, "   %s PC < D000", pCommand->m_sName ); ConsoleBufferPush( sText );
			wsprintf(   sText, "   %s PC = F000:FFFF PC < D000,1000", pCommand->m_sName ); ConsoleBufferPush( sText );
			wsprintf(   sText, "   %s A  <= D5"    , pCommand->m_sName ); ConsoleBufferPush( sText );
			wsprintf(   sText, "   %s A  != 01:FF" , pCommand->m_sName ); ConsoleBufferPush( sText );
			wsprintf(   sText, "   %s X  = A5"     , pCommand->m_sName ); ConsoleBufferPush( sText );
			break;
		case CMD_BREAKPOINT_ADD_SMART:
			ConsoleBufferPush( TEXT(" Usage: [address | register]") );
			ConsoleBufferPush( TEXT("  If address, sets two breakpoints" ) );
			ConsoleBufferPush( TEXT("    1. one memory access at address" ) );
			ConsoleBufferPush( TEXT("    2. if PC reaches address" ) );
//			"Sets a breakpoint at the current PC or specified address." ) );
			ConsoleBufferPush( TEXT("  If an IO address, sets breakpoint on IO access.") );
			ConsoleBufferPush( TEXT("  If register, sets a breakpoint on memory access at address of register.") );
			break;
		case CMD_BREAKPOINT_ADD_PC:
			ConsoleBufferPush( TEXT(" Usage: [address]") );
			ConsoleBufferPush( TEXT("  Sets a breakpoint at the current PC or at the specified address.") );
			break;
		case CMD_BREAKPOINT_CLEAR:
			ConsoleBufferPush( TEXT(" Usage: [# | *]") );
			ConsoleBufferPush( TEXT("  Clears specified breakpoint, or all.") );
			wsprintf( sText,  TEXT("  i.e. %s 1" ), pCommand->m_sName );
			ConsoleBufferPush( sText );
			break;
		case CMD_BREAKPOINT_DISABLE:
			ConsoleBufferPush( TEXT(" Usage: [# [,#] | *]") );
			ConsoleBufferPush( TEXT("  Disable breakpoint previously set, or all.") );
			wsprintf( sText,  TEXT("  i.e. %s 1" ), pCommand->m_sName );
			ConsoleBufferPush( sText );
			break;
		case CMD_BREAKPOINT_ENABLE:
			ConsoleBufferPush( TEXT(" Usage: [# [,#] | *]") );
			ConsoleBufferPush( TEXT("  Re-enables breakpoint previously set, or all.") );
			wsprintf( sText,  TEXT("  i.e. %s 1" ), pCommand->m_sName );
			ConsoleBufferPush( sText );
			break;
		case CMD_BREAKPOINT_LIST:
			break;
	// Config - Load / Save
		case CMD_CONFIG_LOAD:
			ConsoleBufferPush( TEXT(" Usage: [\"filename\"]" ) );
			wsprintf( sText,   TEXT("  Load debugger configuration from '%s', or the specificed file." ), g_sFileNameConfig ); ConsoleBufferPush( sText );
			break;
		case CMD_CONFIG_SAVE:
			ConsoleBufferPush( TEXT(" Usage: [\"filename\"]" ) );
			wsprintf( sText,   TEXT("  Save debugger configuration to '%s', or the specificed file." ), g_sFileNameConfig ); ConsoleBufferPush( sText );
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
			ConsoleBufferPush( TEXT(" Note: All arguments effect the disassembly view" ) );

			wsprintf( sText, TEXT(" Usage: [%s | %s | %s | %s | %s]")
				, g_aParameters[ PARAM_CONFIG_BRANCH ].m_sName
				, g_aParameters[ PARAM_CONFIG_COLON  ].m_sName
				, g_aParameters[ PARAM_CONFIG_OPCODE ].m_sName
				, g_aParameters[ PARAM_CONFIG_SPACES ].m_sName
				, g_aParameters[ PARAM_CONFIG_TARGET ].m_sName );
			ConsoleBufferPush( sText );
			ConsoleBufferPush( TEXT("  Display current settings if no args." ) );

			iParam = PARAM_CONFIG_BRANCH;
			wsprintf( sText, TEXT(" Usage: %s [#]" ), g_aParameters[ iParam ].m_sName );
			ConsoleBufferPush( sText );
			ConsoleBufferPush( TEXT("  Set the type of branch character:" ) );
			wsprintf( sText, TEXT("  %d off, %d plain, %d fancy" ),
				 DISASM_BRANCH_OFF, DISASM_BRANCH_PLAIN, DISASM_BRANCH_FANCY );
			ConsoleBufferPush( sText );
			wsprintf( sText,  TEXT("  i.e. %s %s 1" ), pCommand->m_sName, g_aParameters[ iParam ].m_sName );
			ConsoleBufferPush( sText );

			iParam = PARAM_CONFIG_COLON;
			wsprintf( sText, TEXT(" Usage: %s [0|1]" ), g_aParameters[ iParam ].m_sName );
			ConsoleBufferPush( sText );
			ConsoleBufferPush( TEXT("  Display a colon after the address" ) );
			wsprintf( sText, TEXT("  i.e. %s %s 0" ), pCommand->m_sName, g_aParameters[ iParam ].m_sName );
			ConsoleBufferPush( sText );

			iParam = PARAM_CONFIG_OPCODE;
			wsprintf( sText, TEXT(" Usage: %s [0|1]" ), g_aParameters[ iParam ].m_sName );
			ConsoleBufferPush( sText );
			ConsoleBufferPush( TEXT("  Display opcode(s) after colon" ) );
			wsprintf( sText, TEXT("  i.e. %s %s 1" ), pCommand->m_sName, g_aParameters[ iParam ].m_sName );
			ConsoleBufferPush( sText );

			iParam = PARAM_CONFIG_SPACES;
			wsprintf( sText, TEXT(" Usage: %s [0|1]" ), g_aParameters[ iParam ].m_sName );
			ConsoleBufferPush( sText );
			ConsoleBufferPush( TEXT("  Display spaces between opcodes" ) );
			wsprintf( sText, TEXT("  i.e. %s %s 0" ), pCommand->m_sName, g_aParameters[ iParam ].m_sName );
			ConsoleBufferPush( sText );

			iParam = PARAM_CONFIG_TARGET;
			wsprintf( sText, TEXT(" Usage: %s [#]" ), g_aParameters[ iParam ].m_sName );
			ConsoleBufferPush( sText );
			ConsoleBufferPush( TEXT("  Set the type of target address/value displayed:" ) );
			wsprintf( sText, TEXT("  %d off, %d value only, %d address only, %d both" ),
				DISASM_TARGET_OFF, DISASM_TARGET_VAL, DISASM_TARGET_ADDR, DISASM_TARGET_BOTH );
			ConsoleBufferPush( sText );
			wsprintf( sText, TEXT("  i.e. %s %s %d" ), pCommand->m_sName, g_aParameters[ iParam ].m_sName, DISASM_TARGET_VAL );
			ConsoleBufferPush( sText );
			break;
		}
	// Config - Font
		case CMD_CONFIG_FONT:
			wsprintf( sText, TEXT(" Usage: [%s | %s] \"FontName\" [Height]" ),
				g_aParameters[ PARAM_FONT_MODE ].m_sName, g_aParameters[ PARAM_DISASM ].m_sName );
			ConsoleBufferPush( sText );
			ConsoleBufferPush( TEXT(" i.e. FONT \"Courier\" 12" ) );
			ConsoleBufferPush( TEXT(" i.e. FONT \"Lucida Console\" 12" ) );
			wsprintf( sText, TEXT(" %s Controls line spacing."), g_aParameters[ PARAM_FONT_MODE ].m_sName );
			ConsoleBufferPush( sText );
			wsprintf( sText, TEXT(" Valid values are: %d, %d, %d." ),
				FONT_SPACING_CLASSIC, FONT_SPACING_CLEAN, FONT_SPACING_COMPRESSED );
			ConsoleBufferPush( sText );
			break;

	// Memory
		case CMD_MEMORY_ENTER_BYTE:
			ConsoleBufferPush( TEXT(" Usage: <address | symbol> ## [## ... ##]") );
			ConsoleBufferPush( TEXT("  Sets memory to the specified 8-Bit Values (bytes)" ) );
			break;
		case CMD_MEMORY_ENTER_WORD:
			ConsoleBufferPush( TEXT(" Usage: <address | symbol> #### [#### ... ####]") );
			ConsoleBufferPush( TEXT("  Sets memory to the specified 16-Bit Values (words)" ) );
			break;
		case CMD_MEMORY_FILL:
			ConsoleBufferPush( TEXT(" Usage: <address | symbol> <address | symbol> ##" ) ); 
			ConsoleBufferPush( TEXT("  Fills the memory range with the specified byte" ) );
			ConsoleBufferPush( TEXT("  Can't fill IO address $C0xx" ) );
			break;
//		case CMD_MEM_MINI_DUMP_ASC_1:
//		case CMD_MEM_MINI_DUMP_ASC_2:
		case CMD_MEM_MINI_DUMP_ASCII_1:
		case CMD_MEM_MINI_DUMP_ASCII_2:
			ConsoleBufferPush( TEXT(" Usage: <address | symbol>") ); 
			ConsoleBufferPush( TEXT("  Displays ASCII text in the Mini-Memory area") ); 
			ConsoleBufferPush( TEXT("  ASCII control chars are hilighted") );
			ConsoleBufferPush( TEXT("  ASCII hi-bit chars are normal") ); 
//			break;
//		case CMD_MEM_MINI_DUMP_TXT_LO_1:
//		case CMD_MEM_MINI_DUMP_TXT_LO_2:
		case CMD_MEM_MINI_DUMP_APPLE_1:
		case CMD_MEM_MINI_DUMP_APPLE_2:
			ConsoleBufferPush( TEXT(" Usage: <address | symbol>") ); 
			ConsoleBufferPush( TEXT("  Displays APPLE text in the Mini-Memory area") ); 
			ConsoleBufferPush( TEXT("  APPLE control chars are inverse") );
			ConsoleBufferPush( TEXT("  APPLE hi-bit chars are normal") ); 
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
				ConsoleBufferPush( TEXT(" Usage: [\"Filename\"],address[,length]" ) );
				ConsoleBufferPush( TEXT(" Usage: [\"Filename\"],range"   ) );
				Help_Range();
				ConsoleBufferPush( TEXT("  Notes: If no filename specified, defaults to the last filename (if possible)" ) );
			}
			if (iCommand == CMD_MEMORY_SAVE)
			{			
				ConsoleBufferPush( TEXT(" Usage: [\"Filename\"],address,length"   ) );
				ConsoleBufferPush( TEXT(" Usage: [\"Filename\"],range"   ) );
				Help_Range();
				ConsoleBufferPush( TEXT("  Notes: If no filename specified, defaults to: '####.####.bin'" ) );
				ConsoleBufferPush( TEXT("    Where the form is <address>.<length>.bin"               ) );
			}
			
			ConsoleBufferPush( TEXT(" Examples:" ) );
			ConsoleBufferPush( TEXT("   BSAVE \"test\",FF00,100" ) );
			ConsoleBufferPush( TEXT("   BLOAD \"test\",2000:2010" ) );
			ConsoleBufferPush( TEXT("   BSAVE \"test\",F000:FFFF" ) );
			ConsoleBufferPush( TEXT("   BLOAD \"test\",4000" ) );
			break;
		case CMD_MEMORY_SEARCH:
			ConsoleBufferPush( TEXT(" Usage: range <\"ASCII text\" | 'apple text' | hex>" ) );
			Help_Range();
			ConsoleBufferPush( TEXT("  Where text is of the form:") );
			ConsoleBufferPush( TEXT("    \"...\" designate ASCII text") );
			ConsoleBufferPush( TEXT("    '...' designate Apple High-Bit text") );
			ConsoleBufferPush( TEXT(" Examples: (Text)" ) );
			wsprintf( sText,   TEXT("  %s F000,1000 'Apple'   // search High-Bit"  ), pCommand->m_sName ); ConsoleBufferPush( sText );
			wsprintf( sText,   TEXT("  MT1 @2"                                     ), pCommand->m_sName ); ConsoleBufferPush( sText );
			wsprintf( sText,   TEXT("  %s D000:FFFF \"FLAS\"    // search ASCII "  ), pCommand->m_sName ); ConsoleBufferPush( sText );
			wsprintf( sText,   TEXT("  MA1 @1"                                     ), pCommand->m_sName ); ConsoleBufferPush( sText );
			wsprintf( sText,   TEXT("  %s D000,4000 \"EN\" 'D'  // Mixed text"     ), pCommand->m_sName ); ConsoleBufferPush( sText );
			wsprintf( sText,   TEXT("  MT1 @1"                                     ), pCommand->m_sName ); ConsoleBufferPush( sText );
			wsprintf( sText,   TEXT("  %s D000,4000 'Apple' ? ']'"                 ), pCommand->m_sName ); ConsoleBufferPush( sText );
			break;
		case CMD_MEMORY_SEARCH_HEX:
			ConsoleBufferPush( TEXT(" Usage: range [text | byte1 [byte2 ...]]" ) );
			Help_Range();
			ConsoleBufferPush( TEXT("  Where <byte> is of the form:") );
			ConsoleBufferPush( TEXT("    ##   match specific byte") );
			ConsoleBufferPush( TEXT("    #### match specific 16-bit value") );
			ConsoleBufferPush( TEXT("    ?    match any byte") );
			ConsoleBufferPush( TEXT("    ?#   match any high nibble, match low nibble to specific number") );
			ConsoleBufferPush( TEXT("    #?   match specific high nibble, match any low nibble") );
			ConsoleBufferPush( TEXT(" Examples: (Hex)" ) );
			wsprintf( sText,   TEXT("  %s F000,1000 AD ? C0" ), pCommand->m_sName ); ConsoleBufferPush( sText );
			wsprintf( sText,   TEXT("  U @1"                 ), pCommand->m_sName ); ConsoleBufferPush( sText );
			wsprintf( sText,   TEXT("  %s F000,1000 ?1 C0"   ), pCommand->m_sName ); ConsoleBufferPush( sText );
			wsprintf( sText,   TEXT("  %s F000,1000 5? C0"   ), pCommand->m_sName ); ConsoleBufferPush( sText );
			wsprintf( sText,   TEXT("  %s F000,1000 10 C?"   ), pCommand->m_sName ); ConsoleBufferPush( sText );
			wsprintf( sText,   TEXT("  U @2 - 1"             ), pCommand->m_sName ); ConsoleBufferPush( sText );
			wsprintf( sText,   TEXT("  %s F000:FFFF C030"    ), pCommand->m_sName ); ConsoleBufferPush( sText );
			wsprintf( sText,   TEXT("  U @1 - 1"             ), pCommand->m_sName ); ConsoleBufferPush( sText );
			break;
//		case CMD_MEMORY_SEARCH_APPLE:
//			wsprintf( sText,   TEXT("Deprecated.  Use: %s" ), g_aCommands[ CMD_MEMORY_SEARCH ].m_sName ); ConsoleBufferPush( sText );
//			break;
//		case CMD_MEMORY_SEARCH_ASCII:
//			wsprintf( sText,   TEXT("Deprecated.  Use: %s" ), g_aCommands[ CMD_MEMORY_SEARCH ].m_sName ); ConsoleBufferPush( sText );
//			break;
	// Output
		case CMD_OUTPUT_CALC:
			ConsoleBufferPush( TEXT(" Usage: <address | symbol | expression >" ) );
			ConsoleBufferPush( TEXT("  Expression is one of: + - * / % ^ ~"  ) );
			ConsoleBufferPush( TEXT(" Output order is: Hex Bin Dec Char"     ) );
			ConsoleBufferPush( TEXT("  Note: symbols take piority."          ) );
			ConsoleBufferPush( TEXT("i.e. #A (if you don't want accum. val)" ) );
			ConsoleBufferPush( TEXT("i.e. #F (if you don't want flags val)"  ) );
			break;
		case CMD_OUTPUT_ECHO:
			ConsoleBufferPush( TEXT(" Usage: string"    ) );
			ConsoleBufferPush( TEXT(" Examples:"        ) );
			wsprintf( sText,   TEXT("  %s Checkpoint"), pCommand->m_sName ); ConsoleBufferPush( sText );
			wsprintf( sText,   TEXT("  %s PC"        ), pCommand->m_sName ); ConsoleBufferPush( sText );
//			ConsoleBufferPush( TEXT("  Echo the string to the console" ) );
			break;
		case CMD_OUTPUT_PRINT: 
			ConsoleBufferPush( TEXT(" Usage: <string | expression> [, string | expression]*" ) );
			ConsoleBufferPush( TEXT("  Note: To print Register values, they must be in upper case"          ) );
			ConsoleBufferPush( TEXT(" Examples:") );
			wsprintf( sText,   TEXT("  %s \"A:\",A,\" X:\",X"), pCommand->m_sName ); ConsoleBufferPush( sText );
			wsprintf( sText,   TEXT("  %s A,\" \",X,\" \",Y" ), pCommand->m_sName ); ConsoleBufferPush( sText );
			wsprintf( sText,   TEXT("  %s PC"                ), pCommand->m_sName ); ConsoleBufferPush( sText );
			break;
		case CMD_OUTPUT_PRINTF:
			ConsoleBufferPush( TEXT(" Usage: <string> [, expression, ...]" ) );
			ConsoleBufferPush( TEXT("  The string may contain c-style printf formatting flags: %d %x %z %c" ) );
			ConsoleBufferPush( TEXT("  Where: %d decimal, %x hex, %z binary, %c char, %% percent" ) );
			ConsoleBufferPush( TEXT(" Examples:") );
			wsprintf( sText  , TEXT("  %s \"Dec: %%d  Hex: %%x  Bin: %%z  Char: %c\",A,A,A,A" ), pCommand->m_sName ); ConsoleBufferPush( sText );
			wsprintf( sText,   TEXT("  %s \"%%x %%x %%x\",A,X,Y"                              ), pCommand->m_sName ); ConsoleBufferPush( sText );

			break;
	// Symbols
		case CMD_SYMBOLS_LOOKUP:
			ConsoleBufferPush( TEXT(" Usage: symbol [= <address>]"                                               ) );
			ConsoleBufferPush( TEXT(" Examples:" ) );
			wsprintf( sText,   TEXT("  %s HOME"        ),pCommand->m_sName ); ConsoleBufferPush( sText );
			wsprintf( sText,   TEXT("  %s LIFE = 2000" ),pCommand->m_sName ); ConsoleBufferPush( sText );
			wsprintf( sText,   TEXT("  %s LIFE"        ),pCommand->m_sName ); ConsoleBufferPush( sText );
			break;
		case CMD_SYMBOLS_MAIN:
		case CMD_SYMBOLS_USER:
		case CMD_SYMBOLS_SRC :
//			ConsoleBufferPush( TEXT(" Usage: [ ON | OFF | symbol | address ]" ) );
//			ConsoleBufferPush( TEXT(" Usage: [ LOAD [\"filename\"] | SAVE \"filename\"]" ) ); 
//			ConsoleBufferPush( TEXT("  ON  : Turns symbols on in the disasm window" ) );
//			ConsoleBufferPush( TEXT("  OFF : Turns symbols off in the disasm window" ) );
//			ConsoleBufferPush( TEXT("  LOAD: Loads symbols from last/default filename" ) );
//			ConsoleBufferPush( TEXT("  SAVE: Saves symbol table to file" ) );
//			ConsoleBufferPush( TEXT(" CLEAR: Clears the symbol table" ) );
			ConsoleBufferPush( TEXT(" Usage: [ <cmd> | symbol | address ]") );
			ConsoleBufferPush( TEXT("  Where <cmd> is one of:" ) );
			wsprintf( sText, TEXT("  %s  " ": Turns symbols on in the disasm window"        ), g_aParameters[ PARAM_ON    ].m_sName ); ConsoleBufferPush( sText );
			wsprintf( sText, TEXT("  %s "  ": Turns symbols off in the disasm window"       ), g_aParameters[ PARAM_OFF   ].m_sName ); ConsoleBufferPush( sText );
			wsprintf( sText, TEXT("  %s"   ": Loads symbols from last/default \"filename\"" ), g_aParameters[ PARAM_SAVE  ].m_sName ); ConsoleBufferPush( sText );
			wsprintf( sText, TEXT("  %s"   ": Saves symbol table to \"filename\""           ), g_aParameters[ PARAM_LOAD  ].m_sName ); ConsoleBufferPush( sText );
			wsprintf( sText, TEXT(" %s"    ": Clears the symbol table"                      ), g_aParameters[ PARAM_CLEAR ].m_sName ); ConsoleBufferPush( sText );
			break;
		case CMD_SYMBOLS_LIST :
			ConsoleBufferPush( TEXT(" Usage: symbol"                                               ) );
			ConsoleBufferPush( TEXT("  Looks up symbol in all 3 symbol tables: main, user, source" ) );
			break;
	// Watches
		case CMD_WATCH_ADD:
			ConsoleBufferPush( TEXT(" Usage: <address | symbol>" ) );
			ConsoleBufferPush( TEXT("  Adds the specified memory location to the watch window." ) );
			break;
	// Window
		case CMD_WINDOW_CODE    : // summary is good enough
		case CMD_WINDOW_CODE_2  : // summary is good enough
		case CMD_WINDOW_SOURCE_2: // summary is good enough
			break;
	// Zero Page pointers
		case CMD_ZEROPAGE_POINTER:
		case CMD_ZEROPAGE_POINTER_ADD:
			ConsoleBufferPush(" Usage: <address | symbol>" );
			ConsoleBufferPush(" Usage: # <address | symbol> [address...]" );
			ConsoleBufferPush("  Adds the specified memory location to the zero page pointer window." );
			ConsoleBufferPush("  Update the specified zero page pointer (#) with the address." );
			ConsoleBufferPush(" Note: Displayed as symbol name (if possible) and the 16-bit target pointer" );
			ConsoleBufferPush(" Examples:" );
			wsprintf( sText,  "  %s CH", pCommand->m_sName );
			ConsoleBufferPush( sText );
			wsprintf( sText,  "  %s 0 CV", pCommand->m_sName );
			ConsoleBufferPush( sText );
			wsprintf( sText,  "  %s 0 CV CH", pCommand->m_sName );
			ConsoleBufferPush( sText );
			break;
		case CMD_ZEROPAGE_POINTER_CLEAR:
			ConsoleBufferPush( TEXT(" Usage: [# | *]") );
			ConsoleBufferPush( TEXT("  Clears specified zero page pointer, or all.") );
			wsprintf( sText,  TEXT("  i.e. %s 1" ), pCommand->m_sName );
			ConsoleBufferPush( sText );
			break;
		case CMD_ZEROPAGE_POINTER_0:
		case CMD_ZEROPAGE_POINTER_1:
		case CMD_ZEROPAGE_POINTER_2:
		case CMD_ZEROPAGE_POINTER_3:
		case CMD_ZEROPAGE_POINTER_4:
		case CMD_ZEROPAGE_POINTER_5:
		case CMD_ZEROPAGE_POINTER_6:
		case CMD_ZEROPAGE_POINTER_7:
			ConsoleBufferPush( TEXT(" Usage: [<address | symbol>]" ) );
			ConsoleBufferPush( TEXT("  If no address specified, will remove watching the zero page pointer." ) );
			break;

	// Misc
		case CMD_VERSION:
			ConsoleBufferPush( TEXT(" Usage: [*]") );
			ConsoleBufferPush( TEXT("  * Display extra internal stats" ) );
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
			wsprintf( sText, "Command help not done yet: %s", g_aCommands[ iCommand ].m_sName );
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
	TCHAR sText[ CONSOLE_WIDTH ] = TEXT("Commands: ");
	int nLenLine = _tcslen( sText );
	int y = 0;
	int nLinesScrolled = 0;

	int nMaxWidth = g_nConsoleDisplayWidth - 1;
	int iCommand;

/*
	if (! g_vSortedCommands.size())
	{
		for (iCommand = 0; iCommand < NUM_COMMANDS_WITH_ALIASES; iCommand++ )
		{
//			TCHAR *pName = g_aCommands[ iCommand ].aName );
			g_vSortedCommands.push_back( g_aCommands[ iCommand ] );
		}

		std::sort( g_vSortedCommands.begin(), g_vSortedCommands.end(), commands_functor_compare() );
	}
	int nCommands = g_vSortedCommands.size();
*/
	for( iCommand = 0; iCommand < NUM_COMMANDS_WITH_ALIASES; iCommand++ ) // aliases are not printed
	{
//		Command_t *pCommand = & g_vSortedCommands.at( iCommand );
		Command_t *pCommand = & g_aCommands[ iCommand ];
		TCHAR     *pName = pCommand->m_sName;

		if (! pCommand->pFunction)
			continue; // not implemented function

		int nLenCmd = _tcslen( pName );
		if ((nLenLine + nLenCmd) < (nMaxWidth))
		{
			_tcscat( sText, pName );
		}
		else
		{
			ConsoleBufferPush( sText );
			nLenLine = 1;
			_tcscpy( sText, TEXT(" " ) );
			_tcscat( sText, pName );
		}
		
		_tcscat( sText, TEXT(" ") );
		nLenLine += (nLenCmd + 1);
	}

	ConsoleBufferPush( sText );
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

//	wsprintf( sText, "Version" );	ConsoleBufferPush( sText );
	wsprintf( sText, "  Emulator: %s    Debugger: %d.%d.%d.%d"
		, VERSIONSTRING
		, nMajor, nMinor, nFixMajor, nFixMinor );
	ConsoleBufferPush( sText );

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

