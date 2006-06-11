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

	bool bOverflow = (nSpcDst < nLenSrc);
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

		_tcscpy( sText, TEXT("Usage: [{ ") );
		for (int iCategory = _PARAM_HELPCATEGORIES_BEGIN ; iCategory < _PARAM_HELPCATEGORIES_END; iCategory++)
		{
			TCHAR *pName = g_aParameters[ iCategory ].m_sName;
			if (! TestStringCat( sText, pName, g_nConsoleDisplayWidth - 3 )) // CONSOLE_WIDTH
			{
				ConsoleBufferPush( sText );
				_tcscpy( sText, TEXT("    ") );
			}

			StringCat( sText, pName, CONSOLE_WIDTH );
			if (iCategory < (_PARAM_HELPCATEGORIES_END - 1))
			{
				StringCat( sText, TEXT(" | "), CONSOLE_WIDTH );
			}
		}
		StringCat( sText, TEXT(" }]"), CONSOLE_WIDTH );
		ConsoleBufferPush( sText );

		wsprintf( sText, TEXT("Note: [] = optional, {} = mandatory"), CONSOLE_WIDTH );
		ConsoleBufferPush( sText );
	}


	bool bAllCommands = false;
	bool bCategory = false;

	if (! _tcscmp( g_aArgs[1].sArg, g_aParameters[ PARAM_WILDSTAR ].m_sName))
	{
		bAllCommands = true;
		nArgs = NUM_COMMANDS;
	}

	// If Help on category, push command name as arg
	int nNewArgs  = 0;
	int iCmdBegin = 0;
	int iCmdEnd   = 0;
	for (iArg = 1; iArg <= nArgs; iArg++ )
	{
		int iParam;
		int nFoundCategory = FindParam( g_aArgs[ iArg ].sArg, MATCH_EXACT, iParam, _PARAM_HELPCATEGORIES_BEGIN, _PARAM_HELPCATEGORIES_END );
		switch( iParam )
		{
			case PARAM_CAT_BREAKPOINTS: iCmdBegin = CMD_BREAKPOINT      ; iCmdEnd = CMD_BREAKPOINT_SAVE    + 1; break;
			case PARAM_CAT_CONFIG     : iCmdBegin = CMD_CONFIG_COLOR    ; iCmdEnd = CMD_CONFIG_SAVE        + 1; break;
			case PARAM_CAT_CPU        : iCmdBegin = CMD_ASSEMBLE        ; iCmdEnd = CMD_TRACE_LINE         + 1; break;
			case PARAM_CAT_FLAGS      : iCmdBegin = CMD_FLAG_CLEAR      ; iCmdEnd = CMD_FLAG_SET_N         + 1; break;
			case PARAM_CAT_MEMORY     : iCmdBegin = CMD_MEMORY_COMPARE  ; iCmdEnd = CMD_MEMORY_FILL        + 1; break;
			case PARAM_CAT_SYMBOLS    : iCmdBegin = CMD_SYMBOLS_LOOKUP  ; iCmdEnd = CMD_SYMBOLS_LIST       + 1; break;
			case PARAM_CAT_WATCHES    : iCmdBegin = CMD_WATCH_ADD       ; iCmdEnd = CMD_WATCH_LIST         + 1; break;
			case PARAM_CAT_WINDOW     : iCmdBegin = CMD_WINDOW          ; iCmdEnd = CMD_WINDOW_OUTPUT      + 1; break;
			case PARAM_CAT_ZEROPAGE   : iCmdBegin = CMD_ZEROPAGE_POINTER; iCmdEnd = CMD_ZEROPAGE_POINTER_SAVE+1;break;
			default: break;
		}
		nNewArgs = (iCmdEnd - iCmdBegin);
		if (nNewArgs > 0)
			break;
	}

	if (nNewArgs > 0)
	{
		bCategory = true;

		nArgs = nNewArgs;
		for (iArg = 1; iArg <= nArgs; iArg++ )
		{
			g_aArgs[ iArg ].nVal2 = iCmdBegin + iArg - 1;
		}
	}

	CmdFuncPtr_t pFunction;
	
	for (iArg = 1; iArg <= nArgs; iArg++ )
	{	
		int iCommand = 0;
		int nFound = FindCommand( g_aArgs[iArg].sArg, pFunction, & iCommand );

		if (bCategory)
		{
			iCommand = g_aArgs[iArg].nVal2;
		}

		if (bAllCommands)
		{
			iCommand = iArg;
			if (iCommand == NUM_COMMANDS) // skip: Internal Consistency Check __COMMANDS_VERIFY_TXT__
				continue;
		}

		if (nFound > 1)
		{
			DisplayAmbigiousCommands( nFound );
		}

		if (iCommand > NUM_COMMANDS)
			continue;

		if ((nArgs == 1) && (! nFound))
			iCommand = g_aArgs[iArg].nVal1;

		Command_t *pCommand = & g_aCommands[ iCommand ];

		if (! nFound)
		{
			iCommand = NUM_COMMANDS;
			pCommand = NULL;
		}
		
		if (nFound && (! bAllCommands))
		{
			TCHAR sCategory[ CONSOLE_WIDTH ];
			int iCmd = g_aCommands[ iCommand ].iCommand; // Unaliased command

			// HACK: Major kludge to display category!!!
			if (iCmd <= CMD_TRACE_LINE)
				wsprintf( sCategory, "Main" );
			else
			if (iCmd <= CMD_BREAKPOINT_SAVE)
				wsprintf( sCategory, "Breakpoint" );
			else
			if (iCmd <= CMD_PROFILE)
				wsprintf( sCategory, "Profile" );
			else
			if (iCmd <= CMD_CONFIG_SAVE)
				wsprintf( sCategory, "Config" );
			else
			if (iCmd <= CMD_CURSOR_PAGE_DOWN_4K)
				wsprintf( sCategory, "Scrolling" );
			else
			if (iCmd <= CMD_FLAG_SET_N)
				wsprintf( sCategory, "Flags" );
			else
			if (iCmd <= CMD_MOTD)
				wsprintf( sCategory, "Help" );
			else
			if (iCmd <= CMD_MEMORY_FILL)
				wsprintf( sCategory, "Memory" );
			else
			if (iCmd <= CMD_REGISTER_SET)
				wsprintf( sCategory, "Registers" );
			else
			if (iCmd <= CMD_SYNC)
				wsprintf( sCategory, "Source" );
			else
			if (iCmd <= CMD_STACK_PUSH)
				wsprintf( sCategory, "Stack" );
			else
			if (iCmd <= CMD_SYMBOLS_LIST)
				wsprintf( sCategory, "Symbols" );
			else
			if (iCmd <= CMD_WATCH_SAVE)
				wsprintf( sCategory, "Watch" );
			else
			if (iCmd <= CMD_WINDOW_OUTPUT)
				wsprintf( sCategory, "Window" );
			else
			if (iCmd <= CMD_ZEROPAGE_POINTER_SAVE)
				wsprintf( sCategory, "Zero Page" );
			else
				wsprintf( sCategory, "Unknown!" );

			wsprintf( sText, "Category: %s", sCategory );
			ConsoleBufferPush( sText );
		}
		
		if (pCommand)
		{
			char *pHelp = pCommand->pHelpSummary;
			if (pHelp)
			{
				wsprintf( sText, "%s, ", pCommand->m_sName );
				if (! TryStringCat( sText, pHelp, g_nConsoleDisplayWidth ))
				{
					if (! TryStringCat( sText, pHelp, CONSOLE_WIDTH ))
					{
						StringCat( sText, pHelp, CONSOLE_WIDTH );
						ConsoleBufferPush( sText );
					}
				}
				ConsoleBufferPush( sText );
			}
			else
			{
				wsprintf( sText, "%s", pCommand->m_sName );
				ConsoleBufferPush( sText );
	#if DEBUG_COMMAND_HELP
				if (! bAllCommands) // Release version doesn't display message
				{			
					wsprintf( sText, "Missing Summary Help: %s", g_aCommands[ iCommand ].aName );
					ConsoleBufferPush( sText );
				}
	#endif
			}
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
		case CMD_CALC:
			ConsoleBufferPush( TEXT(" Usage: {address | symbol | + | - }"    ) );
			ConsoleBufferPush( TEXT(" Output order is: Hex Bin Dec Char"     ) );
			ConsoleBufferPush( TEXT("  Note: symbols take piority."          ) );
			ConsoleBufferPush( TEXT("i.e. #A (if you don't want accum. val)" ) );
			ConsoleBufferPush( TEXT("i.e. #F (if you don't want flags val)"  ) );
			break;
		case CMD_GO:
			ConsoleBufferPush( TEXT(" Usage: [address | symbol [Skip,End]]") );
			ConsoleBufferPush( TEXT(" Skip: Start address to skip stepping" ) );
			ConsoleBufferPush( TEXT(" End : End address to skip stepping" ) );
			ConsoleBufferPush( TEXT("  If the Program Counter is outside the" ) );
			ConsoleBufferPush( TEXT(" skip range, resumes single-stepping." ) );
			ConsoleBufferPush( TEXT("  Can be used to skip ROM/OS/user code." ));
			ConsoleBufferPush( TEXT(" i.e.  G C600 F000,FFFF" ) );
			break;
		case CMD_NOP:
			ConsoleBufferPush( TEXT("  Puts a NOP opcode at current instruction") );
			break;
		case CMD_JSR:
			ConsoleBufferPush( TEXT(" Usage: {symbol | address}") );
			ConsoleBufferPush( TEXT("  Pushes PC on stack; calls the named subroutine.") );
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
		case CMD_SOURCE:
			ConsoleBufferPush( TEXT(" Reads assembler source file." ) );
			wsprintf( sText, TEXT(" Usage: [ %s | %s ] \"filename\""            ), g_aParameters[ PARAM_SRC_MEMORY  ].m_sName, g_aParameters[ PARAM_SRC_SYMBOLS ].m_sName ); ConsoleBufferPush( sText );
			wsprintf( sText, TEXT("   %s: read source bytes into memory."        ), g_aParameters[ PARAM_SRC_MEMORY  ].m_sName ); ConsoleBufferPush( sText );
			wsprintf( sText, TEXT("   %s: read symbols into Source symbol table."), g_aParameters[ PARAM_SRC_SYMBOLS ].m_sName ); ConsoleBufferPush( sText );
			wsprintf( sText, TEXT(" Supports: %s."                              ), g_aParameters[ PARAM_SRC_MERLIN  ].m_sName ); ConsoleBufferPush( sText );
			break;
		case CMD_STEP_OUT: 
			ConsoleBufferPush( TEXT("  Steps out of current subroutine") );
			ConsoleBufferPush( TEXT("  Hotkey: Ctrl-Space" ) );
			break;
		case CMD_STEP_OVER: // Bad name? FIXME/TODO: do we need to rename?
			ConsoleBufferPush( TEXT(" Usage: [#]") );
			ConsoleBufferPush( TEXT("  Steps, # times, thru current instruction") );
			ConsoleBufferPush( TEXT("  JSR will be stepped into AND out of.") );
			ConsoleBufferPush( TEXT("  Hotkey: Ctrl-Space" ) );
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
	// Breakpoints
		case CMD_BREAKPOINT:
			wsprintf( sText, " Maximum breakpoints are: %d", NUM_BREAKPOINTS );
			ConsoleBufferPush( sText );
			break;
		case CMD_BREAKPOINT_ADD_REG:
			ConsoleBufferPush( TEXT(" Usage: [A|X|Y|PC|S] [<,=,>] value") );
			ConsoleBufferPush( TEXT("  Set breakpoint when reg is [op] value") );
			break;
		case CMD_BREAKPOINT_ADD_SMART:
		case CMD_BREAKPOINT_ADD_PC:
			ConsoleBufferPush( TEXT(" Usage: [address]") );
			ConsoleBufferPush( TEXT("  Sets a breakpoint at the current PC") );
			ConsoleBufferPush( TEXT("  or at the specified address.") );
			break;
		case CMD_BREAKPOINT_ENABLE:
			ConsoleBufferPush( TEXT(" Usage: [# [,#] | *]") );
			ConsoleBufferPush( TEXT("  Re-enables breakpoint previously set, or all.") );
			break;
	// Config - Color
		case CMD_CONFIG_COLOR:
			ConsoleBufferPush( TEXT(" Usage: [{#} | {# RR GG BB}]" ) );
			ConsoleBufferPush( TEXT("  0 params: switch to 'color' scheme" ) );
			ConsoleBufferPush( TEXT("  1 param : dumps R G B for scheme 'color'") );
			ConsoleBufferPush( TEXT("  4 params: sets  R G B for scheme 'color'" ) );
			break;
		case CMD_CONFIG_MONOCHROME:
			ConsoleBufferPush( TEXT(" Usage: [{#} | {# RR GG BB}]" ) );
			ConsoleBufferPush( TEXT("  0 params: switch to 'monochrome' scheme" ) );
			ConsoleBufferPush( TEXT("  1 param : dumps R G B for scheme 'monochrome'") );
			ConsoleBufferPush( TEXT("  4 params: sets  R G B for scheme 'monochrome'" ) );
			break;
		case CMD_OUTPUT:
			ConsoleBufferPush( TEXT(" Usage: {address8 | address16 | symbol} ## [##]") );
			ConsoleBufferPush( TEXT("  Ouput a byte or word to the IO address $C0xx" ) );
			break;
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
			ConsoleBufferPush( TEXT(" Usage: {address | symbol} ## [## ... ##]") );
			ConsoleBufferPush( TEXT("  Sets memory to the specified 8-Bit Values (bytes)" ) );
			break;
		case CMD_MEMORY_ENTER_WORD:
			ConsoleBufferPush( TEXT(" Usage: {address | symbol} #### [#### ... ####]") );
			ConsoleBufferPush( TEXT("  Sets memory to the specified 16-Bit Values (words)" ) );
			break;
		case CMD_MEMORY_FILL:
			ConsoleBufferPush( TEXT(" Usage: {address | symbol} {address | symbol} ##" ) ); 
			ConsoleBufferPush( TEXT("  Fills the memory range with the specified byte" ) );
			ConsoleBufferPush( TEXT("  Can't fill IO address $C0xx" ) );
			break;
//		case CMD_MEM_MINI_DUMP_ASC_1:
//		case CMD_MEM_MINI_DUMP_ASC_2:
		case CMD_MEM_MINI_DUMP_ASCII_1:
		case CMD_MEM_MINI_DUMP_ASCII_2:
			ConsoleBufferPush( TEXT(" Usage: {address | symbol}") ); 
			ConsoleBufferPush( TEXT("  Displays ASCII text in the Mini-Memory area") ); 
			ConsoleBufferPush( TEXT("  ASCII control chars are hilighted") );
			ConsoleBufferPush( TEXT("  ASCII hi-bit chars are normal") ); 
//			break;
//		case CMD_MEM_MINI_DUMP_TXT_LO_1:
//		case CMD_MEM_MINI_DUMP_TXT_LO_2:
		case CMD_MEM_MINI_DUMP_APPLE_1:
		case CMD_MEM_MINI_DUMP_APPLE_2:
			ConsoleBufferPush( TEXT(" Usage: {address | symbol}") ); 
			ConsoleBufferPush( TEXT("  Displays APPLE text in the Mini-Memory area") ); 
			ConsoleBufferPush( TEXT("  APPLE control chars are inverse") );
			ConsoleBufferPush( TEXT("  APPLE hi-bit chars are normal") ); 
			break;
//		case CMD_MEM_MINI_DUMP_TXT_HI_1:
//		case CMD_MEM_MINI_DUMP_TXT_HI_2:
//			ConsoleBufferPush( TEXT(" Usage: {address | symbol}") ); 
//			ConsoleBufferPush( TEXT("  Displays text in the Memory Mini-Dump area") ); 
//			ConsoleBufferPush( TEXT("  ASCII chars with the hi-bit set, is inverse") ); 
			break;
	// Symbols
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
			ConsoleBufferPush( TEXT(" Usage: [ ... | symbol | address ]") );
			ConsoleBufferPush( TEXT(" Where ... is one of:" ) );
			wsprintf( sText, TEXT("  %s  " ": Turns symbols on in the disasm window"        ), g_aParameters[ PARAM_ON    ].m_sName ); ConsoleBufferPush( sText );
			wsprintf( sText, TEXT("  %s "  ": Turns symbols off in the disasm window"       ), g_aParameters[ PARAM_OFF   ].m_sName ); ConsoleBufferPush( sText );
			wsprintf( sText, TEXT("  %s"   ": Loads symbols from last/default \"filename\"" ), g_aParameters[ PARAM_SAVE  ].m_sName ); ConsoleBufferPush( sText );
			wsprintf( sText, TEXT("  %s"   ": Saves symbol table to \"filename\""           ), g_aParameters[ PARAM_LOAD  ].m_sName ); ConsoleBufferPush( sText );
			wsprintf( sText, TEXT(" %s"    ": Clears the symbol table"                      ), g_aParameters[ PARAM_CLEAR ].m_sName ); ConsoleBufferPush( sText );
			break;
	// Watches
		case CMD_WATCH_ADD:
			ConsoleBufferPush( TEXT(" Usage: {address | symbol}" ) );
			ConsoleBufferPush( TEXT("  Adds the specified memory location to the watch window." ) );
			break;
	// Window
		case CMD_WINDOW_CODE    : // summary is good enough
		case CMD_WINDOW_CODE_2  : // summary is good enough
		case CMD_WINDOW_SOURCE_2: // summary is good enough
			break;

	// Misc
		case CMD_VERSION:
			ConsoleBufferPush( TEXT(" Usage: [*]") );
			ConsoleBufferPush( TEXT("  * Display extra internal stats" ) );
			break;
		default:
			if (bAllCommands)
				break;
#if DEBUG_COMMAND_HELP
			wsprintf( sText, "Command help not done yet: %s", g_aCommands[ iCommand ].aName );
			ConsoleBufferPush( sText );
#endif
			if ((! nFound) || (! pCommand))
			{
				wsprintf( sText, " Invalid command." );
				ConsoleBufferPush( sText );
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
		for (int iArg = 1; iArg <= nArgs; iArg++ )
		{
			if (_tcscmp( g_aArgs[ iArg ].sArg, g_aParameters[ PARAM_WILDSTAR ].m_sName ) == 0)
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

