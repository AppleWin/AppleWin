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

// disable warning C4786: symbol greater than 255 character:
#pragma warning(disable: 4786)

#include "StdAfx.h"
#pragma  hdrstop

// NEW UI debugging
//	#define DEBUG_FORCE_DISPLAY 1
//  #define DEBUG_COMMAND_HELP  1
// #define DEBUG_ASM_HASH 1

// enable warning C4786: symbol greater than 255 character:
//#pragma warning(enable: 4786)

	// TCHAR DEBUG_VERSION = "0.0.0.0"; // App Version
	#define DEBUGGER_VERSION MAKE_VERSION(2,4,2,16);

// TODO: COLOR RESET
// TODO: COLOR SAVE ["filename"]
// TODO: COLOR LOAD ["filename"]

// Patch 22
// .16 fixed BPM. i.e. BPM C400,100.  Boot: ulti4bo.dsk, breaks at: 81BC: STA ($88),Y    $0088=$C480 ... C483: A0

// Patch 21
// .15 Fixed: CmdRegisterSet() equal sign is now optional. i.e. R A [=] Y
// .14 Optimized: ArgsParse()
// .13 Fixed: HELP "..." on ambigious command displays potential commands
// .12 Added: Token %, calculator mod
// .11 Added: Token /, calculator div
// .10 Added: Token ^, calculator bit-xor
// .9 Added: Token |, calculator bit-or
// .8 Added: Token &, calculator bit-and
// .7 Added: Mini-Assembler
// .6 Added: MOTD (Message of the Day) : Moved from DebugBegin()
// .5 Added: HELP SOURCE
// .4 Fixed: SYMMAIN SYMUSER SYMSRC to now only search their respective symbol tables
// .3 Added: _CmdSymbolList_Address2Symbol() _CmdSymbolList_Symbol2Address() now take bit-flags of which tables to search
// .2 Added: EB alias for E
// .1 Added: EW address value16  CmdMemoryEnterWord() to set memory with 16-Bit Values
// 2.4.2.0
// .14 Added: SYM ! name to remove (user) symbol. _CmdSymbolsUpdate()
// .13 Added: JSR address|symbol CmdJSR()
// .12 Fixed: SYM address|symbol now reports if address or symbol not found. CmdSymbolsList()
// .11 Fixed PageUp*, PagDn* not being recognized valid commands.

// Patch 20
// .10 Added: SYM2 symbolname = value
// .9 Added: Undocumented command: FONT *
// .8 Improved: FONT MODE # is now more accurate. i.e. U 301
//    FONT MODE 0  Classic line spacing  (19 lines: 301 .. 313)
//    FONT MODE 1  Improved line spacing (20 lines: 301 .. 314)
//    FONT MODE 2  Optimal line spacing  (22 lines: 301 .. 316)
// .7 Fixed: ArgsParse() wasn't parsing #value properly. i.e. CALC #A+0A
// .6 Cleanup: DrawWatches()

// Patch 19
// .5 Fixed: ProfileSave() doesn't strip '%' percent signs anymore. Changed: fprintf() to fputs()
//    Fixed: PROFILE RESET doesn't do dummy run of formatting profile data.
// .4 Fixed: Exporting Profile data is now Excel/OpenOffice friendly.
//           Zero counts not listed on console. (They are when saved to file.)

// Patch 18
// .3 Fixed: Help_Arg_1() now copies command name into arg.name
// .2 Fixed: Temporarily removed extra windows that aren't done yet from showing up: WINDOW_IO WINDOW_SYMBOLS WINDOW_ZEROPAGE WINDOW_SOURCE
// .1 Added: PROFILE LIST -- can now view the profile data from witin the debugger!
// 2.4.1 Added: PROFILE SAVE -- now we can optimize (D0 BNE), and (10 BPL) sucking up the major of the emulator's time. WOOT!
//     Added: _6502GetOpmodeOpbyte() and _6502GetOpcodeOpmode()
// .37 Fixed: Misc. commands not updating the console when processed

// Patch 17
// .36 Data window now shows text dump based on memory view set (ASCII or APPLE)
//     MA1 D0D0; DATA;
//     MT1 D0D0; DATA;
//     D   D0D0; DATA;
// .35 Renamed: ML1 ML2 to MT1 MT2 (Mem Text)
// .34 Removed: MH1 MH2 (redundant MEM_VIEW_APPLE_TEXT)
// .33 Fixed: Tweaking of Lo,Hi,Norm Ascii colors for Disasm Immediate & Memory Dumps
//     ML1 D0D0; MH2 D0D0; U FA75
// .32 Changed: Lo now maps High Ascii to printable chars. i.e. ML1 D0D0
// .31 Ctrl-Tab, and Ctrl-Shift-Tab now cycle next/prev windows.
// .30 Added: Up,Down,PageUp,PageDown,Shift-PageUp,Shift-PageDown,Ctrl-PageUp,Ctrl-PageDown now scroll Data window
// .29 Added: DATA window now shows memory dump

// Patch 16
// .28 Fixed: MONO wasn't setting monochrome scheme
// .27 Changed: Registers now drawn in light blue for both DrawSubWindow_Info() [DrawRegisters()] and DrawBreakpoints()
//     Reg names in DrawSubWindow_Info no longer hard-coded: using g_areakpointSource[]
// .26 Changed: DrawTargets() now shows Temp Address, and Final Address as orange (FG_INFO_ADDRESS)
// .25 Changed: Flag Clear "FC" to "CL" (like 6502 mnemonic)
//     Changed: Flag Set   "FS" to "SE" (like 6502 mnemonic)
//     Added: Mirrored 6502 Mnemonics to clear/set flags as commands
//     Moved: Legacy Flag Clear/Set commands "R?" "S?" to aliases
// .24 Fixed OPCODE 2F: AM_INVALID1 -> AM_INVALID3
// .23 Fixed OPCODE 0F: AM_INVALID1 -> AM_INVALID3
// .22 Fixed: Shift-PageUp Shift-PageDown Ctrl-PageUp Ctrl-PageDown -> _CursorMoveUpAligned() & _CursorMoveDownAligned()
//   Bug: U F001, Ctrl-PageDown
//   Was using current disasm cursor address instead of top disasm cursor
// .21 Fixed: _CursorMoveUpAligned() & _CursorMoveDownAligned() not wrapping around past FF00 to 0, and wrapping around past 0 to FF00
// .20 Fixed: DisasmCalcTopFromCurAddress() causing bogus g_bDisasmCurBad when disasm invalid opcodes.
// .19 g_aAddressingFormat[] -> g_aOpmodes[]
// .18 Reverted .17 Changed StackPointer Input/Output. 'SP' is now back to 'S' (to be consistent with 6502 mnemonics)

// Patch 15
// .17 Changed StackPointer Input/Output from 'S' to 'SP'
// .16 Fixed Go StopAddress [SkipAddress,Length] Trace() not working afterwards
// .15 Added Info Regs color - Foreground and Background (orange)

// Patch 14
// .14 Added: Stack register now shows stack depth
// .13 Bugfix: CmdBreakpointAddPC() operators now don't add extra breakpoints. i.e. BPX P < FA00
// .12 Bugfix: DrawMemory() should draw memory byte for IO address: ML1 C000

// Patch 13
// NightlyBuild
// .11 Disasm now looks up symbols +1,-1. i.e. U FA85, u FAAE
// .10 Fixed '~' full console height
// .9 Fixed: FindCommand() to update command found if not exact match
// .8 Moved: Flag names from g_aFlagNames[] to "inlined" g_aBreakpointSource[]
// .7 Extended: Calc to show binary values, and char is single-quoted
// Nightly Build
// .6 Bugfix: DrawTargets() should draw target byte for IO address: R PC FB33
// .5 Extended: FONT "FontName" [Height]
// .4 Extended: Go StopAddress [SkipAddress,Len]
// .3 Added BPR F // break on flag
//    Added !     // new breakpoint operator: NOT_EQUAL

// Patch 12
// .2 Fixed: FindCommand() and FindParam() now returns 0 if name length is 0.
// .1 Fixed: CmdHelpSpecific() wasn't printing help for 1 argument set by other commands
// 2.4.0.0 added: uber breakpoints: BPR [A|X|Y|PC|SP] [<,<=,=,>,>=] address
// .2 fixed: parsing console input to detect immediate values on operators '+', '-'
//   You can now do register arithmetic:  CALC A+X+Y
// .1 added: mini-calculator 'CALC'
// 2.3.2 Rewrote parsing of console input
//  You can now do stuff like:
//     BPA INITAN+3
//     FONT "Lucida Console"

// Patch 11
// .19 fixed disasm: new color for disasm target address (orange)
	// U FF14 shouldn't colorize ',X'
	// U FF16 shouldn't colorize ',Y'
	// U FF21 shouldn't colorze  'symbol,Y'
// Addr should be purple or orange to verify is fixed
// .18 fixed disasm: all indirects to do symbol lookup, and colorized '('...')' as operators
// .17 added disasm: syntax coloring to immediate mode prefix '#$'
// .16 fixed disasm window to work with proportional fonts
// .15 fixed buffer-overun crash in TextConvertTabsToSpaces()

// Patch 10
// .14 fixed help CATEGORY & SPECIFIC not working properly
// .13 fixed bug of CODE not removing CODE window split
// .12 fixed split window bug of re-sizing console too large
// .11 removed hard-coded height of 16 for line height in various Draw*()
// .10 added alpha feature: SOURCE2 now shows source assembly listing in bottom half of window DrawSourceLine()
// .9 added P [#] to step number of times specified
// .8 added Up-Arrow, Down-Arrow to scroll console by one line in full screen console window
// .7 Added source level debugger to buffer complete assembly source listing file
// .6 Added help for MA#, ML#, MH#
// .5 added 3rd symbol table: SRC
// .4 Added: seperated console input color, console output color
// .3 SYM now prints which symbol table the symbol was found in.
// .2 Added "DFB" to add assembler variable to user symbol table
// .1 Fixed FindAddressFromSymbol() and FindSymbolFromAddress() to search user symbol table first
// 2.3.1 Renamed all globals to conform to coding standard

// .10 added TAB to move the cursor back to the PC, if there is no console input
// .9 fixed bug of setting reg, always moving the cursor back the PC
// .8 added detection if font is proportional.  Still doesn't draw properly, but slowly getting there.
// .7 added colorized chars for regs, and memory ascii dump
// .6 fixed console buffering BUG: ~,~,VERSION *
// .5 added ~ to toggle full console on/off
// .4 Fixed BUG: 'G' DebugContinueStepping() disasm window cursor getting out of sync:  R PC FA62; U FB40; G FB53
// .3 Fixed spelling of CmdWindowLast()
// .2 Added undocumented "VERSION *" to show the number of bytes in the disasm "window"
// .1 Fixed BUG: CmdTrace() disasm window cursor getting out of sync:  Start, F7, PageUp, Space
// 2.3.0.0 Syntax coloring for the disassembler! Plus a fancy branch indicator to 'PR#6'!  Rock on!
// 38 Fixed FindParam() bug of skipping every other parameter
// 37 Fixed _PARAM_*_BEGIN _PARAM_*_END _PARAM_*_NUM
// 36 Added Regs A,X,Y to show character
// 35 Added syntax coloring for DrawBreakpoints()
// 34 Fixed DrawMemory() viewing memory ASCII to map control-chars to a printable glyph
// 33 Fixed 0 addr not working in CmdMemoryCompare()
// 32 Added BG_INFO_INVERSE, FG_INFO_INVERSE for flag hilighting
// 31 Changed FG_INFO_OPCODE color to yellow
// 30 Fixed MDC 0, bug in CmdUnassemble()with garbage address, due to var not init, and symbol lookup failed
// 29 Fixed typos in help summary, A, and D
// 28 Fixed Scrolling with and without g_bDisasmCurBad
// 27 Found bug -- can't scroll up past $5FFF with Tom's Bug Sample .. CmdCursorLineUp() not checking for gbDisamBad;
// 26 Code cleanup: still have some '#define's to convert over to 'const'
// 25 Fixed disasm singularity not showing up in DrawDisassemblyLine()
// 24 Fixed an disasm singularity in DisasmCalcTopFromCurAddress() i.e. F7, G FB53, then Space (to trace) once.
// 23 Fixed bug Watches not being cleared: _ClearViaArgs()
// 22 Code Cleanup: Moved most ExtTextOut() to DebugDrawText()
// 21 Changed End to jump $8000, due to redundant funcationality of Home, and wrap-around.
// 20 Added: DisasmCalc*From*()
// 19 New colors Enum & Array
// 18 implemented RTS. fixed RET to use common stack return address logic.
// 17 fixed console PageUp PageDn not refreshing console
// .2 WINDOW_CONSOLE & 
//    Implemented commands: MemoryDumpAscii*(): MA1 MA2 ML1 ML2 MH1 MH2
// 16 removed BOOL Cmd*(), replaced with UPDATE_*
// 15 New Console Scrolling
// 14 extended FindParam() to specify [begin,end]
// 13 added split window infastructure, added Window_e
// 12 fixed PARAM aliases
// 11 fixed FindParam() to allow aliases
// 10 fixed SYM1 CLEAR, SYM2 CLEAR not actuall clearing
// 9 fixed BufferOuput() in DebugBegin() clobbering first buffered output line
// .1 since source level debugging is a minor feature
// 8 added command "VERSION *" to show internal debug info
// 7 fixed mem enter not refreshing screen
// 6 added source level debuggig back-end
// 5 fixed "SYM 0" bug

	inline void  UnpackVersion( const unsigned int nVersion,
		int & nMajor_, int & nMinor_, int & nFixMajor_ , int & nFixMinor_ )
	{
		nMajor_    = (nVersion >> 24) & 0xFF;
		nMinor_    = (nVersion >> 16) & 0xFF;
		nFixMajor_ = (nVersion >>  8) & 0xFF;
		nFixMinor_ = (nVersion >>  0) & 0xFF;
	}

	const int MAX_BREAKPOINTS       = 5;
	const int MAX_WATCHES           = 5;
	const int MAX_ZEROPAGE_POINTERS = 5;

	const unsigned int _6502_ZEROPAGE_END    = 0x00FF;
	const unsigned int _6502_STACK_END       = 0x01FF;
	const unsigned int _6502_IO_BEGIN        = 0xC000;
	const unsigned int _6502_IO_END          = 0xC0FF;
	const unsigned int _6502_BEG_MEM_ADDRESS = 0x0000;
	const unsigned int _6502_END_MEM_ADDRESS = 0xFFFF;


// Addressing _____________________________________________________________________________________

	AddressingMode_t g_aOpmodes[ NUM_ADDRESSING_MODES ] =
	{ // Outut, but eventually used for Input when Assembler is working.
		{TEXT("")        , 1 , "(implied)"              }, // (implied)
        {TEXT("")        , 1 , "n/a 1"         }, // INVALID1
        {TEXT("")        , 2 , "n/a 2"         }, // INVALID2
        {TEXT("")        , 3 , "n/a 3"         }, // INVALID3
		{TEXT("%02X")    , 2 , "Immediate"     }, // AM_M // #$%02X -> %02X
        {TEXT("%04X")    , 3 , "Absolute"      }, // AM_A
        {TEXT("%02X")    , 2 , "Zero Page"     }, // AM_Z
        {TEXT("%04X,X")  , 3 , "Absolute,X"    }, // AM_AX     // %s,X
        {TEXT("%04X,Y")  , 3 , "Absolute,Y"    }, // AM_AY     // %s,Y
        {TEXT("%02X,X")  , 2 , "Zero Page,X"   }, // AM_ZX     // %s,X
        {TEXT("%02X,Y")  , 2 , "Zero Page,Y"   }, // AM_ZY     // %s,Y
        {TEXT("%s")      , 2 , "Relative"      }, // AM_R
        {TEXT("(%02X,X)"), 2 , "(Zero Page),X" }, // AM_IZX ADDR_INDX     // ($%02X,X) -> %s,X 
        {TEXT("(%04X,X)"), 3 , "(Absolute),X"  }, // AM_IAX ADDR_ABSIINDX // ($%04X,X) -> %s,X
        {TEXT("(%02X),Y"), 2 , "(Zero Page),Y" }, // AM_NZY ADDR_INDY     // ($%02X),Y
        {TEXT("(%02X)")  , 2 , "(Zero Page)"   }, // AM_NZ ADDR_IZPG     // ($%02X) -> $%02X
        {TEXT("(%04X)")  , 3 , "(Absolute)"    }  // AM_NA ADDR_IABS     // (%04X) -> %s
	};


// Args ___________________________________________________________________________________________
	int   g_nArgRaw;
	Arg_t g_aArgRaw[ MAX_ARGS ]; // pre-processing

	Arg_t g_aArgs  [ MAX_ARGS ]; // post-processing


	const TCHAR CHAR_LF    = 0x0D;
	const TCHAR CHAR_CR    = 0x0A;
	const TCHAR CHAR_SPACE = TEXT(' ');
	const TCHAR CHAR_TAB   = TEXT('\t');
	const TCHAR CHAR_QUOTED= TEXT('"' );

	// NOTE: Token_e and g_aTokens must match!
	const TokenTable_t g_aTokens[ NUM_TOKENS ] =
	{ // Input
		{ TOKEN_ALPHANUMERIC, TYPE_STRING  , 0          }, // Default, if doen't match anything else
		{ TOKEN_AMPERSAND   , TYPE_OPERATOR, TEXT('&')  }, // bit-and
//		{ TOKEN_AT          , TYPE_OPERATOR, TEXT('@')  }, // force Register? or PointerDeref?
		{ TOKEN_BSLASH      , TYPE_OPERATOR, TEXT('\\') },
		{ TOKEN_CARET       , TYPE_OPERATOR, TEXT('^')  }, // bit-eor, C/C++: xor, Math: POWER
		{ TOKEN_COLON       , TYPE_OPERATOR, TEXT(':')  }, 
		{ TOKEN_COMMA       , TYPE_OPERATOR, TEXT(',')  },
		{ TOKEN_DOLLAR      , TYPE_STRING  , TEXT('$')  },
		{ TOKEN_EQUAL       , TYPE_OPERATOR, TEXT('=')  },
		{ TOKEN_EXCLAMATION , TYPE_OPERATOR, TEXT('!')  }, // NOT
		{ TOKEN_FSLASH      , TYPE_OPERATOR, TEXT('/')  }, // div
		{ TOKEN_GREATER_THAN, TYPE_OPERATOR, TEXT('>')  }, // TODO/FIXME: Parser will break up '>=' (needed for uber breakpoints)
		{ TOKEN_HASH        , TYPE_OPERATOR, TEXT('#')  },
		{ TOKEN_LEFT_PAREN  , TYPE_OPERATOR, TEXT('(')  },
		{ TOKEN_LESS_THAN   , TYPE_OPERATOR, TEXT('<')  },
		{ TOKEN_MINUS       , TYPE_OPERATOR, TEXT('-')  }, // sub
//		{ TOKEN_TILDE       , TYPE_OPERATOR, TEXT('~')  }, // C/C++: Not.  Used for console.
		{ TOKEN_PERCENT     , TYPE_OPERATOR, TEXT('%')  }, // mod
		{ TOKEN_PIPE        , TYPE_OPERATOR, TEXT('|')  }, // bit-or
		{ TOKEN_PLUS        , TYPE_OPERATOR, TEXT('+')  }, // add
		{ TOKEN_QUOTED      , TYPE_QUOTED  , TEXT('"')  },
		{ TOKEN_RIGHT_PAREN , TYPE_OPERATOR, TEXT(')')  },
		{ TOKEN_SEMI        , TYPE_STRING  , TEXT(';')  },
		{ TOKEN_SPACE       , TYPE_STRING  , TEXT(' ')  } // space is also a delimiter between tokens/args
//		{ TOKEN_STAR        , TYPE_OPERATOR, TEXT('*')  },
//		{ TOKEN_TAB         , TYPE_STRING  , TEXT('\t') }
	};

//	const TokenTable_t g_aTokens2[  ] =
//	{ // Input
//		{ TOKEN_GREATER_EQUAL,TYPE_OPERATOR, TEXT(">=\x00") }, // TODO/FIXME: Parser will break up '>=' (needed for uber breakpoints)
//		{ TOKEN_LESS_EQUAL  , TYPE_OPERATOR, TEXT("<=\x00") }, // TODO/FIXME: Parser will break up '<=' (needed for uber breakpoints)
//	}

// Breakpoints ____________________________________________________________________________________
	int          g_nBreakpoints = 0;
	Breakpoint_t g_aBreakpoints[ MAX_BREAKPOINTS ];

	// NOTE: Breakpoint_Source_t and g_aBreakpointSource must match!
	const TCHAR *g_aBreakpointSource[ NUM_BREAKPOINT_SOURCES ] =
	{	// Used to be one char, since ArgsParse also uses // TODO/FIXME: Parser use Param[] ?
		// Used for both Input & Output!
		// Regs
		TEXT("A"), // Reg A
		TEXT("X"), // Reg X
		TEXT("Y"), // Reg Y
		// Special
		TEXT("PC"), // Program Counter
		TEXT("S" ), // Stack Pointer
		// Flags -- .8 Moved: Flag names from g_aFlagNames[] to "inlined" g_aBreakpointSource[]
		TEXT("P"), // Processor Status
		TEXT("C"), // ---- ---1 Carry
		TEXT("Z"), // ---- --1- Zero
		TEXT("I"), // ---- -1-- Interrupt
		TEXT("D"), // ---- 1--- Decimal
		TEXT("B"), // ---1 ---- Break
		TEXT("R"), // --1- ---- Reserved
		TEXT("V"), // -1-- ---- Overflow
		TEXT("N"), // 1--- ---- Sign
		// Misc		
		TEXT("OP"), // Opcode/Instruction/Mnemonic
		// Memory
		TEXT("M") // Main
	};

	// Note: BreakpointOperator_t, _PARAM_BREAKPOINT_, and g_aBreakpointSymbols must match!
	TCHAR *g_aBreakpointSymbols[ NUM_BREAKPOINT_OPERATORS] =
	{	// Output: Must be 2 chars!
		TEXT("<="), // LESS_EQAUL
		TEXT("< "), // LESS_THAN
		TEXT("= "), // EQUAL
		TEXT("! "), // NOT_EQUAL
		TEXT("> "), // GREATER_THAN
		TEXT(">="), // GREATER_EQUAL
		TEXT("? "), // READ   // Q. IO Read  use 'I'?  A. No, since I=Interrupt
		TEXT("@ "), // WRITE  // Q. IO Write use 'O'?  A. No, since O=Opcode
		TEXT("* "), // Read/Write
	};


// Commands _______________________________________________________________________________________

	#define __COMMANDS_VERIFY_TXT__ "\xDE\xAD\xC0\xDE"
	#define __PARAMS_VERIFY_TXT__   "\xDE\xAD\xDA\x1A"

	class commands_functor_compare
	{
		public:
			int operator() ( const Command_t & rLHS, const Command_t & rRHS ) const
			{
				return _tcscmp( rLHS.m_sName, rRHS.m_sName );
			}
	};

	int g_iCommand; // last command (enum) // used for consecuitive commands

	vector<int>       g_vPotentialCommands; // global, since TAB-completion also needs
	vector<Command_t> g_vSortedCommands;

	// Setting function to NULL, allows g_aCommands arguments to be safely listed here
	// Commands should be listed alphabetically per category.
	// For the list sorted by category, check Commands_e
	// NOTE: Commands_e and g_aCommands[] must match! Aliases are listed at the end
	Command_t g_aCommands[] =
	{
	// CPU (Main)
		{TEXT("A")           , CmdAssemble          , CMD_ASSEMBLE             , "Assemble instructions"      },
		{TEXT("U")           , CmdUnassemble        , CMD_UNASSEMBLE           , "Disassemble instructions"   },
		{TEXT("CALC")        , CmdCalculator        , CMD_CALC                 , "Display mini calc result"   },
		{TEXT("GOTO")        , CmdGo                , CMD_GO                   , "Run [until PC = address]"   },
		{TEXT("I")           , CmdInput             , CMD_INPUT                , "Input from IO $C0xx"        },
		{TEXT("KEY")         , CmdFeedKey           , CMD_INPUT_KEY            , "Feed key into emulator"     },
		{TEXT("JSR")         , CmdJSR               , CMD_JSR                  , "Call sub-routine"           },
		{TEXT("O")           , CmdOutput            , CMD_OUTPUT               , "Output from io $C0xx"       },
		{TEXT("NOP")         , CmdNOP               , CMD_NOP                  , "Zap the current instruction with a NOP" },
		{TEXT("P")           , CmdStepOver          , CMD_STEP_OVER            , "Step current instruction"   },
		{TEXT("RTS")         , CmdStepOut           , CMD_STEP_OUT             , "Step out of subroutine"     }, 
		{TEXT("T")           , CmdTrace             , CMD_TRACE                , "Trace current instruction"  },
		{TEXT("TF")          , CmdTraceFile         , CMD_TRACE_FILE           , "Save trace to filename" },
		{TEXT("TL")          , CmdTraceLine         , CMD_TRACE_LINE           , "Trace (with cycle counting)" },
	// Breakpoints
		{TEXT("BP")          , CmdBreakpoint        , CMD_BREAKPOINT           , "Access breakpoint options" },
		{TEXT("BPA")         , CmdBreakpointAddSmart, CMD_BREAKPOINT_ADD_SMART , "Add (smart) breakpoint" },
//		{TEXT("BPP")         , CmdBreakpointAddFlag , CMD_BREAKPOINT_ADD_FLAG  , "Add breakpoint on flags" },
		{TEXT("BPR")         , CmdBreakpointAddReg  , CMD_BREAKPOINT_ADD_REG   , "Add breakpoint on register value"      }, // NOTE! Different from SoftICE !!!!
		{TEXT("BPX")         , CmdBreakpointAddPC   , CMD_BREAKPOINT_ADD_PC    , "Add breakpoint at current instruction" },
		{TEXT("BPIO")        , CmdBreakpointAddIO   , CMD_BREAKPOINT_ADD_IO    , "Add breakpoint for IO address $C0xx"   },
		{TEXT("BPM")         , CmdBreakpointAddMem  , CMD_BREAKPOINT_ADD_MEM   , "Add breakpoint on memory access"       },  // SoftICE
		{TEXT("BC")          , CmdBreakpointClear   , CMD_BREAKPOINT_CLEAR     , "Clear breakpoint # or *"               }, // SoftICE
		{TEXT("BD")          , CmdBreakpointDisable , CMD_BREAKPOINT_DISABLE   , "Disable breakpoint # or *"             }, // SoftICE
		{TEXT("BPE")         , CmdBreakpointEdit    , CMD_BREAKPOINT_EDIT      , "Edit breakpoint # or *"                }, // SoftICE
		{TEXT("BE")          , CmdBreakpointEnable  , CMD_BREAKPOINT_ENABLE    , "Enable breakpoint # or *"              }, // SoftICE
		{TEXT("BL")          , CmdBreakpointList    , CMD_BREAKPOINT_LIST      , "List breakpoints"                      }, // SoftICE
		{TEXT("BPLOAD")      , CmdBreakpointLoad    , CMD_BREAKPOINT_LOAD      , "Loads breakpoints" },
		{TEXT("BPSAVE")      , CmdBreakpointSave    , CMD_BREAKPOINT_SAVE      , "Saves breakpoints" },
	// Benchmark / Timing
		{TEXT("BENCHMARK")   , CmdBenchmark         , CMD_BENCHMARK            , "Benchmark the emulator" },
		{TEXT("PROFILE")     , CmdProfile           , CMD_PROFILE              , "List/Save 6502 profiling" },
	// Config
		{TEXT("COLOR")       , CmdConfigColorMono   , CMD_CONFIG_COLOR         , "Sets/Shows RGB for color scheme" },
		{TEXT("CONFIG")      , CmdConfig            , CMD_CONFIG               , "Access config options" },
		{TEXT("FONT")        , CmdConfigFont        , CMD_CONFIG_FONT          , "Shows current font or sets new one" },
		{TEXT("HCOLOR")      , CmdConfigHColor      , CMD_CONFIG_HCOLOR        , "Sets/Shows colors mapped to Apple HGR" },
		{TEXT("LOAD")        , CmdConfigLoad        , CMD_CONFIG_LOAD          , "Load debugger configuration" },
		{TEXT("MONO")        , CmdConfigColorMono   , CMD_CONFIG_MONOCHROME    , "Sets/Shows RGB for monochrome scheme" },
		{TEXT("RUN")         , CmdConfigRun         , CMD_CONFIG_RUN           , "Run script file of debugger commands" },
		{TEXT("SAVE")        , CmdConfigSave        , CMD_CONFIG_SAVE          , "Save debugger configuration" },
	// Cursor
		{TEXT(".")           , CmdCursorJumpPC      , CMD_CURSOR_JUMP_PC       , "Locate the cursor in the disasm window" }, // centered
		{TEXT("=")           , CmdCursorSetPC       , CMD_CURSOR_SET_PC        , "Sets the PC to the current instruction" },
		{TEXT("RET")         , CmdCursorJumpRetAddr , CMD_CURSOR_JUMP_RET_ADDR , "Sets the cursor to the sub-routine caller" }, 
		{TEXT(      "^")     , NULL                 , CMD_CURSOR_LINE_UP       }, // \x2191 = Up Arrow (Unicode)
		{TEXT("Shift ^")     , NULL                 , CMD_CURSOR_LINE_UP_1     },
		{TEXT(      "v")     , NULL                 , CMD_CURSOR_LINE_DOWN     }, // \x2193 = Dn Arrow (Unicode)
		{TEXT("Shift v")     , NULL                 , CMD_CURSOR_LINE_DOWN_1   },
		{TEXT("PAGEUP"   )   , CmdCursorPageUp      , CMD_CURSOR_PAGE_UP       , "Scroll up one screen"   },
		{TEXT("PAGEUP256")   , CmdCursorPageUp256   , CMD_CURSOR_PAGE_UP_256   , "Scroll up 256 bytes"    }, // Shift
		{TEXT("PAGEUP4K" )   , CmdCursorPageUp4K    , CMD_CURSOR_PAGE_UP_4K    , "Scroll up 4096 bytes"   }, // Ctrl
		{TEXT("PAGEDN"     ) , CmdCursorPageDown    , CMD_CURSOR_PAGE_DOWN     , "Scroll down one scren"  }, 
		{TEXT("PAGEDOWN256") , CmdCursorPageDown256 , CMD_CURSOR_PAGE_DOWN_256 , "Scroll down 256 bytes"  }, // Shift
		{TEXT("PAGEDOWN4K" ) , CmdCursorPageDown4K  , CMD_CURSOR_PAGE_DOWN_4K  , "Scroll down 4096 bytes" }, // Ctrl
	// Flags
//		{TEXT("FC")          , CmdFlag              , CMD_FLAG_CLEAR , "Clear specified Flag"           }, // NVRBDIZC see AW_CPU.cpp AF_*
		{TEXT("CL")          , CmdFlag              , CMD_FLAG_CLEAR , "Clear specified Flag"           }, // NVRBDIZC see AW_CPU.cpp AF_*

		{TEXT("CLC")         , CmdFlagClear         , CMD_FLAG_CLR_C , "Clear Flag Carry"               }, // 0 // Legacy
		{TEXT("CLZ")         , CmdFlagClear         , CMD_FLAG_CLR_Z , "Clear Flag Zero"                }, // 1
		{TEXT("CLI")         , CmdFlagClear         , CMD_FLAG_CLR_I , "Clear Flag Interrupts Disabled" }, // 2
		{TEXT("CLD")         , CmdFlagClear         , CMD_FLAG_CLR_D , "Clear Flag Decimal (BCD)"       }, // 3
		{TEXT("CLB")         , CmdFlagClear         , CMD_FLAG_CLR_B , "CLear Flag Break"               }, // 4 // Legacy
		{TEXT("CLR")         , CmdFlagClear         , CMD_FLAG_CLR_R , "Clear Flag Reserved"            }, // 5
		{TEXT("CLV")         , CmdFlagClear         , CMD_FLAG_CLR_V , "Clear Flag Overflow"            }, // 6
		{TEXT("CLN")         , CmdFlagClear         , CMD_FLAG_CLR_N , "Clear Flag Negative (Sign)"     }, // 7

//		{TEXT("FS")          , CmdFlag              , CMD_FLAG_SET   , "Set specified Flag"             },
		{TEXT("SE")          , CmdFlag              , CMD_FLAG_SET   , "Set specified Flag"             },

		{TEXT("SEC")         , CmdFlagSet           , CMD_FLAG_SET_C , "Set Flag Carry"                 }, // 0
		{TEXT("SEZ")         , CmdFlagSet           , CMD_FLAG_SET_Z , "Set Flag Zero"                  }, // 1 
		{TEXT("SEI")         , CmdFlagSet           , CMD_FLAG_SET_I , "Set Flag Interrupts Disabled"   }, // 2
		{TEXT("SED")         , CmdFlagSet           , CMD_FLAG_SET_D , "Set Flag Decimal (BCD)"         }, // 3
		{TEXT("SEB")         , CmdFlagSet           , CMD_FLAG_SET_B , "Set Flag Break"                 }, // 4 // Legacy
		{TEXT("SER")         , CmdFlagSet           , CMD_FLAG_SET_R , "Set Flag Reserved"              }, // 5
		{TEXT("SEV")         , CmdFlagSet           , CMD_FLAG_SET_V , "Set Flag Overflow"              }, // 6
		{TEXT("SEN")         , CmdFlagSet           , CMD_FLAG_SET_N , "Set Flag Negative"              }, // 7
	// Help
		{TEXT("?")           , CmdHelpList          , CMD_HELP_LIST            , "List all available commands"           },
		{TEXT("HELP")        , CmdHelpSpecific      , CMD_HELP_SPECIFIC        , "Help on specific command"              },
		{TEXT("VERSION")     , CmdVersion           , CMD_VERSION              , "Displays version of emulator/debugger" },
		{TEXT("MOTD")        , CmdMOTD              , CMD_MOTD                 },

	// Memory
		{TEXT("MC")          , CmdMemoryCompare     , CMD_MEMORY_COMPARE       },

		{TEXT("D")           , CmdMemoryMiniDumpHex , CMD_MEM_MINI_DUMP_HEX_1  , "Hex dump in the mini memory area 1" }, // FIXME: Must also work in DATA screen
		{TEXT("MD")          , CmdMemoryMiniDumpHex , CMD_MEM_MINI_DUMP_HEX_1  }, // alias
		{TEXT("MD1")         , CmdMemoryMiniDumpHex , CMD_MEM_MINI_DUMP_HEX_1  , "Hex dump in the mini memory area 1" },
		{TEXT("MD2")         , CmdMemoryMiniDumpHex , CMD_MEM_MINI_DUMP_HEX_2  , "Hex dump in the mini memory area 2" },
		{TEXT("M1")          , CmdMemoryMiniDumpHex , CMD_MEM_MINI_DUMP_HEX_1  }, // alias
		{TEXT("M2")          , CmdMemoryMiniDumpHex , CMD_MEM_MINI_DUMP_HEX_2  }, // alias

		{TEXT("MA1")         , CmdMemoryMiniDumpAscii,CMD_MEM_MINI_DUMP_ASCII_1, "ASCII text in mini memory area 1" },
		{TEXT("MA2")         , CmdMemoryMiniDumpAscii,CMD_MEM_MINI_DUMP_ASCII_2, "ASCII text in mini memory area 2" },
		{TEXT("MT1")         , CmdMemoryMiniDumpApple,CMD_MEM_MINI_DUMP_APPLE_1, "Apple Text in mini memory area 1" },
		{TEXT("MT2")         , CmdMemoryMiniDumpApple,CMD_MEM_MINI_DUMP_APPLE_2, "Apple Text in mini memory area 2" },
//		{TEXT("ML1")         , CmdMemoryMiniDumpLow , CMD_MEM_MINI_DUMP_TXT_LO_1, "Text (Ctrl) in mini memory dump area 1" },
//		{TEXT("ML2")         , CmdMemoryMiniDumpLow , CMD_MEM_MINI_DUMP_TXT_LO_2, "Text (Ctrl) in mini memory dump area 2" },
//		{TEXT("MH1")         , CmdMemoryMiniDumpHigh, CMD_MEM_MINI_DUMP_TXT_HI_1, "Text (High) in mini memory dump area 1" },
//		{TEXT("MH2")         , CmdMemoryMiniDumpHigh, CMD_MEM_MINI_DUMP_TXT_HI_2, "Text (High) in mini memory dump area 2" },

		{TEXT("ME")          , CmdMemoryEdit        , CMD_MEMORY_EDIT          }, // TODO: like Copy ][+ Sector Edit
		{TEXT("E")           , CmdMemoryEnterByte   , CMD_MEMORY_ENTER_BYTE    },
		{TEXT("EW")          , CmdMemoryEnterWord   , CMD_MEMORY_ENTER_WORD    },
		{TEXT("M")           , CmdMemoryMove        , CMD_MEMORY_MOVE          },
		{TEXT("S")           , CmdMemorySearch      , CMD_MEMORY_SEARCH        },
		{TEXT("SA")          , CmdMemorySearchText  , CMD_MEMORY_SEARCH_ASCII  }, // Search Ascii
		{TEXT("SAL")         , CmdMemorySearchLowBit, CMD_MEMORY_SEARCH_TXT_LO }, // Search Ascii Low
		{TEXT("SAH")         , CmdMemorySearchHiBit , CMD_MEMORY_SEARCH_TXT_HI }, // Search "Ascii" High
		{TEXT("SH")          , CmdMemorySearchHex   , CMD_MEMORY_SEARCH_HEX    }, // Search Hex
		{TEXT("F")           , CmdMemoryFill        , CMD_MEMORY_FILL          },
	// Registers
		{TEXT("R")           , CmdRegisterSet       , CMD_REGISTER_SET         }, // TODO: Set/Clear flags
	// Source Level Debugging
		{TEXT("SOURCE")      , CmdSource            , CMD_SOURCE               , "Starts/Stops source level debugging" },
		{TEXT("SYNC")        , CmdSync              , CMD_SYNC                 , "Syncs the cursor to the source file" },
	// Stack
		{TEXT("POP")         , CmdStackPop          , CMD_STACK_POP            },
		{TEXT("PPOP")        , CmdStackPopPseudo    , CMD_STACK_POP_PSEUDO     },
		{TEXT("PUSH")        , CmdStackPop          , CMD_STACK_PUSH           },
//		{TEXT("RTS")         , CmdStackReturn       , CMD_STACK_RETURN         },
	// Symbols
		{TEXT("SYM")         , CmdSymbols           , CMD_SYMBOLS_LOOKUP       , "Lookup symbol or address" },

		{TEXT("SYMMAIN")     , CmdSymbolsMain       , CMD_SYMBOLS_MAIN         , "Main symbol table lookup/menu" }, // CLEAR,LOAD,SAVE
		{TEXT("SYMUSER")     , CmdSymbolsUser       , CMD_SYMBOLS_USER         , "User symbol table lookup/menu" }, // CLEAR,LOAD,SAVE
		{TEXT("SYMSRC" )     , CmdSymbolsSource     , CMD_SYMBOLS_SRC          , "Source symbol table lookup/menu" }, // CLEAR,LOAD,SAVE
//		{TEXT("SYMCLEAR")    , CmdSymbolsClear      , CMD_SYMBOLS_CLEAR        }, // can't use SC = SetCarry
		{TEXT("SYMINFO")     , CmdSymbolsInfo       , CMD_SYMBOLS_INFO        },
		{TEXT("SYMLIST")     , CmdSymbolsList       , CMD_SYMBOLS_LIST         }, // 'symbolname', can't use param '*' 
	// Variables
//		{TEXT("CLEAR")       , CmdVarsClear         , CMD_VARIABLES_CLEAR      }, 
//		{TEXT("VAR")         , CmdVarsDefine        , CMD_VARIABLES_DEFINE     },
//		{TEXT("INT8")        , CmdVarsDefineInt8    , CMD_VARIABLES_DEFINE_INT8},
//		{TEXT("INT16")       , CmdVarsDefineInt16   , CMD_VARIABLES_DEFINE_INT16},
//		{TEXT("VARS")        , CmdVarsList          , CMD_VARIABLES_LIST       }, 
//		{TEXT("VARSLOAD")    , CmdVarsLoad          , CMD_VARIABLES_LOAD       },
//		{TEXT("VARSSAVE")    , CmdVarsSave          , CMD_VARIABLES_SAVE       },
//		{TEXT("SET")         , CmdVarsSet           , CMD_VARIABLES_SET        },
	// Watch
		{TEXT("W")           , CmdWatchAdd          , CMD_WATCH_ADD     },
		{TEXT("WC")          , CmdWatchClear        , CMD_WATCH_CLEAR   },
		{TEXT("WD")          , CmdWatchDisable      , CMD_WATCH_DISABLE },
		{TEXT("WE")          , CmdWatchEnable       , CMD_WATCH_ENABLE  },
		{TEXT("WL")          , CmdWatchList         , CMD_WATCH_LIST    },
		{TEXT("WLOAD")       , CmdWatchLoad         , CMD_WATCH_LOAD    }, // Cant use as param to W 
		{TEXT("WSAVE")       , CmdWatchSave         , CMD_WATCH_SAVE    }, // due to symbol look-up
	// Window
		{TEXT("WIN")         , CmdWindow            , CMD_WINDOW         , "Show specified debugger window"              },
		{TEXT("CODE")        , CmdWindowViewCode    , CMD_WINDOW_CODE    , "Switch to full code window"                  },  // Can't use WC = WatchClear
		{TEXT("CODE1")       , CmdWindowShowCode1   , CMD_WINDOW_CODE_1  , "Show code on top split window"               },
		{TEXT("CODE2")       , CmdWindowShowCode2   , CMD_WINDOW_CODE_2  , "Show code on bottom split window"            },
		{TEXT("CONSOLE")     , CmdWindowViewConsole , CMD_WINDOW_CONSOLE , "Switch to full console window"               },
		{TEXT("DATA")        , CmdWindowViewData    , CMD_WINDOW_DATA    , "Switch to full data window"                  },
		{TEXT("DATA1")       , CmdWindowShowCode1   , CMD_WINDOW_CODE_1  , "Show data on top split window"               },
		{TEXT("DATA2")       , CmdWindowShowData2   , CMD_WINDOW_DATA_2  , "Show data on bottom split window"            },
		{TEXT("SOURCE1")     , CmdWindowShowSource1 , CMD_WINDOW_SOURCE_1, "Show source on top split screen"             },
		{TEXT("SOURCE2")     , CmdWindowShowSource2 , CMD_WINDOW_SOURCE_2, "Show source on bottom split screen"          },

		{TEXT("\\")          , CmdWindowViewOutput  , CMD_WINDOW_OUTPUT },
//		{TEXT("INFO")        , CmdToggleInfoPanel   , CMD_WINDOW_TOGGLE },
//		{TEXT("WINSOURCE")   , CmdWindowShowSource  , CMD_WINDOW_SOURCE },
//		{TEXT("ZEROPAGE")    , CmdWindowShowZeropage, CMD_WINDOW_ZEROPAGE },
	// ZeroPage
		{TEXT("ZP")          , CmdZeroPage          , CMD_ZEROPAGE_POINTER       },
		{TEXT("ZP0")         , CmdZeroPagePointer   , CMD_ZEROPAGE_POINTER_0     },
		{TEXT("ZP1")         , CmdZeroPagePointer   , CMD_ZEROPAGE_POINTER_1     },
		{TEXT("ZP2")         , CmdZeroPagePointer   , CMD_ZEROPAGE_POINTER_2     },
		{TEXT("ZP3")         , CmdZeroPagePointer   , CMD_ZEROPAGE_POINTER_3     },
		{TEXT("ZP4")         , CmdZeroPagePointer   , CMD_ZEROPAGE_POINTER_4     },
		{TEXT("ZPA")         , CmdZeroPageAdd       , CMD_ZEROPAGE_POINTER_ADD   },
		{TEXT("ZPC")         , CmdZeroPageClear     , CMD_ZEROPAGE_POINTER_CLEAR },
		{TEXT("ZPD")         , CmdZeroPageDisable   , CMD_ZEROPAGE_POINTER_CLEAR },
		{TEXT("ZPE")         , CmdZeroPageEnable    , CMD_ZEROPAGE_POINTER_CLEAR },
		{TEXT("ZPL")         , CmdZeroPageList      , CMD_ZEROPAGE_POINTER_LIST  },
		{TEXT("ZPLOAD")      , CmdZeroPageLoad      , CMD_ZEROPAGE_POINTER_LOAD  }, // Cant use as param to ZP
		{TEXT("ZPSAVE")      , CmdZeroPageSave      , CMD_ZEROPAGE_POINTER_SAVE  }, // due to symbol look-up

//	{TEXT("TIMEDEMO"),CmdTimeDemo, CMD_TIMEDEMO }, // CmdBenchmarkStart(), CmdBenchmarkStop()
//	{TEXT("WC"),CmdShowCodeWindow}, // Can't use since WatchClear
//	{TEXT("WD"),CmdShowDataWindow}, //

	// Internal Consistency Check
		{TEXT(__COMMANDS_VERIFY_TXT__), NULL, NUM_COMMANDS },

	// Aliasies - Can be in any order
		{TEXT("->")          , NULL                 , CMD_CURSOR_JUMP_PC       },
		{TEXT("Ctrl ->" )    , NULL                 , CMD_CURSOR_SET_PC        },
		{TEXT("Shift ->")    , NULL                 , CMD_CURSOR_JUMP_PC       }, // at top
		{TEXT("INPUT")       , CmdInput             , CMD_INPUT                },
		// Flags - Clear
		{TEXT("RC")          , CmdFlagClear         , CMD_FLAG_CLR_C , "Clear Flag Carry"               }, // 0 // Legacy
		{TEXT("RZ")          , CmdFlagClear         , CMD_FLAG_CLR_Z , "Clear Flag Zero"                }, // 1
		{TEXT("RI")          , CmdFlagClear         , CMD_FLAG_CLR_I , "Clear Flag Interrupts Disabled" }, // 2
		{TEXT("RD")          , CmdFlagClear         , CMD_FLAG_CLR_D , "Clear Flag Decimal (BCD)"       }, // 3
		{TEXT("RB")          , CmdFlagClear         , CMD_FLAG_CLR_B , "CLear Flag Break"               }, // 4 // Legacy
		{TEXT("RR")          , CmdFlagClear         , CMD_FLAG_CLR_R , "Clear Flag Reserved"            }, // 5
		{TEXT("RV")          , CmdFlagClear         , CMD_FLAG_CLR_V , "Clear Flag Overflow"            }, // 6
		{TEXT("RN")          , CmdFlagClear         , CMD_FLAG_CLR_N , "Clear Flag Negative (Sign)"     }, // 7
		// Flags - Set
		{TEXT("SC")          , CmdFlagSet           , CMD_FLAG_SET_C , "Set Flag Carry"                 }, // 0
		{TEXT("SZ")          , CmdFlagSet           , CMD_FLAG_SET_Z , "Set Flag Zero"                  }, // 1 
		{TEXT("SI")          , CmdFlagSet           , CMD_FLAG_SET_I , "Set Flag Interrupts Disabled"   }, // 2
		{TEXT("SD")          , CmdFlagSet           , CMD_FLAG_SET_D , "Set Flag Decimal (BCD)"         }, // 3
		{TEXT("SB")          , CmdFlagSet           , CMD_FLAG_SET_B , "CLear Flag Break"               }, // 4 // Legacy
		{TEXT("SR")          , CmdFlagSet           , CMD_FLAG_SET_R , "Clear Flag Reserved"            }, // 5
		{TEXT("SV")          , CmdFlagSet           , CMD_FLAG_SET_V , "Clear Flag Overflow"            }, // 6
		{TEXT("SN")          , CmdFlagSet           , CMD_FLAG_SET_N , "Clear Flag Negative"            }, // 7

		{TEXT("EB" )         , CmdMemoryEnterByte   , CMD_MEMORY_ENTER_BYTE    },
		{TEXT("E8" )         , CmdMemoryEnterByte   , CMD_MEMORY_ENTER_BYTE    },
		{TEXT("E16")         , CmdMemoryEnterWord   , CMD_MEMORY_ENTER_WORD    },
		{TEXT("MF")          , CmdMemoryFill        , CMD_MEMORY_FILL          },
		{TEXT("MM")          , CmdMemoryMove        , CMD_MEMORY_MOVE          },
		{TEXT("MS")          , CmdMemorySearch      , CMD_MEMORY_SEARCH        }, // CmdMemorySearch
		{TEXT("BW")          , CmdConfigColorMono   , CMD_CONFIG_MONOCHROME    },
		{TEXT("P0")          , CmdZeroPagePointer   , CMD_ZEROPAGE_POINTER_0   },
		{TEXT("P1")          , CmdZeroPagePointer   , CMD_ZEROPAGE_POINTER_1   },
		{TEXT("P2")          , CmdZeroPagePointer   , CMD_ZEROPAGE_POINTER_2   },
		{TEXT("P3")          , CmdZeroPagePointer   , CMD_ZEROPAGE_POINTER_3   },
		{TEXT("P4")          , CmdZeroPagePointer   , CMD_ZEROPAGE_POINTER_4   },
		{TEXT("REGISTER")    , CmdRegisterSet       , CMD_REGISTER_SET         },
//		{TEXT("RET")         , CmdStackReturn       , CMD_STACK_RETURN         },
		{TEXT("TRACE")       , CmdTrace             , CMD_TRACE                },
		{TEXT("SYMBOLS")     , CmdSymbols           , CMD_SYMBOLS_LOOKUP       , "Return " },
//		{TEXT("SYMBOLS1")    , CmdSymbolsInfo       , CMD_SYMBOLS_1            },
//		{TEXT("SYMBOLS2")    , CmdSymbolsInfo       , CMD_SYMBOLS_2            },
		{TEXT("SYM1" )       , CmdSymbolsInfo       , CMD_SYMBOLS_MAIN         },
		{TEXT("SYM2" )       , CmdSymbolsInfo       , CMD_SYMBOLS_USER         },
		{TEXT("SYM3" )       , CmdSymbolsInfo       , CMD_SYMBOLS_SRC          },
		{TEXT("WATCH")       , CmdWatchAdd          , CMD_WATCH_ADD            },
		{TEXT("WINDOW")      , CmdWindow            , CMD_WINDOW               },
//		{TEXT("W?")          , CmdWatchAdd          , CMD_WATCH_ADD            },
		{TEXT("ZAP")         , CmdNOP               , CMD_NOP                  },

	// DEPRECATED  -- Probably should be removed in a future version
		{TEXT("BENCH")       , CmdBenchmarkStart    , CMD_BENCHMARK            },
		{TEXT("EXITBENCH")   , CmdBenchmarkStop     , CMD_BENCHMARK            },
		{TEXT("MDB")         , CmdMemoryMiniDumpHex , CMD_MEM_MINI_DUMP_HEX_1  }, // MemoryDumpByte  // Did anyone actually use this??
		{TEXT("MDC")         , CmdUnassemble        , CMD_UNASSEMBLE           }, // MemoryDumpCode  // Did anyone actually use this??
		{TEXT("MEB")         , CmdMemoryEnterByte   , CMD_MEMORY_ENTER_BYTE    }, // MemoryEnterByte // Did anyone actually use this??
		{TEXT("MEMORY")      , CmdMemoryMiniDumpHex , CMD_MEM_MINI_DUMP_HEX_1  }, // MemoryDumpByte  // Did anyone actually use this??
};

//	static const TCHAR g_aFlagNames[_6502_NUM_FLAGS+1] = TEXT("CZIDBRVN");// Reversed since arrays are from left-to-right

	const int NUM_COMMANDS_WITH_ALIASES = sizeof(g_aCommands) / sizeof (Command_t); // Determined at compile-time ;-)


// Color __________________________________________________________________________________________

	int g_iColorScheme = SCHEME_COLOR;

	// Used when the colors are reset
	COLORREF gaColorPalette[ NUM_PALETTE ] =
	{
		RGB(0,0,0),
		// NOTE: See _SetupColorRamp() if you want to programmitically set/change
		RGB(255,  0,  0), RGB(223,  0,  0), RGB(191,  0,  0), RGB(159,  0,  0), RGB(127,  0,  0), RGB( 95,  0,  0), RGB( 63,  0,  0), RGB( 31,  0,  0),  // 001 // Red
		RGB(  0,255,  0), RGB(  0,223,  0), RGB(  0,191,  0), RGB(  0,159,  0), RGB(  0,127,  0), RGB(  0, 95,  0), RGB(  0, 63,  0), RGB(  0, 31,  0),  // 010 // Green
		RGB(255,255,  0), RGB(223,223,  0), RGB(191,191,  0), RGB(159,159,  0), RGB(127,127,  0), RGB( 95, 95,  0), RGB( 63, 63,  0), RGB( 31, 31,  0),  // 011 // Yellow
		RGB(  0,  0,255), RGB(  0,  0,223), RGB(  0,  0,191), RGB(  0,  0,159), RGB(  0,  0,127), RGB(  0,  0, 95), RGB(  0,  0, 63), RGB(  0,  0, 31),  // 100 // Blue
		RGB(255,  0,255), RGB(223,  0,223), RGB(191,  0,191), RGB(159,  0,159), RGB(127,  0,127), RGB( 95,  0, 95), RGB( 63,  0, 63), RGB( 31,  0, 31),  // 101 // Magenta
		RGB(  0,255,255), RGB(  0,223,223), RGB(  0,191,191), RGB(  0,159,159), RGB(  0,127,127), RGB(  0, 95, 95), RGB(  0, 63, 63), RGB(  0, 31, 31),  // 110	// Cyan
		RGB(255,255,255), RGB(223,223,223), RGB(191,191,191), RGB(159,159,159), RGB(127,127,127), RGB( 95, 95, 95), RGB( 63, 63, 63), RGB( 31, 31, 31),  // 111 // White/Gray

		// Custom Colors
		RGB( 80,192,255), // Light  Sky Blue // Used for console FG
		RGB(  0,128,192), // Darker Sky Blue
		RGB(  0, 64,128), // Deep   Sky Blue
		RGB(255,128,  0), // Orange (Full)
		RGB(128, 64,  0), // Orange (Half)
		RGB(  0,  0,  0),
		RGB(  0,  0,  0),
		RGB(  0,  0,  0),

		RGB(  0,  0,  0),
		RGB(  0,  0,  0),
		RGB(  0,  0,  0),
		RGB(  0,  0,  0),
		RGB(  0,  0,  0),
		RGB(  0,  0,  0),
		RGB(  0,  0,  0),
		RGB(  0,  0,  0),
	};

	// Index into Palette
	int g_aColorIndex[ NUM_COLORS ] =
	{
		K0, W8,              // BG_CONSOLE_OUTPUT   FG_CONSOLE_OUTPUT (W8)

		B1, COLOR_CUSTOM_01, // BG_CONSOLE_INPUT    FG_CONSOLE_INPUT (W8)

		B2, B3, // BG_DISASM_1        BG_DISASM_2

		R8, W8, // BG_DISASM_BP_S_C   FG_DISASM_BP_S_C
		R6, W5, // BG_DISASM_BP_0_C   FG_DISASM_BP_0_C

		R7,    // FG_DISASM_BP_S_X    // Y8 lookes better on Info Cyan // R6
		W5,    // FG_DISASM_BP_0_X 

		W8, B8, // BG_DISASM_C        FG_DISASM_C
		Y8, B8, // BG_DISASM_PC_C     FG_DISASM_PC_C
		Y4, W8, // BG_DISASM_PC_X     FG_DISASM_PC_X

		W8,     // FG_DISASM_ADDRESS
		G192,   // FG_DISASM_OPERATOR
		Y8,     // FG_DISASM_OPCODE
		W8,     // FG_DISASM_MNEMONIC
		COLOR_CUSTOM_04, // FG_DISASM_TARGET (or W8)
		G8,     // FG_DISASM_SYMBOL
		C8,     // FG_DISASM_CHAR
		G8,		// FG_DISASM_BRANCH

		C3,   // BG_INFO (C4, C2 too dark)
		W8,   // FG_INFO_TITLE (or W8)
		Y7,   // FG_INFO_BULLET (W8)
		G192, // FG_INFO_OPERATOR
		COLOR_CUSTOM_04,   // FG_INFO_ADDRESS (was Y8)
		Y8,   // FG_INFO_OPCODE
		COLOR_CUSTOM_01, // FG_INFO_REG (was orange)

		W8,   // BG_INFO_INVERSE
		C3,   // FG_INFO_INVERSE
		C5,   // BG_INFO_CHAR
		W8,   // FG_INFO_CHAR_HI
		Y8,   // FG_INFO_CHAR_LO

		COLOR_CUSTOM_04, // BG_INFO_IO_BYTE
		COLOR_CUSTOM_04, // FG_INFO_IO_BYTE
				
		C2,   // BG_DATA_1
		C3,   // BG_DATA_2
		Y8,   // FG_DATA_BYTE
		W8,   // FG_DATA_TEXT

		G4,   // BG_SYMBOLS_1
		G3,   // BG_SYMBOLS_2
		W8,   // FG_SYMBOLS_ADDRESS
		M8,   // FG_SYMBOLS_NAME

		K0,   // BG_SOURCE_TITLE
		W8,   // FG_SOURCE_TITLE
		W2,   // BG_SOURCE_1 // C2 W2 for "Paper Look"
		W3,   // BG_SOURCE_2
		W8    // FG_SOURCE
	};

	COLORREF g_aColors[ NUM_COLOR_SCHEMES ][ NUM_COLORS ];


// Console ________________________________________________________________________________________

	// See ConsoleInputReset() for why the console input
	// is tied to the zero'th output of g_aConsoleDisplay
	// and not using a seperate var: g_aConsoleInput[ CONSOLE_WIDTH ];
	//
	//          :          g_aConsoleBuffer[4] |      ^ g_aConsoleDisplay[5]        :
	//          :          g_aConsoleBuffer[3] |      | g_aConsoleDisplay[4]  <-  g_nConsoleDisplayTotal
	// g_nConsoleBuffer -> g_aConsoleBuffer[2] |      | g_aConsoleDisplay[3]        :
	//          :          g_aConsoleBuffer[1] v      | g_aConsoleDisplay[2]        :
	//          .          g_aConsoleBuffer[0] -----> | g_aConsoleDisplay[1]        .
	//                                                | 
	//                            ConsoleInput ---->  | g_aConsoleDisplay[0]   
	//
	const int CONSOLE_HEIGHT = 384; // Lines, was 128, but need ~ 256+16 for PROFILE LIST
	const int CONSOLE_WIDTH  =  80;

	// Buffer
		bool  g_bConsoleBufferPaused = false; // buffered output is waiting for user to continue
		int   g_nConsoleBuffer = 0;
		TCHAR g_aConsoleBuffer[ CONSOLE_HEIGHT ][ CONSOLE_WIDTH ]; // TODO: stl::vector< line_t >

	// Display
		TCHAR g_aConsolePrompt[] = TEXT(">!"); // input, assembler
		TCHAR g_sConsolePrompt[] = TEXT(">"); // No, NOT Integer Basic!  The nostalgic '*' "Monitor" doesn't look as good, IMHO. :-(
		bool  g_bConsoleFullWidth = false;

		int   g_iConsoleDisplayStart  = 0; // to allow scrolling
		int   g_nConsoleDisplayTotal  = 0; // number of lines added to console
		int   g_nConsoleDisplayHeight = 0;
		int   g_nConsoleDisplayWidth  = 0;
		TCHAR g_aConsoleDisplay[ CONSOLE_HEIGHT ][ CONSOLE_WIDTH ];

	// Input History
		int   g_nHistoryLinesStart = 0;
		int   g_nHistoryLinesTotal = 0; // number of commands entered
		TCHAR g_aHistoryLines[ CONSOLE_HEIGHT ][ CONSOLE_WIDTH ] = {TEXT("")};

	// Input Line
		// Raw input Line (has prompt)
		const int CONSOLE_FIRST_LINE = 1; // where ConsoleDisplay is pushed up from
		TCHAR * const g_aConsoleInput = g_aConsoleDisplay[0];
		// Cooked input line (no prompt)
		int           g_nConsoleInputChars  = 0;
		TCHAR *       g_pConsoleInput       = 0; // points to past prompt
		bool          g_bConsoleInputQuoted = false; // Allows lower-case to be entered
		bool          g_bConsoleInputSkip   = false;


// Cursor _________________________________________________________________________________________

	WORD g_nDisasmTopAddress = 0;
	WORD g_nDisasmBotAddress = 0;
	WORD g_nDisasmCurAddress = 0;

	bool g_bDisasmCurBad    = false;
	int  g_nDisasmCurLine   = 0; // Aligned to Top or Center
    int  g_iDisasmCurState = CURSOR_NORMAL;

	int  g_nDisasmWinHeight = 0;

// Disassembly
	bool  g_bConfigDisasmOpcodeSpaces = true; // TODO: CONFIG DISASM SPACE  [0|1]
	bool  g_bConfigDisasmAddressColon = true; // TODO: CONFIG DISASM COLON  [0|1]
	bool  g_bConfigDisasmFancyBranch  = true; // TODO: CONFIG DISASM BRANCH [0|1]
//	TCHAR g_aConfigDisasmAddressColon[] = TEXT(" :");

	/*
		* Wingdings
			\xE1   Up Arrow
			\xE2   Down Arrow
		* Webdings // M$ Font
			\x35   Up Arrow
			\x33   Left Arrow
			\x36   Down Arrow
		* Symols
			\xAD   Up Arrow
			\xAF   Down Arrow
	*/
	TCHAR g_sConfigBranchIndicatorUp   [] = TEXT("^\x35"); // was \x18 ?? // string
	TCHAR g_sConfigBranchIndicatorEqual[] = TEXT("=\x33"); // was \x18 ?? // string
	TCHAR g_sConfigBranchIndicatorDown [] = TEXT("v\x36"); // was \x19 ?? // string

// Drawing
	// Width
		const int DISPLAY_WIDTH  = 560;
		#define  SCREENSPLIT1    356	// Horizontal Column (pixels?) of Stack & Regs
//		#define  SCREENSPLIT2    456	// Horizontal Column (pixels?) of BPs, Watches & Mem
		const int SCREENSPLIT2 = 456-7; // moved left one "char" to show PC in breakpoint:
		
		const int DISPLAY_BP_COLUMN      = SCREENSPLIT2;
		const int DISPLAY_MINI_CONSOLE   = SCREENSPLIT1 -  6; // - 1 chars
		const int DISPLAY_DISASM_RIGHT   = SCREENSPLIT1 -  6; // - 1 char
		const int DISPLAY_FLAG_COLUMN    = SCREENSPLIT1 + 63;
		const int DISPLAY_MINIMEM_COLUMN = SCREENSPLIT2 +  7;
		const int DISPLAY_REGS_COLUMN    = SCREENSPLIT1;
		const int DISPLAY_STACK_COLUMN   = SCREENSPLIT1;
		const int DISPLAY_TARGETS_COLUMN = SCREENSPLIT1;
		const int DISPLAY_WATCHES_COLUMN = SCREENSPLIT2;
		const int DISPLAY_ZEROPAGE_COLUMN= SCREENSPLIT1;

	// Height
//		const int DISPLAY_LINES  =  24; // FIXME: Should be pixels
		// 304 = bottom of disassembly
		// 368 = bottom console
		// 384 = 16 * 24 very bottom
		const int DEFAULT_HEIGHT = 16;
		const int DISPLAY_HEIGHT = 384; // 368; // DISPLAY_LINES * g_nFontHeight;

		const int WINDOW_DATA_BYTES_PER_LINE = 8;
// Font
	FontConfig_t g_aFontConfig[ NUM_FONTS  ];

	TCHAR     g_sFontNameDefault[ MAX_FONT_NAME ] = TEXT("Courier New");
//	TCHAR     g_sFontNameCustom [ MAX_FONT_NAME ] = TEXT("Courier New"); // Arial
	TCHAR     g_sFontNameConsole[ MAX_FONT_NAME ] = TEXT("Courier New");
	TCHAR     g_sFontNameDisasm [ MAX_FONT_NAME ] = TEXT("Courier New");
	TCHAR     g_sFontNameInfo   [ MAX_FONT_NAME ] = TEXT("Courier New");
	TCHAR     g_sFontNameBranch [ MAX_FONT_NAME ] = TEXT("Webdings");
	int       g_nFontHeight = 15; // 13 -> 12 Lucida Console is readable
	int       g_iFontSpacing = FONT_SPACING_CLEAN;
//	int       g_nFontWidthAvg = 0;
//	int       g_nFontWidthMax = 0;
//	HFONT     g_hFontDebugger = (HFONT)0;
//	HFONT     g_hFontDisasm   = (HFONT)0;
	HFONT     g_hFontWebDings  = (HFONT)0;


		const int MIN_DISPLAY_CONSOLE_LINES =  4; // doesn't include ConsoleInput

		int g_nTotalLines = 0; // DISPLAY_HEIGHT / g_nFontHeight;
		int MAX_DISPLAY_DISASM_LINES   = 0; // g_nTotalLines -  MIN_DISPLAY_CONSOLE_LINES; // 19;

		int MAX_DISPLAY_STACK_LINES    =  8;
		int MAX_DISPLAY_CONSOLE_LINES  =  0; // MAX_DISPLAY_DISASM_LINES + MIN_DISPLAY_CONSOLE_LINES; // 23


// Instructions / Opcodes _________________________________________________________________________


// @reference: http://www.6502.org/tutorials/compare_instructions.html
// 10   signed: BPL BGE 
// B0 unsigned: BCS BGE

	int g_bOpcodesHashed = false;
	int g_aOpcodesHash[ NUM_OPCODES ]; // for faster mnemonic lookup, for the assembler
	
#define R_ MEM_R
#define _W MEM_W
#define RW MEM_R | MEM_W
#define _S MEM_S
#define im MEM_IM
#define SW MEM_S | MEM_WI
#define SR MEM_S | MEM_RI
const Opcodes_t g_aOpcodes65C02[ NUM_OPCODES ] =
{
	{"BRK", 0     ,  0}, {"ORA", AM_IZX, R_}, {"NOP", AM_2  , 0 }, {"NOP", AM_1  , 0 }, // 00 .. 03
	{"TSB", AM_Z  , _W}, {"ORA", AM_Z  , R_}, {"ASL", AM_Z  , RW}, {"NOP", AM_1  , 0 }, // 04 .. 07
	{"PHP", 0     , SW}, {"ORA", AM_M  , im}, {"ASL", 0     ,  0}, {"NOP", AM_1  , 0 }, // 08 .. 0B
	{"TSB", AM_A  , _W}, {"ORA", AM_A  , R_}, {"ASL", AM_A  , RW}, {"NOP", AM_3  , 0 }, // 0C .. 0F
	{"BPL", AM_R  ,  0}, {"ORA", AM_NZY, R_}, {"ORA", AM_NZ , R_}, {"NOP", AM_1  , 0 }, // 10 .. 13
	{"TRB", AM_Z  , _W}, {"ORA", AM_ZX , R_}, {"ASL", AM_ZX , RW}, {"NOP", AM_1  , 0 }, // 14 .. 17
	{"CLC", 0     ,  0}, {"ORA", AM_AY , R_}, {"INA", 0     ,  0}, {"NOP", AM_1  , 0 }, // 18 .. 1B
	{"TRB", AM_A  , _W}, {"ORA", AM_AX , R_}, {"ASL", AM_AX , RW}, {"NOP", AM_1  , 0 }, // 1C .. 1F

	{"JSR", AM_A  , SW}, {"AND", AM_IZX, R_}, {"NOP", AM_2  ,  0}, {"NOP", AM_1  , 0 }, // 20 .. 23
	{"BIT", AM_Z  , R_}, {"AND", AM_Z  , R_}, {"ROL", AM_Z  , RW}, {"NOP", AM_1  , 0 }, // 24 .. 27
	{"PLP", 0     , SR}, {"AND", AM_M  , im}, {"ROL", 0     ,  0}, {"NOP", AM_1  , 0 }, // 28 .. 2B
	{"BIT", AM_A  , R_}, {"AND", AM_A  , R_}, {"ROL", AM_A  , RW}, {"NOP", AM_3  , 0 }, // 2C .. 2F
	{"BMI", AM_R  ,  0}, {"AND", AM_NZY, R_}, {"AND", AM_NZ , R_}, {"NOP", AM_1  , 0 }, // 30 .. 33
	{"BIT", AM_ZX , R_}, {"AND", AM_ZX , R_}, {"ROL", AM_ZX , RW}, {"NOP", AM_1  , 0 }, // 34 .. 37
	{"SEC", 0     ,  0}, {"AND", AM_AY , R_}, {"DEA", 0     ,  0}, {"NOP", AM_1  , 0 }, // 38 .. 3B
	{"BIT", AM_AX , R_}, {"AND", AM_AX , R_}, {"ROL", AM_AX , RW}, {"NOP", AM_1  , 0 }, // 3C .. 3F

	{"RTI", 0     ,  0}, {"EOR", AM_IZX, R_}, {"NOP", AM_2  ,  0}, {"NOP", AM_1  , 0 }, // 40 .. 43
	{"NOP", AM_2  ,  0}, {"EOR", AM_Z  , R_}, {"LSR", AM_Z  , _W}, {"NOP", AM_1  , 0 }, // 44 .. 47
	{"PHA", 0     , SW}, {"EOR", AM_M  , im}, {"LSR", 0     ,  0}, {"NOP", AM_1  , 0 }, // 48 .. 4B
	{"JMP", AM_A  ,  0}, {"EOR", AM_A  , R_}, {"LSR", AM_A  , _W}, {"NOP", AM_1  , 0 }, // 4C .. 4F
	{"BVC", AM_R  ,  0}, {"EOR", AM_NZY, R_}, {"EOR", AM_NZ , R_}, {"NOP", AM_1  , 0 }, // 50 .. 53
	{"NOP", AM_2  ,  0}, {"EOR", AM_ZX , R_}, {"LSR", AM_ZX , _W}, {"NOP", AM_1  , 0 }, // 54 .. 57
	{"CLI", 0     ,  0}, {"EOR", AM_AY , R_}, {"PHY", 0     , SW}, {"NOP", AM_1  , 0 }, // 58 .. 5B
	{"NOP", AM_3  ,  0}, {"EOR", AM_AX , R_}, {"LSR", AM_AX , RW}, {"NOP", AM_1  , 0 }, // 5C .. 5F

	{"RTS", 0     , SR}, {"ADC", AM_IZX, R_}, {"NOP", AM_2  ,  0}, {"NOP", AM_1  , 0 }, // 60 .. 63
	{"STZ", AM_Z  , _W}, {"ADC", AM_Z  , R_}, {"ROR", AM_Z  , RW}, {"NOP", AM_1  , 0 }, // 64 .. 67
	{"PLA", 0     , SR}, {"ADC", AM_M  , im}, {"ROR", 0     ,  0}, {"NOP", AM_1  , 0 }, // 68 .. 6B
	{"JMP", AM_NA ,  0}, {"ADC", AM_A  , R_}, {"ROR", AM_A  , RW}, {"NOP", AM_1  , 0 }, // 6C .. 6F
	{"BVS", AM_R  ,  0}, {"ADC", AM_NZY, R_}, {"ADC", AM_NZ , R_}, {"NOP", AM_1  , 0 }, // 70 .. 73
	{"STZ", AM_ZX , _W}, {"ADC", AM_ZX , R_}, {"ROR", AM_ZX , RW}, {"NOP", AM_1  , 0 }, // 74 .. 77
	{"SEI", 0     ,  0}, {"ADC", AM_AY , R_}, {"PLY", 0     , SR}, {"NOP", AM_1  , 0 }, // 78 .. 7B
	{"JMP", AM_IAX,  0}, {"ADC", AM_AX , R_}, {"ROR", AM_AX , RW}, {"NOP", AM_1  , 0 }, // 7C .. 7F

	{"BRA", AM_R  ,  0}, {"STA", AM_IZX, _W}, {"NOP", AM_2  ,  0}, {"NOP", AM_1  , 0 }, // 80 .. 83
	{"STY", AM_Z  , _W}, {"STA", AM_Z  , _W}, {"STX", AM_Z  , _W}, {"NOP", AM_1  , 0 }, // 84 .. 87
	{"DEY", 0     ,  0}, {"BIT", AM_M  , im}, {"TXA", 0     ,  0}, {"NOP", AM_1  , 0 }, // 88 .. 8B
	{"STY", AM_A  , _W}, {"STA", AM_A  , _W}, {"STX", AM_A  , _W}, {"NOP", AM_1  , 0 }, // 8C .. 8F
	{"BCC", AM_R  ,  0}, {"STA", AM_NZY, _W}, {"STA", AM_NZ , _W}, {"NOP", AM_1  , 0 }, // 90 .. 93
	{"STY", AM_ZX , _W}, {"STA", AM_ZX , _W}, {"STX", AM_ZY , _W}, {"NOP", AM_1  , 0 }, // 94 .. 97
	{"TYA", 0     ,  0}, {"STA", AM_AY , _W}, {"TXS", 0     ,  0}, {"NOP", AM_1  , 0 }, // 98 .. 9B
	{"STZ", AM_A  , _W}, {"STA", AM_AX , _W}, {"STZ", AM_AX , _W}, {"NOP", AM_1  , 0 }, // 9C .. 9F

	{"LDY", AM_M  , im}, {"LDA", AM_IZX, R_}, {"LDX", AM_M  , im}, {"NOP", AM_1  , 0 }, // A0 .. A3
	{"LDY", AM_Z  , R_}, {"LDA", AM_Z  , R_}, {"LDX", AM_Z  , R_}, {"NOP", AM_1  , 0 }, // A4 .. A7
	{"TAY", 0     ,  0}, {"LDA", AM_M  , im}, {"TAX", 0     , 0 }, {"NOP", AM_1  , 0 }, // A8 .. AB
	{"LDY", AM_A  , R_}, {"LDA", AM_A  , R_}, {"LDX", AM_A  , R_}, {"NOP", AM_1  , 0 }, // AC .. AF
	{"BCS", AM_R  ,  0}, {"LDA", AM_NZY, R_}, {"LDA", AM_NZ , R_}, {"NOP", AM_1  , 0 }, // B0 .. B3
	{"LDY", AM_ZX , R_}, {"LDA", AM_ZX , R_}, {"LDX", AM_ZY , R_}, {"NOP", AM_1  , 0 }, // B4 .. B7
	{"CLV", 0     ,  0}, {"LDA", AM_AY , R_}, {"TSX", 0     , 0 }, {"NOP", AM_1  , 0 }, // B8 .. BB
	{"LDY", AM_AX , R_}, {"LDA", AM_AX , R_}, {"LDX", AM_AY , R_}, {"NOP", AM_1  , 0 }, // BC .. BF

	{"CPY", AM_M  , im}, {"CMP", AM_IZX, R_}, {"NOP", AM_2  ,  0}, {"NOP", AM_1  , 0 }, // C0 .. C3
	{"CPY", AM_Z  , R_}, {"CMP", AM_Z  , R_}, {"DEC", AM_Z  , RW}, {"NOP", AM_1  , 0 }, // C4 .. C7
	{"INY", 0     ,  0}, {"CMP", AM_M  , im}, {"DEX", 0     ,  0}, {"NOP", AM_1  , 0 }, // C8 .. CB
	{"CPY", AM_A  , R_}, {"CMP", AM_A  , R_}, {"DEC", AM_A  , RW}, {"NOP", AM_1  , 0 }, // CC .. CF
	{"BNE", AM_R  ,  0}, {"CMP", AM_NZY, R_}, {"CMP", AM_NZ ,  0}, {"NOP", AM_1  , 0 }, // D0 .. D3
	{"NOP", AM_2  ,  0}, {"CMP", AM_ZX , R_}, {"DEC", AM_ZX , RW}, {"NOP", AM_1  , 0 }, // D4 .. D7
	{"CLD", 0     ,  0}, {"CMP", AM_AY , R_}, {"PHX", 0     ,  0}, {"NOP", AM_1  , 0 }, // D8 .. DB
	{"NOP", AM_3  ,  0}, {"CMP", AM_AX , R_}, {"DEC", AM_AX , RW}, {"NOP", AM_1  , 0 }, // DC .. DF

	{"CPX", AM_M  , im}, {"SBC", AM_IZX, R_}, {"NOP", AM_2  ,  0}, {"NOP", AM_1  , 0 }, // E0 .. E3
	{"CPX", AM_Z  , R_}, {"SBC", AM_Z  , R_}, {"INC", AM_Z  , RW}, {"NOP", AM_1  , 0 }, // E4 .. E7
	{"INX", 0     ,  0}, {"SBC", AM_M  , R_}, {"NOP", 0     ,  0}, {"NOP", AM_1  , 0 }, // E8 .. EB
	{"CPX", AM_A  , R_}, {"SBC", AM_A  , R_}, {"INC", AM_A  , RW}, {"NOP", AM_1  , 0 }, // EC .. EF
	{"BEQ", AM_R  ,  0}, {"SBC", AM_NZY, R_}, {"SBC", AM_NZ ,  0}, {"NOP", AM_1  , 0 }, // F0 .. F3
	{"NOP", AM_2  ,  0}, {"SBC", AM_ZX , R_}, {"INC", AM_ZX , RW}, {"NOP", AM_1  , 0 }, // F4 .. F7
	{"SED", 0     ,  0}, {"SBC", AM_AY , R_}, {"PLX", 0     ,  0}, {"NOP", AM_1  , 0 }, // F8 .. FB
	{"NOP", AM_3  ,  0}, {"SBC", AM_AX , R_}, {"INC", AM_AX , RW}, {"NOP", AM_1  , 0 }  // FF .. FF
};

const Opcodes_t g_aOpcodes6502[ NUM_OPCODES ] =
{ // Should match Cpu.cpp InternalCpuExecute() switch (*(mem+regs.pc++)) !!

/*
	Based on: http://axis.llx.com/~nparker/a2/opcodes.html

	If you really want to know what the undocumented --- (n/a) opcodes do, see:
	http://www.strotmann.de/twiki/bin/view/APG/AsmUnusedOpcodes

	x0     x1         x2       x3   x4       x5       x6       x7   x8   x9       xA      xB   xC        xD       xE      	xF
0x	BRK    ORA (d,X)  ---      ---  tsb d    ORA d    ASL d    ---  PHP  ORA #    ASL A  ---  tsb a      ORA a    ASL a   	---
1x	BPL r  ORA (d),Y  ora (d)  ---  trb d    ORA d,X  ASL d,X  ---  CLC  ORA a,Y  ina A  ---  trb a      ORA a,X  ASL a,X 	---
2x	JSR a  AND (d,X)  ---      ---  BIT d    AND d    ROL d    ---  PLP  AND #    ROL A  ---  BIT a      AND a    ROL a   	---
3x	BMI r  AND (d),Y  and (d)  ---  bit d,X  AND d,X  ROL d,X  ---  SEC  AND a,Y  dea A  ---  bit a,X    AND a,X  ROL a,X 	---
4x	RTI    EOR (d,X)  ---      ---  ---      EOR d    LSR d    ---  PHA  EOR #    LSR A  ---  JMP a      EOR a    LSR a   	---
5x	BVC r  EOR (d),Y  eor (d)  ---  ---      EOR d,X  LSR d,X  ---  CLI  EOR a,Y  phy    ---  ---        EOR a,X  LSR a,X 	---
6x	RTS    ADC (d,X)  ---      ---  stz d    ADC d    ROR d    ---  PLA  ADC #    ROR A  ---  JMP (a)    ADC a    ROR a   	---
7x	BVS r  ADC (d),Y  adc (d)  ---  stz d,X  ADC d,X  ROR d,X  ---  SEI  ADC a,Y  ply    ---  jmp (a,X)  ADC a,X  ROR a,X 	---
8x	bra r  STA (d,X)  ---      ---  STY d    STA d    STX d    ---  DEY  bit #    TXA    ---  STY a      STA a    STX a   	---
9x	BCC r  STA (d),Y  sta (d)  ---  STY d,X  STA d,X  STX d,Y  ---  TYA  STA a,Y  TXS    ---  Stz a      STA a,X  stz a,X 	---
Ax	LDY #  LDA (d,X)  LDX #    ---  LDY d    LDA d    LDX d    ---  TAY  LDA #    TAX    ---  LDY a      LDA a    LDX a   	---
Bx	BCS r  LDA (d),Y  lda (d)  ---  LDY d,X  LDA d,X  LDX d,Y  ---  CLV  LDA a,Y  TSX    ---  LDY a,X    LDA a,X  LDX a,Y 	---
Cx	CPY #  CMP (d,X)  ---      ---  CPY d    CMP d    DEC d    ---  INY  CMP #    DEX    ---  CPY a      CMP a    DEC a   	---
Dx	BNE r  CMP (d),Y  cmp (d)  ---  ---      CMP d,X  DEC d,X  ---  CLD  CMP a,Y  phx    ---  ---        CMP a,X  DEC a,X 	---
Ex	CPX #  SBC (d,X)  ---      ---  CPX d    SBC d    INC d    ---  INX  SBC #    NOP    ---  CPX a      SBC a    INC a   	---
Fx	BEQ r  SBC (d),Y  sbc (d)  ---  ---      SBC d,X  INC d,X  ---  SED  SBC a,Y  plx    ---  ---        SBC a,X  INC a,X 	---

	Legend:
		UPPERCASE 6502
		lowercase 65C02
			80
			12, 32, 52, 72, 92, B2, D2, F2
			04, 14, 34, 64, 74
			89
			1A, 3A, 5A, 7A, DA, FA
			0C, 1C, 3C, 7C, 9C;
		# Immediate
		A Accumulator (implicit for mnemonic)
		a absolute
		r Relative
		d Destination
		z Zero Page
		d,x
		(d,X)
		(d),Y

*/
	// x3 x7 xB xF are never used
	{TEXT("BRK"), 0             , 0  }, // 00h
	{TEXT("ORA"), ADDR_INDX     , R_ }, // 01h
	{TEXT("NOP"), ADDR_INVALID2 , 0  }, // 02h
	{TEXT("NOP"), ADDR_INVALID1 , 0  }, // 03h
	{TEXT("TSB"), ADDR_ZP      , _W  }, // 04h // 65C02
	{TEXT("ORA"), ADDR_ZP      , R_  }, // 05h
	{TEXT("ASL"), ADDR_ZP      , _W  }, // 06h // MEM_R ?
	{TEXT("NOP"), ADDR_INVALID1 , 0  }, // 07h
	{TEXT("PHP"), 0             , SW }, // 08h
	{TEXT("ORA"), ADDR_IMM      , im }, // 09h
	{TEXT("ASL"), 0             , 0  }, // 0Ah // MEM_IMPLICIT
	{TEXT("NOP"), ADDR_INVALID1 , 0  }, // 0Bh
	{TEXT("TSB"), ADDR_ABS      , _W }, // 0Ch // 65C02
	{TEXT("ORA"), ADDR_ABS      , R_ }, // 0Dh
	{TEXT("ASL"), ADDR_ABS      , _W }, // 0Eh // MEM_R ?
	{TEXT("NOP"), ADDR_INVALID3 , 0  }, // 0Fh
	{TEXT("BPL"), ADDR_REL      , 0  }, // 10h // signed: BGE @reference: http://www.6502.org/tutorials/compare_instructions.html
	{TEXT("ORA"), ADDR_INDY     , R_ }, // 11h
	{TEXT("ORA"), ADDR_IZPG     , R_ }, // 12h
	{TEXT("NOP"), ADDR_INVALID1 , 0  }, // 13h
	{TEXT("TRB"), ADDR_ZP       , _W }, // 14h
	{TEXT("ORA"), ADDR_ZP_X     , R_ }, // 15h
	{TEXT("ASL"), ADDR_ZP_X     , _W }, // 16h
	{TEXT("NOP"), ADDR_INVALID1 , 0  }, // 17h
	{TEXT("CLC"), 0             , 0  }, // 18h
	{TEXT("ORA"), ADDR_ABSY     , R_ }, // 19h
	{TEXT("INA"), 0             , 0  }, // 1Ah INC A
	{TEXT("NOP"), ADDR_INVALID1 , 0  }, // 1Bh
	{TEXT("TRB"), ADDR_ABS      , _W }, // 1Ch
	{TEXT("ORA"), ADDR_ABSX     , R_ }, // 1Dh
	{TEXT("ASL"), ADDR_ABSX          }, // 1Eh
	{TEXT("NOP"), ADDR_INVALID1 , 0  }, // 1Fh
	{TEXT("JSR"), ADDR_ABS           }, // 20h
	{TEXT("AND"), ADDR_INDX          }, // 21h
	{TEXT("NOP"), ADDR_INVALID2 , 0  }, // 22h
	{TEXT("NOP"), ADDR_INVALID1 , 0  }, // 23h
	{TEXT("BIT"), ADDR_ZP            }, // 24h
	{TEXT("AND"), ADDR_ZP            }, // 25h
	{TEXT("ROL"), ADDR_ZP            }, // 26h
	{TEXT("NOP"), ADDR_INVALID1 , 0  }, // 27h
	{TEXT("PLP"), 0                  }, // 28h
	{TEXT("AND"), ADDR_IMM           }, // 29h
	{TEXT("ROL"), 0                  }, // 2Ah
	{TEXT("NOP"), ADDR_INVALID1 , 0  }, // 2Bh
	{TEXT("BIT"), ADDR_ABS           }, // 2Ch
	{TEXT("AND"), ADDR_ABS           }, // 2Dh
	{TEXT("ROL"), ADDR_ABS           }, // 2Eh
	{TEXT("NOP"), ADDR_INVALID3 , 0  }, // 2Fh
	{TEXT("BMI"), ADDR_REL           }, // 30h
	{TEXT("AND"), ADDR_INDY          }, // 31h
	{TEXT("AND"), ADDR_IZPG          }, // 32h
	{TEXT("NOP"), ADDR_INVALID1 , 0  }, // 33h
	{TEXT("BIT"), ADDR_ZP_X          }, // 34h
	{TEXT("AND"), ADDR_ZP_X          }, // 35h
	{TEXT("ROL"), ADDR_ZP_X          }, // 36h
	{TEXT("NOP"), ADDR_INVALID1 , 0  }, // 37h
	{TEXT("SEC"), 0                  }, // 38h
	{TEXT("AND"), ADDR_ABSY          }, // 39h
	{TEXT("DEA"), 0                  }, // 3Ah
	{TEXT("NOP"), ADDR_INVALID1 , 0  }, // 3Bh
	{TEXT("BIT"), ADDR_ABSX          }, // 3Ch
	{TEXT("AND"), ADDR_ABSX          }, // 3Dh
	{TEXT("ROL"), ADDR_ABSX          }, // 3Eh
	{TEXT("NOP"), ADDR_INVALID1 , 0  }, // 3Fh
	{TEXT("RTI"), 0             , 0  }, // 40h
	{TEXT("EOR"), ADDR_INDX          }, // 41h
	{TEXT("NOP"), ADDR_INVALID2 , 0  }, // 42h
	{TEXT("NOP"), ADDR_INVALID1 , 0  }, // 43h
	{TEXT("NOP"), ADDR_INVALID2 , 0  }, // 44h
	{TEXT("EOR"), ADDR_ZP            }, // 45h
	{TEXT("LSR"), ADDR_ZP       , _W }, // 46h // MEM_R ?
	{TEXT("NOP"), ADDR_INVALID1 , 0  }, // 47h
	{TEXT("PHA"), 0                  }, // 48h // MEM_WRITE_IMPLIED | MEM_STACK
	{TEXT("EOR"), ADDR_IMM           }, // 49h
	{TEXT("LSR"), 0                  }, // 4Ah
	{TEXT("NOP"), ADDR_INVALID1 , 0  }, // 4Bh
	{TEXT("JMP"), ADDR_ABS           }, // 4Ch
	{TEXT("EOR"), ADDR_ABS           }, // 4Dh
	{TEXT("LSR"), ADDR_ABS      , _W }, // 4Eh // MEM_R ?
	{TEXT("NOP"), ADDR_INVALID1 , 0  }, // 4Fh
	{TEXT("BVC"), ADDR_REL           }, // 50h
	{TEXT("EOR"), ADDR_INDY          }, // 51h
	{TEXT("EOR"), ADDR_IZPG          }, // 52h
	{TEXT("NOP"), ADDR_INVALID1 , 0  }, // 53h
	{TEXT("NOP"), ADDR_INVALID2 , 0  }, // 54h
	{TEXT("EOR"), ADDR_ZP_X          }, // 55h
	{TEXT("LSR"), ADDR_ZP_X     , _W }, // 56h // MEM_R ?
	{TEXT("NOP"), ADDR_INVALID1 , 0  }, // 57h
	{TEXT("CLI"), 0                  }, // 58h
	{TEXT("EOR"), ADDR_ABSY          }, // 59h
	{TEXT("PHY"), 0                  }, // 5Ah
	{TEXT("NOP"), ADDR_INVALID1 , 0  }, // 5Bh
	{TEXT("NOP"), ADDR_INVALID3 , 0  }, // 5Ch
	{TEXT("EOR"), ADDR_ABSX          }, // 5Dh
	{TEXT("LSR"), ADDR_ABSX          }, // 5Eh
	{TEXT("NOP"), ADDR_INVALID1 , 0  }, // 5Fh
	{TEXT("RTS"), 0             , SR }, // 60h
	{TEXT("ADC"), ADDR_INDX          }, // 61h
	{TEXT("NOP"), ADDR_INVALID2 , 0  }, // 62h
	{TEXT("NOP"), ADDR_INVALID1 , 0  }, // 63h
	{TEXT("STZ"), ADDR_ZP            }, // 64h
	{TEXT("ADC"), ADDR_ZP            }, // 65h
	{TEXT("ROR"), ADDR_ZP            }, // 66h
	{TEXT("NOP"), ADDR_INVALID1 , 0  }, // 67h
	{TEXT("PLA"), 0             , SW }, // 68h
	{TEXT("ADC"), ADDR_IMM           }, // 69h
	{TEXT("ROR"), 0                  }, // 6Ah
	{TEXT("NOP"), ADDR_INVALID1 , 0  }, // 6Bh
	{TEXT("JMP"), ADDR_IABS          }, // 6Ch
	{TEXT("ADC"), ADDR_ABS           }, // 6Dh
	{TEXT("ROR"), ADDR_ABS           }, // 6Eh
	{TEXT("NOP"), ADDR_INVALID1 , 0  }, // 6Fh
	{TEXT("BVS"), ADDR_REL           }, // 70h
	{TEXT("ADC"), ADDR_INDY          }, // 71h
	{TEXT("ADC"), ADDR_IZPG          }, // 72h
	{TEXT("NOP"), ADDR_INVALID1 , 0  }, // 73h 
	{TEXT("STZ"), ADDR_ZP_X          }, // 74h
	{TEXT("ADC"), ADDR_ZP_X          }, // 75h
	{TEXT("ROR"), ADDR_ZP_X          }, // 76h
	{TEXT("NOP"), ADDR_INVALID1 , 0  }, // 77h
	{TEXT("SEI"), 0                  }, // 78h
	{TEXT("ADC"), ADDR_ABSY          }, // 79h
	{TEXT("PLY"), 0             , SR }, // 7Ah
	{TEXT("NOP"), ADDR_INVALID1 , 0  }, // 7Bh
	{TEXT("JMP"), ADDR_ABSIINDX      }, // 7Ch
	{TEXT("ADC"), ADDR_ABSX          }, // 7Dh
	{TEXT("ROR"), ADDR_ABSX          }, // 7Eh
	{TEXT("NOP"), ADDR_INVALID1 , 0  }, // 7Fh
	{TEXT("BRA"), ADDR_REL           }, // 80h
	{TEXT("STA"), ADDR_INDX     , _W }, // 81h
	{TEXT("NOP"), ADDR_INVALID2 , 0  }, // 82h
	{TEXT("NOP"), ADDR_INVALID1 , 0  }, // 83h
	{TEXT("STY"), ADDR_ZP       , _W }, // 84h
	{TEXT("STA"), ADDR_ZP       , _W }, // 85h
	{TEXT("STX"), ADDR_ZP       , _W }, // 86h
	{TEXT("NOP"), ADDR_INVALID1 , 0  }, // 87h
	{TEXT("DEY"), 0             , 0  }, // 88h // Explicit
	{TEXT("BIT"), ADDR_IMM           }, // 89h
	{TEXT("TXA"), 0             , 0  }, // 8Ah // Explicit
	{TEXT("NOP"), ADDR_INVALID1 , 0  }, // 8Bh
	{TEXT("STY"), ADDR_ABS      , _W }, // 8Ch
	{TEXT("STA"), ADDR_ABS      , _W }, // 8Dh
	{TEXT("STX"), ADDR_ABS      , _W }, // 8Eh
	{TEXT("NOP"), ADDR_INVALID1 , 0  }, // 8Fh
	{TEXT("BCC"), ADDR_REL      , 0  }, // 90h // MEM_IMMEDIATE
	{TEXT("STA"), ADDR_INDY     , _W }, // 91h
	{TEXT("STA"), ADDR_IZPG     , _W }, // 92h
	{TEXT("NOP"), ADDR_INVALID1 , 0  }, // 93h
	{TEXT("STY"), ADDR_ZP_X     , _W }, // 94h
	{TEXT("STA"), ADDR_ZP_X     , _W }, // 95h
	{TEXT("STX"), ADDR_ZP_Y     , _W }, // 96h
	{TEXT("NOP"), ADDR_INVALID1 , 0  }, // 97h
	{TEXT("TYA"), 0             , 0  }, // 98h // Explicit
	{TEXT("STA"), ADDR_ABSY     , _W }, // 99h
	{TEXT("TXS"), 0             , 0  }, // 9Ah // EXplicit
	{TEXT("NOP"), ADDR_INVALID1 , 0  }, // 9Bh
	{TEXT("STZ"), ADDR_ABS      , _W }, // 9Ch
	{TEXT("STA"), ADDR_ABSX     , _W }, // 9Dh
	{TEXT("STZ"), ADDR_ABSX     , _W }, // 9Eh
	{TEXT("NOP"), ADDR_INVALID1 , 0  }, // 9Fh
	{TEXT("LDY"), ADDR_IMM      , 0  }, // A0h // MEM_IMMEDIATE
	{TEXT("LDA"), ADDR_INDX     , R_ }, // A1h
	{TEXT("LDX"), ADDR_IMM      , 0  }, // A2h // MEM_IMMEDIATE
	{TEXT("NOP"), ADDR_INVALID1 , 0  }, // A3h
	{TEXT("LDY"), ADDR_ZP       , R_ }, // A4h
	{TEXT("LDA"), ADDR_ZP       , R_ }, // A5h
	{TEXT("LDX"), ADDR_ZP       , R_ }, // A6h
	{TEXT("NOP"), ADDR_INVALID1 , 0  }, // A7h
	{TEXT("TAY"), 0             , 0  }, // A8h // Explicit
	{TEXT("LDA"), ADDR_IMM      , 0  }, // A9h // MEM_IMMEDIATE
	{TEXT("TAX"), 0             , 0  }, // AAh
	{TEXT("NOP"), ADDR_INVALID1 , 0  }, // ABh
	{TEXT("LDY"), ADDR_ABS      , R_ }, // ACh
	{TEXT("LDA"), ADDR_ABS      , R_ }, // ADh
	{TEXT("LDX"), ADDR_ABS      , R_ }, // AEh
	{TEXT("NOP"), ADDR_INVALID1 , 0  }, // AFh
	{TEXT("BCS"), ADDR_REL      , 0  }, // B0h // MEM_IMMEDIATE // unsigned: BGE
	{TEXT("LDA"), ADDR_INDY     , R_ }, // B1h
	{TEXT("LDA"), ADDR_IZPG     , R_ }, // B2h
	{TEXT("NOP"), ADDR_INVALID1 , 0  }, // B3h
	{TEXT("LDY"), ADDR_ZP_X     , R_ }, // B4h
	{TEXT("LDA"), ADDR_ZP_X     , R_ }, // B5h
	{TEXT("LDX"), ADDR_ZP_Y     , R_ }, // B6h
	{TEXT("NOP"), ADDR_INVALID1 , 0  }, // B7h
	{TEXT("CLV"), 0             , 0  }, // B8h // Explicit
	{TEXT("LDA"), ADDR_ABSY     , R_ }, // B9h
	{TEXT("TSX"), 0             , 0  }, // BAh // Explicit
	{TEXT("NOP"), ADDR_INVALID1 , 0  }, // BBh
	{TEXT("LDY"), ADDR_ABSX     , R_ }, // BCh
	{TEXT("LDA"), ADDR_ABSX     , R_ }, // BDh
	{TEXT("LDX"), ADDR_ABSY     , R_ }, // BEh
	{TEXT("NOP"), ADDR_INVALID1 , 0  }, // BFh
	{TEXT("CPY"), ADDR_IMM           }, // C0h
	{TEXT("CMP"), ADDR_INDX          }, // C1h
	{TEXT("NOP"), ADDR_INVALID2 , 0  }, // C2h
	{TEXT("NOP"), ADDR_INVALID1 , 0  }, // C3h
	{TEXT("CPY"), ADDR_ZP            }, // C4h
	{TEXT("CMP"), ADDR_ZP            }, // C5h
	{TEXT("DEC"), ADDR_ZP            }, // C6h
	{TEXT("NOP"), ADDR_INVALID1 , 0  }, // C7h
	{TEXT("INY"), 0                  }, // C8h
	{TEXT("CMP"), ADDR_IMM           }, // C9h
	{TEXT("DEX"), 0                  }, // CAh
	{TEXT("NOP"), ADDR_INVALID1 , 0  }, // CBh
	{TEXT("CPY"), ADDR_ABS           }, // CCh
	{TEXT("CMP"), ADDR_ABS           }, // CDh
	{TEXT("DEC"), ADDR_ABS           }, // CEh
	{TEXT("NOP"), ADDR_INVALID1      }, // CFh
	{TEXT("BNE"), ADDR_REL           }, // D0h
	{TEXT("CMP"), ADDR_INDY          }, // D1h
	{TEXT("CMP"), ADDR_IZPG          }, // D2h
	{TEXT("NOP"), ADDR_INVALID1 , 0  }, // D3h
	{TEXT("NOP"), ADDR_INVALID2 , 0  }, // D4h
	{TEXT("CMP"), ADDR_ZP_X          }, // D5h
	{TEXT("DEC"), ADDR_ZP_X          }, // D6h
	{TEXT("NOP"), ADDR_INVALID1 , 0  }, // D7h
	{TEXT("CLD"), 0                  }, // D8h
	{TEXT("CMP"), ADDR_ABSY          }, // D9h
	{TEXT("PHX"), 0                  }, // DAh
	{TEXT("NOP"), ADDR_INVALID1 , 0  }, // DBh
	{TEXT("NOP"), ADDR_INVALID3 , 0  }, // DCh
	{TEXT("CMP"), ADDR_ABSX          }, // DDh
	{TEXT("DEC"), ADDR_ABSX          }, // DEh
	{TEXT("NOP"), ADDR_INVALID1 , 0  }, // DFh
	{TEXT("CPX"), ADDR_IMM           }, // E0h
	{TEXT("SBC"), ADDR_INDX          }, // E1h
	{TEXT("NOP"), ADDR_INVALID2 , 0  }, // E2h
	{TEXT("NOP"), ADDR_INVALID1 , 0  }, // E3h
	{TEXT("CPX"), ADDR_ZP            }, // E4h
	{TEXT("SBC"), ADDR_ZP            }, // E5h
	{TEXT("INC"), ADDR_ZP            }, // E6h
	{TEXT("NOP"), ADDR_INVALID1 , 0  }, // E7h
	{TEXT("INX"), 0                  }, // E8h
	{TEXT("SBC"), ADDR_IMM           }, // E9h
	{TEXT("NOP"), 0             , 0  }, // EAh
	{TEXT("NOP"), ADDR_INVALID1      }, // EBh
	{TEXT("CPX"), ADDR_ABS           }, // ECh
	{TEXT("SBC"), ADDR_ABS           }, // EDh
	{TEXT("INC"), ADDR_ABS           }, // EEh
	{TEXT("NOP"), ADDR_INVALID1 , 0  }, // EFh
	{TEXT("BEQ"), ADDR_REL           }, // F0h
	{TEXT("SBC"), ADDR_INDY          }, // F1h
	{TEXT("SBC"), ADDR_IZPG          }, // F2h
	{TEXT("NOP"), ADDR_INVALID1 , 0  }, // F3h
	{TEXT("NOP"), ADDR_INVALID2 , 0  }, // F4h
	{TEXT("SBC"), ADDR_ZP_X          }, // F5h
	{TEXT("INC"), ADDR_ZP_X          }, // F6h
	{TEXT("NOP"), ADDR_INVALID1 , 0  }, // F7h
	{TEXT("SED"), 0                  }, // F8h
	{TEXT("SBC"), ADDR_ABSY          }, // F9h
	{TEXT("PLX"), 0                  }, // FAh
	{TEXT("NOP"), ADDR_INVALID1 , 0  }, // FBh
	{TEXT("NOP"), ADDR_INVALID3 , 0  }, // FCh
	{TEXT("SBC"), ADDR_ABSX          }, // FDh
	{TEXT("INC"), ADDR_ABSX          }, // FEh
	{TEXT("NOP"), ADDR_INVALID1 , 0  }  // FFh
};

#undef R_
#undef _W
#undef RW
#undef _S
#undef im
#undef SW
#undef SR

	const Opcodes_t *g_aOpcodes = NULL; // & g_aOpcodes65C02[ 0 ];


// Memory _________________________________________________________________________________________
	MemoryDump_t g_aMemDump[ NUM_MEM_DUMPS ];


// Parameters _____________________________________________________________________________________

	// NOTE: Order MUST match Parameters_e[] !!!
	Command_t g_aParameters[] =
	{
// Breakpoint
		{TEXT("<=")         , NULL, PARAM_BP_LESS_EQUAL     },
		{TEXT("<" )         , NULL, PARAM_BP_LESS_THAN      },
		{TEXT("=" )         , NULL, PARAM_BP_EQUAL          },
		{TEXT("!" )         , NULL, PARAM_BP_NOT_EQUAL      },
		{TEXT(">" )         , NULL, PARAM_BP_GREATER_THAN   },
		{TEXT(">=")         , NULL, PARAM_BP_GREATER_EQUAL  },
		{TEXT("R")          , NULL, PARAM_BP_READ           },
		{TEXT("?")          , NULL, PARAM_BP_READ           },
		{TEXT("W")          , NULL, PARAM_BP_WRITE          },
		{TEXT("@")          , NULL, PARAM_BP_WRITE          },
		{TEXT("*")          , NULL, PARAM_BP_READ_WRITE     },
// Regs (for PUSH / POP)
		{TEXT("A")          , NULL, PARAM_REG_A          },
		{TEXT("X")          , NULL, PARAM_REG_X          },
		{TEXT("Y")          , NULL, PARAM_REG_Y          },
		{TEXT("PC")         , NULL, PARAM_REG_PC         },
		{TEXT("S")          , NULL, PARAM_REG_SP         },
// Flags
		{TEXT("P")          , NULL, PARAM_FLAGS          },
		{TEXT("C")          , NULL, PARAM_FLAG_C         }, // ---- ---1 Carry
		{TEXT("Z")          , NULL, PARAM_FLAG_Z         }, // ---- --1- Zero
		{TEXT("I")          , NULL, PARAM_FLAG_I         }, // ---- -1-- Interrupt
		{TEXT("D")          , NULL, PARAM_FLAG_D         }, // ---- 1--- Decimal
		{TEXT("B")          , NULL, PARAM_FLAG_B         }, // ---1 ---- Break
		{TEXT("R")          , NULL, PARAM_FLAG_R         }, // --1- ---- Reserved
		{TEXT("V")          , NULL, PARAM_FLAG_V         }, // -1-- ---- Overflow
		{TEXT("N")          , NULL, PARAM_FLAG_N         }, // 1--- ---- Sign
// Font
		{TEXT("MODE")       , NULL, PARAM_FONT_MODE      }, // also INFO, CONSOLE, DISASM (from Window)
// General
		{TEXT("FIND")       , NULL, PARAM_FIND           },
		{TEXT("CLEAR")      , NULL, PARAM_CLEAR          },
		{TEXT("LOAD")       , NULL, PARAM_LOAD           },
		{TEXT("LIST")       , NULL, PARAM_LIST           },
		{TEXT("OFF")        , NULL, PARAM_OFF            },
		{TEXT("ON")         , NULL, PARAM_ON             },
		{TEXT("RESET")      , NULL, PARAM_RESET          },
		{TEXT("SAVE")       , NULL, PARAM_SAVE           },
		{TEXT("START")      , NULL, PARAM_START          }, // benchmark
		{TEXT("STOP")       , NULL, PARAM_STOP           }, // benchmark
// Help Categories
		{TEXT("*")          , NULL, PARAM_WILDSTAR        },
		{TEXT("BREAKPOINTS"), NULL, PARAM_CAT_BREAKPOINTS },
		{TEXT("CONFIG")     , NULL, PARAM_CAT_CONFIG      },
		{TEXT("CPU")        , NULL, PARAM_CAT_CPU         },
		{TEXT("FLAGS")      , NULL, PARAM_CAT_FLAGS       },
		{TEXT("MEMORY")     , NULL, PARAM_CAT_MEMORY      },
		{TEXT("MEM")        , NULL, PARAM_CAT_MEMORY      }, // alias // SOURCE [SYMBOLS] [MEMORY] filename
		{TEXT("SYMBOLS")    , NULL, PARAM_CAT_SYMBOLS     },
		{TEXT("WATCHES")    , NULL, PARAM_CAT_WATCHES     },
		{TEXT("WINDOW")     , NULL, PARAM_CAT_WINDOW      },
		{TEXT("ZEROPAGE")   , NULL, PARAM_CAT_ZEROPAGE    },	
// Source level debugging
		{TEXT("MEM")        , NULL, PARAM_SRC_MEMORY      },
		{TEXT("MEMORY")     , NULL, PARAM_SRC_MEMORY      },
		{TEXT("SYM")        , NULL, PARAM_SRC_SYMBOLS     },	
		{TEXT("SYMBOLS")    , NULL, PARAM_SRC_SYMBOLS     },	
		{TEXT("MERLIN")     , NULL, PARAM_SRC_MERLIN      },	
		{TEXT("ORCA")       , NULL, PARAM_SRC_ORCA        },	
// Window                                                       Win   Cmd   WinEffects      CmdEffects
		{TEXT("CODE")       , NULL, PARAM_CODE           }, //   x     x    code win only   switch to code window
//		{TEXT("CODE1")      , NULL, PARAM_CODE_1         }, //   -     x    code/data win   
		{TEXT("CODE2")      , NULL, PARAM_CODE_2         }, //   -     x    code/data win   
		{TEXT("CONSOLE")    , NULL, PARAM_CONSOLE        }, //   x     -                    switch to console window
		{TEXT("DATA")       , NULL, PARAM_DATA           }, //   x     x    data win only   switch to data window
//		{TEXT("DATA1")      , NULL, PARAM_DATA_1         }, //   -     x    code/data win   
		{TEXT("DATA2")      , NULL, PARAM_DATA_2         }, //   -     x    code/data win   
		{TEXT("DISASM")     , NULL, PARAM_DISASM         }, //                              
		{TEXT("INFO")       , NULL, PARAM_INFO           }, //   -     x    code/data       Toggles showing/hiding Regs/Stack/BP/Watches/ZP
		{TEXT("SOURCE")     , NULL, PARAM_SOURCE         }, //   x     x                    switch to source window
		{TEXT("SRC")        , NULL, PARAM_SOURCE         }, // alias                        
//		{TEXT("SOURCE_1")   , NULL, PARAM_SOURCE_1       }, //   -     x    code/data       
		{TEXT("SOURCE2 ")   , NULL, PARAM_SOURCE_2       }, //   -     x                    
		{TEXT("SYMBOLS")    , NULL, PARAM_SYMBOLS        }, //   x     x    code/data win   switch to symbols window
		{TEXT("SYM")        , NULL, PARAM_SYMBOLS        }, // alias   x                    SOURCE [SYM] [MEM] filename
//		{TEXT("SYMBOL1")    , NULL, PARAM_SYMBOL_1       }, //   -     x    code/data win   
		{TEXT("SYMBOL2")    , NULL, PARAM_SYMBOL_2       }, //   -     x    code/data win   
// Internal Consistency Check
		{ TEXT( __PARAMS_VERIFY_TXT__), NULL, NUM_PARAMS     },
	};

// Profile
	const int NUM_PROFILE_LINES = NUM_OPCODES + NUM_OPMODES + 16;

	ProfileOpcode_t g_aProfileOpcodes[ NUM_OPCODES ];
	ProfileOpmode_t g_aProfileOpmodes[ NUM_OPMODES ];

	TCHAR g_FileNameProfile[] = TEXT("Profile.txt"); // changed from .csv to .txt since Excel doesn't give import options.
	int   g_nProfileLine = 0;
	char  g_aProfileLine[ NUM_PROFILE_LINES ][ CONSOLE_WIDTH ];

	void ProfileReset  ();
	bool ProfileSave   ();
	void ProfileFormat( bool bSeperateColumns, ProfileFormat_e eFormatMode );

	char * ProfileLinePeek ( int iLine );
	char * ProfileLinePush ();
	void ProfileLineReset  ();


// Source Level Debugging _________________________________________________________________________
	bool  g_bSourceLevelDebugging = false;
	bool  g_bSourceAddSymbols     = false;
	bool  g_bSourceAddMemory      = false;

	TCHAR  g_aSourceFileName[ MAX_PATH ] = TEXT("");
	TCHAR  g_aSourceFontName[ MAX_FONT_NAME ] = TEXT("Arial");
	int    g_nSourceBuffer =    0; // Allocated bytes for buffer
	char  *g_pSourceBuffer = NULL; // Buffer of File
	vector<char *> g_vSourceLines; // array of pointers to start of lines

	int   g_iSourceDisplayStart  = 0;
//	int   g_nSourceDisplayTotal  = 0;

	int    g_nSourceAssembleBytes = 0;
	int    g_nSourceAssemblyLines = 0;
	int    g_nSourceAssemblySymbols = 0;

	// TODO: Support multiple source filenames
	SourceAssembly_t g_aSourceDebug;


// Symbols ________________________________________________________________________________________
	TCHAR * g_aSymbolTableNames[ NUM_SYMBOL_TABLES ] =
	{
		TEXT("Main"),
		TEXT("User"),
		TEXT("Src" )
	};

	SymbolTable_t g_aSymbols[ NUM_SYMBOL_TABLES ];
	int           g_nSymbolsLoaded = 0;  // on Last Load
	bool          g_aConfigSymbolsDisplayed[ NUM_SYMBOL_TABLES ] =
	{
		true,
		true,
		true
	};


// Watches ________________________________________________________________________________________
	int       g_nWatches = 0;
	Watches_t g_aWatches[ MAX_WATCHES ]; // use vector<Watch_t> ??


// Window _________________________________________________________________________________________
	int           g_iWindowLast = WINDOW_CODE;
	int           g_iWindowThis = WINDOW_CODE;
	WindowSplit_t g_aWindowConfig[ NUM_WINDOWS ];


// Zero Page Pointers _____________________________________________________________________________
	int                g_nZeroPagePointers = 0;
	ZeroPagePointers_t g_aZeroPagePointers[ MAX_ZEROPAGE_POINTERS ]; // TODO: use vector<> ?


	enum DebugConfigVersion_e
	{
		VERSION_0,
		CURRENT_VERSION = VERSION_0
	};


// Misc. __________________________________________________________________________________________

	TCHAR     g_FileNameConfig [] = TEXT("Debug.cfg"); // CONFIGSAVE
	TCHAR     g_FileNameSymbolsMain[] =  TEXT("APPLE2E.SYM");
	TCHAR     g_FileNameSymbolsUser[ MAX_PATH ] = TEXT("");
	TCHAR     g_FileNameTrace  [] = TEXT("Trace.txt");

	bool      g_bBenchmarking = false;

	BOOL      fulldisp      = 0;
	WORD      lastpc        = 0;
	LPBYTE    membank       = NULL;

	BOOL      g_bProfiling       = 0;
	int       g_nDebugSteps      = 0;
	DWORD     g_nDebugStepCycles = 0;
	int       g_nDebugStepStart  = 0;
	int       g_nDebugStepUntil  = -1;

	int       g_nDebugSkipStart = 0;
	int       g_nDebugSkipLen   = 0;

	FILE     *g_hTraceFile       = NULL;

// PUBLIC
	DWORD     extbench      = 0;
	bool      g_bDebuggerViewingAppleOutput = false;

// Prototypes _______________________________________________________________

// Command Processing
	Update_t Help_Arg_1( int iCommandHelp );
	int _Arg_1     ( int nValue );
	int _Arg_1     ( LPTSTR pName );
	int _Arg_Shift ( int iSrc, int iEnd, int iDst = 0 );
	void ArgsClear ();
	int  ArgsGet   (TCHAR *pInput);
	int  ArgsParse (const int nArgs);

	void DisplayAmbigiousCommands ( int nFound );

	enum Match_e
	{
		MATCH_EXACT,
		MATCH_FUZZY
	};
	int FindParam( LPTSTR pLookupName, Match_e eMatch, int & iParam_, const int iParamBegin = 0, const int iParamEnd = NUM_PARAMS - 1 );
	int FindCommand( LPTSTR pName, CmdFuncPtr_t & pFunction_, int * iCommand_ = NULL );

	int ParseConsoleInput( LPTSTR pConsoleInput );

// Console
//	DWORD _Color( int iWidget );

	// Buffered
	void     ConsoleBufferToDisplay();
	LPCSTR   ConsoleBufferPeek();
	void     ConsoleBufferPop();
	bool     ConsoleBufferPush( const TCHAR * pString );

	// Display
	Update_t ConsoleDisplayError (LPCTSTR errortext);
	void     ConsoleDisplayPause ();
	void     ConsoleDisplayPush  ( LPCSTR pText );
	Update_t ConsoleUpdate       ();

	// Input
	void     ConsoleInputToDisplay();
	LPCSTR   ConsoleInputPeek     ();
	bool     ConsoleInputClear    ();
	bool     ConsoleInputBackSpace();
	bool     ConsoleInputChar     ( TCHAR ch );
	void     ConsoleInputReset    ();
	int      ConsoleInputTabCompletion();

	// Scrolling
	Update_t ConsoleScrollHome   ();
	Update_t ConsoleScrollEnd    ();
	Update_t ConsoleScrollUp     ( int nLines );
	Update_t ConsoleScrollDn     ( int nLines );
	Update_t ConsoleScrollPageUp ();
	Update_t ConsoleScrollPageDn ();

// Source Level Debugging
	bool BufferAssemblyListing ( TCHAR * pFileName );
	bool ParseAssemblyListing  ( bool bBytesToMemory, bool bAddSymbols );

// Font
	void _UpdateWindowFontHeights(int nFontHeight);

// Drawing
	COLORREF DebugGetColor ( int iColor );
	bool DebugSetColor ( const int iScheme, const int iColor, const COLORREF nColor );
	void _CmdColorGet ( const int iScheme, const int iColor );

	HDC g_hDC = 0;

	int DebugDrawText      ( LPCTSTR pText, RECT & rRect );
	int DebugDrawTextFixed ( LPCTSTR pText, RECT & rRect );
	int DebugDrawTextLine  ( LPCTSTR pText, RECT & rRect );
	int DebugDrawTextHorz  ( LPCTSTR pText, RECT & rRect );


	void DrawWindow_Source      (Update_t bUpdate);

	void DrawSubWindow_IO       (Update_t bUpdate);
	void DrawSubWindow_Source1  (Update_t bUpdate);
	void DrawSubWindow_Source2  (Update_t bUpdate);
	void DrawSubWindow_Symbols  (Update_t bUpdate);
	void DrawSubWindow_ZeroPage (Update_t bUpdate);

	void DrawBreakpoints      (HDC dc, int line);
	void DrawConsoleInput     (HDC dc);
	void DrawConsoleLine      (LPCSTR pText, int y);
	WORD DrawDisassemblyLine  (HDC dc, int line, WORD offset, LPTSTR text);
	void DrawFlags            (HDC dc, int line, WORD nRegFlags, LPTSTR pFlagNames_);
	void DrawMemory           (HDC dc, int line, int iMem );
	void DrawRegister         (HDC dc, int line, LPCTSTR name, int bytes, WORD value, int iSource = 0 );
	void DrawStack            (HDC dc, int line);
	void DrawTargets          (HDC dc, int line);
	void DrawWatches          (HDC dc, int line);
	void DrawZeroPagePointers (HDC dc, int line);

	void UpdateDisplay( Update_t bUpdate );

// Symbol Table / Memory
	Update_t _CmdSymbolsClear      ( Symbols_e eSymbolTable );
	Update_t _CmdSymbolsCommon     ( int nArgs, int bSymbolTables );
	Update_t _CmdSymbolsListTables (int nArgs, int bSymbolTables );
	Update_t _CmdSymbolsUpdate     ( int nArgs );

	bool _CmdSymbolList_Address2Symbol( int nAddress   , int bSymbolTables );
	bool _CmdSymbolList_Symbol2Address( LPCTSTR pSymbol, int bSymbolTables );

	bool FindAddressFromSymbol( LPCSTR pSymbol, WORD * pAddress_ = NULL, int * iTable_ = NULL );
	WORD GetAddressFromSymbol (LPCTSTR symbol); // -PATCH MJP, HACK: returned 0 if symbol not found

	LPCTSTR FindSymbolFromAddress (WORD nAdress, int * iTable_ = NULL );
	LPCTSTR GetSymbol   (WORD nAddress, int nBytes);
	bool Get6502Targets (int *pTemp_, int *pFinal_, int *pBytes_ );

// Window
	void _WindowJoin    ();
	void _WindowSplit   (Window_e eNewBottomWindow );
	void _WindowLast    ();
	void _WindowSwitch  ( int eNewWindow );
	int  WindowGetHeight ( int iWindow );
	void WindowUpdateDisasmSize           ();
	void WindowUpdateConsoleDisplayedSize ();
	void WindowUpdateSizes                ();
	Update_t _CmdWindowViewFull   (int iNewWindow);
	Update_t _CmdWindowViewCommon (int iNewWindow);


// Utility
	BYTE  Chars2ToByte( char *pText );
	bool  IsHexString( LPCSTR pText );

	int          RemoveWhiteSpaceReverse    (       char *pSrc );
	const TCHAR* SkipEOL                    ( const TCHAR *pSrc );
	const TCHAR* SkipUntilChar              ( const TCHAR *pSrc, const TCHAR nDelim );
	const TCHAR* SkipUntilEOL               ( const TCHAR *pSrc );
	const TCHAR* SkipWhiteSpace             ( const TCHAR *pSrc );
	const TCHAR* SkipUntilWhiteSpace        ( const TCHAR *pSrc );
	const TCHAR* SkipUntilTab               ( const TCHAR *pSrc);
	const TCHAR* SkipWhiteSpaceReverse      ( const TCHAR *pSrc, const TCHAR *pStart );
	const TCHAR* SkipUntilWhiteSpaceReverse ( const TCHAR *pSrc, const TCHAR *pStart );
	void TextConvertTabsToSpaces( TCHAR *pDeTabified_, LPCTSTR pText, const int nDstSize, int nTabStop = 0 );

	bool  StringCat( TCHAR * pDst, LPCSTR pSrc, const int nDstSize );
	bool  TestStringCat ( TCHAR * pDst, LPCSTR pSrc, const int nDstSize );
	bool  TryStringCat ( TCHAR * pDst, LPCSTR pSrc, const int nDstSize );

	char FormatCharTxtCtrl ( const BYTE b, bool *pWasCtrl_ );
	char FormatCharTxtAsci ( const BYTE b, bool *pWasAsci_ );
	char FormatCharTxtHigh ( const BYTE b, bool *pWasHi_ );
	char FormatChar4Font ( const BYTE b, bool *pWasHi_, bool *pWasLo_ );
	void SetupColorsHiLoBits ( HDC dc, bool bHiBit, bool bLoBit, 
		const int iBackground, const int iForeground,
		const int iColorHiBG , /*const int iColorHiFG,
		const int iColorLoBG , */const int iColorLoFG );
	char ColorizeSpecialChar( HDC hDC, TCHAR * sText, BYTE nData, const MemoryView_e iView,
		const int iTxtBackground  = BG_INFO     , const int iTxtForeground  = FG_DISASM_CHAR,
		const int iHighBackground = BG_INFO_CHAR, const int iHighForeground = FG_INFO_CHAR_HI,
		const int iLowBackground  = BG_INFO_CHAR, const int iLowForeground  = FG_INFO_CHAR_LO );

	int FormatDisassemblyLine(  WORD nOffset, int iMode, int nOpBytes,
		char *sAddress_, char *sOpCodes_, char *sTarget_, char *sTargetOffset_, int & nTargetOffset_, char * sImmediate_, char & nImmediate_, char *sBranch_ );

//	bool CheckBreakpoint (WORD address, BOOL memory);
	bool CheckBreakpointsIO   ();
	bool CheckBreakpointsReg  ();
	bool _CmdBreakpointAddReg ( Breakpoint_t *pBP, BreakpointSource_t iSrc, BreakpointOperator_t iCmp, WORD nAddress, int nLen );
	bool _CmdBreakpointAddCommonArg ( int iArg, int nArg, BreakpointSource_t iSrc, BreakpointOperator_t iCmp );
	void _CursorMoveDownAligned( int nDelta );
	void _CursorMoveUpAligned( int nDelta );

	void DisasmCalcTopFromCurAddress( bool bUpdateTop = true );
	bool InternalSingleStep ();

	void DisasmCalcCurFromTopAddress();
	void DisasmCalcBotFromTopAddress();
	void DisasmCalcTopBotAddress ();
	WORD DisasmCalcAddressFromLines( WORD iAddress, int nLines );

	int  _6502GetOpmodeOpbyte( const int iAddress, int & iOpmode_, int & nOpbyte_ );
	void _6502GetOpcodeOpmode( int & iOpcode_, int & iOpmode_, int & nOpbyte_ );
	
// Utility ________________________________________________________________________________________


int _6502GetOpmodeOpbyte( const int iAddress, int & iOpmode_, int & nOpbyte_ )
{
	int iOpcode_ = *(mem + iAddress);
	iOpmode_ = g_aOpcodes[ iOpcode_ ].nAddressMode;
	nOpbyte_ = g_aOpmodes[ iOpmode_ ].m_nBytes;

#if _DEBUG
	if (iOpcode_ >= NUM_OPCODES)
	{
		bool bStop = true;
	}
#endif

	return iOpcode_;
}

inline
void _6502GetOpcodeOpmodeOpbyte( int & iOpcode_, int & iOpmode_, int & nOpbyte_ )
{
	iOpcode_ = _6502GetOpmodeOpbyte( regs.pc, iOpmode_, nOpbyte_ );
}


//===========================================================================
char  FormatCharTxtAsci ( const BYTE b, bool * pWasAsci_ )
{
	if (pWasAsci_)
		*pWasAsci_ = false;

	char c = (b & 0x7F);
	if (b <= 0x7F)
	{
		if (pWasAsci_)
		{
			*pWasAsci_ = true;			
		}
	}
	return c;
}

//===========================================================================
char  FormatCharTxtCtrl ( const BYTE b, bool * pWasCtrl_ )
{
	if (pWasCtrl_)
		*pWasCtrl_ = false;

	char c = (b & 0x7F); // .32 Changed: Lo now maps High Ascii to printable chars. i.e. ML1 D0D0
	if (b < 0x20) // SPACE
	{
		if (pWasCtrl_)
		{
			*pWasCtrl_ = true;			
		}
		c = b + '@'; // map ctrl chars to visible
	}
	return c;
}

//===========================================================================
char  FormatCharTxtHigh ( const BYTE b, bool *pWasHi_ )
{
	if (pWasHi_)
		*pWasHi_ = false;

	char c = b;
	if (b > 0x7F)
	{
		if (pWasHi_)
		{
			*pWasHi_ = true;			
		}
		c = (b & 0x7F);
	}
	return c;
}


//===========================================================================
char FormatChar4Font ( const BYTE b, bool *pWasHi_, bool *pWasLo_ )
{
	// Most Windows Fonts don't have (printable) glyphs for control chars
	BYTE b1 = FormatCharTxtHigh( b , pWasHi_ );
	BYTE b2 = FormatCharTxtCtrl( b1, pWasLo_ );
	return b2;
}


//===========================================================================
void SetupColorsHiLoBits ( HDC hDC, bool bHighBit, bool bCtrlBit,
	const int iBackground, const int iForeground,
	const int iColorHiBG , const int iColorHiFG,
	const int iColorLoBG , const int iColorLoFG )
{
	// 4 cases: 
	// Hi Lo Background Foreground -> just map Lo -> FG, Hi -> BG
	// 0  0  normal     normal     BG_INFO        FG_DISASM_CHAR   (dark cyan bright cyan)
	// 0  1  normal     LoFG       BG_INFO        FG_DISASM_OPCODE (dark cyan yellow)
	// 1  0  HiBG       normal     BG_INFO_CHAR   FG_DISASM_CHAR   (mid cyan  bright cyan)
	// 1  1  HiBG       LoFG       BG_INFO_CHAR   FG_DISASM_OPCODE (mid cyan  yellow)

	SetBkColor(   hDC, DebugGetColor( iBackground ));
	SetTextColor( hDC, DebugGetColor( iForeground ));

	if (bHighBit)
	{
		SetBkColor(   hDC, DebugGetColor( iColorHiBG ));
		SetTextColor( hDC, DebugGetColor( iColorHiFG )); // was iForeground
	}

	if (bCtrlBit)
	{
		SetBkColor(   hDC, DebugGetColor( iColorLoBG ));
		SetTextColor( hDC, DebugGetColor( iColorLoFG ));
	}
}


// To flush out color bugs... swap: iAsciBackground & iHighBackground
//===========================================================================
char ColorizeSpecialChar( HDC hDC, TCHAR * sText, BYTE nData, const MemoryView_e iView,
		const int iAsciBackground /*= 0           */, const int iTextForeground /*= FG_DISASM_CHAR */,
		const int iHighBackground /*= BG_INFO_CHAR*/, const int iHighForeground /*= FG_INFO_CHAR_HI*/,
		const int iCtrlBackground /*= BG_INFO_CHAR*/, const int iCtrlForeground /*= FG_INFO_CHAR_LO*/ )
{
	bool bHighBit = false;
	bool bAsciBit = false;
	bool bCtrlBit = false;

	int iTextBG = iAsciBackground;
	int iHighBG = iHighBackground;
	int iCtrlBG = iCtrlBackground;
	int iTextFG = iTextForeground;
	int iHighFG = iHighForeground;
	int iCtrlFG = iCtrlForeground;

	BYTE nByte = FormatCharTxtHigh( nData, & bHighBit );
	char nChar = FormatCharTxtCtrl( nByte, & bCtrlBit );

	switch (iView)
	{
		case MEM_VIEW_ASCII:
			iHighBG = iTextBG;
			iCtrlBG = iTextBG;
			break;
		case MEM_VIEW_APPLE:
			iHighBG = iTextBG;
			if (!bHighBit)
			{
				iTextBG = iCtrlBG;
			}
						
			if (bCtrlBit)
			{
				iTextFG = iCtrlFG;
				if (bHighBit)
				{
					iHighFG = iTextFG;
				}
			}
			bCtrlBit = false;
			break;
		default: break;
	}

	if (sText)
		wsprintf( sText, TEXT("%c"), nChar );

	if (hDC)
	{
		SetupColorsHiLoBits( hDC, bHighBit, bCtrlBit
			, iTextBG, iTextFG // FG_DISASM_CHAR   
			, iHighBG, iHighFG // BG_INFO_CHAR     
			, iCtrlBG, iCtrlFG // FG_DISASM_OPCODE 
		);
	}
	return nChar;
}


//===========================================================================
LPCTSTR FormatAddress( WORD nAddress, int nBytes )
{
	// No symbol for this addres -- string with nAddress
	static TCHAR sSymbol[8] = TEXT("");
	switch (nBytes)
	{
		case  2:	wsprintf(sSymbol,TEXT("$%02X"),(unsigned)nAddress);  break;
		case  3:	wsprintf(sSymbol,TEXT("$%04X"),(unsigned)nAddress);  break;
		default:	sSymbol[0] = 0; break; // clear since is static
	}
	return sSymbol;
}


//===========================================================================
void TextConvertTabsToSpaces( TCHAR *pDeTabified_, LPCTSTR pText, const int nDstSize, int nTabStop )
{
	int nLen = _tcslen( pText );

	int TAB_SPACING = 8;
	int TAB_SPACING_1 = 16;
	int TAB_SPACING_2 = 21;

	if (nTabStop)
		TAB_SPACING = nTabStop;

	LPCTSTR pSrc = pText;
	LPTSTR  pDst = pDeTabified_;

	int iTab = 0; // number of tabs seen
	int nTab = 0; // gap left to next tab
	int nGap = 0; // actual gap
	int nCur = 0; // current cursor position
	while (pSrc && *pSrc && (nCur < nDstSize))
	{
		if (*pSrc == CHAR_TAB)
		{
			if (nTabStop)
			{
				nTab = nCur % TAB_SPACING;
				nGap = (TAB_SPACING - nTab);
			}
			else
			{
				if (nCur <= TAB_SPACING_1)
				{
					nGap = (TAB_SPACING_1 - nCur);
				}
				else
				if (nCur <= TAB_SPACING_2)
				{
					nGap = (TAB_SPACING_2 - nCur);
				}
				else
				{
					nTab = nCur % TAB_SPACING;
					nGap = (TAB_SPACING - nTab);
				}
			}
			

			if ((nCur + nGap) >= nDstSize)
				break;

			for( int iSpc = 0; iSpc < nGap; iSpc++ )
			{
				*pDst++ = CHAR_SPACE;
			}
			nCur += nGap;
		}
		else
		if ((*pSrc == CHAR_LF) || (*pSrc == CHAR_CR))
		{
			*pDst++ = 0; // *pSrc;
			nCur++;
		}
		else
		{
			*pDst++ = *pSrc;
			nCur++;
		}
		pSrc++;
	}	
	*pDst = 0;
}


/*
	String types:

	http://www.codeproject.com/cpp/unicode.asp

				TEXT()       _tcsrev
	_UNICODE    Unicode      _wcsrev
	_MBCS       Multi-byte   _mbsrev
	n/a         ASCIi        strrev

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

//===========================================================================
inline
BYTE Chars2ToByte ( char *pText )
{
	BYTE n = ((pText[0] <= '@') ? (pText[0] - '0') : (pText[0] - 'A' + 10)) << 4;
		n += ((pText[1] <= '@') ? (pText[1] - '0') : (pText[1] - 'A' + 10)) << 0;
	return n;
}

//===========================================================================
inline
bool IsHexString ( LPCSTR pText )
{
//	static const TCHAR sHex[] = "0123456789ABCDEF";

	while (*pText)
	{
		if (*pText < TEXT('0'))
			return false;
		if (*pText > TEXT('f'))
			return false;
		if ((*pText > TEXT('9')) && (*pText < TEXT('A')) ||
			((*pText > TEXT('F') && (*pText < TEXT('a')))))
			return false;

		pText++;
	}
	return true;
}


// @return Length of new string
//===========================================================================
int RemoveWhiteSpaceReverse ( TCHAR *pSrc )
{
	int   nLen = _tcslen( pSrc );
	char *pDst = pSrc + nLen;
	while (nLen--)
	{
		pDst--;
		if (*pDst == CHAR_SPACE)
		{
			*pDst = 0;
		}
		else
		{
			break;
		}
	}
	return nLen;
}


//===========================================================================
inline
const TCHAR* SkipEOL ( const TCHAR *pSrc )
{
	while (pSrc && ((*pSrc == CHAR_LF) || (*pSrc == CHAR_CR)))
	{
		pSrc++;
	}
	return pSrc;
}

//===========================================================================
//const char* SkipUntilChar ( const char *pSrc, const char nDelim )
const TCHAR* SkipUntilChar ( const TCHAR *pSrc, const TCHAR nDelim )
{
	while (pSrc && (*pSrc))
	{
		if (*pSrc == nDelim)
			break;
		pSrc++;
	}
	return pSrc;
}

//===========================================================================
const TCHAR * FindTokenOrAlphaNumeric ( const TCHAR *pSrc, const TokenTable_t *aTokens, const int nTokens, ArgToken_e * pToken_ )
{
	if (pToken_)
		*pToken_ = NO_TOKEN;

	const TCHAR *pEnd = pSrc;

	if (pSrc && (*pSrc))
	{
		if (isalnum( *pSrc ))
		{
			if (pToken_)
				*pToken_ = TOKEN_ALPHANUMERIC;
		}			
		else
		{
			const TokenTable_t *pEntry = aTokens;
			const TCHAR        *pToken = NULL;
			for (int iToken = 0; iToken < nTokens; iToken++ )
			{
				pToken = & (pEntry->sToken);
				if (*pSrc == *pToken)
				{
					if (pToken_)
						*pToken_ = (ArgToken_e) iToken;
					pEnd = pSrc + 1; // _tcslen( pToken );
					break;
				}
				pEntry++;
			}
		}
	}
	return pEnd;
}

//===========================================================================
const TCHAR* SkipUntilToken ( const TCHAR *pSrc, const TokenTable_t *aTokens, const int nTokens, ArgToken_e *pToken_ )
{
	if ( pToken_)
		*pToken_ = NO_TOKEN;


	while (pSrc && (*pSrc))
	{
		// Common case is TOKEN_ALPHANUMERIC, so continue until we don't have one
//		if (isalnum( *pSrc ))
//		{
//			if ( pToken_)
//			{
//				*pToken_ = TOKEN_ALPHANUMERIC;
//			}
//			return pSrc;
//		}			

		const TokenTable_t *pEntry = aTokens;
		const TCHAR        *pToken = NULL;
		for (int iToken = 0; iToken < nTokens; iToken++ )
		{
			pToken = & (pEntry->sToken);
			if (*pSrc == *pToken)
			{
				if ( pToken_)
				{
					*pToken_ = (ArgToken_e) iToken;
				}
				return pSrc;
			}
			pEntry++;
		}
		pSrc++;
	}
	return pSrc;
}

//===========================================================================
inline
// const char* SkipUntilEOL ( const char *pSrc )
const TCHAR* SkipUntilEOL ( const TCHAR *pSrc )
{

	while (pSrc && (*pSrc))
	{
		if ((*pSrc == CHAR_LF) || (*pSrc == CHAR_CR))
		{
			break;
		}
		pSrc++;
	}
	return pSrc;
}

//===========================================================================
inline
//const char* SkipWhiteSpace ( const char *pSrc )
const TCHAR* SkipWhiteSpace ( const TCHAR *pSrc )
{
	while (pSrc && ((*pSrc == CHAR_SPACE) || (*pSrc == CHAR_TAB)))
	{
		pSrc++;
	}
	return pSrc;
}

//===========================================================================
inline
//const char* SkipUntilWhiteSpace ( const char *pSrc )
const TCHAR* SkipUntilWhiteSpace ( const TCHAR *pSrc )
{
	while (pSrc && (*pSrc))
	{
		if ((*pSrc == CHAR_SPACE) || (*pSrc == CHAR_TAB))
		{
			break;
		}
		pSrc++;
	}
	return pSrc;
}

//===========================================================================
inline
//const char* SkipUntilTab ( const char *pSrc )
const TCHAR* SkipUntilTab ( const TCHAR *pSrc )
{
	while (pSrc && (*pSrc))
	{
		if (*pSrc == CHAR_TAB)
		{
			break;
		}
		pSrc++;
	}
	return pSrc;
}

// @param pStart Start of line.
//===========================================================================
inline
//const char *SkipWhiteSpaceReverse ( const char *pSrc, const char *pStart )
const TCHAR *SkipWhiteSpaceReverse ( const TCHAR *pSrc, const TCHAR *pStart )
{
	while (pSrc && ((*pSrc == CHAR_SPACE) || (*pSrc == CHAR_TAB)) && (pSrc > pStart))
	{
		pSrc--;
	}
	return pSrc;
}

// @param pStart Start of line.
//===========================================================================
inline
//const char *SkipUntilWhiteSpaceReverse ( const char *pSrc, const char *pStart )
const TCHAR *SkipUntilWhiteSpaceReverse ( const TCHAR *pSrc, const TCHAR *pStart )
{
	while (pSrc && (pSrc > pStart))
	{
		if ((*pSrc == CHAR_SPACE) || (*pSrc == CHAR_TAB))
		{
			break;		
		}
		pSrc--;
	}
	return pSrc;
}


// Breakpoints ____________________________________________________________________________________


// bool bBP = g_nBreakpoints && CheckBreakpoint(nOffset,nOffset == regs.pc);
//===========================================================================
bool GetBreakpointInfo ( WORD nOffset, bool & bBreakpointActive_, bool & bBreakpointEnable_ )
{
	for (int iBreakpoint = 0; iBreakpoint < MAX_BREAKPOINTS; iBreakpoint++)
	{
		Breakpoint_t *pBP = &g_aBreakpoints[ iBreakpoint ];
		
		if ((pBP->nLength)
//			 && (pBP->bEnabled) // not bSet
			 && (nOffset >= pBP->nAddress) && (nOffset < (pBP->nAddress + pBP->nLength))) // [nAddress,nAddress+nLength]
		{
			bBreakpointActive_ = pBP->bSet;
			bBreakpointEnable_ = pBP->bEnabled;
			return true;
		}

//		if (g_aBreakpoints[iBreakpoint].nLength && g_aBreakpoints[iBreakpoint].bEnabled &&
//			(g_aBreakpoints[iBreakpoint].nAddress <= targetaddr) &&
//			(g_aBreakpoints[iBreakpoint].nAddress + g_aBreakpoints[iBreakpoint].nLength > targetaddr))
	}

	bBreakpointActive_ = false;
	bBreakpointEnable_ = false;

	return false;
}


// Returns true if we should continue checking breakpoint details, else false
//===========================================================================
bool _BreakpointValid( Breakpoint_t *pBP ) //, BreakpointSource_t iSrc )
{
	bool bStatus = false;

	if (! pBP->bEnabled)
		return bStatus;

//	if (pBP->eSource != iSrc)
//		return bStatus;

	if (! pBP->nLength)
		return bStatus;

	return true;
}


//===========================================================================
bool _CheckBreakpointValue( Breakpoint_t *pBP, int nVal )
{
	bool bStatus = false;

	int iCmp = pBP->eOperator;
	switch (iCmp)
	{
		case BP_OP_LESS_EQUAL   :
			if (nVal <= pBP->nAddress)
				bStatus = true;
			break;
		case BP_OP_LESS_THAN    :
			if (nVal < pBP->nAddress)
				bStatus = true;
			break;
		case BP_OP_EQUAL        : // Range is like C++ STL: [,)  (inclusive,not-inclusive)
			 if ((nVal >= pBP->nAddress) && (nVal < (pBP->nAddress + pBP->nLength)))
			 	bStatus = true;
			break;
		case BP_OP_NOT_EQUAL    : // Rnage is: (,] (not-inclusive, inclusive)
			 if ((nVal < pBP->nAddress) || (nVal >= (pBP->nAddress + pBP->nLength)))
			 	bStatus = true;
			break;
		case BP_OP_GREATER_THAN :
			if (nVal > pBP->nAddress)
				bStatus = true;
			break;
		case BP_OP_GREATER_EQUAL:
			if (nVal >= pBP->nAddress)
				bStatus = true;
			break;
		default:
			break;
	}

	return bStatus;
}


bool CheckBreakpointsIO ()
{
	const int NUM_TARGETS = 2;

	int aTarget[ NUM_TARGETS ] =
	{
		NO_6502_TARGET,
		NO_6502_TARGET
	};
	int  nBytes;
	bool bStatus = false;

	int  iTarget;
	int  nAddress;

	Get6502Targets( &aTarget[0], &aTarget[1], &nBytes );
	
	if (nBytes)
	{
		for (iTarget = 0; iTarget < NUM_TARGETS; iTarget++ )
		{
			nAddress = aTarget[ iTarget ];
			if (nAddress != NO_6502_TARGET)
			{
				for (int iBreakpoint = 0; iBreakpoint < MAX_BREAKPOINTS; iBreakpoint++)
				{
					Breakpoint_t *pBP = &g_aBreakpoints[iBreakpoint];
					if (_BreakpointValid( pBP ))
					{
						if (pBP->eSource == BP_SRC_MEM_1)
						{
							if (_CheckBreakpointValue( pBP, nAddress ))
							{
								return true;
							}
						}
					}
				}
			}
		}
	}
	return bStatus;
}

// Returns true if a register breakpoint is triggered
//===========================================================================
bool CheckBreakpointsReg ()
{
	bool bStatus = false;

	for (int iBreakpoint = 0; iBreakpoint < MAX_BREAKPOINTS; iBreakpoint++)
	{
		Breakpoint_t *pBP = &g_aBreakpoints[iBreakpoint];

		if (! _BreakpointValid( pBP ))
			continue;

		switch (pBP->eSource)
		{
			case BP_SRC_REG_PC:
				if (_CheckBreakpointValue( pBP, regs.pc ))
					return true;
				break;
			case BP_SRC_REG_A:
				if (_CheckBreakpointValue( pBP, regs.a ))
					return true;
				break;
			case BP_SRC_REG_X:
				if (_CheckBreakpointValue( pBP, regs.x ))
					return true;
				break;
			case BP_SRC_REG_Y:
				if (_CheckBreakpointValue( pBP, regs.y ))
					return true;
				break;
			case BP_SRC_REG_P:
				if (_CheckBreakpointValue( pBP, regs.ps ))
					return true;
				break;
			case BP_SRC_REG_S:
				if (_CheckBreakpointValue( pBP, regs.sp ))
					return true;
				break;
			default:
				break;
		}
	}

	return bStatus;
}

//===========================================================================
BOOL CheckJump (WORD targetaddress)
{
	WORD savedpc = regs.pc;
	InternalSingleStep();
	BOOL result = (regs.pc == targetaddress);
	regs.pc = savedpc;
	return result;
}


// Console ________________________________________________________________________________________

//===========================================================================
LPCSTR ConsoleBufferPeek()
{
	return g_aConsoleBuffer[ 0 ];
}


// Add string to buffered output
// Shifts the buffered console output lines "Up"
//===========================================================================
bool ConsoleBufferPush( const TCHAR * pString ) // LPCSTR
{
	if (g_nConsoleBuffer < CONSOLE_HEIGHT)
	{
		int nLen = _tcslen( pString );
		if (nLen < g_nConsoleDisplayWidth)
		{
			_tcscpy( g_aConsoleBuffer[ g_nConsoleBuffer ], pString );
			g_nConsoleBuffer++;
			return true;
		}
		else
		{
#if _DEBUG
//			TCHAR sText[ CONSOLE_WIDTH * 2 ];
//			sprintf( sText, "ConsoleBufferPush(pString) > g_nConsoleDisplayWidth: %d", g_nConsoleDisplayWidth );
//			MessageBox( framewindow, sText, "Warning", MB_OK );
#endif
			// push multiple lines
			while ((nLen >= g_nConsoleDisplayWidth) && (g_nConsoleBuffer < CONSOLE_HEIGHT))
			{
//				_tcsncpy( g_aConsoleBuffer[ g_nConsoleBuffer ], pString, (g_nConsoleDisplayWidth-1) );
//				pString += g_nConsoleDisplayWidth;
				_tcsncpy( g_aConsoleBuffer[ g_nConsoleBuffer ], pString, (CONSOLE_WIDTH-1) );
				pString += (CONSOLE_WIDTH-1);
				g_nConsoleBuffer++;
				nLen = _tcslen( pString );
			}
			return true;
		}
	}

	// TODO: Warning: Too much output.
	return false;
}

// Shifts the buffered console output "down"
//===========================================================================
void ConsoleBufferPop()
{
	int y = 0;
	while (y < g_nConsoleBuffer)
	{
		_tcscpy( g_aConsoleBuffer[ y ], g_aConsoleBuffer[ y+1 ] );
		y++;
	}

	g_nConsoleBuffer--;
	if (g_nConsoleBuffer < 0)
		g_nConsoleBuffer = 0;
}

// Remove string from buffered output
//===========================================================================
void ConsoleBufferToDisplay()
{
	ConsoleDisplayPush( ConsoleBufferPeek() );
	ConsoleBufferPop();
}

//===========================================================================
Update_t ConsoleDisplayError (LPCTSTR pText)
{
	ConsoleBufferPush( pText );
	return ConsoleUpdate();
}

// ConsoleDisplayPush()
// Shifts the console display lines "up"
//===========================================================================
void ConsoleDisplayPush( LPCSTR pText )
{
	int nLen = MIN( g_nConsoleDisplayTotal, CONSOLE_HEIGHT - 1 - CONSOLE_FIRST_LINE);
	while (nLen--)
	{
		_tcsncpy(
			g_aConsoleDisplay[(nLen + 1 + CONSOLE_FIRST_LINE )],
			g_aConsoleDisplay[nLen + CONSOLE_FIRST_LINE],
			CONSOLE_WIDTH );
	}

	if (pText)
		_tcsncpy( g_aConsoleDisplay[ CONSOLE_FIRST_LINE ], pText, CONSOLE_WIDTH );

	g_nConsoleDisplayTotal++;
	if (g_nConsoleDisplayTotal > (CONSOLE_HEIGHT - CONSOLE_FIRST_LINE))
		g_nConsoleDisplayTotal = (CONSOLE_HEIGHT - CONSOLE_FIRST_LINE);

}


//===========================================================================
void ConsoleDisplayPause()
{
	if (g_nConsoleBuffer)
	{
		_tcscpy( g_pConsoleInput, TEXT("...press SPACE continue, ESC skip..." ) );
		g_bConsoleBufferPaused = true;
	}
	else
	{
		ConsoleInputReset();
	}
}

//===========================================================================
bool ConsoleInputBackSpace()
{
	if (g_nConsoleInputChars)
	{
		g_nConsoleInputChars--;

		if (g_pConsoleInput[ g_nConsoleInputChars ] == TEXT('"'))
			g_bConsoleInputQuoted = ! g_bConsoleInputQuoted;

		g_pConsoleInput[ g_nConsoleInputChars ] = 0;
		return true;
	}
	return false;
}

//===========================================================================
bool ConsoleInputClear()
{
	if (g_nConsoleInputChars)
	{
		ZeroMemory( g_pConsoleInput, g_nConsoleDisplayWidth );
		g_nConsoleInputChars = 0;
		return true;
	}
	return false;
}

//===========================================================================
bool ConsoleInputChar( TCHAR ch )
{
	if (g_nConsoleInputChars < g_nConsoleDisplayWidth) // bug? include prompt?
	{
		g_pConsoleInput[ g_nConsoleInputChars ] = ch;
		g_nConsoleInputChars++;
		return true;
	}
	return false;
}

//===========================================================================
LPCSTR ConsoleInputPeek()
{
	return g_aConsoleDisplay[0];
}

//===========================================================================
void ConsoleInputReset ()
{
	// Not using g_aConsoleInput since we get drawing of the input Line for "Free"
	// Even if we add console scrolling, we don't need any special logic to draw the input line.
	g_bConsoleInputQuoted = false;

	ZeroMemory( g_aConsoleInput, CONSOLE_WIDTH );
	_tcscpy( g_aConsoleInput, g_sConsolePrompt );
	_tcscat( g_aConsoleInput, TEXT(" " ) );

	int nLen = _tcslen( g_aConsoleInput );
	g_pConsoleInput = &g_aConsoleInput[nLen];
	g_nConsoleInputChars = 0;
}

//===========================================================================
int ConsoleInputTabCompletion ()
{
	return UPDATE_CONSOLE_INPUT;
}

//===========================================================================
Update_t ConsoleScrollHome ()
{
	g_iConsoleDisplayStart = g_nConsoleDisplayTotal - CONSOLE_FIRST_LINE;
	if (g_iConsoleDisplayStart < 0)
		g_iConsoleDisplayStart = 0;

	return UPDATE_CONSOLE_DISPLAY;
}

//===========================================================================
Update_t ConsoleScrollEnd ()
{
	g_iConsoleDisplayStart = 0;

	return UPDATE_CONSOLE_DISPLAY;
}

//===========================================================================
Update_t ConsoleScrollUp ( int nLines )
{
	g_iConsoleDisplayStart += nLines;

	if (g_iConsoleDisplayStart > (g_nConsoleDisplayTotal - CONSOLE_FIRST_LINE))
		g_iConsoleDisplayStart = (g_nConsoleDisplayTotal - CONSOLE_FIRST_LINE);

	if (g_iConsoleDisplayStart < 0)
		g_iConsoleDisplayStart = 0;

	return UPDATE_CONSOLE_DISPLAY;
}

//===========================================================================
Update_t ConsoleScrollDn ( int nLines )
{
	g_iConsoleDisplayStart -= nLines;
	if (g_iConsoleDisplayStart < 0)
		g_iConsoleDisplayStart = 0;

	return UPDATE_CONSOLE_DISPLAY;
}

//===========================================================================
Update_t ConsoleScrollPageUp ()
{
	ConsoleScrollUp( g_nConsoleDisplayHeight - CONSOLE_FIRST_LINE );

	return UPDATE_CONSOLE_DISPLAY;
}

//===========================================================================
Update_t ConsoleScrollPageDn()
{
	ConsoleScrollDn( g_nConsoleDisplayHeight - CONSOLE_FIRST_LINE );

	return UPDATE_CONSOLE_DISPLAY;
}

//===========================================================================
void ConsoleBufferTryUnpause (int nLines)
{
	for( int y = 0; y < nLines; y++ )
	{
		ConsoleBufferToDisplay();
	}

	g_bConsoleBufferPaused = false;
	if (g_nConsoleBuffer)
	{
		g_bConsoleBufferPaused = true;
		ConsoleDisplayPause();
	}
}

//===========================================================================
Update_t ConsoleUpdate()
{
	if (! g_bConsoleBufferPaused)
	{
		int nLines = MIN( g_nConsoleBuffer, g_nConsoleDisplayHeight - 1);
		ConsoleBufferTryUnpause( nLines );
	}

	return UPDATE_CONSOLE_DISPLAY;
}



// Commands _______________________________________________________________________________________


//===========================================================================
int HashMnemonic( const TCHAR * pMnemonic )
{
	const TCHAR *pText = pMnemonic;
	int nMnemonicHash = 0;
	int iHighBits;

	const int    NUM_LOW_BITS = 19; // 24 -> 19 prime
	const int    NUM_MSK_BITS =  5; //  4 ->  5 prime
	const Hash_t BIT_MSK_HIGH = ((1 << NUM_MSK_BITS) - 1) << NUM_LOW_BITS;

	for( int iChar = 0; iChar < 4; iChar++ )
	{	
		nMnemonicHash = (nMnemonicHash << NUM_MSK_BITS) + *pText;
		iHighBits = (nMnemonicHash & BIT_MSK_HIGH);
		if (iHighBits)
		{
			nMnemonicHash = (nMnemonicHash ^ (iHighBits >> NUM_LOW_BITS)) & ~ BIT_MSK_HIGH;
		}
		pText++;
	}

	return nMnemonicHash;
}


//===========================================================================
void _CmdAssembleHashOpcodes()
{
	int nMnemonicHash;
	int iOpcode;

	for( iOpcode = 0; iOpcode < NUM_OPCODES; iOpcode++ )
	{
		const TCHAR *pMnemonic = g_aOpcodes65C02[ iOpcode ].sMnemonic;
		nMnemonicHash = HashMnemonic( pMnemonic );
		g_aOpcodesHash[ iOpcode ] = nMnemonicHash;
	}
}


//===========================================================================
void _CmdAssembleHashDump()
{
// #if DEBUG_ASM_HASH
	vector<HashOpcode_t> vHashes;
	HashOpcode_t         tHash;
	TCHAR                sText[ CONSOLE_WIDTH ];

	int iOpcode;
	for( iOpcode = 0; iOpcode < NUM_OPCODES; iOpcode++ )
	{
		tHash.m_iOpcode = iOpcode;
		tHash.m_nValue  = g_aOpcodesHash[ iOpcode ]; 
		vHashes.push_back( tHash );
	}	
	
	sort( vHashes.begin(), vHashes.end(), HashOpcode_t() );

	Hash_t nPrevHash = vHashes.at( 0 ).m_nValue;
	Hash_t nThisHash = 0;

	for( iOpcode = 0; iOpcode < NUM_OPCODES; iOpcode++ )
	{
		tHash = vHashes.at( iOpcode );

		Hash_t iThisHash = tHash.m_nValue;
		int    nOpcode   = tHash.m_iOpcode;
		int    nOpmode   = g_aOpcodes[ nOpcode ].nAddressMode;

		wsprintf( sText, "%08X %02X %s %s"
			, iThisHash
			, nOpcode
			, g_aOpcodes65C02[ nOpcode ].sMnemonic
			, g_aOpmodes[ nOpmode  ].m_sName
		);
		ConsoleBufferPush( sText );
		nThisHash++;
		
//		if (nPrevHash != iThisHash)
//		{
//			wsprintf( sText, "Total: %d", nThisHash );
//			ConsoleBufferPush( sText );
//			nThisHash = 0;
//		}
	}

	ConsoleUpdate();
//#endif
}


//===========================================================================
bool AssemblerOpcodeIsBranch( int nOpcode )
{
	// 76543210 Bit
	// xxx10000 Branch

	if (nOpcode == OPCODE_BRA)
		return true;

	if ((nOpcode & 0x1F) != 0x10) // low nibble not zero?
		return false;

	if ((nOpcode >> 4) & 1)
		return true;
	
//		(nOpcode == 0x10) || // BPL
//		(nOpcode == 0x30) || // BMI
//		(nOpcode == 0x50) || // BVC
//		(nOpcode == 0x70) || // BVS
//		(nOpcode == 0x90) || // BCC
//		(nOpcode == 0xB0) || // BCS
//		(nOpcode == 0xD0) || // BNE
//		(nOpcode == 0xF0) || // BEQ
	return false;
}


//===========================================================================
bool AssemblerGetAddressingMode( int nArgs, WORD nAddress, vector<int> vOpcodes )
{
	bool bHaveComma      = false;
	bool bHaveHash       = false;
	bool bHaveDollar     = false;
	bool bHaveLeftParen  = false;
	bool bHaveRightParen = false;
	bool bHaveParen      = false;
	bool bHaveRegisterX  = false;
	bool bHaveRegisterY  = false;
	bool bHaveZeroPage   = false;

//	int iArgRawMnemonic = 0;
	int iArg;

	// Sync up to Raw Args for matching mnemonic
	iArg = 3;
	Arg_t *pArg = &g_aArgRaw[ iArg ];

	// Process them instead of the cooked args, since we need the orginal tokens
	int  iAddressMode = AM_IMPLIED;
	int  nBytes  = 0;
	WORD nTarget = 0;

		while (iArg <= g_nArgRaw)
		{
			int iToken = pArg->eToken;
			int iType  = pArg->bType;

			if (iToken == TOKEN_HASH)
			{
				if (bHaveHash)
				{
					ConsoleBufferPush( TEXT( " Syntax Error: Extra '#'" ) ); // No thanks, we already have one
					return false;
				}
				bHaveHash = true;

				iAddressMode = AM_M; // Immediate
				nTarget = pArg[1].nVal1;
				nBytes = 1;
			}
			else
			if (iToken == TOKEN_DOLLAR)
			{
				if (bHaveDollar)
				{
					ConsoleBufferPush( TEXT( " Syntax Error: Extra '$'" ) ); // No thanks, we already have one
					return false;
				}
				bHaveDollar = true;
				
				iAddressMode = AM_A; // Absolute
				nTarget = pArg[1].nVal1;
				nBytes = 2;

				if (nTarget <= _6502_ZEROPAGE_END)
				{
					iAddressMode = AM_Z;
					nBytes = 1;
				}
			}
			else
			if (iToken == TOKEN_LEFT_PAREN)
			{
				if (bHaveLeftParen)
				{
					ConsoleBufferPush( TEXT( " Syntax Error: Extra '('" ) ); // No thanks, we already have one
					return false;
				}
				bHaveLeftParen = true;

				// Indexed or Indirect
				iAddressMode = AM_IZX;
			}
			else
			if (iToken == TOKEN_RIGHT_PAREN)
			{
				if (bHaveRightParen)
				{
					ConsoleBufferPush( TEXT( " Syntax Error: Extra ')'" ) ); // No thanks, we already have one
					return false;
				}
				bHaveRightParen = true;

				// Indexed or Indirect
				iAddressMode = AM_IZX;
			}
			else
			if (iToken == TOKEN_COMMA)
			{
				if (bHaveComma)
				{
					ConsoleBufferPush( TEXT( " Syntax Error: Extra ','" ) ); // No thanks, we already have one
					return false;
				}
				bHaveComma = true;
				// We should have address by now
			}
			else
			if (iToken = TOKEN_ALPHANUMERIC)
			{
				if (pArg->nArgLen == 1)
				{
					if (pArg->sArg[0] == 'X')
					{
						if (! bHaveComma)
						{
							ConsoleBufferPush( TEXT( " Syntax Error: Missing ','" ) );
							return false;
						}
						bHaveRegisterX = true;
					}
					if (pArg->sArg[0] == 'Y')
					{
						if (! bHaveComma)
						{
							ConsoleBufferPush( TEXT( " Syntax Error: Missing ','" ) );
							return false;
						}
						bHaveRegisterY = true;
					}
				}
			}
/*
			if (iType & TYPE_VALUE)
			{
				iAddressMode = AM_M; // Immediate
				nTarget = g_aArgs[ iArg ].nVal1;
				nBytes = 1;
			}
			if (iType & TYPE_ADDRESS)
			{
				iAddressMode = AM_A; // Absolute
				nTarget = g_aArgs[ iArg ].nVal1;
				nBytes = 2;

				if (nTarget <= _6502_ZEROPAGE_END)
				{
					iAddressMode = AM_Z;
					nBytes = 1;
				}
			}
*/

			iArg++;
		}

		if ((  bHaveLeftParen) && (! bHaveRightParen))
		{
			ConsoleBufferPush( TEXT( " Syntax Error: Missing ')'" ) );
			return false;
		}

		if ((! bHaveLeftParen) && (  bHaveRightParen))
		{
			ConsoleBufferPush( TEXT( " Syntax Error: Missing '('" ) );
			return false;
		}

		if (bHaveComma)
		{
			if ((! bHaveRegisterX) && (! bHaveRegisterY))
			{
				ConsoleBufferPush( TEXT( " Syntax Error: Index 'X' or 'Y'" ) );
				return false;
			}
		}

		bHaveParen = (bHaveLeftParen || bHaveRightParen);
		if (! bHaveParen)
		{
			if (bHaveComma)
			{
				if (bHaveRegisterX)
				{
					iAddressMode = AM_AX;
					nBytes = 2;
					if (nTarget <= _6502_ZEROPAGE_END)
					{
						iAddressMode = AM_ZX;
						nBytes = 1;
					}
				}
				if (bHaveRegisterY)
				{
					iAddressMode = AM_AY;
					nBytes = 2;
					if (nTarget <= _6502_ZEROPAGE_END)
					{
						iAddressMode = AM_ZY;
						nBytes = 1;
					}
				}
			}
		}		

		int iOpcode;
		int nOpcodes = vOpcodes.size();

		for( iOpcode = 0; iOpcode < nOpcodes; iOpcode++ )
		{
			int nOpcode = vOpcodes.at( iOpcode ); // m_iOpcode;
			int nOpmode = g_aOpcodes[ nOpcode ].nAddressMode;

			if (AssemblerOpcodeIsBranch( nOpcode))
				nOpmode = AM_R;

			if (nOpmode == iAddressMode)
			{
				*(memdirty + (nAddress >> 8)) |= 1;
				*(mem + nAddress) = (BYTE) nOpcode;

				if (nBytes > 0)
					*(mem + nAddress + 1) = (BYTE)(nTarget >> 0);

				if (nBytes > 1)
					*(mem + nAddress + 2) = (BYTE)(nTarget >> 8);

				break;
			}

			return true;
		}

	return false;
}


//===========================================================================
Update_t CmdAssemble (int nArgs)
{
	if (! g_bOpcodesHashed)
	{
		_CmdAssembleHashOpcodes();
		g_bOpcodesHashed = true;
	}

	if (! nArgs)
		return Help_Arg_1( CMD_ASSEMBLE );
	
	if (nArgs == 1) // undocumented ASM *
	{
		if (_tcscmp( g_aArgs[ 1 ].sArg, g_aParameters[ PARAM_WILDSTAR ].m_sName ) == 0)
		{
			_CmdAssembleHashDump();

			return UPDATE_CONSOLE_DISPLAY;
		}

//		g_nAssemblerAddress = g_aArgs[1].nVal1;
		return Help_Arg_1( CMD_ASSEMBLE );
	}

	if (nArgs < 2)
		return Help_Arg_1( CMD_ASSEMBLE );

	WORD nAddress = g_aArgs[1].nVal1;

	TCHAR *pMnemonic = g_aArgs[2].sArg;
	int nMnemonicHash = HashMnemonic( pMnemonic );

	vector<int> vOpcodes; // Candiate opcodes
	int iOpcode;
	
	// Ugh! Linear search.
	for( iOpcode = 0; iOpcode < NUM_OPCODES; iOpcode++ )
	{
		if (nMnemonicHash == g_aOpcodesHash[ iOpcode ])
		{
			vOpcodes.push_back( iOpcode );
		}
	}

	int nOpcodes = vOpcodes.size();
	if (! nOpcodes)
	{
		ConsoleBufferPush( TEXT(" Syntax Error: Invalid mnemonic") );
	}
	else
	if (nOpcodes == 1)
	{
		*(memdirty + (nAddress >> 8)) |= 1;

		int nOpcode = vOpcodes.at( 0 );
		*(mem + nAddress) = (BYTE) nOpcode;
	}
	else // ambigious -- need to parse Addressing Mode
	{
		bool bStatus = AssemblerGetAddressingMode( nArgs, nAddress, vOpcodes );
	}

	return UPDATE_CONSOLE_DISPLAY;
}




// Unassemble
//===========================================================================
Update_t CmdUnassemble (int nArgs)
{
	if (! nArgs)
		return Help_Arg_1( CMD_UNASSEMBLE );
  
	WORD nAddress = g_aArgs[1].nVal1;
	g_nDisasmTopAddress = nAddress;

	DisasmCalcCurFromTopAddress();
	DisasmCalcBotFromTopAddress();

	return UPDATE_DISASM;
}


//===========================================================================
Update_t CmdCalculator (int nArgs)
{
	const int nBits = 8;

	if (! nArgs)
		return Help_Arg_1( CMD_CALC );

	WORD nAddress = g_aArgs[1].nVal1;
	TCHAR sText [ CONSOLE_WIDTH ];

	bool bHi = false;
	bool bLo = false;
	char c = FormatChar4Font( (BYTE) nAddress, &bHi, &bLo );
	bool bParen = bHi || bLo;

	int nBit = 0;
	int iBit = 0;
	for( iBit = 0; iBit < nBits; iBit++ )
	{
		bool bSet = (nAddress >> iBit) & 1;
		if (bSet)
			nBit |= (1 << (iBit * 4)); // 4 bits per hex digit
	}

	wsprintf( sText, TEXT("$%04X  0z%08X  %5d  '%c' "),
		nAddress, nBit, nAddress, c );

	if (bParen)
		_tcscat( sText, TEXT("(") );

	if (bHi & bLo)
		_tcscat( sText, TEXT("High Ctrl") );
	else
	if (bHi)
		_tcscat( sText, TEXT("High") );
	else
	if (bLo)
		_tcscat( sText, TEXT("Ctrl") );

	if (bParen)
		_tcscat( sText, TEXT(")") );

	ConsoleBufferPush( sText );
	return ConsoleUpdate();
}

//===========================================================================
Update_t CmdFeedKey (int nArgs)
{
	KeybQueueKeypress(
		nArgs ? g_aArgs[1].nVal1 ? g_aArgs[1].nVal1 : g_aArgs[1].sArg[0] : TEXT(' '), 1); // FIXME!!!
	return UPDATE_CONSOLE_DISPLAY;
}

//===========================================================================
Update_t CmdInput (int nArgs)
{
	if (!nArgs)
		return Help_Arg_1( CMD_INPUT );
  
	WORD nAddress = g_aArgs[1].nVal1;
	
//	ioread[ g_aArgs[1].nVal1 & 0xFF ](regs.pc,g_aArgs[1].nVal1 & 0xFF,0,0,0);
	ioread[ nAddress & 0xFF ](regs.pc, nAddress  & 0xFF,0,0,0); // g_aArgs[1].nVal1 

	return UPDATE_CONSOLE_DISPLAY; // TODO: Verify // 1
}


//===========================================================================
Update_t CmdJSR (int nArgs)
{
	if (! nArgs)
		return Help_Arg_1( CMD_JSR );

	WORD nAddress = g_aArgs[1].nVal1 & _6502_END_MEM_ADDRESS;

	// Mark Stack Page as dirty
	*(memdirty+(regs.sp >> 8)) = 1;

	// Push PC onto stack
	*(mem + regs.sp) = ((regs.pc >> 8) & 0xFF);
	regs.sp--;

	*(mem + regs.sp) = ((regs.pc >> 0) - 1) & 0xFF;
	regs.sp--;


	// Jump to new address
	regs.pc = nAddress;

	return UPDATE_ALL;
}

//===========================================================================
Update_t CmdNOP (int nArgs)
{
	int iOpcode;
	int iOpmode;
	int nOpbyte;

 	_6502GetOpcodeOpmodeOpbyte( iOpcode, iOpmode, nOpbyte );

	while (nOpbyte--)
	{
		*(mem+regs.pc + nOpbyte) = 0xEA;
	}

	return UPDATE_ALL;
}

//===========================================================================
Update_t CmdOutput (int nArgs)
{
//  if ((!nArgs) ||
//      ((g_aArgs[1].sArg[0] != TEXT('0')) && (!g_aArgs[1].nVal1) && (!GetAddress(g_aArgs[1].sArg))))
//     return DisplayHelp(CmdInput);

	if (!nArgs)
		Help_Arg_1( CMD_OUTPUT );

	WORD nAddress = g_aArgs[1].nVal1;

//  iowrite[ g_aArgs[1].nVal1 & 0xFF](regs.pc,g_aArgs[1].nVal1 & 0xFF,1,g_aArgs[2].nVal1 & 0xFF,0);
	iowrite[ nAddress & 0xFF ] (regs.pc, nAddress & 0xFF, 1, g_aArgs[2].nVal1 & 0xFF,0);

	return UPDATE_CONSOLE_DISPLAY; // TODO: Verify // 1
}


// Benchmark ______________________________________________________________________________________

//===========================================================================
Update_t CmdBenchmark (int nArgs)
{
	if (g_bBenchmarking)
		CmdBenchmarkStart(0);
	else
		CmdBenchmarkStop(0);
	
	return UPDATE_ALL; // TODO/FIXME Verify
}

//===========================================================================
Update_t CmdBenchmarkStart (int nArgs)
{
	CpuSetupBenchmark();
	g_nDisasmCurAddress = regs.pc;
	DisasmCalcTopBotAddress();
	g_bBenchmarking = true;
	return UPDATE_ALL; // 1;
}

//===========================================================================
Update_t CmdBenchmarkStop (int nArgs)
{
	g_bBenchmarking = false;
	DebugEnd();
	mode = MODE_RUNNING;
	FrameRefreshStatus(DRAW_TITLE);
	VideoRedrawScreen();
	DWORD currtime = GetTickCount();
	while ((extbench = GetTickCount()) != currtime)
		; // intentional busy-waiting
	KeybQueueKeypress(TEXT(' '),1);
	resettiming = 1;

	return UPDATE_ALL; // 0;
}

//===========================================================================
Update_t CmdProfile (int nArgs)
{
	if (! nArgs)
	{
		sprintf( g_aArgs[ 1 ].sArg, g_aParameters[ PARAM_RESET ].m_sName );
		nArgs = 1;
	}

	if (nArgs == 1)
	{
		int iParam;
		int nFound = FindParam( g_aArgs[ 1 ].sArg, MATCH_EXACT, iParam, _PARAM_GENERAL_BEGIN, _PARAM_GENERAL_END );

		if (! nFound)
			goto _Help;

		if (iParam == PARAM_RESET)
		{
			ProfileReset();
			g_bProfiling = 1;
			ConsoleBufferPush( TEXT(" Resetting profile data." ) );
		}
		else
		{
			if ((iParam != PARAM_SAVE) && (iParam != PARAM_LIST))
				goto _Help;

			bool bExport = true;
			if (iParam == PARAM_LIST)
				bExport = false;

			// .csv (Comma Seperated Value)
//			ProfileFormat( bExport, bExport ? PROFILE_FORMAT_COMMA : PROFILE_FORMAT_SPACE );
			// .txt (Tab Seperated Value)
			ProfileFormat( bExport, bExport ? PROFILE_FORMAT_TAB : PROFILE_FORMAT_SPACE );

			// Dump to console
			if (iParam == PARAM_LIST)
			{

				char *pText;
				char  sText[ CONSOLE_WIDTH ];

				int   nLine = g_nProfileLine;
				int   iLine;
				
				for( iLine = 0; iLine < nLine; iLine++ )
				{
					pText = ProfileLinePeek( iLine );
					if (pText)
					{
						TextConvertTabsToSpaces( sText, pText, CONSOLE_WIDTH, 4 );
						ConsoleBufferPush( sText );
					}
				}
			}
		
			if (iParam == PARAM_SAVE)
			{
				if (ProfileSave())
				{
					TCHAR sText[ CONSOLE_WIDTH ];
					wsprintf( sText, " Saved: %s", g_FileNameProfile );
					ConsoleBufferPush( sText );
				}
				else
					ConsoleBufferPush( TEXT(" ERROR: Couldn't save file. (In use?)" ) );
			}
		}
	}
	else
		goto _Help;

	return ConsoleUpdate(); // UPDATE_CONSOLE_DISPLAY;

_Help:
	return Help_Arg_1( CMD_PROFILE );
}


// Breakpoint _____________________________________________________________________________________

// Menu: LOAD SAVE RESET
//===========================================================================
Update_t CmdBreakpoint (int nArgs)
{
	// This is temporary until the menu is in.
	CmdBreakpointAddPC( nArgs );

	return UPDATE_CONSOLE_DISPLAY;
}


// smart breakpoint
//===========================================================================
Update_t CmdBreakpointAddSmart (int nArgs)
{
	int nAddress = g_aArgs[1].nVal1;

	if (! nArgs)
	{
//		return Help_Arg_1( CMD_BREAKPOINT_ADD_SMART );
		g_aArgs[1].nVal1 = g_nDisasmCurAddress;		
	}

	if ((nAddress >= _6502_IO_BEGIN) && (nAddress <= _6502_IO_END))
	{
		return CmdBreakpointAddIO( nArgs );
	}
	else
	{
		CmdBreakpointAddReg( nArgs );
		CmdBreakpointAddMem( nArgs );	
		return UPDATE_BREAKPOINTS;
	}
		
	return UPDATE_CONSOLE_DISPLAY;
}


//===========================================================================
Update_t CmdBreakpointAddReg (int nArgs)
{
	if (! nArgs)
	{
		return Help_Arg_1( CMD_BREAKPOINT_ADD_REG );
	}

	BreakpointSource_t   iSrc = BP_SRC_REG_PC;
	BreakpointOperator_t iCmp = BP_OP_EQUAL  ;
	int nLen = 1;

	bool bHaveSrc = false;
	bool bHaveCmp = false;

	int iParamSrc;
	int iParamCmp;

	int  nFound;
	bool bAdded = false;

	int  iArg   = 0;
	while (iArg++ < nArgs)
	{
		char *sArg = g_aArgs[iArg].sArg;

		bHaveSrc = false;
		bHaveCmp = false;

		nFound = FindParam( sArg, MATCH_EXACT, iParamSrc, _PARAM_REGS_BEGIN, _PARAM_REGS_END ); 
		if (nFound)
		{
			switch (iParamSrc)
			{
				case PARAM_REG_A : iSrc = BP_SRC_REG_A ; bHaveSrc = true; break;
				case PARAM_FLAGS : iSrc = BP_SRC_REG_P ; bHaveSrc = true; break;
				case PARAM_REG_X : iSrc = BP_SRC_REG_X ; bHaveSrc = true; break;
				case PARAM_REG_Y : iSrc = BP_SRC_REG_Y ; bHaveSrc = true; break;
				case PARAM_REG_PC: iSrc = BP_SRC_REG_PC; bHaveSrc = true; break;
				case PARAM_REG_SP: iSrc = BP_SRC_REG_S ; bHaveSrc = true; break;
				default:
					break;
			}
		}

		nFound = FindParam( sArg, MATCH_EXACT, iParamCmp, _PARAM_BREAKPOINT_BEGIN, _PARAM_BREAKPOINT_END );
		if (nFound)
		{
			switch (iParamCmp)
			{
				case PARAM_BP_LESS_EQUAL   : iCmp = BP_OP_LESS_EQUAL   ; bHaveCmp = true; break;
				case PARAM_BP_LESS_THAN    : iCmp = BP_OP_LESS_THAN    ; bHaveCmp = true; break;
				case PARAM_BP_EQUAL        : iCmp = BP_OP_EQUAL        ; bHaveCmp = true; break;
				case PARAM_BP_NOT_EQUAL    : iCmp = BP_OP_NOT_EQUAL    ; bHaveCmp = true; break;
				case PARAM_BP_GREATER_THAN : iCmp = BP_OP_GREATER_THAN ; bHaveCmp = true; break;
				case PARAM_BP_GREATER_EQUAL: iCmp = BP_OP_GREATER_EQUAL; bHaveCmp = true; break;
				default:
					break;
			}
		}

		if ((! bHaveSrc) && (! bHaveCmp))
		{
			bAdded = _CmdBreakpointAddCommonArg( iArg, nArgs, iSrc, iCmp );
			if (!bAdded)
			{
				return Help_Arg_1( CMD_BREAKPOINT_ADD_REG );
			}
		}
	}

	return UPDATE_ALL;
}


//===========================================================================
bool _CmdBreakpointAddReg( Breakpoint_t *pBP, BreakpointSource_t iSrc, BreakpointOperator_t iCmp, WORD nAddress, int nLen )
{
	bool bStatus = false;

	if (pBP)
	{
		pBP->eSource   = iSrc;
		pBP->eOperator = iCmp;
		pBP->nAddress  = nAddress;
		pBP->nLength   = nLen;
		pBP->bSet      = true;
		pBP->bEnabled  = true;
		bStatus = true;
	}

	return bStatus;
}


//===========================================================================
bool _CmdBreakpointAddCommonArg ( int iArg, int nArg, BreakpointSource_t iSrc, BreakpointOperator_t iCmp )
{
	bool bStatus = false;

	int iBreakpoint = 0;
	Breakpoint_t *pBP = & g_aBreakpoints[ iBreakpoint ];

	while ((iBreakpoint < MAX_BREAKPOINTS) && g_aBreakpoints[iBreakpoint].nLength)
	{
		iBreakpoint++;
		pBP++;
	}

	if (iBreakpoint >= MAX_BREAKPOINTS)
	{
		ConsoleDisplayError(TEXT("All Breakpoints slots are currently in use."));
		return bStatus;
	}

	if (iArg <= nArg)
	{
		WORD nAddress = g_aArgs[iArg].nVal1;

		int nLen = g_aArgs[iArg].nVal2;
		if ( !nLen)
		{
			nLen = 1;
		}
		else
		{
			// Clamp Length so it stays within the 6502 memory address
			int nSlack        = (_6502_END_MEM_ADDRESS + 1) - nAddress;
			int nWantedLength = g_aArgs[iArg].nVal2;
			nLen = MIN( nSlack, nWantedLength );
		}

		bStatus = _CmdBreakpointAddReg( pBP, iSrc, iCmp, nAddress, nLen );
		g_nBreakpoints++;
	}

	return bStatus;
}


//===========================================================================
Update_t CmdBreakpointAddPC (int nArgs)
{
	BreakpointSource_t   iSrc = BP_SRC_REG_PC;
	BreakpointOperator_t iCmp = BP_OP_EQUAL  ;

	if (!nArgs)
	{
		nArgs = 1;
		g_aArgs[1].nVal1 = regs.pc;
	}

	bool bHaveSrc = false;
	bool bHaveCmp = false;

//	int iParamSrc;
	int iParamCmp;

	int  nFound = 0;
	bool bAdded = false;

	int  iArg   = 0;
	while (iArg++ < nArgs)
	{
		char *sArg = g_aArgs[iArg].sArg;

		if (g_aArgs[iArg].bType & TYPE_OPERATOR)
		{
			nFound = FindParam( sArg, MATCH_EXACT, iParamCmp, _PARAM_BREAKPOINT_BEGIN, _PARAM_BREAKPOINT_END );
			if (nFound)
			{
				switch (iParamCmp)
				{
					case PARAM_BP_LESS_EQUAL   : iCmp = BP_OP_LESS_EQUAL   ; bHaveCmp = true; break;
					case PARAM_BP_LESS_THAN    : iCmp = BP_OP_LESS_THAN    ; bHaveCmp = true; break;
					case PARAM_BP_EQUAL        : iCmp = BP_OP_EQUAL        ; bHaveCmp = true; break;
					case PARAM_BP_NOT_EQUAL    : iCmp = BP_OP_NOT_EQUAL    ; bHaveCmp = true; break;
					case PARAM_BP_GREATER_THAN : iCmp = BP_OP_GREATER_THAN ; bHaveCmp = true; break;
					case PARAM_BP_GREATER_EQUAL: iCmp = BP_OP_GREATER_EQUAL; bHaveCmp = true; break;
					default:
						break;
				}
			}
		}
		else
		{
			bAdded = _CmdBreakpointAddCommonArg( iArg, nArgs, iSrc, iCmp );
			if (!bAdded)
			{
				return Help_Arg_1( CMD_BREAKPOINT_ADD_PC );
			}
		}
	}
	
	return UPDATE_BREAKPOINTS | UPDATE_CONSOLE_DISPLAY; // 1;
}


//===========================================================================
Update_t CmdBreakpointAddIO   (int nArgs)
{
	return UPDATE_CONSOLE_DISPLAY;
}


//===========================================================================
Update_t CmdBreakpointAddMem  (int nArgs)
{
	BreakpointSource_t   iSrc = BP_SRC_MEM_1;
	BreakpointOperator_t iCmp = BP_OP_EQUAL  ;

	bool bAdded = false;

	int iArg = 0;
	
	while (iArg++ < nArgs)
	{
		char *sArg = g_aArgs[iArg].sArg;

		if (g_aArgs[iArg].bType & TYPE_OPERATOR)
		{
				return Help_Arg_1( CMD_BREAKPOINT_ADD_MEM );
		}
		else
		{
			bAdded = _CmdBreakpointAddCommonArg( iArg, nArgs, iSrc, iCmp );
			if (!bAdded)
			{
				return Help_Arg_1( CMD_BREAKPOINT_ADD_MEM );
			}
		}
	}

	return UPDATE_BREAKPOINTS | UPDATE_CONSOLE_DISPLAY;
}


//===========================================================================
void _Clear( Breakpoint_t * aBreakWatchZero, int iBreakpoint )
{
	aBreakWatchZero[iBreakpoint].bSet = false;
	aBreakWatchZero[iBreakpoint].bEnabled = false;
	aBreakWatchZero[iBreakpoint].nLength = 0;
}


// called by BreakpointsClear, WatchesClear, ZeroPagePointersClear
//===========================================================================
void _ClearViaArgs( int nArgs, Breakpoint_t * aBreakWatchZero, const int nMax, int & gnBWZ )
{
	int iBWZ;
	
	// CLEAR EACH BREAKPOINT IN THE LIST
	while (nArgs)
	{
		iBWZ = g_aArgs[nArgs].nVal1;
		if (! _tcscmp(g_aArgs[nArgs].sArg, g_aParameters[ PARAM_WILDSTAR ].m_sName))
		{
			iBWZ = nMax;
			while (iBWZ--)
			{
				if (aBreakWatchZero[iBWZ].bSet)
				{
					_Clear( aBreakWatchZero, iBWZ );
					gnBWZ--;
				}
			}
			break;
		}
		else if ((iBWZ >= 1) && (iBWZ <= nMax))
		{
			if (aBreakWatchZero[iBWZ-1].bSet)
			{
				_Clear( aBreakWatchZero, iBWZ - 1 );
				gnBWZ--;
			}
		}
		nArgs--;
	}
}

// called by BreakpointsEnable, WatchesEnable, ZeroPagePointersEnable
// called by BreakpointsDisable, WatchesDisable, ZeroPagePointersDisable
void _EnableDisableViaArgs( int nArgs, Breakpoint_t * aBreakWatchZero, const int nMax, const bool bEnabled )
{
	// Enable each breakpoint in the list
	while (nArgs)
	{
		int iBWZ = g_aArgs[nArgs].nVal1;
		if (! _tcscmp(g_aArgs[nArgs].sArg, g_aParameters[ PARAM_WILDSTAR ].m_sName))
		{
			int iBWZ = nMax;
			while (iBWZ--)
			{
				aBreakWatchZero[iBWZ].bEnabled = bEnabled;
			}			
			break;
		}
		else
		if ((iBWZ >= 1) && (iBWZ <= nMax))
		{
			aBreakWatchZero[iBWZ-1].bEnabled = bEnabled;
		}
		nArgs--;
	}
}

//===========================================================================
Update_t CmdBreakpointClear (int nArgs)
{
	if (!g_nBreakpoints)
		return ConsoleDisplayError(TEXT("There are no breakpoints defined."));

	int iBreakpoint;

	// CHECK FOR ERRORS
	if (!nArgs)
	{
		iBreakpoint = MAX_BREAKPOINTS;
		while (iBreakpoint--)
        {
			if ((g_aBreakpoints[iBreakpoint].bSet) && 
				(g_aBreakpoints[iBreakpoint].nAddress == regs.pc)) // TODO: FIXME
			{
				_Clear( g_aBreakpoints, iBreakpoint );
				g_nBreakpoints--;
			}
		}
//	    return DisplayHelp(CmdBreakpointClear);
	}
	
	_ClearViaArgs( nArgs, g_aBreakpoints, MAX_BREAKPOINTS, g_nBreakpoints );

	if (! g_nBreakpoints)
	{
		return (UPDATE_DISASM | UPDATE_BREAKPOINTS);
	}

    return UPDATE_BREAKPOINTS | UPDATE_CONSOLE_DISPLAY; // 1;
}

//===========================================================================
Update_t CmdBreakpointDisable (int nArgs)
{
	if (! g_nBreakpoints)
		return ConsoleDisplayError(TEXT("There are no (PC) Breakpoints defined."));

	if (! nArgs)
		return Help_Arg_1( CMD_BREAKPOINT_DISABLE );

	_EnableDisableViaArgs( nArgs, g_aBreakpoints, MAX_BREAKPOINTS, false );

	return UPDATE_BREAKPOINTS;
}

//===========================================================================
Update_t CmdBreakpointEdit (int nArgs)
{
	return (UPDATE_DISASM | UPDATE_BREAKPOINTS);
}


//===========================================================================
Update_t CmdBreakpointEnable (int nArgs) {

	if (! g_nBreakpoints)
		return ConsoleDisplayError(TEXT("There are no (PC) Breakpoints defined."));

	if (! nArgs)
		return Help_Arg_1( CMD_BREAKPOINT_ENABLE );

	_EnableDisableViaArgs( nArgs, g_aBreakpoints, MAX_BREAKPOINTS, true );

	return UPDATE_BREAKPOINTS;
}

void _ListBreakWatchZero( Breakpoint_t * aBreakWatchZero, int iBWZ )
{
	static TCHAR sText[ CONSOLE_WIDTH ];
	static const TCHAR sFlags[] = "-*";
	static TCHAR sName[ MAX_SYMBOLS_LEN+1 ];

	WORD nAddress = aBreakWatchZero[ iBWZ ].nAddress;
	LPCTSTR pSymbol = GetSymbol( nAddress, 2 );
	if (! pSymbol)
	{
		sName[0] = 0;
		pSymbol = sName;
	}

	wsprintf( sText, "  #%d %c %04X %s",
		iBWZ + 1,
		sFlags[ (int) aBreakWatchZero[ iBWZ ].bEnabled ],
		aBreakWatchZero[ iBWZ ].nAddress,
		pSymbol
	);
	ConsoleBufferPush( sText );
}

//===========================================================================
Update_t CmdBreakpointList (int nArgs)
{
//	ConsoleBufferPush( );
//	vector<int> vBreakpoints;
//	int iBreakpoint = MAX_BREAKPOINTS;
//	while (iBreakpoint--)
//	{
//		if (g_aBreakpoints[iBreakpoint].enabled)
//		{
//			vBreakpoints.push_back( g_aBreakpoints[iBreakpoint].address );
//		}
//	}
//	sort( vBreakpoints.begin(), vBreakpoints.end() );
//	iBreakpoint = vBreakPoints.size();

	if (! g_nBreakpoints)
	{
		ConsoleBufferPush( TEXT("  There are no current breakpoints." ) );
	}
	else
	{	
		int iBreakpoint = 0;
		while (iBreakpoint < MAX_BREAKPOINTS)
		{
			if (g_aBreakpoints[ iBreakpoint ].bSet)
			{
				_ListBreakWatchZero( g_aBreakpoints, iBreakpoint );
			}
			iBreakpoint++;
		}
	}
	return ConsoleUpdate();
}

//===========================================================================
Update_t CmdBreakpointLoad (int nArgs)
{
	return UPDATE_ALL;
}


//===========================================================================
Update_t CmdBreakpointSave (int nArgs)
{
	return UPDATE_CONSOLE_DISPLAY;
}


// Config _________________________________________________________________________________________

//===========================================================================
Update_t CmdConfig (int nArgs)
{
	if (nArgs)
	{
		int iParam;
		int iArg = 1;
		if (FindParam( g_aArgs[iArg].sArg, MATCH_FUZZY, iParam ))
		{
			switch (iParam)
			{
				case PARAM_SAVE:
					nArgs = _Arg_Shift( iArg, nArgs );
					return CmdConfigSave( nArgs );					
					break;
				case PARAM_LOAD:
					nArgs = _Arg_Shift( iArg, nArgs );
					return CmdConfigLoad( nArgs );					
					break;
				default:
				{
					TCHAR sText[ CONSOLE_WIDTH ];
					wsprintf( sText, "Syntax error: %s", g_aArgs[1].sArg );
					return ConsoleDisplayError( sText );
					break;
				}
			}
		}
	}

	return UPDATE_CONSOLE_DISPLAY;
}


//===========================================================================
Update_t CmdConfigLoad (int nArgs)
{
	// TODO: CmdRun( gaFileNameConfig )
	
//	TCHAR sFileNameConfig[ MAX_PATH ];
	if (! nArgs)
	{

	}

//	gDebugConfigName
	// DEBUGLOAD file // load debugger setting
	return UPDATE_ALL;
}


Update_t CmdConfigRun (int nArgs)
{
	return UPDATE_CONSOLE_DISPLAY;
}


// Save Debugger Settings
//===========================================================================
Update_t CmdConfigSave (int nArgs)
{
	TCHAR filename[ MAX_PATH ];
	_tcscpy(filename, progdir);
	_tcscat(filename, g_FileNameConfig );

	HANDLE hFile = CreateFile(filename,
		GENERIC_WRITE,
		0,
		(LPSECURITY_ATTRIBUTES)NULL,
		CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
		NULL);

	if (hFile != INVALID_HANDLE_VALUE)
	{
		void *pSrc;
		int   nLen;
		DWORD nPut;

		int nVersion = CURRENT_VERSION;
		pSrc = (void *) &nVersion;
		nLen = sizeof( nVersion );
		WriteFile( hFile, pSrc, nLen, &nPut, NULL );

		pSrc = (void *) & gaColorPalette;
		nLen = sizeof( gaColorPalette );
		WriteFile( hFile, pSrc, nLen, &nPut, NULL );

		pSrc = (void *) & g_aColorIndex;
		nLen = sizeof( g_aColorIndex );
		WriteFile( hFile, pSrc, nLen, &nPut, NULL );

		// History
		// UserSymbol

		CloseHandle( hFile );
	}
	return UPDATE_CONSOLE_DISPLAY;
}


//===========================================================================
Update_t CmdConfigFontLoad( int nArgs )
{
	return UPDATE_CONSOLE_DISPLAY;
}


//===========================================================================
Update_t CmdConfigFontSave( int nArgs )
{
	return UPDATE_CONSOLE_DISPLAY;
}


//===========================================================================
Update_t CmdConfigFontMode( int nArgs )
{
	if (nArgs != 2)
		return Help_Arg_1( CMD_CONFIG_FONT );

	int nMode = g_aArgs[ 2 ].nVal1;

	if ((nMode < 0) || (nMode >= NUM_FONT_SPACING))
		return Help_Arg_1( CMD_CONFIG_FONT );
		
	g_iFontSpacing = nMode;
	_UpdateWindowFontHeights( g_aFontConfig[ FONT_DISASM_DEFAULT ]._nFontHeight );

	return UPDATE_CONSOLE_DISPLAY | UPDATE_DISASM;
}


//===========================================================================
Update_t CmdConfigFont (int nArgs)
{
	int iArg;

	if (! nArgs)
		return CmdConfigGetFont( nArgs );
	else
	if (nArgs <= 2)
	{
		iArg = 1;

		if (nArgs == 1)
		{
			// FONT * is undocumented, like VERSION *
			if (_tcscmp( g_aArgs[ iArg ].sArg, g_aParameters[ PARAM_WILDSTAR ].m_sName ) == 0)
			{
				TCHAR sText[ CONSOLE_WIDTH ];
				wsprintf( sText, "Lines: %d  Font Px: %d  Line Px: %d"
					, g_nTotalLines
					, g_aFontConfig[ FONT_DISASM_DEFAULT ]._nFontHeight
					, g_aFontConfig[ FONT_DISASM_DEFAULT ]._nLineHeight );
				ConsoleBufferPush( sText );
				ConsoleBufferToDisplay();
				return UPDATE_CONSOLE_DISPLAY;
			}
		}

		int iFound;
		int nFound;
		
		nFound = FindParam( g_aArgs[iArg].sArg, MATCH_EXACT, iFound, _PARAM_GENERAL_BEGIN, _PARAM_GENERAL_END );
		if (nFound)
		{
			switch( iFound )
			{
				case PARAM_LOAD:
					return CmdConfigFontLoad( nArgs );
					break;
				case PARAM_SAVE:
					return CmdConfigFontSave( nArgs );
					break;
				// TODO: FONT SIZE #
				// TODO: AA {ON|OFF}
				default:
					break;		
			}
		}

		nFound = FindParam( g_aArgs[iArg].sArg, MATCH_EXACT, iFound, _PARAM_FONT_BEGIN, _PARAM_FONT_END );
		if (nFound)
		{
			if (iFound == PARAM_FONT_MODE)
				return CmdConfigFontMode( nArgs );
		}

		return CmdConfigSetFont( nArgs );
	}
	
	return Help_Arg_1( CMD_CONFIG_FONT );
}

int GetConsoleHeightPixels()
{
	int nHeight = nHeight = g_aFontConfig[ FONT_CONSOLE ]._nFontHeight; // _nLineHeight; // _nFontHeight;

	if (g_iFontSpacing == FONT_SPACING_CLEAN)
	{
//	int nHeight = g_aFontConfig[ FONT_DEFAULT ]._nFontHeight + 1; // "Classic" Height/Spacing
		nHeight++; // "Classic" Height/Spacing
	}
	else
	if (g_iFontSpacing == FONT_SPACING_COMPRESSED)
	{
		// default case handled
	}
	else
	if (g_iFontSpacing == FONT_SPACING_CLASSIC)
	{
		nHeight++;
	}
	
	return nHeight;
}

int GetConsoleTopPixels( int y )
{
	int nLineHeight = GetConsoleHeightPixels();
	int nTop = DISPLAY_HEIGHT - ((y + 1) * nLineHeight); // DISPLAY_HEIGHT - (y * nFontHeight); // ((g_nTotalLines - y) * g_nFontHeight; // 368 = 23 lines * 16 pixels/line // MAX_DISPLAY_CONSOLE_LINES
	return nTop;
}


// Only for FONT_DISASM_DEFAULT !
void _UpdateWindowFontHeights( int nFontHeight )
{
	if (nFontHeight)
	{
		// The screen layout defaults to a "virtal" font height of 16 pixels.
		// The disassmebler has 19 lines.
		// Total: 19 * 16 pixels
		//
		// Figure out how many lines we can display, given our new font height.
		// Given: Total_Pixels = Lines * Pixels_Line
		// Calc: Lines = Total_Pixels / Pixels_Line
		int nConsoleTopY = 19 * 16;
		int nHeight = 0;

		if (g_iFontSpacing == FONT_SPACING_CLASSIC)
		{
			nHeight = nFontHeight + 1;
			g_nTotalLines = nConsoleTopY / nHeight;
		}
		else
		if (g_iFontSpacing == FONT_SPACING_CLEAN)
		{		
			nHeight = nFontHeight;
			g_nTotalLines = nConsoleTopY / nHeight;
		}
		else
		if (g_iFontSpacing == FONT_SPACING_COMPRESSED)
		{
			nHeight = nFontHeight - 1;
			g_nTotalLines = (nConsoleTopY + nHeight) / nHeight; // Ceil()
		}

		g_aFontConfig[ FONT_DISASM_DEFAULT ]._nLineHeight = nHeight;
		
//		int nHeightOptimal = (nHeight0 + nHeight1) / 2;
//		int nLinesOptimal = nConsoleTopY / nHeightOptimal;
//		g_nTotalLines = nLinesOptimal;

		MAX_DISPLAY_DISASM_LINES = g_nTotalLines;

		// TODO/FIXME: Needs to take into account the console height
		MAX_DISPLAY_CONSOLE_LINES = MAX_DISPLAY_DISASM_LINES + MIN_DISPLAY_CONSOLE_LINES; // 23
//		if (MAX_DISPLAY_CONSOLE_LINES > 23)
//			MAX_DISPLAY_CONSOLE_LINES = 23;

		WindowUpdateSizes();
	}
}

//===========================================================================
bool _CmdConfigFont ( int iFont, LPCSTR pFontName, int iPitchFamily, int nFontHeight )
{
	bool bStatus = false;
	HFONT         hFont = (HFONT) 0;
	FontConfig_t *pFont = NULL;

	if (iFont < NUM_FONTS)
		pFont = & g_aFontConfig[ iFont ];

	if (pFontName)
	{	
//		int nFontHeight = g_nFontHeight - 1;

		int bAntiAlias = (nFontHeight < 14) ? DEFAULT_QUALITY : ANTIALIASED_QUALITY;

		// Try allow new font
		hFont = CreateFont(
			nFontHeight
			, 0 // Width
			, 0 // Escapement
			, 0 // Orientatin
			, FW_MEDIUM // Weight
			, 0 // Italic
			, 0 // Underline
			, 0 // Strike Out
			, DEFAULT_CHARSET // OEM_CHARSET
			, OUT_DEFAULT_PRECIS
			, CLIP_DEFAULT_PRECIS
			, bAntiAlias // ANTIALIASED_QUALITY // DEFAULT_QUALITY
			, iPitchFamily // HACK: MAGIC #: 4
			, pFontName );

		if (hFont)
		{
			if (iFont == FONT_DISASM_DEFAULT)
				_UpdateWindowFontHeights( nFontHeight );

			_tcsncpy( pFont->_sFontName, pFontName, MAX_FONT_NAME-1 );
			pFont->_sFontName[ MAX_FONT_NAME-1 ] = 0;

			HDC hDC = FrameGetDC();

			TEXTMETRIC tm;
			GetTextMetrics(hDC, &tm);

			SIZE  size;
			TCHAR sText[] = "W";
			int   nLen = 1;

			int nFontWidthAvg;
			int nFontWidthMax;

//			if (! (tm.tmPitchAndFamily & TMPF_FIXED_PITCH)) // Windows has this bitflag reversed!
//			{	// Proportional font?
//				bool bStop = true;
//			}

			// GetCharWidth32() doesn't work with TrueType Fonts
			if (GetTextExtentPoint32( hDC, sText, nLen,  &size ))
			{
				nFontWidthAvg = tm.tmAveCharWidth;
				nFontWidthMax = size.cx;
			}
			else
			{
				//  Font Name     Avg Max  "W"
				//  Arial           7   8   11
				//  Courier         5  32   11
				//  Courier New     7  14   
				nFontWidthAvg = tm.tmAveCharWidth;
				nFontWidthMax = tm.tmMaxCharWidth;
			}

			if (! nFontWidthAvg)
			{
				nFontWidthAvg = 7;
				nFontWidthMax = 7;
			}

			FrameReleaseDC();

//			DeleteObject( g_hFontDisasm );		
//			g_hFontDisasm = hFont;
			pFont->_hFont = hFont;

			pFont->_nFontWidthAvg = nFontWidthAvg;
			pFont->_nFontWidthMax = nFontWidthMax;
			pFont->_nFontHeight = nFontHeight;

			bStatus = true;
		}
	}
	return bStatus;
}


//===========================================================================
Update_t CmdConfigSetFont (int nArgs)
{
	HFONT  hFont = (HFONT) 0;
	TCHAR *pFontName = NULL;
	int    nHeight = g_nFontHeight;
	int    iFontTarget = FONT_DISASM_DEFAULT;
	int    iFontPitch  = FIXED_PITCH   | FF_MODERN;
//	int    iFontMode   = 
	bool   bHaveTarget = false;
	bool   bHaveFont = false;
		
	if (! nArgs)
	{ // reset to defaut font
		pFontName = g_sFontNameDefault;
	}
	else
	if (nArgs <= 3)
	{
		int iArg = 1;
		pFontName = g_aArgs[1].sArg;

		// [DISASM|INFO|CONSOLE] "FontName" [#]
		// "FontName" can be either arg 1 or 2

		int iFound;
		int nFound = FindParam( g_aArgs[iArg].sArg, MATCH_EXACT, iFound, _PARAM_WINDOW_BEGIN, _PARAM_WINDOW_END );
		if (nFound)
		{
			switch( iFound )
			{
				case PARAM_DISASM : iFontTarget = FONT_DISASM_DEFAULT; iFontPitch = FIXED_PITCH   | FF_MODERN    ; bHaveTarget = true; break;
				case PARAM_INFO   : iFontTarget = FONT_INFO          ; iFontPitch = FIXED_PITCH   | FF_MODERN    ; bHaveTarget = true; break;
				case PARAM_CONSOLE: iFontTarget = FONT_CONSOLE       ; iFontPitch = DEFAULT_PITCH | FF_DECORATIVE; bHaveTarget = true; break;
				default:
					if (g_aArgs[2].bType != TOKEN_QUOTED)
						return Help_Arg_1( CMD_CONFIG_FONT );
					break;
			}
			if (bHaveTarget)
			{
				pFontName = g_aArgs[2].sArg;
			}
		}
		else		
		if (nArgs == 2)
		{
			nHeight = atoi(g_aArgs[2].sArg );
			if ((nHeight < 6) || (nHeight > 36))
				nHeight = g_nFontHeight;			
		}	
	}
	else
	{
		return Help_Arg_1( CMD_CONFIG_FONT );
	}

	if (! _CmdConfigFont( iFontTarget, pFontName, iFontPitch, nHeight ))
	{
	}

	return UPDATE_ALL;
}

//===========================================================================
Update_t CmdConfigGetFont (int nArgs)
{
	if (! nArgs)
	{
		for (int iFont = 0; iFont < NUM_FONTS; iFont++ )
		{
			TCHAR sText[ CONSOLE_WIDTH ] = TEXT("");
			wsprintf( sText, "  Font: %-20s  A:%2d  M:%2d",
//				g_sFontNameCustom, g_nFontWidthAvg, g_nFontWidthMax );
				g_aFontConfig[ iFont ]._sFontName,
				g_aFontConfig[ iFont ]._nFontWidthAvg,
				g_aFontConfig[ iFont ]._nFontWidthMax );
			ConsoleBufferPush( sText );
		}
		return ConsoleUpdate();
	}

	return UPDATE_CONSOLE_DISPLAY;
}



// Cursor _________________________________________________________________________________________

// Given an Address, and Line to display it on
// Calculate the address of the top and bottom lines
// @param bUpdateCur
// true  = Update Cur based on Top
// false = Update Top & Bot based on Cur
//===========================================================================
void DisasmCalcTopFromCurAddress( bool bUpdateTop )
{
	int nLen = ((g_nDisasmWinHeight - g_nDisasmCurLine) * 3); // max 3 opcodes/instruction, is our search window

	// Look for a start address that when disassembled,
	// will have the cursor on the specified line and address
	int iTop = g_nDisasmCurAddress - nLen;
	int iCur = g_nDisasmCurAddress;

	g_bDisasmCurBad = false;
	
	bool bFound = false;
	while (iTop <= iCur)
	{
		WORD iAddress = iTop;
		for( int iLine = 0; iLine <= nLen; iLine++ ) // min 1 opcode/instruction
		{
			if (iLine == g_nDisasmCurLine) // && (iAddress == g_nDisasmCurAddress))
			{
				if (iAddress == g_nDisasmCurAddress)
				{
					g_nDisasmTopAddress = iTop;
					bFound = true;
					break;
				}
			}

			int iOpmode;
			int nOpbyte;
			_6502GetOpmodeOpbyte( iAddress, iOpmode, nOpbyte );

			// .20 Fixed: DisasmCalcTopFromCurAddress()
			//if ((eMode >= ADDR_INVALID1) && (eMode <= ADDR_INVALID3))
			{
#if 0 // _DEBUG
				TCHAR sText[ CONSOLE_WIDTH ];
				wsprintf( sText, "%04X : %d bytes\n", iAddress, nOpbyte );
				OutputDebugString( sText );
#endif
				iAddress += nOpbyte;
			}
		}
		if (bFound)
		{
			break;
		}
		iTop++;
 	}

	if (! bFound)
	{
		// Well, we're up the creek.
		// There is no (valid) solution!
		// Basically, there is no address, that when disassembled,
		// will put our Address on the cursor Line!
		// So, like typical game programming, when we don't like the solution, change the problem!
//		if (bUpdateTop)
			g_nDisasmTopAddress = g_nDisasmCurAddress;

		g_bDisasmCurBad = true; // Bad Disassembler, no opcode for you!

		// We reall should move the cursor line to the top for one instruction.
		// Moving the cursor line is not really a good idea, since we're breaking the user paradigm.
		// g_nDisasmCurLine = 0;
#if 0 // _DEBUG
			TCHAR sText[ CONSOLE_WIDTH * 2 ];
			sprintf( sText, TEXT("DisasmCalcTopFromCurAddress()\n"
				"\tTop: %04X\n"
				"\tLen: %04X\n"
				"\tMissed: %04X"),
				g_nDisasmCurAddress - nLen, nLen, g_nDisasmCurAddress );
			MessageBox( framewindow, sText, "ERROR", MB_OK );
#endif
	}
 }



WORD DisasmCalcAddressFromLines( WORD iAddress, int nLines )
{
	while (nLines-- > 0)
	{
		int iOpmode;
		int nOpbyte;
		_6502GetOpmodeOpbyte( iAddress, iOpmode, nOpbyte );
		iAddress += nOpbyte;
	}
	return iAddress;
}

void DisasmCalcCurFromTopAddress()
{
	g_nDisasmCurAddress = DisasmCalcAddressFromLines( g_nDisasmTopAddress, g_nDisasmCurLine );
}

void DisasmCalcBotFromTopAddress( )
{
	g_nDisasmBotAddress = DisasmCalcAddressFromLines( g_nDisasmTopAddress, g_nDisasmWinHeight );
}

void DisasmCalcTopBotAddress ()
{
	DisasmCalcTopFromCurAddress();
	DisasmCalcBotFromTopAddress();
}


//===========================================================================
Update_t CmdCursorLineDown (int nArgs)
{
	int iOpmode; 
	int nOpbyte;
	_6502GetOpmodeOpbyte( g_nDisasmCurAddress, iOpmode, nOpbyte ); // g_nDisasmTopAddress

	if (g_iWindowThis == WINDOW_DATA)
	{
		_CursorMoveDownAligned( WINDOW_DATA_BYTES_PER_LINE );
	}
	else
	if (nArgs)
	{
		nOpbyte = nArgs; // HACKL g_aArgs[1].val

		g_nDisasmTopAddress += nOpbyte;
		g_nDisasmCurAddress += nOpbyte;
		g_nDisasmBotAddress += nOpbyte;
	}
	else
	{
		// Works except on one case: G FB53, SPACE, DOWN
		WORD nTop = g_nDisasmTopAddress;
		WORD nCur = g_nDisasmCurAddress + nOpbyte;
		if (g_bDisasmCurBad)
		{
			g_nDisasmCurAddress = nCur;
			g_bDisasmCurBad = false;
			DisasmCalcTopFromCurAddress();
			return UPDATE_DISASM;
		}

		// Adjust Top until nNewCur is at > Cur
		do
		{
			g_nDisasmTopAddress++;
			DisasmCalcCurFromTopAddress();
		} while (g_nDisasmCurAddress < nCur);

		DisasmCalcCurFromTopAddress();
		DisasmCalcBotFromTopAddress();
		g_bDisasmCurBad = false;
	}

	// Can't use use + nBytes due to Disasm Singularity
//	DisasmCalcTopBotAddress();

	return UPDATE_DISASM;
}

//===========================================================================
Update_t CmdCursorLineUp (int nArgs)
{
	WORD nBest = g_nDisasmTopAddress;
	int nBytes = 1;
	
	if (g_iWindowThis == WINDOW_DATA)
	{
		_CursorMoveUpAligned( WINDOW_DATA_BYTES_PER_LINE );
	}
	else
	if (nArgs)
	{
		nBytes = nArgs; // HACK: g_aArgs[1].nVal1

		g_nDisasmTopAddress--;
		DisasmCalcCurFromTopAddress();
		DisasmCalcBotFromTopAddress();
	}
	else
	{
		nBytes = 1;

		// SmartLineUp()
		// Figure out if we should move up 1, 2, or 3 bytes since we have 2 possible cases:
		//
		// a) Scroll up by 2 bytes
		//    xx-2: A9 yy      LDA #xx
		//    xxxx: top
		//
		// b) Scroll up by 3 bytes
		//    xx-3: 20 A9 xx   JSR $00A9
		//    xxxx: top of window
		// 
		WORD nCur = g_nDisasmCurAddress - nBytes;

		// Adjust Top until nNewCur is at > Cur
		do
		{
			g_nDisasmTopAddress--;
			DisasmCalcCurFromTopAddress();
		} while (g_nDisasmCurAddress > nCur);

		DisasmCalcCurFromTopAddress();
		DisasmCalcBotFromTopAddress();
		g_bDisasmCurBad = false;
	}
	
	return UPDATE_DISASM;
}

//===========================================================================
Update_t CmdCursorJumpPC (int nArgs)
{
	// TODO: Allow user to decide if they want next g_aOpcodes at
	// 1) Centered (traditionaly), or
	// 2) Top of the screen

	// if (UserPrefs.bNextInstructionCentered)
	if (CURSOR_ALIGN_CENTER == nArgs)
	{
		g_nDisasmCurAddress = regs.pc;       // (2)
		WindowUpdateDisasmSize(); // calc cur line
	}
	else
	if (CURSOR_ALIGN_TOP == nArgs)
	{
		g_nDisasmCurAddress = regs.pc;       // (2)
		g_nDisasmCurLine = 0;
	}

	DisasmCalcTopBotAddress();

	return UPDATE_ALL;
}


//===========================================================================
bool _Get6502ReturnAddress( WORD & nAddress_ )
{
	unsigned nStack = regs.sp;
	nStack++;

	if (nStack <= (_6502_STACK_END - 1))
	{
		nAddress_ = 0;
		nAddress_ = (unsigned)*(LPBYTE)(mem + nStack);
		nStack++;
		
		nAddress_ += ((unsigned)*(LPBYTE)(mem + nStack)) << 8;
		nAddress_++;
		return true;
	}
	return false;
}

//===========================================================================
Update_t CmdCursorJumpRetAddr (int nArgs)
{
	WORD nAddress;
	if (_Get6502ReturnAddress( nAddress ))
	{	
		g_nDisasmCurAddress = nAddress;

		if (CURSOR_ALIGN_CENTER == nArgs)
		{
			WindowUpdateDisasmSize();
		}
		else
		if (CURSOR_ALIGN_TOP == nArgs)
		{
			g_nDisasmCurLine = 0;
		}
		DisasmCalcTopBotAddress();
	}

	return UPDATE_ALL;
}

//===========================================================================
Update_t CmdCursorRunUntil (int nArgs)
{
	nArgs = _Arg_1( g_nDisasmCurAddress );
	return CmdGo( nArgs );
}


WORD _ClampAddress( int nAddress )
{
	if (nAddress < 0)
		nAddress = 0;
	if (nAddress > 0xFFFF)
		nAddress = 0xFFFF;

	return (WORD) nAddress;
}


// nDelta must be a power of 2
//===========================================================================
void _CursorMoveDownAligned( int nDelta )
{
	if (g_iWindowThis == WINDOW_DATA)
	{
		if (g_aMemDump[0].bActive)
		{
			if (g_aMemDump[0].eDevice == DEV_MEMORY)
			{
				g_aMemDump[0].nAddress += nDelta;
				g_aMemDump[0].nAddress &= _6502_END_MEM_ADDRESS;
			}
		}
	}
	else
	{
		int nNewAddress = g_nDisasmTopAddress; // BUGFIX: g_nDisasmCurAddress;

		if ((nNewAddress & (nDelta-1)) == 0)
			nNewAddress += nDelta;
		else
			nNewAddress += (nDelta - (nNewAddress & (nDelta-1))); // .22 Fixed: Shift-PageUp Shift-PageDown Ctrl-PageUp Ctrl-PageDown -> _CursorMoveUpAligned() & _CursorMoveDownAligned()

		g_nDisasmTopAddress = nNewAddress & _6502_END_MEM_ADDRESS; // .21 Fixed: _CursorMoveUpAligned() & _CursorMoveDownAligned() not wrapping around past FF00 to 0, and wrapping around past 0 to FF00
	}
}

// nDelta must be a power of 2
//===========================================================================
void _CursorMoveUpAligned( int nDelta )
{
	if (g_iWindowThis == WINDOW_DATA)
	{
		if (g_aMemDump[0].bActive)
		{
			if (g_aMemDump[0].eDevice == DEV_MEMORY)
			{
				g_aMemDump[0].nAddress -= nDelta;
				g_aMemDump[0].nAddress &= _6502_END_MEM_ADDRESS;
			}
		}
	}
	else
	{
		int nNewAddress = g_nDisasmTopAddress; // BUGFIX: g_nDisasmCurAddress;

		if ((nNewAddress & (nDelta-1)) == 0)
			nNewAddress -= nDelta;
		else
			nNewAddress -= (nNewAddress & (nDelta-1)); // .22 Fixed: Shift-PageUp Shift-PageDown Ctrl-PageUp Ctrl-PageDown -> _CursorMoveUpAligned() & _CursorMoveDownAligned()

		g_nDisasmTopAddress = nNewAddress & _6502_END_MEM_ADDRESS; // .21 Fixed: _CursorMoveUpAligned() & _CursorMoveDownAligned() not wrapping around past FF00 to 0, and wrapping around past 0 to FF00
	}
}


//===========================================================================
Update_t CmdCursorPageDown (int nArgs)
{
	int iLines = 1; // show at least 1 line from previous display
	int nLines = WindowGetHeight( g_iWindowThis );

	if (nLines < 2)
		nLines = 2;

	if (g_iWindowThis == WINDOW_DATA)
	{
		const int nStep = 128;
		_CursorMoveDownAligned( nStep );
	}
	else
	{
		while (++iLines < nLines)
			CmdCursorLineDown(nArgs);
	}

	return UPDATE_DISASM;
}


//===========================================================================
Update_t CmdCursorPageDown256 (int nArgs)
{
	const int nStep = 256;
	_CursorMoveDownAligned( nStep );
	return UPDATE_DISASM;
}

//===========================================================================
Update_t CmdCursorPageDown4K (int nArgs)
{
	const int nStep = 4096;
	_CursorMoveDownAligned( nStep );
	return UPDATE_DISASM;
}

//===========================================================================
Update_t CmdCursorPageUp (int nArgs)
{
	int iLines = 1; // show at least 1 line from previous display
	int nLines = WindowGetHeight( g_iWindowThis );
	
	if (nLines < 2)
		nLines = 2;

	if (g_iWindowThis == WINDOW_DATA)
	{
		const int nStep = 128;
		_CursorMoveUpAligned( nStep );
	}
	else
	{
		while (++iLines < nLines)
			CmdCursorLineUp(nArgs);
	}
		
	return UPDATE_DISASM;
}

//===========================================================================
Update_t CmdCursorPageUp256 (int nArgs)
{
	const int nStep = 256;
	_CursorMoveUpAligned( nStep );
	return UPDATE_DISASM;
}

//===========================================================================
Update_t CmdCursorPageUp4K (int nArgs)
{
	const int nStep = 4096;
	_CursorMoveUpAligned( nStep );
	return UPDATE_DISASM;
}

//===========================================================================
Update_t CmdCursorSetPC( int nArgs) // TODO rename
{
	regs.pc = nArgs; // HACK:
	return UPDATE_DISASM;
}


// Flags __________________________________________________________________________________________


//===========================================================================
Update_t CmdFlagClear (int nArgs)
{
	int iFlag = (g_iCommand - CMD_FLAG_CLR_C);

	if (g_iCommand == CMD_FLAG_CLEAR)
	{
		int iArg = nArgs;
		while (iArg)
		{
			iFlag = 0;
			while (iFlag < _6502_NUM_FLAGS)
			{
//				if (g_aFlagNames[iFlag] == g_aArgs[iArg].sArg[0])
				if (g_aBreakpointSource[ BP_SRC_FLAG_N + iFlag ][0] == g_aArgs[iArg].sArg[0])
				{
					regs.ps &= ~(1 << iFlag);
				}
				iFlag++;
			}
			iArg--;			
		}
	}
	else
	{
		regs.ps &= ~(1 << iFlag);
	}

	return UPDATE_FLAGS; // 1;
}

//===========================================================================
Update_t CmdFlagSet (int nArgs)
{
	int iFlag = (g_iCommand - CMD_FLAG_SET_C);

	if (g_iCommand == CMD_FLAG_SET)
	{
		int iArg = nArgs;
		while (iArg)
		{
			iFlag = 0;
			while (iFlag < _6502_NUM_FLAGS)
			{
//				if (g_aFlagNames[iFlag] == g_aArgs[iArg].sArg[0])
				if (g_aBreakpointSource[ BP_SRC_FLAG_N + iFlag ][0] == g_aArgs[iArg].sArg[0])
				{
					regs.ps |= (1 << iFlag);
				}
				iFlag++;
			}
			iArg--;			
		}
	}
	else
	{
		regs.ps |= (1 << iFlag);
	}
	return UPDATE_FLAGS; // 1;
}

//===========================================================================
Update_t CmdFlag (int nArgs)
{
//	if (g_aArgs[0].sArg[0] == g_aParameters[PARAM_FLAG_CLEAR].aName[0] ) // TEXT('R')
	if (g_iCommand == CMD_FLAG_CLEAR)
		return CmdFlagClear( nArgs );
	else
	if (g_iCommand == CMD_FLAG_SET)
//	if (g_aArgs[0].sArg[0] == g_aParameters[PARAM_FLAG_SET].aName[0] ) // TEXT('S')
		return CmdFlagSet( nArgs );

	return UPDATE_ALL; // 0;
}


// Help ___________________________________________________________________________________________


// Loads the arguments with the command to get help on and call display help.
//===========================================================================
Update_t Help_Arg_1( int iCommandHelp )
{
	_Arg_1( iCommandHelp );

	wsprintf( g_aArgs[ 1 ].sArg, g_aCommands[ iCommandHelp ].m_sName ); // .3 Fixed: Help_Arg_1() now copies command name into arg.name

	return CmdHelpSpecific( 1 );
}



//===========================================================================
void _CmdHelpSpecific()
{

}


//===========================================================================
Update_t CmdMOTD( int nArgs )
{
	TCHAR sText[ CONSOLE_WIDTH ];

	ConsoleBufferPush( TEXT(" Apple ][+ //e Emulator for Windows") );
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
			wsprintf( sText, " Maximum breakpoints are: %d", MAX_BREAKPOINTS );
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


// Memory _________________________________________________________________________________________


// TO DO:
// . Add support for dumping Disk][ device
//===========================================================================
bool MemoryDumpCheck (int nArgs, WORD * pAddress_ )
{
	if (! nArgs)
		return false;

	Arg_t *pArg = &g_aArgs[1];
	WORD nAddress = pArg->nVal1;
	bool bUpdate = false;

	pArg->eDevice = DEV_MEMORY;						// Default

	if(strncmp(g_aArgs[1].sArg, "SY", 2) == 0)			// SY6522
	{
		nAddress = (g_aArgs[1].sArg[2] - '0') & 3;
		pArg->eDevice = DEV_SY6522;
		bUpdate = true;
	}
	else if(strncmp(g_aArgs[1].sArg, "AY", 2) == 0)		// AY8910
	{
		nAddress  = (g_aArgs[1].sArg[2] - '0') & 3;
		pArg->eDevice = DEV_AY8910;
		bUpdate = true;
	}
#ifdef SUPPORT_Z80_EMU
	else if(strcmp(g_aArgs[1].sArg, "*AF") == 0)
	{
		nAddress = *(WORD*)(membank+REG_AF);
		bUpdate = true;
	}
	else if(strcmp(g_aArgs[1].sArg, "*BC") == 0)
	{
		nAddress = *(WORD*)(membank+REG_BC);
		bUpdate = true;
	}
	else if(strcmp(g_aArgs[1].sArg, "*DE") == 0)
	{
		nAddress = *(WORD*)(membank+REG_DE);
		bUpdate = true;
	}
	else if(strcmp(g_aArgs[1].sArg, "*HL") == 0)
	{
		nAddress = *(WORD*)(membank+REG_HL);
		bUpdate = true;
	}
	else if(strcmp(g_aArgs[1].sArg, "*IX") == 0)
	{
		nAddress = *(WORD*)(membank+REG_IX);
		bUpdate = true;
	}
#endif

	if (bUpdate)
	{
		pArg->nVal1 = nAddress;
		sprintf( pArg->sArg, "%04X", nAddress );
	}

	if (pAddress_)
	{
			*pAddress_ = nAddress;
	}

	return true;
}

//===========================================================================
Update_t CmdMemoryCompare (int nArgs )
{
	if (nArgs < 3)
		return Help_Arg_1( CMD_MEMORY_COMPARE );

	WORD nSrcAddr = g_aArgs[1].nVal1;
	WORD nLenByte = 0;
	WORD nDstAddr = g_aArgs[3].nVal1;

	WORD nSrcSymAddr;
	WORD nDstSymAddr;

	if (!nSrcAddr)
	{
		nSrcSymAddr = GetAddressFromSymbol( g_aArgs[1].sArg );
		if (nSrcAddr != nSrcSymAddr)
			nSrcAddr = nSrcSymAddr;
	}

	if (!nDstAddr)
	{
		nDstSymAddr = GetAddressFromSymbol( g_aArgs[3].sArg );
		if (nDstAddr != nDstSymAddr)
			nDstAddr = nDstSymAddr;
	}

//	if ((!nSrcAddr) || (!nDstAddr))
//		return Help_Arg_1( CMD_MEMORY_COMPARE );

	return UPDATE_CONSOLE_DISPLAY;
}

//===========================================================================
static Update_t _CmdMemoryDump (int nArgs, int iWhich, int iView )
{
	WORD nAddress = 0;

	if( ! MemoryDumpCheck(nArgs, & nAddress ) )
	{
		return Help_Arg_1( g_iCommand );
	}

	g_aMemDump[iWhich].nAddress = nAddress;
	g_aMemDump[iWhich].eDevice = g_aArgs[1].eDevice;
	g_aMemDump[iWhich].bActive = true;
	g_aMemDump[iWhich].eView = (MemoryView_e) iView;

	return UPDATE_ALL; // TODO: This really needed? Don't think we do any actual ouput
}

//===========================================================================
bool _MemoryCheckMiniDump ( int iWhich )
{
	if ((iWhich < 0) || (iWhich > NUM_MEM_MINI_DUMPS))
	{
		TCHAR sText[ CONSOLE_WIDTH ];
		wsprintf( sText, TEXT("  Only %d memory mini dumps"), NUM_MEM_MINI_DUMPS );
		ConsoleDisplayError( sText );
		return true;
	}
	return false;
}

//===========================================================================
Update_t CmdMemoryMiniDumpHex (int nArgs)
{
	int iWhich = g_iCommand - CMD_MEM_MINI_DUMP_HEX_1;
	if (_MemoryCheckMiniDump( iWhich ))
		return UPDATE_CONSOLE_DISPLAY;

	return _CmdMemoryDump(nArgs, iWhich, MEM_VIEW_HEX );
}

//===========================================================================
Update_t CmdMemoryMiniDumpAscii (int nArgs)
{
	int iWhich = g_iCommand - CMD_MEM_MINI_DUMP_ASCII_1;
	if (_MemoryCheckMiniDump( iWhich ))
		return UPDATE_CONSOLE_DISPLAY;

	return _CmdMemoryDump(nArgs, iWhich, MEM_VIEW_ASCII );
}

//===========================================================================
Update_t CmdMemoryMiniDumpApple (int nArgs)
{
	int iWhich = g_iCommand - CMD_MEM_MINI_DUMP_APPLE_1;
	if (_MemoryCheckMiniDump( iWhich ))
		return UPDATE_CONSOLE_DISPLAY;

	return _CmdMemoryDump(nArgs, iWhich, MEM_VIEW_APPLE ); // MEM_VIEW_TXT_LO );
}

//===========================================================================
//Update_t CmdMemoryMiniDumpLow (int nArgs)
//{
//	int iWhich = g_iCommand - CMD_MEM_MINI_DUMP_TXT_LO_1;
//	if (_MemoryCheckMiniDump( iWhich ))
//		return UPDATE_CONSOLE_DISPLAY;
//
//	return _CmdMemoryDump(nArgs, iWhich, MEM_VIEW_APPLE ); // MEM_VIEW_TXT_LO );
//}

//===========================================================================
//Update_t CmdMemoryMiniDumpHigh (int nArgs)
//{
//	int iWhich = g_iCommand - CMD_MEM_MINI_DUMP_TXT_HI_1;
//	if (_MemoryCheckMiniDump( iWhich ))
//		return UPDATE_CONSOLE_DISPLAY;
//
//	return _CmdMemoryDump(nArgs, iWhich, MEM_VIEW_APPLE ); // MEM_VIEW_TXT_HI );
//}

//===========================================================================
Update_t CmdMemoryEdit (int nArgs)
{

	return UPDATE_CONSOLE_DISPLAY;
}


//===========================================================================
Update_t CmdMemoryEnterByte (int nArgs)
{
	if ((nArgs < 2) ||
		((g_aArgs[2].sArg[0] != TEXT('0')) && (!g_aArgs[2].nVal1))) // arg2 not numeric or not specified
	{
		Help_Arg_1( CMD_MEMORY_ENTER_WORD );
	}
	
	WORD nAddress = g_aArgs[1].nVal1;
	while (nArgs >= 2)
	{
		*(membank+nAddress+nArgs-2)  = (BYTE)g_aArgs[nArgs].nVal1;
		*(memdirty+(nAddress >> 8)) = 1;
		nArgs--;
	}

	return UPDATE_ALL;
}


//===========================================================================
Update_t CmdMemoryEnterWord (int nArgs)
{
	if ((nArgs < 2) ||
		((g_aArgs[2].sArg[0] != TEXT('0')) && (!g_aArgs[2].nVal1))) // arg2 not numeric or not specified
	{
		Help_Arg_1( CMD_MEMORY_ENTER_WORD );
	}
	
	WORD nAddress = g_aArgs[1].nVal1;
	while (nArgs >= 2)
	{
		WORD nData = g_aArgs[nArgs].nVal1;

		// Little Endian
		*(membank + nAddress + nArgs - 2)  = (BYTE)(nData >> 0);
		*(membank + nAddress + nArgs - 1)  = (BYTE)(nData >> 8);

		*(memdirty+(nAddress >> 8)) |= 1;
		nArgs--;
	}

	return UPDATE_ALL;
}

//===========================================================================
Update_t CmdMemoryFill (int nArgs)
{
	if (!nArgs)
		return Help_Arg_1( CMD_MEMORY_FILL );
  
	WORD nAddress = g_aArgs[1].nVal1;
	WORD nBytes   = MAX(1,g_aArgs[1].nVal2); // TODO: This actually work??

	while (nBytes--)
	{
		if ((nAddress < _6502_IO_BEGIN) || (nAddress > _6502_IO_END))
		{
			*(membank+nAddress) = (BYTE)(g_aArgs[2].nVal1 & 0xFF); // HACK: Undocumented fill with ZERO
		}
		nAddress++;
	}

	return UPDATE_CONSOLE_DISPLAY;
}

//===========================================================================
Update_t CmdMemoryMove (int nArgs)
{
	if (nArgs < 3)
		return Help_Arg_1( CMD_MEMORY_MOVE );

	WORD nSrc = g_aArgs[1].nVal1;
	WORD nLen = g_aArgs[2].nVal1 - nSrc;
	WORD nDst = g_aArgs[3].nVal1;
	
	return UPDATE_CONSOLE_DISPLAY;
}

//===========================================================================
Update_t CmdMemorySearch (int nArgs)
{
	if (nArgs < 3)
		Help_Arg_1( CMD_MEMORY_SEARCH );

	 if (g_aArgs[2].sArg[0] == TEXT('\"'))
		CmdMemorySearchText( nArgs );
	else
		CmdMemorySearchHex( nArgs );

	return UPDATE_CONSOLE_DISPLAY;
}

// Search for Ascii Hi-Bit set
//===========================================================================
Update_t CmdMemorySearchHiBit (int nArgs)
{
	return UPDATE_CONSOLE_DISPLAY;
}

//===========================================================================
Update_t CmdMemorySearchLowBit (int nArgs)
{
	return UPDATE_CONSOLE_DISPLAY;
}

//===========================================================================
bool _GetStartEnd( WORD & nAddressStart_, WORD & nAddressEnd_ )
{
	nAddressStart_ = g_aArgs[1].nVal1 ? g_aArgs[1].nVal1 : GetAddressFromSymbol( g_aArgs[1].sArg );
	nAddressEnd_   = g_aArgs[2].nVal1 ? g_aArgs[2].nVal1 : GetAddressFromSymbol( g_aArgs[2].sArg );
	return true;
}

//===========================================================================
Update_t CmdMemorySearchHex (int nArgs)
{
	WORD nAddrStart;
	WORD nAddrEnd;
	_GetStartEnd( nAddrStart, nAddrEnd );
// TODO: if (!_tcscmp(g_aArgs[nArgs].sArg,TEXT("*"))) { }
// TODO: if (!_tcscmp(g_aArgs[nArgs].sArg,TEXT("?"))) { }
	return UPDATE_CONSOLE_DISPLAY;
}

//===========================================================================
Update_t CmdMemorySearchText (int nArgs)
{
	WORD nAddrStart;
	WORD nAddrEnd;
	_GetStartEnd( nAddrStart, nAddrEnd );

	return UPDATE_CONSOLE_DISPLAY;
}




// Registers ______________________________________________________________________________________


//===========================================================================
Update_t CmdRegisterSet (int nArgs)
{
  if ((nArgs == 2) &&
      (g_aArgs[1].sArg[0] == TEXT('P')) && (g_aArgs[2].sArg[0] == TEXT('L'))) //HACK: TODO/FIXME: undocumented hard-coded command?!?!
	{
		regs.pc = lastpc;
	}
	else
	if (nArgs < 2) // || ((g_aArgs[2].sArg[0] != TEXT('0')) && !g_aArgs[2].nVal1))
	{
	    return Help_Arg_1( CMD_REGISTER_SET );
	}
	else
	{
		TCHAR *pName = g_aArgs[1].sArg;
		int iParam;
		if (FindParam( pName, MATCH_EXACT, iParam, _PARAM_REGS_BEGIN, _PARAM_REGS_END ))
		{
			int iArg = 2;
			if (g_aArgs[ iArg ].eToken == TOKEN_EQUAL)
				iArg++;

			if (iArg > nArgs)
				return Help_Arg_1( CMD_REGISTER_SET );
				
			BYTE b = (BYTE)(g_aArgs[ iArg ].nVal1 & 0xFF);
			WORD w = (WORD)(g_aArgs[ iArg ].nVal1 & 0xFFFF);

			switch (iParam)
			{
				case PARAM_REG_A : regs.a  = b;    break;
				case PARAM_REG_PC: regs.pc = w; g_nDisasmCurAddress = regs.pc; DisasmCalcTopBotAddress(); break;
				case PARAM_REG_SP: regs.sp = b | 0x100; break;
				case PARAM_REG_X : regs.x  = b; break;
				case PARAM_REG_Y : regs.y  = b; break;
				default:        return Help_Arg_1( CMD_REGISTER_SET );
			}
		}
	}
	
//	g_nDisasmCurAddress = regs.pc;
//	DisasmCalcTopBotAddress();

	return UPDATE_ALL; // 1
}


// Source Level Debugging _________________________________________________________________________

//===========================================================================
bool BufferAssemblyListing( TCHAR *pFileName )
{
	bool bStatus = false; // true = loaded

	if (! pFileName)
		return bStatus;

	FILE *hFile = fopen(pFileName,"rt");

	if (hFile)
	{
		fseek( hFile, 0, SEEK_END );
		long nSize = ftell( hFile );
		fseek( hFile, 0, SEEK_SET );

		if (nSize > 0)
			g_nSourceBuffer = nSize;

		g_pSourceBuffer = new char [ nSize + 1 ];
		ZeroMemory( g_pSourceBuffer, nSize + 1 );

		fread( (void*)g_pSourceBuffer, nSize, 1, hFile );
		fclose(hFile);

		g_vSourceLines.erase( g_vSourceLines.begin(), g_vSourceLines.end() );

		g_nSourceAssemblyLines = 0;

		char *pBegin = g_pSourceBuffer;
		char *pEnd = NULL;
		char *pEnd2;
		while (pBegin)
		{
			g_nSourceAssemblyLines++;
			g_vSourceLines.push_back( pBegin );

			pEnd = const_cast<char*>( SkipUntilEOL( pBegin ));
			pEnd2 = const_cast<char*>( SkipEOL( pEnd ));
			if (pEnd)
			{
				*pEnd = 0;
			}
			if (! *pEnd2)
			{
				break;
			}
			pBegin = pEnd2;
//			*(pEnd-1) = 0;
//			if (! pEnd)
//			{
//				break;
//			}
//			pEnd = const_cast<char*>( SkipEOL( pEnd ));
//			pBegin = pEnd;
		}			
		bStatus = true;
	}

	if (g_nSourceAssemblyLines)
	{
		g_bSourceLevelDebugging = true;
	}

	return bStatus;
}


//===========================================================================
bool ParseAssemblyListing( bool bBytesToMemory, bool bAddSymbols )
{
	bool bStatus = false; // true = loaded

	// Assembler source listing file:
	//
	// xxxx:_b1_[b2]_[b3]__n_[label]_[opcode]_[param]
//	char  sByte1[ 2 ];
//	char  sByte2[ 2 ];
//	char  sByte3[ 2 ];
//	char  sLineN[ W ];
	char sName[ MAX_SYMBOLS_LEN ];
//	char  sAsm  [ W ];
//	char  sParam[ W ];

	const int MAX_LINE = 256;
	char  sLine[ MAX_LINE ];
	char  sText[ MAX_LINE ];
//	char  sLabel[ MAX_LINE ];

	g_nSourceAssembleBytes = 0;
	g_nSourceAssemblySymbols = 0;

	const DWORD INVALID_ADDRESS = _6502_END_MEM_ADDRESS + 1;

	bool bPrevSymbol = false;
	bool bFourBytes = false;		
	BYTE nByte4 = 0;


	int nLines = g_vSourceLines.size();
	for( int iLine = 0; iLine < nLines; iLine++ )
	{
		ZeroMemory( sText, MAX_LINE );
		strncpy( sText, g_vSourceLines[ iLine ], MAX_LINE-1 );

		DWORD nAddress = INVALID_ADDRESS;

		_tcscpy( sLine, sText );
		char *p = sLine;
		p = strstr( sLine, ":" );
		if (p)
		{
			*p = 0;
			//	sscanf( sLine, "%s %s %s %s %s %s %s %s", sAddr1, sByte1, sByte2, sByte3, sLineN, sLabel, sAsm, sParam );
			sscanf( sLine, "%X", &nAddress );

			if (nAddress >= INVALID_ADDRESS) // || (sName[0] == 0) )
				continue;

			if (bBytesToMemory)
			{
				char *pEnd = p + 1;				
				char *pStart;
				for (int iByte = 0; iByte < 3; iByte++ )
				{
					// xx xx xx
					// ^ ^
					// | |
					// | end
					// start
					pStart = pEnd + 1;
					pEnd = const_cast<char*>( SkipUntilWhiteSpace( pStart ));
					int nLen = (pEnd - pStart);
					if (nLen != 2)
					{
						break;
					}
					*pEnd = 0;
					BYTE nByte = Chars2ToByte( pStart );
					*(mem + ((WORD)nAddress) + iByte ) = nByte;
				}
				g_nSourceAssembleBytes += iByte;
			}

			g_aSourceDebug[ (WORD) nAddress ] = iLine; // g_nSourceAssemblyLines;
		}

		_tcscpy( sLine, sText );
		if (bAddSymbols)
		{
			// Add user symbol:          symbolname EQU $address
			//  or user symbol: address: symbolname DFB #bytes
 			char *pEQU = strstr( sLine, "EQU" ); // EQUal / EQUate
			char *pDFB = strstr( sLine, "DFB" ); // DeFine Byte
			char *pLabel = NULL;

			if (pEQU)
				pLabel = pEQU;
			if (pDFB)
				pLabel = pDFB;

			if (pLabel)
			{	
				char *pLabelEnd = pLabel - 1;
				pLabelEnd = const_cast<char*>( SkipWhiteSpaceReverse( pLabelEnd, &sLine[ 0 ] ));
				char * pLabelStart = NULL; // SkipWhiteSpaceReverse( pLabelEnd, &sLine[ 0 ] );
				if (pLabelEnd)
				{
					pLabelStart = const_cast<char*>( SkipUntilWhiteSpaceReverse( pLabelEnd, &sLine[ 0 ] ));
					pLabelEnd++;
					pLabelStart++;
					
					int nLen = pLabelEnd - pLabelStart;
					nLen = MIN( nLen, MAX_SYMBOLS_LEN );
					strncpy( sName, pLabelStart, nLen );
					sName[ nLen ] = 0;

					char *pAddressEQU = strstr( pLabel, "$" );
					char *pAddressDFB = strstr( sLine, ":" ); // Get address from start of line
					char *pAddress = NULL;

					if (pAddressEQU)
						pAddress = pAddressEQU + 1;
					if (pAddressDFB)
					{
						*pAddressDFB = 0;
						pAddress = sLine;
					}

					if (pAddress)
					{
						char *pAddressEnd;
						nAddress = (DWORD) strtol( pAddress, &pAddressEnd, 16 );
						g_aSymbols[SYMBOLS_SRC][ (WORD) nAddress] = sName;
						g_nSourceAssemblySymbols++;
					}
				}
			}
		}
	} // for

	bStatus = true;
	
	return bStatus;
}


//===========================================================================
Update_t CmdSource (int nArgs)
{
	if (! nArgs)
	{
		g_bSourceLevelDebugging = false;
	}
	else
	{
		g_bSourceAddMemory = false;
		g_bSourceAddSymbols = false;

		for( int iArg = 1; iArg <= nArgs; iArg++ )
		{	
			TCHAR *pFileName = g_aArgs[ iArg ].sArg;

			int iParam;
			bool bFound = FindParam( pFileName, MATCH_EXACT, iParam, _PARAM_SOURCE_BEGIN, _PARAM_SOURCE_END ) > 0 ? true : false;
			if (bFound && (iParam == PARAM_SRC_SYMBOLS))
			{
				g_bSourceAddSymbols = true;
			}
			else
			if (bFound && (iParam == PARAM_SRC_MEMORY))
			{
				g_bSourceAddMemory = true;	
			}
			else
			{
				TCHAR  sFileName[MAX_PATH];
				_tcscpy(sFileName,progdir);
				_tcscat(sFileName, pFileName);

				if (BufferAssemblyListing( sFileName ))
				{
					_tcscpy( g_aSourceFileName, pFileName );

					if (! ParseAssemblyListing( g_bSourceAddMemory, g_bSourceAddSymbols ))
					{
						const int MAX_MINI_FILENAME = 20;
						TCHAR sMiniFileName[ MAX_MINI_FILENAME + 1 ];
						_tcsncpy( sMiniFileName, pFileName, MAX_MINI_FILENAME - 1 );
						sMiniFileName[ MAX_MINI_FILENAME ] = 0;

						wsprintf( sFileName, "Couldn't load filename: %s", sMiniFileName );
						ConsoleBufferPush( sFileName );
					}
					else
					{
						TCHAR sText[ CONSOLE_WIDTH ];
						wsprintf( sFileName, "  Read: %d lines, %d symbols", g_nSourceAssemblyLines, g_nSourceAssemblySymbols );
						
						if (g_nSourceAssembleBytes)
						{
							wsprintf( sText, ", %d bytes", g_nSourceAssembleBytes );
							_tcscat( sFileName, sText );
						}
						ConsoleBufferPush( sFileName );
					}
				}
			}
		}
		return ConsoleUpdate();
	}

	return UPDATE_CONSOLE_DISPLAY;	
}

//===========================================================================
Update_t CmdSync (int nArgs)
{
	// TODO
	return UPDATE_CONSOLE_DISPLAY;	
}


// Stack __________________________________________________________________________________________


//===========================================================================
Update_t CmdStackPush (int nArgs)
{
	return UPDATE_CONSOLE_DISPLAY;
}

//===========================================================================
Update_t CmdStackPop (int nArgs)
{
	return UPDATE_CONSOLE_DISPLAY;
}

//===========================================================================
Update_t CmdStackPopPseudo (int nArgs)
{
	return UPDATE_CONSOLE_DISPLAY;
}

// Symbols ________________________________________________________________________________________


//===========================================================================
Update_t CmdSymbols (int nArgs)
{
	if (! nArgs)
		return CmdSymbolsInfo( 0 );

	Update_t iUpdate = _CmdSymbolsUpdate( nArgs);
	if (iUpdate != UPDATE_NOTHING)
		return iUpdate;

//	return CmdSymbolsList( nArgs );
	int bSymbolTables = SYMBOL_TABLE_MAIN | SYMBOL_TABLE_USER | SYMBOL_TABLE_SRC;
	return _CmdSymbolsListTables( nArgs, bSymbolTables );
}

//===========================================================================
Update_t CmdSymbolsClear (int nArgs)
{
	Symbols_e eSymbolsTable = SYMBOLS_USER;	
	_CmdSymbolsClear( eSymbolsTable );
	return (UPDATE_DISASM | UPDATE_SYMBOLS);
}

//===========================================================================
Update_t CmdSymbolsInfo (int nArgs)
{
	TCHAR sText[ CONSOLE_WIDTH ];

	bool bDisplayMain = false;
	bool bDisplayUser = false;
	bool bDisplaySrc  = false;

	if (! nArgs)
	{
		bDisplayMain = true;
		bDisplayUser = true;
		bDisplaySrc  = true;
	}
	else
	if (CMD_SYMBOLS_MAIN == g_iCommand)
		bDisplayMain = true;
	else
	if (CMD_SYMBOLS_USER == g_iCommand)
		bDisplayUser = true;
	else
	if (CMD_SYMBOLS_SRC == g_iCommand)
		bDisplaySrc = true;

	int nSymbolsMain = g_aSymbols[SYMBOLS_MAIN].size();
	int nSymbolsUser = g_aSymbols[SYMBOLS_USER].size();
	int nSymbolsSrc  = g_aSymbols[SYMBOLS_SRC ].size();

	if (bDisplayMain && bDisplayUser && bDisplaySrc)
	{
		wsprintf( sText, "  Symbols  Main: %d  User: %d   Source: %d",
			nSymbolsMain, nSymbolsUser, nSymbolsSrc );
		ConsoleBufferPush( sText );
	}
	else
	if (bDisplayMain)
	{
		wsprintf( sText, "  Main symbols: %d", nSymbolsMain ); ConsoleBufferPush( sText );
	}
	else
	if (bDisplayUser)
	{
		wsprintf( sText, "  User symbols: %d", nSymbolsUser ); ConsoleBufferPush( sText );
	}
	else
	if (bDisplaySrc)
	{
		wsprintf( sText, "  Source symbols: %d", nSymbolsSrc ); ConsoleBufferPush( sText );
	}
	
	if (bDisplayMain || bDisplayUser || bDisplaySrc)
		return ConsoleUpdate();

	return UPDATE_CONSOLE_DISPLAY;
}

void _CmdPrintSymbol( LPCTSTR pSymbol, WORD nAddress, int iTable )
{
	TCHAR   sText[ CONSOLE_WIDTH ];
	wsprintf( sText, "  $%04X (%s) %s", nAddress, g_aSymbolTableNames[ iTable ], pSymbol );
	ConsoleBufferPush( sText );
}



//=========================================================================== */
bool _FindSymbolTable( int bSymbolTables, int iTable )
{
	// iTable is enumeration
	// bSymbolTables is bit-flags of enabled tables to search

	if (bSymbolTables & SYMBOL_TABLE_MAIN)
		if (iTable == SYMBOLS_MAIN)
			return true;

	if (bSymbolTables & SYMBOL_TABLE_USER)
		if (iTable == SYMBOLS_USER)
			return true;

	if (bSymbolTables & SYMBOL_TABLE_SRC )
		if (iTable == SYMBOLS_SRC)
			return true;

	return false;
}


//=========================================================================== */
int _GetSymbolTableFromFlag( int bSymbolTables )
{
	int iTable = NUM_SYMBOL_TABLES;
	
	if (bSymbolTables & SYMBOL_TABLE_MAIN)
		iTable = SYMBOLS_MAIN;
	else
	if (bSymbolTables & SYMBOL_TABLE_USER)
		iTable = SYMBOLS_USER;
	else
	if (bSymbolTables & SYMBOL_TABLE_SRC )
		iTable = SYMBOLS_SRC;

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


//===========================================================================
bool String2Address( LPCTSTR pText, WORD & nAddress_ )
{
	TCHAR sHexApple[ CONSOLE_WIDTH ];

	if (pText[0] == TEXT('$'))
	{
		if (!IsHexString( pText+1))
			return false;

		_tcscpy( sHexApple, TEXT("0x") );
		_tcsncpy( sHexApple+2, pText+1, MAX_SYMBOLS_LEN - 3 );
		pText = sHexApple;
	}

	if (pText[0] == TEXT('0'))
	{
		if ((pText[1] == TEXT('X')) || pText[1] == TEXT('x'))
		{
			if (!IsHexString( pText+2))
				return false;

			TCHAR *pEnd;
			nAddress_ = (WORD) _tcstol( pText, &pEnd, 16 );
			return true;
		}
		if (IsHexString( pText ))
		{
			TCHAR *pEnd;
			nAddress_ = (WORD) _tcstol( pText, &pEnd, 16 );
			return true;
		}
	}

	return false;
}		


// LIST is normally an implicit "LIST *", but due to the numbers of symbols
// only look up symbols the user specifies
//===========================================================================
Update_t CmdSymbolsList (int nArgs )
{
	int bSymbolTables = SYMBOL_TABLE_MAIN | SYMBOL_TABLE_USER | SYMBOL_TABLE_SRC;
	return _CmdSymbolsListTables( nArgs, bSymbolTables );
}


//===========================================================================
Update_t _CmdSymbolsListTables (int nArgs, int bSymbolTables)
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
		
	TCHAR sText[ CONSOLE_WIDTH ];

	for( int iArgs = 1; iArgs <= nArgs; iArgs++ )
	{
		WORD nAddress = g_aArgs[iArgs].nVal1;
		LPCTSTR pSymbol = g_aArgs[iArgs].sArg;
		if (nAddress)
		{	// Have address, do symbol lookup first
			if (! _CmdSymbolList_Symbol2Address( pSymbol, bSymbolTables ))
			{
				// nope, ok, try as address
				if (! _CmdSymbolList_Address2Symbol( nAddress, bSymbolTables))
				{
					wsprintf( sText, TEXT(" Address not found: %04X" ), nAddress );
					ConsoleBufferPush( sText );
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
						wsprintf( sText, TEXT(" Symbol not found: %s"), pSymbol );
						ConsoleBufferPush( sText );
					}
				}
				else
				{
					wsprintf( sText, TEXT(" Symbol not found: %s"), pSymbol );
					ConsoleBufferPush( sText );
				}
			}
		}
	}
	return ConsoleUpdate();
}

//===========================================================================
int ParseSymbolTable( TCHAR *pFileName, Symbols_e eWhichTableToLoad )
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
	while(hFile && !feof(hFile))
	{
		// Support 2 types of symbols files:
		// 1) AppleWin:
		//    . 0000 SYMBOL
		//    . FFFF SYMBOL
		// 2) ACME:
		//    . SYMBOL  =$0000; Comment
		//    . SYMBOL  =$FFFF; Comment
		//
		DWORD INVALID_ADDRESS = 0xFFFF + 1;
		DWORD nAddress = INVALID_ADDRESS;
		char  sName[ MAX_SYMBOLS_LEN+1 ]  = "";

		const int MAX_LINE = 256;
		char  szLine[ MAX_LINE ];
		fgets(szLine, MAX_LINE-1, hFile);	// Get next line

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
				memset(&szLine[MAX_SYMBOLS_LEN], ' ', nLen-MAX_SYMBOLS_LEN);	// sscanf fails for nAddress if string too long

			sscanf(szLine, sFormat2, sName, &nAddress);
		}

		if( (nAddress >= INVALID_ADDRESS) || (sName[0] == 0) )
			continue;

		g_aSymbols[ eWhichTableToLoad ] [ (WORD) nAddress ] = sName;
		nSymbolsLoaded++;
	}

	if(hFile)
		fclose(hFile);

	return nSymbolsLoaded;
}


//===========================================================================
Update_t CmdSymbolsLoad (int nArgs)
{
	TCHAR sFileName[MAX_PATH];
	_tcscpy(sFileName,progdir);

	int iWhichTable = (g_iCommand - CMD_SYMBOLS_MAIN);
	if ((iWhichTable < 0) || (iWhichTable >= NUM_SYMBOL_TABLES))
	{
		wsprintf( sFileName, "Only %d symbol tables supported!", NUM_SYMBOL_TABLES );
		return ConsoleDisplayError( sFileName );
	}

	if (! nArgs)
	{
		// Default to main table
		if (g_iCommand == CMD_SYMBOLS_MAIN)
			_tcscat(sFileName, g_FileNameSymbolsMain );
		else
		{
			if (! _tcslen( g_FileNameSymbolsUser ))
			{
				return ConsoleDisplayError(TEXT("No user symbol file to reload."));
			}
			// load user symbols
			_tcscat( sFileName, g_FileNameSymbolsUser );
		}
		g_nSymbolsLoaded = ParseSymbolTable( sFileName, (Symbols_e) iWhichTable );

		return (UPDATE_DISASM || UPDATE_SYMBOLS);
	}

	int iArg = 0;
	while (iArg++ <= nArgs)
	{
		TCHAR *pFileName = g_aArgs[ iArg ].sArg;

		_tcscpy(sFileName,progdir);
		_tcscat(sFileName, pFileName);

		// Remember File ame of symbols loaded
		_tcscpy( g_FileNameSymbolsUser, pFileName );

		g_nSymbolsLoaded = ParseSymbolTable( sFileName, (Symbols_e) iWhichTable );
	}

	return (UPDATE_DISASM || UPDATE_SYMBOLS);
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
Update_t _CmdSymbolsClear( Symbols_e eSymbolTable )
{
	g_aSymbols[ eSymbolTable ].clear();
	
	return UPDATE_SYMBOLS;
}


//===========================================================================
Update_t _CmdSymbolsUpdate( int nArgs )
{
	bool bRemoveSymbol = false;
	bool bUpdateSymbol = false;

	if ((nArgs == 2) && (g_aArgs[ 1 ].eToken == TOKEN_EXCLAMATION))
		bRemoveSymbol = true;

	if ((nArgs == 3) && (g_aArgs[ 2 ].eToken == TOKEN_EQUAL      ))
		bUpdateSymbol = true;

	if (bRemoveSymbol || bUpdateSymbol)
	{
		TCHAR *pSymbolName = g_aArgs[1].sArg;
		WORD   nAddress    = g_aArgs[3].nVal1;

		if (bRemoveSymbol)
			pSymbolName = g_aArgs[2].sArg;

		if (_tcslen( pSymbolName ) < MAX_SYMBOLS_LEN)
		{
			WORD nAddressPrev;
			int  iTable;
			bool bExists = FindAddressFromSymbol( pSymbolName, &nAddressPrev, &iTable );

			if (bExists)
			{
				if (iTable == SYMBOLS_USER)
				{
					if (bRemoveSymbol)
					{
						ConsoleBufferPush( TEXT(" Removing symbol." ) );
					}

					g_aSymbols[SYMBOLS_USER].erase( nAddressPrev );

					if (bUpdateSymbol)
					{
						ConsoleBufferPush( TEXT(" Updating symbol to new address." ) );
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
				g_aSymbols[SYMBOLS_USER][ nAddress ] = pSymbolName;
			}

			return ConsoleUpdate();
		}
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

	Update_t iUpdate = _CmdSymbolsUpdate( nArgs );
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
					Update_t iUpdate = _CmdSymbolsClear( (Symbols_e) iTable );
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
				CmdSymbolsLoad( nArgs );

				int iTable = _GetSymbolTableFromFlag( bSymbolTables );
				if (iTable != NUM_SYMBOL_TABLES)
				{
					wsprintf( sText, "  Symbol Table: %s, loaded symbols: %d",
						g_aSymbolTableNames[ iTable ], g_nSymbolsLoaded );
					ConsoleBufferPush( sText );
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
			return _CmdSymbolsListTables( nArgs, bSymbolTables );
		}

	}

	return ConsoleUpdate();
}

//===========================================================================
Update_t CmdSymbolsMain (int nArgs)
{
	if (! nArgs)
	{
		return CmdSymbolsInfo( nArgs );
	}

	return _CmdSymbolsCommon( nArgs, SYMBOL_TABLE_MAIN ); // SYMBOLS_MAIN );
}


//===========================================================================
Update_t CmdSymbolsUser (int nArgs)
{
	if (! nArgs)
	{
		return CmdSymbolsInfo( nArgs );
	}

	return _CmdSymbolsCommon( nArgs, SYMBOL_TABLE_USER ); // SYMBOLS_USER );
}

//===========================================================================
Update_t CmdSymbolsSource (int nArgs)
{
	if (! nArgs)
	{
		return CmdSymbolsInfo( nArgs );
	}

	return _CmdSymbolsCommon( nArgs, SYMBOL_TABLE_SRC ); // SYMBOLS_SRC );
}

//===========================================================================
LPCTSTR GetSymbol (WORD nAddress, int nBytes)
{
	LPCSTR pSymbol = FindSymbolFromAddress( nAddress );
	if (pSymbol)
		return pSymbol;

	return FormatAddress( nAddress, nBytes );
}

//===========================================================================
Update_t CmdSymbolsSave (int nArgs)
{
	return UPDATE_CONSOLE_DISPLAY;
}


// CPU Step, Trace ________________________________________________________________________________

//===========================================================================
Update_t CmdGo (int nArgs)
{
	g_nDebugSteps = -1;
	g_nDebugStepCycles  = 0;
	g_nDebugStepStart = regs.pc;
	g_nDebugStepUntil = nArgs ? g_aArgs[1].nVal1 : -1;
	g_nDebugSkipStart = -1;
	g_nDebugSkipLen   = -1;

	// Extended Go ... addr_skip, len
	if (nArgs > 1)
	{
		int iArg = 2;
		
		g_nDebugSkipStart = g_aArgs[ iArg ].nVal1;
		WORD nAddress     = g_aArgs[ iArg ].nVal2;

//		int nSlack        = (_6502_END_MEM_ADDRESS + 1) - nAddress;
//		int nWantedLength = g_aArgs[iArg].nVal2;
//		g_nDebugSkipLen   = MIN( nSlack, nWantedLength );
		int nEnd = nAddress - g_nDebugSkipStart;
		if (nEnd < 0)
			g_nDebugSkipLen = g_nDebugSkipStart - nAddress;
		else
			g_nDebugSkipLen = nAddress - g_nDebugSkipStart;

		g_nDebugSkipLen &= _6502_END_MEM_ADDRESS;			
	}

//	WORD nAddressSymbol = 0;
//	bool bFoundSymbol = FindAddressFromSymbol( g_aArgs[1].sArg, & nAddressSymbol );
//	if (bFoundSymbol)
//		g_nDebugStepUntil = nAddressSymbol;

//  if (!g_nDebugStepUntil)
//    g_nDebugStepUntil = GetAddress(g_aArgs[1].sArg);

	mode = MODE_STEPPING;
	FrameRefreshStatus(DRAW_TITLE);

	return UPDATE_CONSOLE_DISPLAY; // TODO: Verify // 0;
}

//===========================================================================
Update_t CmdStepOver (int nArgs)
{
	// assert( g_nDisasmCurAddress == regs.pc );

//	g_nDebugSteps = nArgs ? g_aArgs[1].nVal1 : 1;
	WORD nDebugSteps = nArgs ? g_aArgs[1].nVal1 : 1;

	while (nDebugSteps -- > 0)
	{
		int nOpcode = *(mem + regs.pc); // g_nDisasmCurAddress
	//	int eMode = g_aOpcodes[ nOpcode ].addrmode;
	//	int nByte = g_aOpmodes[eMode]._nBytes;
	//	if ((eMode ==  ADDR_ABS) && 

		CmdTrace(0);
		if (nOpcode == OPCODE_JSR)
		{
			CmdStepOut(0);
			g_nDebugSteps = 0xFFFF;
			while (g_nDebugSteps != 0)
				DebugContinueStepping();
		}
	}

	return UPDATE_ALL;
}

//===========================================================================
Update_t CmdStepOut (int nArgs)
{
	// TODO: "RET" should probably pop the Call stack
	// Also see: CmdCursorJumpRetAddr
	WORD nAddress;
	if (_Get6502ReturnAddress( nAddress ))
	{
		nArgs = _Arg_1( nAddress );
		g_aArgs[1].sArg[0] = 0;
		CmdGo( 1 );
	}

	return UPDATE_ALL;
}

//===========================================================================
Update_t CmdTrace (int nArgs)
{
	g_nDebugSteps = nArgs ? g_aArgs[1].nVal1 : 1;
	g_nDebugStepCycles  = 0;
	g_nDebugStepStart = regs.pc;
	g_nDebugStepUntil = -1;
	mode = MODE_STEPPING;
	FrameRefreshStatus(DRAW_TITLE);
	DebugContinueStepping();

	return UPDATE_ALL; // TODO: Verify // 0
}

//===========================================================================
Update_t CmdTraceFile (int nArgs) {
	if (g_hTraceFile)
		fclose(g_hTraceFile);

	TCHAR filename[MAX_PATH];
	_tcscpy(filename,progdir);
	_tcscat(filename,(nArgs && g_aArgs[1].sArg[0]) ? g_aArgs[1].sArg : g_FileNameTrace );
	g_hTraceFile = fopen(filename,TEXT("wt"));

	return UPDATE_ALL; // TODO: Verify // 0
}

//===========================================================================
Update_t CmdTraceLine (int nArgs)
{
	g_nDebugSteps = nArgs ? g_aArgs[1].nVal1 : 1;
	g_nDebugStepCycles  = 1;
	g_nDebugStepStart = regs.pc;
	g_nDebugStepUntil = -1;

	mode = MODE_STEPPING;
	FrameRefreshStatus(DRAW_TITLE);
	DebugContinueStepping();

	return UPDATE_ALL; // TODO: Verify // 0
}


// Variables ______________________________________________________________________________________


//===========================================================================
Update_t CmdVarsClear (int nArgs)
{
	return UPDATE_CONSOLE_DISPLAY;
}

//===========================================================================
Update_t CmdVarsDefine (int nArgs)
{
	return UPDATE_CONSOLE_DISPLAY;
}

//===========================================================================
Update_t CmdVarsDefineInt8 (int nArgs)
{
	return UPDATE_CONSOLE_DISPLAY;
}

//===========================================================================
Update_t CmdVarsDefineInt16 (int nArgs)
{
	return UPDATE_CONSOLE_DISPLAY;
}

//===========================================================================
Update_t CmdVarsList (int nArgs)
{
	return UPDATE_CONSOLE_DISPLAY;
}

//===========================================================================
Update_t CmdVarsLoad        (int nArgs)
{
	return UPDATE_CONSOLE_DISPLAY;
}

//===========================================================================
Update_t CmdVarsSave        (int nArgs)
{
	return UPDATE_CONSOLE_DISPLAY;
}

//===========================================================================
Update_t CmdVarsSet         (int nArgs)
{
	return UPDATE_CONSOLE_DISPLAY;
}


// Watches ________________________________________________________________________________________

//===========================================================================
Update_t CmdWatchAdd (int nArgs)
{
	if (!nArgs)
		return Help_Arg_1( CMD_WATCH_ADD );

	bool bAdded = false;
	int  iArg = 0;

	while (iArg++ < nArgs)
	{
		WORD nAddress = g_aArgs[iArg].nVal1;
		{
			// FIND A FREE SLOT FOR THIS NEW WATCH
			int iWatch = 0;
			while ((iWatch < MAX_WATCHES) && (g_aWatches[iWatch].bSet))
				iWatch++;
			if ((iWatch >= MAX_WATCHES) && !bAdded)
				return ConsoleDisplayError(TEXT("All watches slots are currently in use."));

			// VERIFY THAT THE WATCH IS NOT ON AN I/O LOCATION
			if ((nAddress >= _6502_IO_BEGIN) && (nAddress <= _6502_IO_END))
				return ConsoleDisplayError(TEXT("You may not watch an I/O location."));

			// ADD THE WATCH
			if (iWatch < MAX_WATCHES) {
				g_aWatches[iWatch].bSet = true;
				g_aWatches[iWatch].bEnabled = true;
				g_aWatches[iWatch].nAddress = nAddress;
				bAdded = true;
				g_nWatches++;
			}
		}
	}

	if (!bAdded)
		return Help_Arg_1( CMD_WATCH_ADD );

  return UPDATE_WATCH; // 1;
}

//===========================================================================
Update_t CmdWatchClear (int nArgs)
{
	if (!g_nWatches)
		return ConsoleDisplayError(TEXT("There are no watches defined."));

	if (!nArgs)
		return Help_Arg_1( CMD_WATCH_CLEAR );	

	_ClearViaArgs( nArgs, g_aWatches, MAX_WATCHES, g_nWatches );

//	if (! g_nWatches)
//	{
//		UpdateDisplay(UPDATE_BACKGROUND); // 1
//		return UPDATE_NOTHING; // 0
//	}

    return UPDATE_CONSOLE_DISPLAY | UPDATE_WATCH; // 1
}

//===========================================================================
Update_t CmdWatchDisable (int nArgs)
{
	if (! g_nWatches)
		return ConsoleDisplayError(TEXT("There are no watches defined."));

	if (!nArgs)
		return Help_Arg_1( CMD_WATCH_DISABLE );

	_EnableDisableViaArgs( nArgs, g_aWatches, MAX_WATCHES, false );

	return UPDATE_WATCH;
}

//===========================================================================
Update_t CmdWatchEnable (int nArgs)
{
	if (! g_nWatches)
		return ConsoleDisplayError(TEXT("There are no watches defined."));

	if (!nArgs)
		return Help_Arg_1( CMD_WATCH_ENABLE );

	_EnableDisableViaArgs( nArgs, g_aWatches, MAX_WATCHES, true );

	return UPDATE_WATCH;
}

//===========================================================================
Update_t CmdWatchList (int nArgs)
{
	int iWatch = 0;
	while (iWatch < MAX_WATCHES)
	{
		if (g_aWatches[ iWatch ].bSet)
		{
			_ListBreakWatchZero( g_aWatches, iWatch );
		}
		iWatch++;
	}
	return ConsoleUpdate();
}

//===========================================================================
Update_t CmdWatchLoad (int nArgs)
{
	if (!nArgs)
		return Help_Arg_1( CMD_WATCH_LOAD );

	return UPDATE_CONSOLE_DISPLAY;
}

//===========================================================================
Update_t CmdWatchSave (int nArgs)
{
	if (!nArgs)
		return Help_Arg_1( CMD_WATCH_SAVE );

	return UPDATE_CONSOLE_DISPLAY;
}


// Window _________________________________________________________________________________________

void _ColorPrint( int iColor, COLORREF nColor )
{
	int R = (nColor >>  0) & 0xFF;
	int G = (nColor >>  8) & 0xFF;
	int B = (nColor >> 16) & 0xFF;

	TCHAR sText[ CONSOLE_WIDTH ];
	wsprintf( sText, " Color %01X: %02X %02X %02X", iColor, R, G, B ); // TODO: print name of colors!
	ConsoleBufferPush( sText );
}

void _CmdColorGet( const int iScheme, const int iColor )
{
	if (iColor < NUM_COLORS)
	{
//	COLORREF nColor = g_aColors[ iScheme ][ iColor ];
		DebugColors_e eColor = static_cast<DebugColors_e>( iColor );
		COLORREF nColor = DebugGetColor( eColor );
		_ColorPrint( iColor, nColor );
	}
	else
	{
		TCHAR sText[ CONSOLE_WIDTH ];
		wsprintf( sText, "Color: %d\nOut of range!", iColor );
		MessageBox( framewindow, sText, TEXT("ERROR"), MB_OK );
	}
}

//===========================================================================
inline COLORREF DebugGetColor( int iColor )
{
	COLORREF nColor = RGB(0,255,255); // 0xFFFF00; // Hot Pink! -- so we notice errors. Not that there is anything wrong with pink...

	if ((g_iColorScheme < NUM_COLOR_SCHEMES) && (iColor < NUM_COLORS))
	{
		nColor = g_aColors[ g_iColorScheme ][ iColor ];
	}

/*
	if (SCHEME_COLOR == g_iColorScheme)
//		nColor = gaColorValue[ iColor ];
//		nColor = gaColorPalette[ g_aColorIndex[ iColor ] ];
	else
	if (SCHEME_MONO == g_iColorScheme)
		nColor = gaMonoValue[ iColor ];
//			nColor = gaMonoPalette[ g_aColorIndex[ iColor ] ];
*/
	return nColor;
}


bool DebugSetColor( const int iScheme, const int iColor, const COLORREF nColor )
{
	bool bStatus = false;
	if ((g_iColorScheme < NUM_COLOR_SCHEMES) && (iColor < NUM_COLORS))
	{
		g_aColors[ iScheme ][ iColor ] = nColor;
		bStatus = true;
	}
	return bStatus;
}

//===========================================================================
Update_t CmdConfigColorMono (int nArgs)
{
	int iScheme;
	
	if (g_iCommand == CMD_CONFIG_COLOR)
		iScheme = SCHEME_COLOR;
	if (g_iCommand == CMD_CONFIG_MONOCHROME)
		iScheme = SCHEME_MONO;

	if ((iScheme < 0) || (iScheme > NUM_COLOR_SCHEMES)) // sanity check
		iScheme = SCHEME_COLOR;

	if (! nArgs)
	{
		g_iColorScheme = iScheme;
		UpdateDisplay( UPDATE_BACKGROUND ); // 1
		return UPDATE_ALL;
	}

	if ((nArgs != 1) && (nArgs != 4))
		return Help_Arg_1( g_iCommand );

	int iColor = g_aArgs[ 1 ].nVal1;
	if ((iColor < 0) || iColor >= NUM_COLORS)
		return Help_Arg_1( g_iCommand );

	if (nArgs == 1)
	{	// Dump Color
		_CmdColorGet( iScheme, iColor );
		return ConsoleUpdate();
	}
	else
	{	// Set Color
		int R = g_aArgs[2].nVal1 & 0xFF;
		int G = g_aArgs[3].nVal1 & 0xFF;
		int B = g_aArgs[4].nVal1 & 0xFF;
		COLORREF nColor = RGB(R,G,B);

		DebugSetColor( iScheme, iColor, nColor );
		return UPDATE_ALL;
	}
}

Update_t CmdConfigHColor (int nArgs)
{
	if ((nArgs != 1) && (nArgs != 4))
		return Help_Arg_1( g_iCommand );

	int iColor = g_aArgs[ 1 ].nVal1;
	if ((iColor < 0) || iColor >= NUM_COLORS)
		return Help_Arg_1( g_iCommand );

	if (nArgs == 1)
	{	// Dump Color
//		_CmdColorGet( iScheme, iColor );
// TODO/FIXME: must export AW_Video.cpp: static LPBITMAPINFO  framebufferinfo;
//		COLORREF nColor = g_aColors[ iScheme ][ iColor ];
//		_ColorPrint( iColor, nColor );
		return ConsoleUpdate();
	}
	else
	{	// Set Color
//		DebugSetColor( iScheme, iColor );
		return UPDATE_ALL;
	}
}

// Window _________________________________________________________________________________________

//===========================================================================
void _WindowJoin ()
{
	g_aWindowConfig[ g_iWindowThis ].bSplit = false;
}

//===========================================================================
void _WindowSplit ( Window_e eNewBottomWindow )
{
	g_aWindowConfig[ g_iWindowThis ].bSplit = true;
	g_aWindowConfig[ g_iWindowThis ].eBot = eNewBottomWindow;
}

//===========================================================================
void _WindowLast ()
{
	int eNew = g_iWindowLast;
	g_iWindowLast = g_iWindowThis;
	g_iWindowThis = eNew;
}

//===========================================================================
void _WindowSwitch( int eNewWindow )
{
	g_iWindowLast = g_iWindowThis;
	g_iWindowThis = eNewWindow;
}

//===========================================================================
Update_t _CmdWindowViewCommon ( int iNewWindow )
{
	// Switching to same window, remove split
	if (g_iWindowThis == iNewWindow)
	{
		g_aWindowConfig[ iNewWindow ].bSplit = false;
	}
	else
	{
		_WindowSwitch( iNewWindow ); 
	}

//	WindowUpdateConsoleDisplayedSize();
	WindowUpdateSizes();
	return UPDATE_ALL;
}

//===========================================================================
Update_t _CmdWindowViewFull ( int iNewWindow )
{
	if (g_iWindowThis != iNewWindow)
	{
		g_aWindowConfig[ iNewWindow ].bSplit = false;
		_WindowSwitch( iNewWindow ); 
		WindowUpdateConsoleDisplayedSize();
	}
	return UPDATE_ALL;
}

//===========================================================================
void WindowUpdateConsoleDisplayedSize()
{
	g_nConsoleDisplayHeight = MIN_DISPLAY_CONSOLE_LINES;
	g_nConsoleDisplayWidth = (CONSOLE_WIDTH / 2) + 8;
	g_bConsoleFullWidth = false;

	if (g_iWindowThis == WINDOW_CONSOLE)
	{
		g_nConsoleDisplayHeight = MAX_DISPLAY_CONSOLE_LINES;
		g_nConsoleDisplayWidth = CONSOLE_WIDTH - 1;
		g_bConsoleFullWidth = true;
	}
}

//===========================================================================
int WindowGetHeight( int iWindow )
{
//	if (iWindow == WINDOW_CODE)
	return g_nDisasmWinHeight;
}

//===========================================================================
void WindowUpdateDisasmSize()
{
	if (g_aWindowConfig[ g_iWindowThis ].bSplit)
	{
		g_nDisasmWinHeight = (MAX_DISPLAY_DISASM_LINES) / 2;
	}
	else
	{
		g_nDisasmWinHeight = MAX_DISPLAY_DISASM_LINES;
	}
	g_nDisasmCurLine = MAX(0, (g_nDisasmWinHeight - 1) / 2);
#if _DEBUG
#endif
}

//===========================================================================
void WindowUpdateSizes()
{
	WindowUpdateDisasmSize();
	WindowUpdateConsoleDisplayedSize();
}


//===========================================================================
Update_t CmdWindowCycleNext( int nArgs )
{
	g_iWindowThis++;
	if (g_iWindowThis >= NUM_WINDOWS)
		g_iWindowThis = 0;

	WindowUpdateSizes();

	return UPDATE_ALL;
}

//===========================================================================
Update_t CmdWindowCyclePrev( int nArgs )
{
	g_iWindowThis--;
	if (g_iWindowThis < 0)
		g_iWindowThis = NUM_WINDOWS-1;

	WindowUpdateSizes();

	return UPDATE_ALL;
}

//===========================================================================
Update_t CmdWindowShowCode (int nArgs)
{
	if (g_iWindowThis == WINDOW_CODE)
	{
		g_aWindowConfig[ g_iWindowThis ].bSplit = false;
		g_aWindowConfig[ g_iWindowThis ].eBot = WINDOW_CODE; // not really needed, but SAFE HEX ;-)
	}
	else	
	if (g_iWindowThis == WINDOW_DATA)
	{
		g_aWindowConfig[ g_iWindowThis ].bSplit = true;
		g_aWindowConfig[ g_iWindowThis ].eBot = WINDOW_CODE;
	}

	WindowUpdateSizes();

	return UPDATE_CONSOLE_DISPLAY;
}

//===========================================================================
Update_t CmdWindowShowCode1 (int nArgs)
{
/*
	if ((g_iWindowThis == WINDOW_CODE) || (g_iWindowThis != WINDOW_CODE))
	{
		g_aWindowConfig[ g_iWindowThis ].bSplit = true;
		g_aWindowConfig[ g_iWindowThis ].eTop = WINDOW_CODE;
		
		Window_e eWindow = WINDOW_CODE;
		if (g_iWindowThis == WINDOW_DATA)
			eWindow = WINDOW_DATA;

		g_aWindowConfig[ g_iWindowThis ].eBot = eWindow;
		return UPDATE_ALL;
	}
*/
	return UPDATE_CONSOLE_DISPLAY;
}

//===========================================================================
Update_t CmdWindowShowCode2 (int nArgs)
{
	if ((g_iWindowThis == WINDOW_CODE) || (g_iWindowThis == WINDOW_CODE))
	{
		if (g_iWindowThis == WINDOW_CODE)
		{
			_WindowJoin();
			WindowUpdateDisasmSize();
		}
		else		
		if (g_iWindowThis == WINDOW_DATA)
		{
			_WindowSplit( WINDOW_CODE );
			WindowUpdateDisasmSize();
		}		
		return UPDATE_DISASM;

	}
	return UPDATE_CONSOLE_DISPLAY;
}


//===========================================================================
Update_t CmdWindowShowData (int nArgs)
{
	if (g_iWindowThis == WINDOW_CODE)
	{
		g_aWindowConfig[ g_iWindowThis ].bSplit = true;
		g_aWindowConfig[ g_iWindowThis ].eBot = WINDOW_DATA;
		return UPDATE_ALL;
	}
	else
	if (g_iWindowThis == WINDOW_DATA)
	{
		g_aWindowConfig[ g_iWindowThis ].bSplit = false;
		g_aWindowConfig[ g_iWindowThis ].eBot = WINDOW_DATA; // not really needed, but SAFE HEX ;-)
		return UPDATE_ALL;
	}

	return UPDATE_CONSOLE_DISPLAY;
}


//===========================================================================
Update_t CmdWindowShowData1 (int nArgs)
{
/*
	if (g_iWindowThis != PARAM_CODE_1)
	{
		g_iWindowLast = g_iWindowThis;
		g_iWindowThis = PARAM_DATA_1;
		return UPDATE_ALL;
	}
*/
	return UPDATE_CONSOLE_DISPLAY;
}

//===========================================================================
Update_t CmdWindowShowData2 (int nArgs)
{
	if ((g_iWindowThis == WINDOW_CODE) || (g_iWindowThis == WINDOW_CODE))
	{
		if (g_iWindowThis == WINDOW_CODE)
		{
			_WindowJoin();
		}
		else		
		if (g_iWindowThis == WINDOW_DATA)
		{
			_WindowSplit( WINDOW_DATA );
		}		
		return UPDATE_DISASM;

	}
	return UPDATE_CONSOLE_DISPLAY;
}

//===========================================================================
Update_t CmdWindowShowSource (int nArgs)
{
	return UPDATE_CONSOLE_DISPLAY;
}


//===========================================================================
Update_t CmdWindowShowSource1 (int nArgs)
{
	return UPDATE_CONSOLE_DISPLAY;
}

//===========================================================================
Update_t CmdWindowShowSource2 (int nArgs)
{
	_WindowSplit( WINDOW_SOURCE );
	WindowUpdateSizes();

	return UPDATE_CONSOLE_DISPLAY;
}

//===========================================================================
Update_t CmdWindowViewCode (int nArgs)
{
	return _CmdWindowViewCommon( WINDOW_CODE );
}

//===========================================================================
Update_t CmdWindowViewConsole (int nArgs)
{
	return _CmdWindowViewFull( WINDOW_CONSOLE );
	return UPDATE_ALL;
}

//===========================================================================
Update_t CmdWindowViewData (int nArgs)
{
	return _CmdWindowViewCommon( WINDOW_DATA );
}

//===========================================================================
Update_t CmdWindowViewOutput (int nArgs)
{
	VideoRedrawScreen();
	g_bDebuggerViewingAppleOutput = true;

	return UPDATE_NOTHING; // intentional
}

//===========================================================================
Update_t CmdWindowViewSource (int nArgs)
{
	return _CmdWindowViewFull( WINDOW_CONSOLE );
	return UPDATE_ALL;
}

//===========================================================================
Update_t CmdWindowViewSymbols (int nArgs)
{
	return _CmdWindowViewFull( WINDOW_CONSOLE );
	return UPDATE_ALL;
}

//===========================================================================
Update_t CmdWindow (int nArgs)
{
	if (!nArgs)
		return Help_Arg_1( CMD_WINDOW );

	int iParam;
	TCHAR *pName = g_aArgs[1].sArg;
	int nFound = FindParam( pName, MATCH_EXACT, iParam, _PARAM_WINDOW_BEGIN, _PARAM_WINDOW_END );
	if (nFound)
	{
		switch (iParam)
		{
			case PARAM_CODE   : return CmdWindowViewCode(0)   ; break;
			case PARAM_CONSOLE: return CmdWindowViewConsole(0); break;
			case PARAM_DATA   : return CmdWindowViewData(0)   ; break;
//			case PARAM_INFO   : CmdWindowInfo(); break;
			case PARAM_SOURCE : return CmdWindowViewSource(0) ; break;
			case PARAM_SYMBOLS: return CmdWindowViewSymbols(0); break;
			default:
				return Help_Arg_1( CMD_WINDOW );
				break;
		}
	}	

	WindowUpdateConsoleDisplayedSize();

	return UPDATE_ALL;
}

//===========================================================================
Update_t CmdWindowLast (int nArgs)
{
	_WindowLast();
	WindowUpdateConsoleDisplayedSize();
	return UPDATE_ALL;
}

// ZeroPage _______________________________________________________________________________________


//===========================================================================
Update_t CmdZeroPage        (int nArgs)
{
	if (!nArgs)
		return Help_Arg_1( CMD_ZEROPAGE_POINTER );

	return UPDATE_CONSOLE_DISPLAY;
}

//===========================================================================
Update_t CmdZeroPageAdd     (int nArgs)
{
	if (!nArgs)
		return Help_Arg_1( CMD_ZEROPAGE_POINTER_ADD );

	bool bAdded = false;
	int  iArg = 0;
	while (iArg++ < nArgs)
	{
		WORD nAddress = g_aArgs[iArg].nVal1;
		{
			int iZP = 0;
			while ((iZP < MAX_ZEROPAGE_POINTERS) && (g_aZeroPagePointers[iZP].bSet))
			{
				iZP++;
			}
			if ((iZP >= MAX_ZEROPAGE_POINTERS) && !bAdded)
				return ConsoleDisplayError(TEXT("All (ZP) pointers are currently in use."));

			if (iZP < MAX_ZEROPAGE_POINTERS)
			{
				g_aZeroPagePointers[iZP].bSet = true;
				g_aZeroPagePointers[iZP].bEnabled = true;
				g_aZeroPagePointers[iZP].nAddress = (BYTE) nAddress;
				bAdded = true;
				g_nZeroPagePointers++;
			}
		}
	}

	if (!bAdded)
		return Help_Arg_1( CMD_ZEROPAGE_POINTER_ADD );

  return UPDATE_ZERO_PAGE;
}

//===========================================================================
Update_t CmdZeroPageClear   (int nArgs)
{
	if (!g_nBreakpoints)
		return ConsoleDisplayError(TEXT("There are no (ZP) pointers defined."));

	// CHECK FOR ERRORS
	if (!nArgs)
		return Help_Arg_1( CMD_ZEROPAGE_POINTER_CLEAR );

	_ClearViaArgs( nArgs, g_aZeroPagePointers, MAX_ZEROPAGE_POINTERS, g_nZeroPagePointers );

	if (! g_nZeroPagePointers)
	{
		UpdateDisplay( UPDATE_BACKGROUND );
		return UPDATE_CONSOLE_DISPLAY;
	}

    return UPDATE_CONSOLE_DISPLAY | UPDATE_ZERO_PAGE;
}

//===========================================================================
Update_t CmdZeroPageDisable   (int nArgs)
{
	if (!nArgs)
		return Help_Arg_1( CMD_ZEROPAGE_POINTER_DISABLE );
	if (! g_nZeroPagePointers)
		return ConsoleDisplayError(TEXT("There are no (ZP) pointers defined."));

	_EnableDisableViaArgs( nArgs, g_aZeroPagePointers, MAX_ZEROPAGE_POINTERS, false );

	return UPDATE_ZERO_PAGE;
}

//===========================================================================
Update_t CmdZeroPageEnable (int nArgs)
{
	if (! g_nZeroPagePointers)
		return ConsoleDisplayError(TEXT("There are no (ZP) pointers defined."));

	if (!nArgs)
		return Help_Arg_1( CMD_ZEROPAGE_POINTER_ENABLE );

	_EnableDisableViaArgs( nArgs, g_aZeroPagePointers, MAX_ZEROPAGE_POINTERS, true );

	return UPDATE_ZERO_PAGE;
}

//===========================================================================
Update_t CmdZeroPageList    (int nArgs)
{
	int iZP = 0;
	while (iZP < MAX_ZEROPAGE_POINTERS)
	{
		if (g_aZeroPagePointers[ iZP ].bEnabled)
		{
			_ListBreakWatchZero( g_aZeroPagePointers, iZP );
		}
		iZP++;
	}
	return ConsoleUpdate();
}

//===========================================================================
Update_t CmdZeroPageLoad    (int nArgs)
{
	return UPDATE_CONSOLE_DISPLAY;

}

//===========================================================================
Update_t CmdZeroPageSave    (int nArgs)
{
	return UPDATE_CONSOLE_DISPLAY;
}


//===========================================================================
Update_t CmdZeroPagePointer (int nArgs)
{
	// p[0..4]                : disable
	// p[0..4] <ZeroPageAddr> : enable

	if( (nArgs != 0) && (nArgs != 1) )
		return Help_Arg_1( g_iCommand );
//		return DisplayHelp(CmdZeroPagePointer);

//	int nPtrNum = g_aArgs[0].sArg[1] - '0'; // HACK: hard-coded to command length
	int iZP = g_iCommand - CMD_ZEROPAGE_POINTER_0;

	if( (iZP < 0) || (iZP > MAX_ZEROPAGE_POINTERS) )
		return Help_Arg_1( g_iCommand );

	if (nArgs == 0)
	{
		g_aZeroPagePointers[iZP].bEnabled = false;
	}
	else
	{
		g_aZeroPagePointers[iZP].bSet = true;
		g_aZeroPagePointers[iZP].bEnabled = true;

		WORD nAddress = g_aArgs[1].nVal1;
		g_aZeroPagePointers[iZP].nAddress = (BYTE) nAddress;
	}

	return UPDATE_ZERO_PAGE;
}




// Drawing ________________________________________________________________________________________

//===========================================================================
void DrawBreakpoints (HDC dc, int line)
{
	if (! ((g_iWindowThis == WINDOW_CODE) || ((g_iWindowThis == WINDOW_DATA))))
		return;

	RECT rect;
	rect.left   = DISPLAY_BP_COLUMN;
	rect.top    = (line * g_nFontHeight);
	rect.right  = DISPLAY_WIDTH;
	rect.bottom = rect.top + g_nFontHeight;

	const int MAX_BP_LEN = 16;
	TCHAR sText[16] = TEXT("Breakpoints"); // TODO: Move to BP1

	SetBkColor(dc, DebugGetColor( BG_INFO )); // COLOR_BG_DATA
	SetTextColor(dc, DebugGetColor( FG_INFO_TITLE )); //COLOR_STATIC
	DebugDrawText( sText, rect );
	rect.top    += g_nFontHeight;
	rect.bottom += g_nFontHeight;

	int iBreakpoint;
	for (iBreakpoint = 0; iBreakpoint < MAX_BREAKPOINTS; iBreakpoint++ )
	{
		Breakpoint_t *pBP = &g_aBreakpoints[iBreakpoint];
		WORD nLength   = pBP->nLength;

#if DEBUG_FORCE_DISPLAY
		nLength = 2;
#endif
		if (nLength)
		{
			bool bSet      = pBP->bSet;
			bool bEnabled  = pBP->bEnabled;
			WORD nAddress1 = pBP->nAddress;
			WORD nAddress2 = nAddress1 + nLength - 1;
			
			RECT rect2;
			rect2 = rect;
			
			SetBkColor( dc, DebugGetColor( BG_INFO ));
			SetTextColor( dc, DebugGetColor( FG_INFO_BULLET ) );
			wsprintf( sText, TEXT("%d"), iBreakpoint+1 );
			DebugDrawTextFixed( sText, rect2 );

			SetTextColor( dc, DebugGetColor( FG_INFO_OPERATOR ) );
			_tcscpy( sText, TEXT(":") );
			DebugDrawTextFixed( sText, rect2 );

#if DEBUG_FORCE_DISPLAY
	pBP->eSource = (BreakpointSource_t) iBreakpoint;
#endif
			SetTextColor( dc, DebugGetColor( FG_INFO_REG ) );
			int nRegLen = DebugDrawTextFixed( g_aBreakpointSource[ pBP->eSource ], rect2 );

			// Pad to 2 chars
			if (nRegLen < 2) // (g_aBreakpointSource[ pBP->eSource ][1] == 0) // HACK: Avoid strlen()
				rect2.left += g_aFontConfig[ FONT_INFO ]._nFontWidthAvg;

			SetBkColor( dc, DebugGetColor( BG_INFO ));
			SetTextColor( dc, DebugGetColor( FG_INFO_BULLET ) );
#if DEBUG_FORCE_DISPLAY
	if (iBreakpoint < 3)
		pBP->eOperator = (BreakpointOperator_t)(iBreakpoint * 2);
	else
		pBP->eOperator = (BreakpointOperator_t)(iBreakpoint-3 + BP_OP_READ);
#endif
			DebugDrawTextFixed( g_aBreakpointSymbols [ pBP->eOperator ], rect2 );

			DebugColors_e iForeground;
			DebugColors_e iBackground = BG_INFO;

			if (bSet)
			{
				if (bEnabled)
				{
					iBackground = BG_DISASM_BP_S_C;
//					iForeground = FG_DISASM_BP_S_X;
					iForeground = FG_DISASM_BP_S_C;
				}
				else
				{
					iForeground = FG_DISASM_BP_0_X;
				}
			}
			else
			{
				iForeground = FG_INFO_TITLE;
			}

			SetBkColor( dc, DebugGetColor( iBackground ) );
			SetTextColor( dc, DebugGetColor( iForeground ) );

#if DEBUG_FORCE_DISPLAY
	int iColor = R8 + (iBreakpoint*2);
	COLORREF nColor = gaColorPalette[ iColor ];
	if (iBreakpoint >= 4)
	{
		SetBkColor  ( dc, DebugGetColor( BG_DISASM_BP_S_C ) );
		nColor = DebugGetColor( FG_DISASM_BP_S_C );
	}
	SetTextColor( dc, nColor );
#endif

			wsprintf( sText, TEXT("%04X"), nAddress1 );
			DebugDrawTextFixed( sText, rect2 );

			if (nLength > 1)
			{
				SetBkColor( dc, DebugGetColor( BG_INFO ) );
				SetTextColor( dc, DebugGetColor( FG_INFO_OPERATOR ) );

//				if (g_bConfigDisasmOpcodeSpaces)
//				{
//					DebugDrawTextHorz( TEXT(" "), rect2 );
//					rect2.left += g_nFontWidthAvg;
//				}

				DebugDrawTextFixed( TEXT("-"), rect2 );
//				rect2.left += g_nFontWidthAvg;
//				if (g_bConfigDisasmOpcodeSpaces) // TODO: Might have to remove spaces, for BPIO... addr-addr xx
//				{
//					rect2.left += g_nFontWidthAvg;
//				}

				SetBkColor( dc, DebugGetColor( iBackground ) );
				SetTextColor( dc, DebugGetColor( iForeground ) );
#if DEBUG_FORCE_DISPLAY
	iColor++;
	COLORREF nColor = gaColorPalette[ iColor ];
	if (iBreakpoint >= 4)
	{
		nColor = DebugGetColor( BG_INFO );
		SetBkColor( dc, nColor );
		nColor = DebugGetColor( FG_DISASM_BP_S_X );
	}
	SetTextColor( dc, nColor );
#endif
				wsprintf( sText, TEXT("%04X"), nAddress2 );
				DebugDrawTextFixed( sText, rect2 );
			}

			// Bugfix: Rest of line is still breakpoint background color
			SetBkColor(dc, DebugGetColor( BG_INFO )); // COLOR_BG_DATA
			SetTextColor(dc, DebugGetColor( FG_INFO_TITLE )); //COLOR_STATIC
			DebugDrawTextHorz( TEXT(" "), rect2 );

		}
		rect.top    += g_nFontHeight;
		rect.bottom += g_nFontHeight;
	}
}


//===========================================================================
void DrawConsoleInput( HDC dc )
{
	g_hDC = dc;

	SetTextColor( g_hDC, DebugGetColor( FG_CONSOLE_INPUT ));
	SetBkColor(   g_hDC, DebugGetColor( BG_CONSOLE_INPUT ));

	DrawConsoleLine( g_aConsoleInput, 0 );
}

//===========================================================================
void DrawConsoleLine( LPCSTR pText, int y )
{
	if (y < 0)
		return;

//	int nHeight = WindowGetHeight( g_iWindowThis );
	int nLineHeight = GetConsoleHeightPixels();

	RECT rect;
	rect.left   = 0;
//	rect.top    = (g_nTotalLines - y) * nFontHeight; // DISPLAY_HEIGHT - (y * nFontHeight); // ((g_nTotalLines - y) * g_nFontHeight; // 368 = 23 lines * 16 pixels/line // MAX_DISPLAY_CONSOLE_LINES

	rect.top    = GetConsoleTopPixels( y );
	rect.bottom = rect.top + nLineHeight; //g_nFontHeight;

	// Default: (356-14) = 342 pixels ~= 48 chars (7 pixel width)
	//	rect.right = SCREENSPLIT1-14;
//	rect.right = (g_nConsoleDisplayWidth * g_nFontWidthAvg) + (g_nFontWidthAvg - 1);

	int nMiniConsoleRight = DISPLAY_MINI_CONSOLE; // (g_nConsoleDisplayWidth * g_nFontWidthAvg) + (g_nFontWidthAvg * 2); // +14
	int nFullConsoleRight = DISPLAY_WIDTH;
	int nRight = g_bConsoleFullWidth ? nFullConsoleRight : nMiniConsoleRight;
	rect.right = nRight;

	DebugDrawText( pText, rect );
}


// Disassembly formatting flags returned
//===========================================================================
int FormatDisassemblyLine( WORD nOffset, int iMode, int nOpBytes,
	char *sAddress_, char *sOpCodes_, char *sTarget_, char *sTargetOffset_, int & nTargetOffset_, char * sImmediate_, char & nImmediate_, char *sBranch_ )
{
	const int nMaxOpcodes = 3;
	unsigned int nMinBytesLen = (nMaxOpcodes * (2 + g_bConfigDisasmOpcodeSpaces)); // 2 char for byte (or 3 with space)

	int bDisasmFormatFlags = 0;

	// Composite string that has the symbol or target nAddress
	WORD nTarget = 0;
	nTargetOffset_ = 0;

//	if (g_aOpmodes[ iMode ]._sFormat[0])
	if ((iMode != AM_IMPLIED) &&
		(iMode != AM_1) &&
		(iMode != AM_2) &&
		(iMode != AM_3))
	{
		nTarget = *(LPWORD)(mem+nOffset+1);
		if (nOpBytes == 2)
			nTarget &= 0xFF;

		if (iMode == AM_R) // Relative ADDR_REL)
		{
			nTarget = nOffset+2+(int)(signed char)nTarget;

			// Always show branch indicators
			//	if ((nOffset == regs.pc) && CheckJump(nAddress))
			bDisasmFormatFlags |= DISASM_BRANCH_INDICATOR;

			if (nTarget < nOffset)
			{
				wsprintf( sBranch_, TEXT("%c"), g_sConfigBranchIndicatorUp[ g_bConfigDisasmFancyBranch ] );
			}
			else
			if (nTarget > nOffset)
			{
				wsprintf( sBranch_, TEXT("%c"), g_sConfigBranchIndicatorDown[ g_bConfigDisasmFancyBranch ] );
			}
			else
			{
				wsprintf( sBranch_, TEXT("%c"), g_sConfigBranchIndicatorEqual[ g_bConfigDisasmFancyBranch ] );
			}
		}

//		if (_tcsstr(g_aOpmodes[ iMode ]._sFormat,TEXT("%s")))
//		if ((iMode >= ADDR_ABS) && (iMode <= ADDR_IABS))
		if ((iMode == ADDR_ABS ) ||
			(iMode == ADDR_ZP ) ||
			(iMode == ADDR_ABSX) ||
			(iMode == ADDR_ABSY) ||
			(iMode == ADDR_ZP_X) ||
			(iMode == ADDR_ZP_Y) ||
			(iMode == ADDR_REL ) ||
			(iMode == ADDR_INDX) ||
			(iMode == ADDR_ABSIINDX) ||
			(iMode == ADDR_INDY) ||
			(iMode == ADDR_ABS ) ||
			(iMode == ADDR_IZPG) ||
			(iMode == ADDR_IABS))
		{
			LPCTSTR pTarget = NULL;
			LPCTSTR pSymbol = FindSymbolFromAddress( nTarget );
			if (pSymbol)
			{
				bDisasmFormatFlags |= DISASM_TARGET_SYMBOL;
				pTarget = pSymbol;
			}

			if (! (bDisasmFormatFlags & DISASM_TARGET_SYMBOL))
			{
				pSymbol = FindSymbolFromAddress( nTarget - 1 );
				if (pSymbol)
				{
					bDisasmFormatFlags |= DISASM_TARGET_SYMBOL;
					bDisasmFormatFlags |= DISASM_TARGET_OFFSET;
					pTarget = pSymbol;
					nTargetOffset_ = +1; // U FA82   LDA #3F1 BREAK+1
				}
			}
			
			if (! (bDisasmFormatFlags & DISASM_TARGET_SYMBOL))
			{
				pSymbol = FindSymbolFromAddress( nTarget + 1 );
				if (pSymbol)
				{
					bDisasmFormatFlags |= DISASM_TARGET_SYMBOL;
					bDisasmFormatFlags |= DISASM_TARGET_OFFSET;
					pTarget = pSymbol;
					nTargetOffset_ = -1; // U FA82 LDA #3F3 BREAK-1
				}
			}

			if (! (bDisasmFormatFlags & DISASM_TARGET_SYMBOL))
			{
					pTarget = FormatAddress( nTarget, nOpBytes );
			}				

//			wsprintf( sTarget, g_aOpmodes[ iMode ]._sFormat, pTarget );
			if (bDisasmFormatFlags & DISASM_TARGET_OFFSET)
			{
				int nAbsTargetOffset =  (nTargetOffset_ > 0) ? nTargetOffset_ : -nTargetOffset_;
				wsprintf( sTargetOffset_, "%d", nAbsTargetOffset );
			}
			wsprintf( sTarget_, "%s", pTarget );
		}
		else
		if (iMode == AM_M)
		{
//			wsprintf( sTarget, g_aOpmodes[ iMode ]._sFormat, (unsigned)nTarget );
			wsprintf( sTarget_, "%02X", (unsigned)nTarget );

			if (iMode == ADDR_IMM)
			{
				bDisasmFormatFlags |= DISASM_IMMEDIATE_CHAR;
				nImmediate_ = (BYTE) nTarget;
				wsprintf( sImmediate_, "%c", FormatCharTxtCtrl( FormatCharTxtHigh( nImmediate_, NULL ), NULL ) );
			}
		}
	}

	wsprintf( sAddress_, "%04X", nOffset );

	// Opcode Bytes
	TCHAR *pDst = sOpCodes_;
	for( int iBytes = 0; iBytes < nOpBytes; iBytes++ )
	{
		BYTE nMem = (unsigned)*(mem+nOffset+iBytes);
		wsprintf( pDst, TEXT("%02X"), nMem ); // sBytes+_tcslen(sBytes)
		pDst += 2;

		if (g_bConfigDisasmOpcodeSpaces)
		{
			_tcscat( pDst, TEXT(" " ) );
		}
		pDst++;
	}
    while (_tcslen(sOpCodes_) < nMinBytesLen)
	{
		_tcscat( sOpCodes_, TEXT(" ") );
	}

	return bDisasmFormatFlags;
}	


//===========================================================================
WORD DrawDisassemblyLine (HDC dc, int iLine, WORD nOffset, LPTSTR text)
{
	if (! ((g_iWindowThis == WINDOW_CODE) || ((g_iWindowThis == WINDOW_DATA))))
		return 0;

	const int nMaxAddressLen = 40;
	const int nMaxOpcodes = 3;

	int iOpcode;
	int iOpmode;
	int nOpbyte;
	iOpcode = _6502GetOpmodeOpbyte( nOffset, iOpmode, nOpbyte );

	TCHAR sAddress  [ 5];
	TCHAR sOpcodes  [(nMaxOpcodes*3)+1] = TEXT("");
	TCHAR sTarget   [nMaxAddressLen] = TEXT("");
	TCHAR sTargetOffset[ 4 ] = TEXT(""); // +/- 255, realistically +/-1
	int   nTargetOffset;
	char  nImmediate = 0;
	TCHAR sImmediate[ 4 ]; // 'c'
	TCHAR sBranch   [ 2 ]; // ^

	bool bTargetIndirect = false;
	bool bTargetX = false;
	bool bTargetY = false;

	if ((iOpmode >= ADDR_INDX) && (iOpmode <= ADDR_IABS))
		bTargetIndirect = true;

	if ((iOpmode == ADDR_ABSX) || (iOpmode == ADDR_ZP_X) || (iOpmode == ADDR_INDX) || (iOpmode == ADDR_ABSIINDX))
		bTargetX = true;

	if ((iOpmode == ADDR_ABSY) || (iOpmode == ADDR_ZP_Y))
		bTargetY = true;

	int bDisasmFormatFlags = FormatDisassemblyLine( nOffset, iOpmode, nOpbyte,
		sAddress, sOpcodes, sTarget, sTargetOffset, nTargetOffset, sImmediate, nImmediate, sBranch );

//	wsprintf(sLine,
//           TEXT("%04X%c %s  %-9s %s %s"),
//           (unsigned)nOffset,
//			g_aConfigDisasmAddressColon[ g_bConfigDisasmAddressColon ],
//           (LPCTSTR)sBytes,
//           (LPCTSTR)GetSymbol(nOffset,0),              // Label
//           (LPCTSTR)g_aOpcodes[nOpcode].mnemonic, // Instruct
//           (LPCTSTR)sTarget);                          // Target


	//> Address Seperator Opcodes   Label Mnemonic Target [Immediate] [Branch]
	//> xxxx: xx xx xx   LABEL    MNEMONIC    'E' v
	//>       ^          ^        ^           ^   ^
	//>       6          17       27          41  46
	const int nDefaultFontWidth = 7; // g_aFontConfig[FONT_DISASM_DEFAULT]._nFontWidth or g_nFontWidthAvg
	int X_OPCODE      =  6 * nDefaultFontWidth;
	int X_LABEL       = 17 * nDefaultFontWidth;
	int X_INSTRUCTION = 27 * nDefaultFontWidth;
	int X_IMMEDIATE   = 41 * nDefaultFontWidth;
	int X_BRANCH      = 46 * nDefaultFontWidth;

	const int DISASM_SYMBOL_LEN = 9;

	if (dc)
	{
		int nFontHeight = g_aFontConfig[ FONT_DISASM_DEFAULT ]._nLineHeight; // _nFontHeight; // g_nFontHeight

		RECT linerect;
		linerect.left   = 0;
		linerect.top    = iLine * nFontHeight;
		linerect.right  = DISPLAY_DISASM_RIGHT; // HACK: MAGIC #: 14 -> g_nFontWidthAvg
		linerect.bottom = linerect.top + nFontHeight;

//		BOOL bp = g_nBreakpoints && CheckBreakpoint(nOffset,nOffset == regs.pc);

		bool bBreakpointActive;
		bool bBreakpointEnable;
		GetBreakpointInfo( nOffset, bBreakpointActive, bBreakpointEnable );
		bool bAddressAtPC = (nOffset == regs.pc);

		DebugColors_e iBackground = BG_DISASM_1;
		DebugColors_e iForeground = FG_DISASM_MNEMONIC; // FG_DISASM_TEXT;
		bool bCursorLine = false;

		if (((! g_bDisasmCurBad) && (iLine == g_nDisasmCurLine))
			|| (g_bDisasmCurBad && (iLine == 0)))
		{
			bCursorLine = true;

			// Breakpoint,
			if (bBreakpointActive)
			{
				if (bBreakpointEnable)
				{
					iBackground = BG_DISASM_BP_S_C; iForeground = FG_DISASM_BP_S_C; 
				}
				else
				{
					iBackground = BG_DISASM_BP_0_C; iForeground = FG_DISASM_BP_0_C;
				}
			}
			else
			if (bAddressAtPC)
			{
				iBackground = BG_DISASM_PC_C; iForeground = FG_DISASM_PC_C;
			}
			else
			{
				iBackground = BG_DISASM_C; iForeground = FG_DISASM_C;
				// HACK?  Sync Cursor back up to Address
				// The cursor line may of had to be been moved, due to Disasm Singularity.
				g_nDisasmCurAddress = nOffset; 
			}
		}
		else
		{
			if (iLine & 1)
			{
				iBackground = BG_DISASM_1;
			}
			else
			{
				iBackground = BG_DISASM_2;
			}

			// This address has a breakpoint, but the cursor is not on it (atm)
			if (bBreakpointActive)
			{
				if (bBreakpointEnable) 
				{
					iForeground = FG_DISASM_BP_S_X; // Red (old Yellow)
				}
				else
				{
					iForeground = FG_DISASM_BP_0_X; // Yellow
				}				
			}
			else
			if (bAddressAtPC)
			{
				iBackground = BG_DISASM_PC_X; iForeground = FG_DISASM_PC_X;
			}
			else
			{
				iForeground = FG_DISASM_MNEMONIC;
			}
		}
		SetBkColor(   dc, DebugGetColor( iBackground ) );
		SetTextColor( dc, DebugGetColor( iForeground ) );

	// Address
		if (! bCursorLine)
			SetTextColor( dc, DebugGetColor( FG_DISASM_ADDRESS ) );
		DebugDrawTextHorz( (LPCTSTR) sAddress, linerect );

	// Address Seperator		
		if (! bCursorLine)
			SetTextColor( dc, DebugGetColor( FG_DISASM_OPERATOR ) );
		if (g_bConfigDisasmAddressColon)
			DebugDrawTextHorz( TEXT(":"), linerect );

	// Opcodes
		linerect.left = X_OPCODE;

		if (! bCursorLine)
			SetTextColor( dc, DebugGetColor( FG_DISASM_OPCODE ) );
//		DebugDrawTextHorz( TEXT(" "), linerect );
		DebugDrawTextHorz( (LPCTSTR) sOpcodes, linerect );
//		DebugDrawTextHorz( TEXT("  "), linerect );

	// Label
		linerect.left = X_LABEL;

		LPCSTR pSymbol = FindSymbolFromAddress( nOffset );
		if (pSymbol)
		{
			if (! bCursorLine)
				SetTextColor( dc, DebugGetColor( FG_DISASM_SYMBOL ) );
			DebugDrawTextHorz( pSymbol, linerect );
		}	
//		linerect.left += (g_nFontWidthAvg * DISASM_SYMBOL_LEN);
//		DebugDrawTextHorz( TEXT(" "), linerect );

	// Instruction
		linerect.left = X_INSTRUCTION;

		if (! bCursorLine)
			SetTextColor( dc, DebugGetColor( iForeground ) );

		LPCTSTR pMnemonic = g_aOpcodes[ iOpcode ].sMnemonic;
		DebugDrawTextHorz( pMnemonic, linerect );

		DebugDrawTextHorz( TEXT(" "), linerect );

	// Target
		if (iOpmode == ADDR_IMM)
		{
			if (! bCursorLine)
				SetTextColor( dc, DebugGetColor( FG_DISASM_OPERATOR ));	
			DebugDrawTextHorz( TEXT("#$"), linerect );
		}

		if (bTargetIndirect)
		{
			if (! bCursorLine)
				SetTextColor( dc, DebugGetColor( FG_DISASM_OPERATOR ));	
			DebugDrawTextHorz( TEXT("("), linerect );
		}

		char *pTarget = sTarget;
		if (*pTarget == '$')
		{
			pTarget++;
			if (! bCursorLine)
				SetTextColor( dc, DebugGetColor( FG_DISASM_OPERATOR ));	
			DebugDrawTextHorz( TEXT("$"), linerect );
		}

		if (! bCursorLine)
		{
			if (bDisasmFormatFlags & DISASM_TARGET_SYMBOL)
			{
				SetTextColor( dc, DebugGetColor( FG_DISASM_SYMBOL ) );
			}
			else
			{
				if (bDisasmFormatFlags & DISASM_IMMEDIATE_CHAR)
				{
					SetTextColor( dc, DebugGetColor( FG_DISASM_OPCODE ) );
				}
				else	
				{
					SetTextColor( dc, DebugGetColor( FG_DISASM_TARGET ) );
				}
			}
		}
		DebugDrawTextHorz( pTarget, linerect );
//		DebugDrawTextHorz( TEXT(" "), linerect );

		// Target Offset +/-		
		if (bDisasmFormatFlags & DISASM_TARGET_OFFSET)
		{
			if (! bCursorLine)
				SetTextColor( dc, DebugGetColor( FG_DISASM_OPERATOR ));	

			if (nTargetOffset > 0)
				DebugDrawTextHorz( TEXT("+" ), linerect );
			if (nTargetOffset < 0)
				DebugDrawTextHorz( TEXT("-" ), linerect );

			if (! bCursorLine)
			{
				SetTextColor( dc, DebugGetColor( FG_DISASM_OPCODE )); // Technically, not a hex number, but decimal
			}		
			DebugDrawTextHorz( sTargetOffset, linerect );
		}
		// Indirect Target Regs
		if (bTargetIndirect || bTargetX || bTargetY)
		{
			if (! bCursorLine)
				SetTextColor( dc, DebugGetColor( FG_DISASM_OPERATOR ));	

			if (bTargetX)
				DebugDrawTextHorz( TEXT(",X"), linerect );

			if (bTargetY)
				DebugDrawTextHorz( TEXT(",Y"), linerect );

			if (bTargetIndirect)
				DebugDrawTextHorz( TEXT(")"), linerect );

			if (iOpmode == ADDR_INDY)
				DebugDrawTextHorz( TEXT(",Y"), linerect );
		}

	// Immediate Char
		linerect.left = X_IMMEDIATE;

		if (bDisasmFormatFlags & DISASM_IMMEDIATE_CHAR)
		{
			if (! bCursorLine)
			{
				SetTextColor( dc, DebugGetColor( FG_DISASM_OPERATOR ) );
			}
			DebugDrawTextHorz( TEXT("'"), linerect ); // TEXT("    '")

			if (! bCursorLine)
			{
				ColorizeSpecialChar( dc, NULL, nImmediate, MEM_VIEW_ASCII, iBackground );
//					iBackground, FG_INFO_CHAR_HI, FG_DISASM_CHAR, FG_INFO_CHAR_LO );
			}
			DebugDrawTextHorz( sImmediate, linerect );

			SetBkColor(   dc, DebugGetColor( iBackground ) ); // Hack: Colorize can "color bleed to EOL"
			if (! bCursorLine)
			{
				SetTextColor( dc, DebugGetColor( FG_DISASM_OPERATOR ) );
			}
			DebugDrawTextHorz( TEXT("'"), linerect );
		}
	
	// Branch Indicator		
		linerect.left = X_BRANCH;

		if (bDisasmFormatFlags & DISASM_BRANCH_INDICATOR)
		{
			if (! bCursorLine)
			{
				SetTextColor( dc, DebugGetColor( FG_DISASM_BRANCH ) );
			}

			if (g_bConfigDisasmFancyBranch)
				SelectObject( dc, g_aFontConfig[ FONT_DISASM_BRANCH ]._hFont );  // g_hFontWebDings

			DebugDrawText( sBranch, linerect );

			if (g_bConfigDisasmFancyBranch)
				SelectObject( dc, g_aFontConfig[ FONT_DISASM_DEFAULT ]._hFont ); // g_hFontDisasm
		}
	}

	return nOpbyte;
}

// Optionally copy the flags to pText_
//===========================================================================
void DrawFlags (HDC dc, int line, WORD nRegFlags, LPTSTR pFlagNames_)
{
	if (! ((g_iWindowThis == WINDOW_CODE) || ((g_iWindowThis == WINDOW_DATA))))
		return;

	TCHAR sFlagNames[ _6502_NUM_FLAGS+1 ] = TEXT(""); // = TEXT("NVRBDIZC"); // copy from g_aFlagNames
	TCHAR sText[2] = TEXT("?");
	RECT  rect;

	if (dc)
	{
		rect.left   = DISPLAY_FLAG_COLUMN;
		rect.top    = line * g_nFontHeight;
		rect.right  = rect.left + 9;
		rect.bottom = rect.top + g_nFontHeight;
		SetBkColor(dc, DebugGetColor( BG_INFO )); // COLOR_BG_DATA
	}

	int iFlag = 0;
	int nFlag = _6502_NUM_FLAGS;
	while (nFlag--)
	{
		iFlag = BP_SRC_FLAG_C + (_6502_NUM_FLAGS - nFlag - 1);

		bool bSet = (nRegFlags & 1);
		if (dc)
		{

//			sText[0] = g_aFlagNames[ MAX_FLAGS - iFlag - 1]; // mnemonic[iFlag]; // mnemonic is reversed
			sText[0] = g_aBreakpointSource[ iFlag ][0];
			if (bSet)
			{
				SetBkColor( dc, DebugGetColor( BG_INFO_INVERSE ));
				SetTextColor( dc, DebugGetColor( FG_INFO_INVERSE ));
			}
			else
			{
				SetBkColor(dc, DebugGetColor( BG_INFO ));
				SetTextColor( dc, DebugGetColor( FG_INFO_TITLE ));
			}
			DebugDrawText( sText, rect );
			rect.left  -= 9; // HACK: Font Width
			rect.right -= 9; // HACK: Font Width
		}

		if (pFlagNames_)
		{
			if (! bSet) //(nFlags & 1))
			{
				sFlagNames[nFlag] = TEXT('.');
			}
			else
			{
				sFlagNames[nFlag] = g_aBreakpointSource[ iFlag ][0]; 
			}
		}

		nRegFlags >>= 1;
	}

	if (pFlagNames_)
		_tcscpy(pFlagNames_,sFlagNames);
}

//===========================================================================
void DrawMemory (HDC hDC, int line, int iMemDump )
{
	if (! ((g_iWindowThis == WINDOW_CODE) || ((g_iWindowThis == WINDOW_DATA))))
		return;

	MemoryDump_t* pMD = &g_aMemDump[ iMemDump ];

	USHORT       nAddr   = pMD->nAddress;
	DEVICE_e     eDevice = pMD->eDevice;
	MemoryView_e iView   = pMD->eView;

	SS_CARD_MOCKINGBOARD SS_MB;

	if ((eDevice == DEV_SY6522) || (eDevice == DEV_AY8910))
		MB_GetSnapshot(&SS_MB, 4+(nAddr>>1));		// Slot4 or Slot5

	int nFontWidth = g_aFontConfig[ FONT_INFO ]._nFontWidthAvg;

	RECT rect;
	rect.left   = DISPLAY_MINIMEM_COLUMN - nFontWidth;
	rect.top    = (line * g_nFontHeight);
	rect.right  = DISPLAY_WIDTH;
	rect.bottom = rect.top + g_nFontHeight;


	const int MAX_MEM_VIEW_TXT = 16;
	TCHAR sText[ MAX_MEM_VIEW_TXT * 2 ];
	TCHAR sData[ MAX_MEM_VIEW_TXT * 2 ];

	TCHAR sType   [ 4 ] = TEXT("Mem");
	TCHAR sAddress[ 8 ] = TEXT("");

	int iForeground = FG_INFO_OPCODE;
	int iBackground = BG_INFO;

	if (eDevice == DEV_SY6522)
	{
//		wsprintf(sData,TEXT("Mem at SY#%d"), nAddr);
		wsprintf( sAddress,TEXT("SY#%d"), nAddr );
	}
	else if(eDevice == DEV_AY8910)
	{
//		wsprintf(sData,TEXT("Mem at AY#%d"), nAddr);
		wsprintf( sAddress,TEXT("AY#%d"), nAddr );
	}
	else
	{
		wsprintf( sAddress,TEXT("%04X"),(unsigned)nAddr);

		if (iView == MEM_VIEW_HEX)
			wsprintf( sType, TEXT("HEX") );
		else
		if (iView == MEM_VIEW_ASCII)
			wsprintf( sType, TEXT("ASCII") );
		else
			wsprintf( sType, TEXT("TEXT") );
	}

	RECT rect2;

	rect2 = rect;	
	SetTextColor( hDC, DebugGetColor( FG_INFO_TITLE ));
	SetBkColor( hDC, DebugGetColor( BG_INFO ));
	DebugDrawTextFixed( sType, rect2 );

	SetTextColor( hDC, DebugGetColor( FG_INFO_OPERATOR ));
	DebugDrawTextFixed( TEXT(" at " ), rect2 );

	SetTextColor( hDC, DebugGetColor( FG_INFO_ADDRESS ));
	DebugDrawTextLine( sAddress, rect2 );

	rect.top    = rect2.top;
	rect.bottom = rect2.bottom;

	sData[0] = 0;

	WORD iAddress = nAddr;

	if( (eDevice == DEV_SY6522) || (eDevice == DEV_AY8910) )
	{
		iAddress = 0;
	}

	int nLines = 4;
	int nCols = 4;

	if (iView != MEM_VIEW_HEX)
	{
		nCols = MAX_MEM_VIEW_TXT;
	}
	rect.right  = MAX( rect.left + (nFontWidth * nCols), DISPLAY_WIDTH );

	SetTextColor( hDC, DebugGetColor( FG_INFO_OPCODE ));

	for (int iLine = 0; iLine < nLines; iLine++ )
	{
		RECT rect2;
		rect2 = rect;

		if (iView == MEM_VIEW_HEX)
		{
			wsprintf( sText, TEXT("%04X"), iAddress );
			SetTextColor( hDC, DebugGetColor( FG_INFO_ADDRESS ));
			DebugDrawTextFixed( sText, rect2 );

			SetTextColor( hDC, DebugGetColor( FG_INFO_OPERATOR ));
			DebugDrawTextFixed( TEXT(":"), rect2 );
		}

		for (int iCol = 0; iCol < nCols; iCol++)
		{
			bool bHiBit = false;
			bool bLoBit = false;

			SetBkColor  ( hDC, DebugGetColor( iBackground ));
			SetTextColor( hDC, DebugGetColor( iForeground ));

// .12 Bugfix: DrawMemory() should draw memory byte for IO address: ML1 C000
//			if ((iAddress >= _6502_IO_BEGIN) && (iAddress <= _6502_IO_END))
//			{
//				wsprintf( sText, TEXT("IO ") );
//			}
//			else
			if (eDevice == DEV_SY6522)
			{
				wsprintf( sText, TEXT("%02X "), (unsigned) ((BYTE*)&SS_MB.Unit[nAddr & 1].RegsSY6522)[iAddress] );
			}
			else
			if (eDevice == DEV_AY8910)
			{
				wsprintf( sText, TEXT("%02X "), (unsigned)SS_MB.Unit[nAddr & 1].RegsAY8910[iAddress] );
			}
			else
			{
				BYTE nData = (unsigned)*(LPBYTE)(membank+iAddress);
				sText[0] = 0;

				char c = nData;

				if (iView == MEM_VIEW_HEX)
				{
					if ((iAddress >= _6502_IO_BEGIN) && (iAddress <= _6502_IO_END))
					{
						SetTextColor( hDC, DebugGetColor( FG_INFO_IO_BYTE ));
					}
					wsprintf(sText, TEXT("%02X "), nData );
				}
				else
				{
// .12 Bugfix: DrawMemory() should draw memory byte for IO address: ML1 C000
					if ((iAddress >= _6502_IO_BEGIN) && (iAddress <= _6502_IO_END))
						iBackground = BG_INFO_IO_BYTE;

					ColorizeSpecialChar( hDC, sText, nData, iView, iBackground );
				}
			}
			int nChars = DebugDrawTextFixed( sText, rect2 ); // DebugDrawTextFixed()

			iAddress++;
		}
		rect.top    += g_nFontHeight; // TODO/FIXME: g_nFontHeight;
		rect.bottom += g_nFontHeight; // TODO/FIXME: g_nFontHeight;
		sData[0] = 0;
	}
}

//===========================================================================
void DrawRegister (HDC dc, int line, LPCTSTR name, const int nBytes, const WORD nValue, int iSource )
{
	if (! ((g_iWindowThis == WINDOW_CODE) || ((g_iWindowThis == WINDOW_DATA))))
		return;

	RECT rect;
	rect.left   = DISPLAY_REGS_COLUMN;
	rect.top    = line * g_nFontHeight;
	rect.right  = rect.left + 40; // TODO:FIXME: g_nFontWidthAvg * 
	rect.bottom = rect.top + g_nFontHeight;

	if ((PARAM_REG_A  == iSource) ||
		(PARAM_REG_X  == iSource) ||
		(PARAM_REG_Y  == iSource) ||
		(PARAM_REG_PC == iSource) ||
		(PARAM_REG_SP == iSource))
	{		
		SetTextColor(dc, DebugGetColor( FG_INFO_REG ));
	}
	else
	{
		SetTextColor(dc, DebugGetColor( FG_INFO_TITLE ));
	}
	SetBkColor(dc, DebugGetColor( BG_INFO ));
	DebugDrawText( name, rect );

	unsigned int nData = nValue;
	int nOffset = 6;

	TCHAR sValue[8];

	if (PARAM_REG_SP == iSource)
	{
		WORD nStackDepth = _6502_STACK_END - nValue;
		wsprintf( sValue, "%02X", nStackDepth );
		int nFontWidth = g_aFontConfig[ FONT_INFO ]._nFontWidthAvg;
		rect.left += (2 * nFontWidth) + (nFontWidth >> 1); // 2.5 looks a tad nicer then 2

		// ## = Stack Depth (in bytes)
		SetTextColor(dc, DebugGetColor( FG_INFO_OPERATOR )); // FG_INFO_OPCODE, FG_INFO_TITLE
		DebugDrawText( sValue, rect );
	}

	if (nBytes == 2)
	{
		wsprintf(sValue,TEXT("%04X"), nData);
	}
	else
	{
		rect.left  = DISPLAY_REGS_COLUMN + 21; // HACK: MAGIC #: 21 // +3 chars
		rect.right = SCREENSPLIT2;

		SetTextColor(dc, DebugGetColor( FG_INFO_OPERATOR ));
		DebugDrawTextFixed( TEXT("'"), rect ); // DebugDrawTextFixed

		ColorizeSpecialChar( dc, sValue, nData, MEM_VIEW_ASCII ); // MEM_VIEW_APPLE for inverse background little hard on the eyes
		DebugDrawTextFixed( sValue, rect ); // DebugDrawTextFixed()

		SetBkColor(dc, DebugGetColor( BG_INFO ));
		SetTextColor(dc, DebugGetColor( FG_INFO_OPERATOR ));
		DebugDrawTextFixed( TEXT("'"), rect ); // DebugDrawTextFixed()

		wsprintf(sValue,TEXT("  %02X"), nData );
	}

	// Needs to be far enough over, since 4 chars of ZeroPage symbol also calls us
	rect.left  = DISPLAY_REGS_COLUMN + (nOffset * g_aFontConfig[ FONT_INFO ]._nFontWidthAvg); // HACK: MAGIC #: 40 
	rect.right = SCREENSPLIT2;

	if ((PARAM_REG_PC == iSource) || (PARAM_REG_SP == iSource)) // Stack Pointer is target address, but doesn't look as good.
	{
		SetTextColor(dc, DebugGetColor( FG_INFO_ADDRESS ));
	}
	else
	{
		SetTextColor(dc, DebugGetColor( FG_INFO_OPCODE )); // FG_DISASM_OPCODE
	}
	DebugDrawText( sValue, rect );
}

//===========================================================================
void DrawStack (HDC dc, int line)
{
	if (! ((g_iWindowThis == WINDOW_CODE) || ((g_iWindowThis == WINDOW_DATA))))
		return;

	unsigned nAddress = regs.sp;
#if DEBUG_FORCE_DISPLAY
	nAddress = 0x100;
#endif

	int      iStack   = 0;
	while (iStack < MAX_DISPLAY_STACK_LINES)
	{
		nAddress++;

		RECT rect;
		rect.left   = DISPLAY_STACK_COLUMN;
		rect.top    = (iStack+line) * g_nFontHeight;
		rect.right  = DISPLAY_STACK_COLUMN + 40; // TODO/FIXME/HACK MAGIC #: g_nFontWidthAvg * 
		rect.bottom = rect.top + g_nFontHeight;

		SetTextColor(dc, DebugGetColor( FG_INFO_TITLE )); // [COLOR_STATIC
		SetBkColor(dc, DebugGetColor( BG_INFO )); // COLOR_BG_DATA

		TCHAR sText[8] = TEXT("");
		if (nAddress <= _6502_STACK_END)
		{
			wsprintf(sText,TEXT("%04X"),nAddress);
		}

//		ExtTextOut(dc,rect.left,rect.top,
//			ETO_CLIPPED | ETO_OPAQUE,&rect,
//			sText,_tcslen(sText),NULL);
		DebugDrawText( sText, rect );

		rect.left   = DISPLAY_STACK_COLUMN + 40; // TODO/FIXME/HACK MAGIC #: g_nFontWidthAvg * 
		rect.right  = SCREENSPLIT2;
		SetTextColor(dc, DebugGetColor( FG_INFO_OPCODE )); // COLOR_FG_DATA_TEXT

		if (nAddress <= _6502_STACK_END)
		{
			wsprintf(sText,TEXT("%02X"),(unsigned)*(LPBYTE)(mem+nAddress));
		}
//		ExtTextOut(dc,rect.left,rect.top,
//			ETO_CLIPPED | ETO_OPAQUE,&rect,
//			sText,_tcslen(sText),NULL);
		DebugDrawText( sText, rect );
	    iStack++;
	}
}

//===========================================================================
void DrawTargets (HDC dc, int line)
{
	if (! ((g_iWindowThis == WINDOW_CODE) || ((g_iWindowThis == WINDOW_DATA))))
		return;

	int aTarget[2];
	Get6502Targets( &aTarget[0],&aTarget[1], NULL );

	RECT rect;
	
	int iAddress = 2;
	while (iAddress--)
	{
		// .6 Bugfix: DrawTargets() should draw target byte for IO address: R PC FB33
//		if ((aTarget[iAddress] >= _6502_IO_BEGIN) && (aTarget[iAddress] <= _6502_IO_END))
//			aTarget[iAddress] = NO_6502_TARGET;

		TCHAR sAddress[8] = TEXT("");
		TCHAR sData[8]   = TEXT("");

#if DEBUG_FORCE_DISPLAY
		if (aTarget[iAddress] == NO_6502_TARGET)
			aTarget[iAddress] = 0;
#endif
		if (aTarget[iAddress] != NO_6502_TARGET)
		{
			wsprintf(sAddress,TEXT("%04X"),aTarget[iAddress]);
			if (iAddress)
				wsprintf(sData,TEXT("%02X"),*(LPBYTE)(mem+aTarget[iAddress]));
			else
				wsprintf(sData,TEXT("%04X"),*(LPWORD)(mem+aTarget[iAddress]));
		}

		rect.left   = DISPLAY_TARGETS_COLUMN;
		rect.top    = (line+iAddress) * g_nFontHeight;
		int nColumn = DISPLAY_TARGETS_COLUMN + 40; // TODO/FIXME/HACK MAGIC #: g_nFontWidthAvg * 
		rect.right  = nColumn;
		rect.bottom = rect.top + g_nFontHeight;

		if (iAddress == 0)
			SetTextColor(dc, DebugGetColor( FG_INFO_TITLE )); // Temp Address
		else
			SetTextColor(dc, DebugGetColor( FG_INFO_ADDRESS )); // Target Address

		SetBkColor(dc, DebugGetColor( BG_INFO ));
		DebugDrawText( sAddress, rect );

		rect.left  = nColumn; // SCREENSPLIT1+40; // + 40
		rect.right = SCREENSPLIT2;

		if (iAddress == 0)
			SetTextColor(dc, DebugGetColor( FG_INFO_ADDRESS )); // Temp Target
		else
			SetTextColor(dc, DebugGetColor( FG_INFO_OPCODE )); // Target Bytes

		DebugDrawText( sData, rect );
  }
}

//===========================================================================
void DrawWatches (HDC dc, int line)
{
	if (! ((g_iWindowThis == WINDOW_CODE) || ((g_iWindowThis == WINDOW_DATA))))
		return;

	RECT rect;
	rect.left   = DISPLAY_WATCHES_COLUMN;
	rect.top    = (line * g_nFontHeight);
	rect.right  = DISPLAY_WIDTH;
	rect.bottom = rect.top + g_nFontHeight;

	TCHAR sText[16] = TEXT("Watches");
	SetTextColor(dc, DebugGetColor( FG_INFO_TITLE ));
	SetBkColor(dc, DebugGetColor( BG_INFO ));
	DebugDrawTextLine( sText, rect );

	int iWatch;
	for (iWatch = 0; iWatch < MAX_WATCHES; iWatch++ )
	{
#if DEBUG_FORCE_DISPLAY
		if (true)
#else
		if (g_aWatches[iWatch].bEnabled)
#endif
		{
			RECT rect2 = rect;

			wsprintf( sText,TEXT("%d"),iWatch+1  );
			SetTextColor( dc, DebugGetColor( FG_INFO_BULLET ));
			DebugDrawTextFixed( sText, rect2 );
			
			wsprintf( sText,TEXT(":") );
			SetTextColor( dc, DebugGetColor( FG_INFO_OPERATOR ));
			DebugDrawTextFixed( sText, rect2 );

			wsprintf( sText,TEXT(" %04X"), g_aWatches[iWatch].nAddress );
			SetTextColor( dc, DebugGetColor( FG_INFO_ADDRESS ));
			DebugDrawTextFixed( sText, rect2 );

			wsprintf(sText,TEXT(" %02X"),(unsigned)*(LPBYTE)(mem+g_aWatches[iWatch].nAddress));
			SetTextColor(dc, DebugGetColor( FG_INFO_OPCODE ));
			DebugDrawTextFixed( sText, rect2 );
		}

		rect.top    += g_nFontHeight; // HACK: 
		rect.bottom += g_nFontHeight; // HACK:
	}
}


//===========================================================================
void DrawZeroPagePointers(HDC dc, int line)
{
	if (! ((g_iWindowThis == WINDOW_CODE) || ((g_iWindowThis == WINDOW_DATA))))
		return;

	for(int iZP = 0; iZP < MAX_ZEROPAGE_POINTERS; iZP++)
	{
		RECT rect;
		rect.left   = DISPLAY_ZEROPAGE_COLUMN;
		rect.top    = (line+iZP) * g_nFontHeight;
		rect.right  = SCREENSPLIT2; // TODO/FIXME:
		rect.bottom = rect.top + g_nFontHeight;

		TCHAR sText[8] = TEXT("       ");

		SetTextColor(dc, DebugGetColor( FG_INFO_TITLE )); // COLOR_STATIC
		SetBkColor(dc, DebugGetColor( BG_INFO ));
		DebugDrawText( sText, rect );

		Breakpoint_t *pZP = &g_aZeroPagePointers[iZP];
		bool bEnabled = pZP->bEnabled;

#if DEBUG_FORCE_DISPLAY
		bEnabled = true;
#endif

		if (bEnabled)
//		if (g_aZeroPagePointers[iZP].bSet) // TODO: Only list enanbled ones
		{
			const UINT nSymbolLen = 4;
			char szZP[nSymbolLen+1];

			BYTE nZPAddr1 = (g_aZeroPagePointers[iZP].nAddress  ) & 0xFF; // +MJP missig: "& 0xFF", or "(BYTE) ..."
			BYTE nZPAddr2 = (g_aZeroPagePointers[iZP].nAddress+1) & 0xFF;

			// Get nZPAddr1 last (for when neither symbol is not found - GetSymbol() return ptr to static buffer)
			const char* pszSymbol2 = GetSymbol(nZPAddr2, 2);		// 2:8-bit value (if symbol not found)
			const char* pszSymbol1 = GetSymbol(nZPAddr1, 2);		// 2:8-bit value (if symbol not found)

			if( (strlen(pszSymbol1)==1) && (strlen(pszSymbol2)==1) )
			{
				sprintf(szZP, "%s%s", pszSymbol1, pszSymbol2);
			}
			else
			{
				memcpy(szZP, pszSymbol1, nSymbolLen);
				szZP[nSymbolLen] = 0;
			}

			WORD nZPPtr = (WORD)membank[nZPAddr1] | ((WORD)membank[nZPAddr2]<<8);
			DrawRegister(dc, line+iZP, szZP, 2, nZPPtr);
		}
	}
}


//===========================================================================
int _Arg_1( int nValue )
{
	g_aArgs[1].nVal1 = nValue;
	return 1;
}
	
//===========================================================================
int _Arg_1( LPTSTR pName )
{
	int nLen = _tcslen( g_aArgs[1].sArg );
	if (nLen < MAX_ARG_LEN)
	{
		_tcscpy( g_aArgs[1].sArg, pName );
	}
	else
	{
		_tcsncpy( g_aArgs[1].sArg, pName, MAX_ARG_LEN - 1 );
	}
	return 1;
}

/**
	@description Copies Args[iSrc .. iEnd] to Args[0]
	@param iSrc First argument to copy
	@param iEnd Last argument to end
	@return nArgs Number of new args
	Usually called as: nArgs = _Arg_Shift( iArg, nArgs );
//=========================================================================== */
int _Arg_Shift( int iSrc, int iEnd, int iDst )
{
	if (iDst < 0)
		return ARG_SYNTAX_ERROR;
	if (iDst > MAX_ARGS)
		return ARG_SYNTAX_ERROR;

	int nArgs = (iEnd - iSrc);
	int nLen = nArgs + 1;

	if ((iDst + nLen) > MAX_ARGS)
		return ARG_SYNTAX_ERROR;

	while (nLen--)
	{
		g_aArgs[iDst] = g_aArgs[iSrc];
		iSrc++;
		iDst++;
	}
	return nArgs;
}


//===========================================================================
void ArgsClear ()
{
	Arg_t *pArg = &g_aArgs[0];

	for (int iArg = 0; iArg < MAX_ARGS; iArg++ )
	{
		pArg->bSymbol = false;
		pArg->eDevice = NUM_DEVICES; // none
		pArg->eToken  = NO_TOKEN   ; // none
		pArg->bType   = TYPE_STRING;
		pArg->nVal1   = 0;
		pArg->nVal2   = 0;
		pArg->sArg[0] = 0;

		pArg++;
	}
}


// Processes the raw args, turning them into tokens and types.
//===========================================================================
int	ArgsGet ( TCHAR * pInput )
{
	LPCTSTR pSrc = pInput;
	LPCTSTR pEnd = NULL;
	int     nBuf;

	ArgToken_e iTokenSrc = NO_TOKEN;
	ArgToken_e iTokenEnd = NO_TOKEN;
	ArgType_e  iType     = TYPE_STRING;
	int     nLen;

	int     iArg = 0;
	int     nArg = 0;
	Arg_t  *pArg = &g_aArgRaw[0]; // &g_aArgs[0];

	// BP FAC8:FACA // Range=3
	// BP FAC8,2    // Length=2
	// ^ ^^   ^^
	// | ||   |pSrc
	// | ||   pSrc
	// | |pSrc
	// | pEnd
	// pSrc
	while ((*pSrc) && (iArg < MAX_ARGS))
	{
		// Technically, there shouldn't be any leading spaces,
		// since pressing the spacebar is an alias for TRACE.
		// However, there is spaces between arguments
		pSrc = const_cast<char*>( SkipWhiteSpace( pSrc ));

		if (pSrc)
		{
			pEnd = FindTokenOrAlphaNumeric( pSrc, g_aTokens, NUM_TOKENS, &iTokenSrc );
			if ((iTokenSrc == NO_TOKEN) || (iTokenSrc == TOKEN_ALPHANUMERIC))
			{
				pEnd = SkipUntilToken( pSrc+1, g_aTokens, NUM_TOKENS, &iTokenEnd );
			}

			if (iTokenSrc == NO_TOKEN)
			{
				iTokenSrc = TOKEN_ALPHANUMERIC;
			}

			iType = g_aTokens[ iTokenSrc ].eType;

			if (iTokenSrc == TOKEN_SEMI)
			{
				// TODO - command seperator, must handle non-quoted though!
			}

			if (iTokenSrc == TOKEN_QUOTED)
			{
				pSrc++; // Don't store start of quote
				pEnd = SkipUntilChar( pSrc, CHAR_QUOTED );
			}

			if (pEnd)
			{
				nBuf = pEnd - pSrc;
			}

			if (nBuf > 0)
			{
				nLen = MIN( nBuf, MAX_ARG_LEN-1 );
				_tcsncpy( pArg->sArg, pSrc, nLen );
				pArg->sArg[ nLen ] = 0;			
				pArg->nArgLen      = nLen;
				pArg->eToken       = iTokenSrc;
				pArg->bType        = iType;

				if (iTokenSrc == TOKEN_QUOTED)
				{
					pEnd++; // Don't store end of quote
				}

				pSrc = pEnd;
				iArg++;
				pArg++;
			}
		}
	}

	if (iArg)
	{
		nArg = iArg - 1; // first arg is command
	}

	g_nArgRaw = nArg;

	return nArg;
}


//===========================================================================
bool ArgsGetRegisterValue( Arg_t *pArg, WORD * pAddressValue_ )
{
	bool bStatus = false;

	if (pArg && pAddressValue_)
	{
		// Check if we refer to reg A X Y P S
		for( int iReg = 0; iReg < (NUM_BREAKPOINT_SOURCES-1); iReg++ )
		{
			// Skip Opcode/Instruction/Mnemonic
			if (iReg == BP_SRC_OPCODE)
				continue;

			// Skip individual flag names
			if ((iReg >= BP_SRC_FLAG_C) && (iReg <= BP_SRC_FLAG_N))
				continue;

			// Handle one char names
			if ((pArg->nArgLen == 1) && (pArg->sArg[0] == g_aBreakpointSource[ iReg ][0]))
			{
				switch( iReg )
				{
					case BP_SRC_REG_A : *pAddressValue_ = regs.a  & 0xFF; bStatus = true; break;
					case BP_SRC_REG_P : *pAddressValue_ = regs.ps & 0xFF; bStatus = true; break;
					case BP_SRC_REG_X : *pAddressValue_ = regs.x  & 0xFF; bStatus = true; break;
					case BP_SRC_REG_Y : *pAddressValue_ = regs.y  & 0xFF; bStatus = true; break;
					case BP_SRC_REG_S : *pAddressValue_ = regs.sp       ; bStatus = true; break;
					default:
						break;
				}
			}
			else
			if (iReg == BP_SRC_REG_PC)
			{
				if ((pArg->nArgLen == 2) && (_tcscmp( pArg->sArg, g_aBreakpointSource[ iReg ] ) == 0))
				{
					*pAddressValue_ = regs.pc       ; bStatus = true; break;
				}
			}
		}
	}
	return bStatus;
}


//===========================================================================
bool ArgsGetValue( Arg_t *pArg, WORD * pAddressValue_ )
{
	const int BASE = 16; // hex
	TCHAR *pSrc = & (pArg->sArg[ 0 ]);
	TCHAR *pEnd = NULL;

	if (pArg && pAddressValue_)
	{
		*pAddressValue_ = (WORD)(_tcstoul( pSrc, &pEnd, BASE) & _6502_END_MEM_ADDRESS);
		return true;
	}
	return false;
}


//===========================================================================
bool ArgsGetImmediateValue( Arg_t *pArg, WORD * pAddressValue_ )
{
	if (pArg && pAddressValue_)
	{
		if (pArg->eToken == TOKEN_HASH)
		{
			pArg++;
			return ArgsGetValue( pArg, pAddressValue_ );
		}
	}

	return false;
}


//===========================================================================
void ArgsRawParse( void )
{
	const int BASE = 16; // hex
	TCHAR *pSrc  = NULL;
	TCHAR *pEnd  = NULL;

	int    iArg = 1;
	Arg_t *pArg = & g_aArgRaw[ iArg ];
	int    nArg = g_nArgRaw;

	WORD   nAddressArg;
	WORD   nAddressSymbol;
	WORD   nAddressValue;
	int    nParamLen = 0;

	while (iArg <= nArg)
	{
		pSrc  = & (pArg->sArg[ 0 ]);

		nAddressArg = (WORD)(_tcstoul( pSrc, &pEnd, BASE) & _6502_END_MEM_ADDRESS);
		nAddressValue = nAddressArg;

		bool bFound = false;
		if (! (pArg->bType & TYPE_NO_SYM))
		{
			bFound = FindAddressFromSymbol( pSrc, & nAddressSymbol );
			if (bFound)
			{
				nAddressValue = nAddressSymbol;
				pArg->bSymbol = true;
			}
		}

		if (! (pArg->bType & TYPE_VALUE)) // already up to date?
			pArg->nVal1 = nAddressValue;

		pArg->bType |= TYPE_ADDRESS;

		iArg++;
		pArg++;
	}
}


// Note: The number of args can be changed via:
//   address1,length    Length
//   address1:address2  Range
//   address1+delta     Delta
//   address1-delta     Delta
//===========================================================================
int ArgsParse( const int nArgs )
{
	const int BASE = 16; // hex
	TCHAR *pSrc  = NULL;
	TCHAR *pEnd2 = NULL;

	int    nArg = nArgs;
	int    iArg = 1;
	Arg_t *pArg = NULL; 
	Arg_t *pPrev = NULL;
	Arg_t *pNext = NULL;

	WORD   nAddressArg;
	WORD   nAddressRHS;
	WORD   nAddressSym;
	WORD   nAddressVal;
	int    nParamLen = 0;
	int    nArgsLeft = 0;

	while (iArg <= nArg)
	{
		pArg  = & (g_aArgs[ iArg ]);
		pSrc  = & (pArg->sArg[ 0 ]);

		if (pArg->eToken == TOKEN_DOLLAR) // address
		{
// TODO: Need to flag was a DOLLAR token for assembler
			pNext = NULL;

			nArgsLeft = (nArg - iArg);
			if (nArgsLeft > 0)
			{
				pNext = pArg + 1;

				_Arg_Shift( iArg + 1, nArgs, iArg );
				nArg--;
				iArg--; // inc for start of next loop

				// Don't do register lookup
				pArg->bType |= TYPE_NO_REG;
			}
			else
				return ARG_SYNTAX_ERROR;
		}

		if (pArg->bType & TYPE_OPERATOR) // prev op type == address?
		{
			pPrev = NULL; // pLHS
			pNext = NULL; // pRHS
			nParamLen = 0;

			if (pArg->eToken == TOKEN_HASH) // HASH    # immediate
				nParamLen = 1;

			nArgsLeft = (nArg - iArg);
			if (nArgsLeft < nParamLen)
			{
				return ARG_SYNTAX_ERROR;
			}

			pPrev = pArg - 1;

			if (nArgsLeft > 0) // These ops take at least 1 argument
			{
				pNext = pArg + 1;
				pSrc = &pNext->sArg[0];

				nAddressVal = 0;
				if (ArgsGetValue( pNext, & nAddressRHS ))
					nAddressVal = nAddressRHS;

				bool bFound = FindAddressFromSymbol( pSrc, & nAddressSym );
				if (bFound)
				{
					nAddressVal = nAddressSym;
					pArg->bSymbol = true;
				}

				if (pArg->eToken == TOKEN_COMMA) // COMMMA , length
				{
					pPrev->nVal2  = nAddressVal;
					pPrev->eToken = TOKEN_COMMA;
					pPrev->bType |= TYPE_ADDRESS;
					pPrev->bType |= TYPE_LENGTH;
					nParamLen = 2;
				}

				if (pArg->eToken == TOKEN_COLON) // COLON  : range
				{
					pPrev->nVal2  = nAddressVal;
					pPrev->eToken = TOKEN_COLON;
					pPrev->bType |= TYPE_ADDRESS;
					pPrev->bType |= TYPE_RANGE;
					nParamLen = 2;
				}

				if (pArg->eToken == TOKEN_AMPERSAND) // AND   & delta
				{
					if (! ArgsGetImmediateValue( pNext, & nAddressRHS ))
					{
						  ArgsGetRegisterValue( pNext, & nAddressRHS );
					}
					pPrev->nVal1 &= nAddressRHS;
					pPrev->bType |= TYPE_VALUE; // signal already up to date
					nParamLen = 2;
				}								

				if (pArg->eToken == TOKEN_PIPE) // OR   | delta
				{
					if (! ArgsGetImmediateValue( pNext, & nAddressRHS ))
					{
						  ArgsGetRegisterValue( pNext, & nAddressRHS );
					}
					pPrev->nVal1 |= nAddressRHS;
					pPrev->bType |= TYPE_VALUE; // signal already up to date
					nParamLen = 2;
				}								

				if (pArg->eToken == TOKEN_CARET) // XOR   ^ delta
				{
					if (! ArgsGetImmediateValue( pNext, & nAddressRHS ))
					{
						  ArgsGetRegisterValue( pNext, & nAddressRHS );
					}
					pPrev->nVal1 ^= nAddressRHS;
					pPrev->bType |= TYPE_VALUE; // signal already up to date
					nParamLen = 2;
				}								

				if (pArg->eToken == TOKEN_PLUS) // PLUS   + delta
				{
					if (! ArgsGetImmediateValue( pNext, & nAddressRHS ))
					{
						  ArgsGetRegisterValue( pNext, & nAddressRHS );
					}
					pPrev->nVal1 += nAddressRHS;
					pPrev->bType |= TYPE_VALUE; // signal already up to date
					nParamLen = 2;
				}

				if (pArg->eToken == TOKEN_MINUS) // MINUS  - delta
				{
					if (! ArgsGetImmediateValue( pNext, & nAddressRHS ))
					{
						ArgsGetRegisterValue( pNext, & nAddressRHS );
					}
					pPrev->nVal1 -= nAddressRHS;
					pPrev->bType |= TYPE_VALUE; // signal already up to date
					nParamLen = 2;
				}

				if (pArg->eToken == TOKEN_PERCENT) // PERCENT % delta
				{
					if (! ArgsGetImmediateValue( pNext, & nAddressRHS ))
					{
						ArgsGetRegisterValue( pNext, & nAddressRHS );
					}
					pPrev->nVal1 %= nAddressRHS;
					pPrev->bType |= TYPE_VALUE; // signal already up to date
					nParamLen = 2;
				}

				if (pArg->eToken == TOKEN_FSLASH) // FORWARD SLASH / delta
				{
					if (! ArgsGetImmediateValue( pNext, & nAddressRHS ))
					{
						ArgsGetRegisterValue( pNext, & nAddressRHS );
					}
					pPrev->nVal1 /= nAddressRHS;
					pPrev->bType |= TYPE_VALUE; // signal already up to date
					nParamLen = 2;
				}

				if (pArg->eToken == TOKEN_EQUAL) // EQUAL  = assign
				{
					pPrev->nVal1 = nAddressRHS; 
					pPrev->bType |= TYPE_VALUE; // signal already up to date
					nParamLen = 0; // need token for Smart BreakPoints
				}					

				if (pArg->eToken == TOKEN_HASH) // HASH    # immediate
				{
					_Arg_Shift( iArg + nParamLen, nArgs, iArg );
					nArg--;

					pArg->nVal1   = nAddressRHS;
					pArg->bSymbol = false;
					pArg->bType   = TYPE_VALUE | TYPE_ADDRESS | TYPE_NO_REG | TYPE_NO_SYM;
					nParamLen = 0;
				}

				if (pArg->eToken == TOKEN_LESS_THAN) // <
				{
					nParamLen = 0;
				}

				if (pArg->eToken == TOKEN_GREATER_THAN) // >
				{
					nParamLen = 0;
				}

				if (pArg->eToken == TOKEN_EXCLAMATION) // NOT_EQUAL !
				{
					if (! ArgsGetImmediateValue( pNext, & nAddressRHS ))
					{
						if (! ArgsGetRegisterValue( pNext, & nAddressRHS ))
						{
							nAddressRHS = nAddressVal;
						}
					}
					pArg->nVal1 = ~nAddressRHS;
					pArg->bType |= TYPE_VALUE; // signal already up to date
					// Don't remove, since "SYM ! symbol" needs token to remove symbol
				}
				
				if (nParamLen)
				{
					_Arg_Shift( iArg + nParamLen, nArgs, iArg );
					nArg -= nParamLen;
					iArg = 0; // reset args, to handle multiple operators
				}
			}
			else
				return ARG_SYNTAX_ERROR;
		}
		else // not an operator, try (1) address, (2) symbol lookup
		{
			nAddressArg = (WORD)(_tcstoul( pSrc, &pEnd2, BASE) & _6502_END_MEM_ADDRESS);

			if (! (pArg->bType & TYPE_NO_REG))
			{
				ArgsGetRegisterValue( pArg, & nAddressArg );
			}

			nAddressVal = nAddressArg;

			bool bFound = false;
			if (! (pArg->bType & TYPE_NO_SYM))
			{
				bFound = FindAddressFromSymbol( pSrc, & nAddressSym );
				if (bFound)
				{
					nAddressVal = nAddressSym;
					pArg->bSymbol = true;
				}
			}

			if (! (pArg->bType & TYPE_VALUE)) // already up to date?
				pArg->nVal1 = nAddressVal;

			pArg->bType |= TYPE_ADDRESS;
		}

		iArg++;
	}

	return nArg;
}


// Note: Range is [iParamBegin,iParamEnd], not the usually (STL) expected [iParamBegin,iParamEnd)
//===========================================================================
int FindParam( LPTSTR pLookupName, Match_e eMatch, int & iParam_, int iParamBegin, int iParamEnd )
{
	int nFound = 0;
	int nLen     = _tcslen( pLookupName );
	int iParam = 0;

	if (! nLen)
		return nFound;

	if (eMatch == MATCH_EXACT)
	{
//		while (iParam < NUM_PARAMS )
		for (iParam = iParamBegin; iParam <= iParamEnd; iParam++ )
		{
			TCHAR *pParamName = g_aParameters[iParam].m_sName;
			int eCompare = _tcscmp(pLookupName, pParamName);
			if (! eCompare) // exact match?
			{
				nFound++;
				iParam_ = g_aParameters[iParam].iCommand;
				break;
			}
		}
	}
	else
	if (eMatch == MATCH_FUZZY)
	{	
//		while (iParam < NUM_PARAMS)
		for (iParam = iParamBegin; iParam <= iParamEnd; iParam++ )
		{
			TCHAR *pParamName = g_aParameters[ iParam ].m_sName;
			if (! _tcsncmp(pLookupName, pParamName ,nLen))
			{
				nFound++;

				if (!_tcscmp(pLookupName, pParamName)) // exact match?
				{
					nFound = 1; // Exact match takes precidence over fuzzy matches
					iParam_ = iParam_ = g_aParameters[iParam].iCommand;
					break;
				}
			}		
		}
	}
	return nFound;
}

//===========================================================================
int FindCommand( LPTSTR pName, CmdFuncPtr_t & pFunction_, int * iCommand_ )
{
	g_vPotentialCommands.erase( g_vPotentialCommands.begin(), g_vPotentialCommands.end() );

	int nFound   = 0;
	int nLen     = _tcslen( pName );
	int iCommand = 0;

	if (! nLen)
		return nFound;

	while ((iCommand < NUM_COMMANDS_WITH_ALIASES)) // && (name[0] >= g_aCommands[iCommand].aName[0])) Command no longer in Alphabetical order
	{
		TCHAR *pCommandName = g_aCommands[iCommand].m_sName;
		if (! _tcsncmp(pName, pCommandName, nLen))
		{
			pFunction_ = g_aCommands[iCommand].pFunction;
			if (pFunction_)
			{
				nFound++;
				g_iCommand = g_aCommands[iCommand].iCommand;
				g_vPotentialCommands.push_back( g_iCommand );

				if (iCommand_)
					*iCommand_ = iCommand;

				if (!_tcscmp(pName, pCommandName)) // exact match?
				{
//					if (iCommand_)
//						*iCommand_ = iCommand;

					nFound = 1; // Exact match takes precidence over fuzzy matches
					g_vPotentialCommands.erase( g_vPotentialCommands.begin(), g_vPotentialCommands.end() );
					break;
				}
			}
		}
		iCommand++;
	}

//	if (nFound == 1)
//	{
//
//	}

	return nFound;
}

//===========================================================================
void DisplayAmbigiousCommands( int nFound )
{
	TCHAR sText[ CONSOLE_WIDTH ];
	wsprintf( sText, TEXT("Ambiguous %d Commands:"), g_vPotentialCommands.size() );
	ConsoleBufferPush( sText );

	int iCommand = 0;
	while (iCommand < nFound)
	{
		TCHAR sPotentialCommands[ CONSOLE_WIDTH ] = TEXT(" ");
		int iWidth = _tcslen( sPotentialCommands );
		while ((iCommand < nFound) && (iWidth < g_nConsoleDisplayWidth))
		{
			int nCommand = g_vPotentialCommands[ iCommand ];
			TCHAR *pName = g_aCommands[ nCommand ].m_sName;
			int    nLen = _tcslen( pName );

			wsprintf( sText, TEXT("%s "), pName );
			_tcscat( sPotentialCommands, sText );
			iWidth += nLen + 1;
			iCommand++;
		}
		ConsoleBufferPush( sPotentialCommands );
	}
}

//===========================================================================
Update_t ExecuteCommand (int nArgs) 
{
	LPTSTR name = _tcstok( g_pConsoleInput,TEXT(" ,-="));
	if (!name)
		name = g_pConsoleInput;

	CmdFuncPtr_t pCommand = NULL;
	int nFound = FindCommand( name, pCommand );

	if (nFound > 1)
	{
// ASSERT (nFound == g_vPotentialCommands.size() );
		DisplayAmbigiousCommands( nFound );
		
		return ConsoleUpdate();
//		return ConsoleDisplayError( gaPotentialCommands );
	}
	else
	if (pCommand)
		return pCommand(nArgs);
	else
		return ConsoleDisplayError(TEXT("Illegal Command"));
}



// ________________________________________________________________________________________________

//===========================================================================
bool Get6502Targets (int *pTemp_, int *pFinal_, int * pBytes_)
{
	bool bStatus = false;

	if (! pTemp_)
		return bStatus;

	if (! pFinal_)
		return bStatus;

//	if (! pBytes_)
//		return bStatus;

	*pTemp_   = NO_6502_TARGET;
	*pFinal_  = NO_6502_TARGET;

	if (pBytes_)
		*pBytes_  = 0;	

	bStatus   = true;

	WORD nAddress  = regs.pc;
	BYTE nOpcode   = *(LPBYTE)(mem + nAddress    );
	BYTE nTarget8  = *(LPBYTE)(mem + nAddress + 1);
	WORD nTarget16 = *(LPWORD)(mem + nAddress + 1);

	int eMode = g_aOpcodes[ nOpcode ].nAddressMode;

	switch (eMode)
	{
		case ADDR_ABS:
			*pFinal_ = nTarget16;
			if (pBytes_)
				*pBytes_ = 2;
			break;

		case ADDR_ABSIINDX:
			nTarget16 += regs.x;
			*pTemp_    = nTarget16;
			*pFinal_   = *(LPWORD)(mem + nTarget16);
			if (pBytes_)
				*pBytes_   = 2;
			break;

		case ADDR_ABSX:
			nTarget16 += regs.x;
			*pFinal_   = nTarget16;
			if (pBytes_)
				*pBytes_   = 2;
			break;

		case ADDR_ABSY:
			nTarget16 += regs.y;
			*pFinal_   = nTarget16;
			if (pBytes_)
				*pBytes_   = 2;
			break;

		case ADDR_IABS:
			*pTemp_    = nTarget16;
			*pFinal_   = *(LPWORD)(mem + nTarget16);
			if (pBytes_)
				*pBytes_   = 2;
			break;

		case ADDR_INDX:
			nTarget8  += regs.x;
			*pTemp_    = nTarget8;
			*pFinal_   = *(LPWORD)(mem + nTarget8);
			if (pBytes_)
				*pBytes_   = 2;
			break;

		case ADDR_INDY:
			*pTemp_    = nTarget8;
			*pFinal_   = (*(LPWORD)(mem + nTarget8)) + regs.y;
			if (pBytes_)
				*pBytes_   = 1;
			break;

		case ADDR_IZPG:
			*pTemp_    = nTarget8;
			*pFinal_   = *(LPWORD)(mem + nTarget8);
			if (pBytes_)
				*pBytes_   = 2;
			break;

		case ADDR_ZP:
			*pFinal_   = nTarget8;
			if (pBytes_)
				*pBytes_   = 1;
			break;

		case ADDR_ZP_X:
			*pFinal_   = nTarget8 + regs.x; // shouldn't this wrap around?
			if (pBytes_)
				*pBytes_   = 1;
			break;

		case ADDR_ZP_Y:
			*pFinal_   = nTarget8 + regs.y; // shouldn't this wrap around?
			if (pBytes_)
				*pBytes_   = 1;
			break;

		default:
			if (pBytes_)
				*pBytes_   = 0;
			break;
		
	}

	// wtf??
//	if ((*final >= 0) &&
//	  ((!_tcscmp(g_aOpcodes[*(mem+regs.pc)].mnemonic,TEXT("JMP"))) ||
//	   (!_tcscmp(g_aOpcodes[*(mem+regs.pc)].mnemonic,TEXT("JSR")))))

	// If 6502 is jumping, don't show byte [nAddressTarget]
	if ((*pFinal_ >= 0) &&
		((nOpcode == OPCODE_JSR        ) || // 0x20
		 (nOpcode == OPCODE_JMP_ABS    ) || // 0x4C
		 (nOpcode == OPCODE_JMP_IABS   ) || // 0x6C
		 (nOpcode == OPCODE_JMP_ABSINDX)))  // 0x7C
	{
		*pFinal_ = NO_6502_TARGET;
		if (pBytes_)
			*pBytes_ = 0;
	}
	return bStatus;
}

//===========================================================================
bool InternalSingleStep ()
{
	bool bResult = false;
	_try
	{
		BYTE nOpcode = *(mem+regs.pc);
		int  nOpmode = g_aOpcodes[ nOpcode ].nAddressMode;

		g_aProfileOpcodes[ nOpcode ].m_nCount++;
		g_aProfileOpmodes[ nOpmode ].m_nCount++;

		CpuExecute(g_nDebugStepCycles);
		bResult = true;
	}
	_except (EXCEPTION_EXECUTE_HANDLER)
	{
		bResult = false;
	}

	return bResult;
}


//===========================================================================
void OutputTraceLine ()
{
	// HACK: MAGIC #: 50 -> 64 chars for disassembly
	TCHAR sDisassembly[ 64 ]       ; DrawDisassemblyLine((HDC)0,0,regs.pc, sDisassembly); // Get Disasm String
	TCHAR sFlags[ _6502_NUM_FLAGS+1]; DrawFlags( (HDC)0, 0, regs.ps, sFlags); // Get Flags String

	_ftprintf(g_hTraceFile,
		TEXT("a=%02x x=%02x y=%02x sp=%03x ps=%s   %s\n"),
		(unsigned)regs.a,
		(unsigned)regs.x,
		(unsigned)regs.y,
		(unsigned)regs.sp,
		(LPCTSTR) sFlags,
		(LPCTSTR) sDisassembly);
}

//===========================================================================
int ParseConsoleInput ( LPTSTR pConsoleInput )
{
	int nArg = 0;

	// TODO: need to check for non-quoted command seperator ';', and buffer input
	RemoveWhiteSpaceReverse( pConsoleInput );

	ArgsClear();
	nArg = ArgsGet( pConsoleInput ); // Get the Raw Args

	int iArg;
	for( iArg = 0; iArg <= nArg; iArg++ )
	{
		g_aArgs[ iArg ] = g_aArgRaw[ iArg ];
	}

	nArg = ArgsParse( nArg ); // Cook them

	return nArg;
}

//===========================================================================
void ParseParameter( )
{
}

// Return address of next line to write to.
//===========================================================================
char * ProfileLinePeek ( int iLine )
{
	char *pText = NULL;

	if (iLine < 0)
		iLine = 0;
	
	if (! g_nProfileLine)
		pText = & g_aProfileLine[ iLine ][ 0 ];

	if (iLine <= g_nProfileLine)
		pText = & g_aProfileLine[ iLine ][ 0 ];
	
	return pText;
}

//===========================================================================
char * ProfileLinePush ()
{
	if (g_nProfileLine < NUM_PROFILE_LINES)
	{
		g_nProfileLine++;	
	}

	return ProfileLinePeek( g_nProfileLine  );
}

void ProfileLineReset()
{
	g_nProfileLine = 0;
}


#define DELIM "%s"
//===========================================================================
void ProfileFormat( bool bExport, ProfileFormat_e eFormatMode )
{
	char sSeperator7[ 32 ] = "\t";
	char sSeperator2[ 32 ] = "\t";
	char sSeperator1[ 32 ] = "\t";
	char sOpcode [ 8 ]; // 2 chars for opcode in hex, plus quotes on either side
	char sAddress[MAX_OPMODE_NAME];

	if (eFormatMode == PROFILE_FORMAT_COMMA)
	{
		sSeperator7[0] = ',';
		sSeperator2[0] = ',';
		sSeperator1[0] = ',';
	}
	else
	if (eFormatMode == PROFILE_FORMAT_SPACE)
	{
		sprintf( sSeperator7, "       " ); // 7
		sprintf( sSeperator2, "  "      ); // 2
		sprintf( sSeperator1, " "       ); // 1
	}

	ProfileLineReset();
	char *pText = ProfileLinePeek( 0 );

	int iOpcode;
	int iOpmode;

	bool bOpcodeGood = true;
	bool bOpmodeGood = true;

	vector< ProfileOpcode_t > vProfileOpcode( &g_aProfileOpcodes[0], &g_aProfileOpcodes[ NUM_OPCODES ] );
	vector< ProfileOpmode_t > vProfileOpmode( &g_aProfileOpmodes[0], &g_aProfileOpmodes[ NUM_OPMODES ] ); 

	// sort >
	sort( vProfileOpcode.begin(), vProfileOpcode.end(), ProfileOpcode_t() );
	sort( vProfileOpmode.begin(), vProfileOpmode.end(), ProfileOpmode_t() );

	Profile_t nOpcodeTotal = 0;
	Profile_t nOpmodeTotal = 0;

	for (iOpcode = 0; iOpcode < NUM_OPCODES; ++iOpcode )
	{
		nOpcodeTotal += vProfileOpcode[ iOpcode ].m_nCount;
	}

	for (iOpmode = 0; iOpmode < NUM_OPMODES; ++iOpmode )
	{
		nOpmodeTotal += vProfileOpmode[ iOpmode ].m_nCount;
	}

	if (nOpcodeTotal < 1.)
	{
		nOpcodeTotal = 1;
		bOpcodeGood = false;
	}
	
	
// Opcode
	if (bExport) // Export = SeperateColumns
		sprintf( pText
			, "\"Percent\"" DELIM "\"Count\"" DELIM "\"Opcode\"" DELIM "\"Mnemonic\"" DELIM "\"Addressing Mode\"\n"
			, sSeperator7, sSeperator2, sSeperator1, sSeperator1 );
	else
		sprintf( pText
			, "Percent" DELIM "Count" DELIM "Mnemonic" DELIM "Addressing Mode\n"
			, sSeperator7, sSeperator2, sSeperator1 );

	pText = ProfileLinePush();
			
	for (iOpcode = 0; iOpcode < NUM_OPCODES; ++iOpcode )
	{
		ProfileOpcode_t tProfileOpcode = vProfileOpcode.at( iOpcode );

		Profile_t nCount  = tProfileOpcode.m_nCount;

		// Don't spam with empty data if dumping to the console
		if ((! nCount) && (! bExport))
			continue;

		int       nOpcode = tProfileOpcode.m_iOpcode;
		int       nOpmode = g_aOpcodes[ nOpcode ].nAddressMode;
		double    nPercent = (100. * nCount) / nOpcodeTotal;
		
		char sOpmode[ MAX_OPMODE_FORMAT ];
		sprintf( sOpmode, g_aOpmodes[ nOpmode ].m_sFormat, 0 );

		if (bExport)
		{
			// Excel Bug: Quoted numbers are NOT treated as strings in .csv! WTF?
			// @reference: http://support.microsoft.com/default.aspx?scid=kb;EN-US;Q214233
			//
			// Workaround: Prefix with (') apostrophe -- this doesn't break HEX2DEC()
			// This works properly in Openoffice.
			// In Excel, this ONLY works IF you TYPE it in!
			// 
			// Solution: Quote the numbers, but you must select the "TEXT" Column data format for the "Opcode" column.
			// We don't use .csv, since you aren't given the Import Dialog in Excel!
			sprintf( sOpcode, "\"%02X\"", nOpcode ); // Works with Excel, IF using Import dialog & choose Text. (also works with OpenOffice)
//			sprintf( sOpcode, "'%02X", nOpcode ); // SHOULD work with Excel, but only works with OpenOffice.
			sprintf( sAddress, "\"%s\"", g_aOpmodes[ nOpmode ].m_sName );
		}
		else // not qouted if dumping to console
		{
			sprintf( sOpcode, "%02X", nOpcode ); 
			strcpy( sAddress, g_aOpmodes[ nOpmode ].m_sName );
		}
		
		// BUG: Yeah 100% is off by 1 char. Profiling only one opcode isn't worth fixing this visual alignment bug.
		sprintf( pText,
			"%7.4f%%" DELIM "%9u" DELIM "%s" DELIM "%s" DELIM "%s\n"
			, nPercent, sSeperator2
			, static_cast<unsigned int>(nCount), sSeperator2
			, sOpcode, sSeperator2
			, g_aOpcodes[ nOpcode ].sMnemonic, sSeperator2
			, sAddress
		);
		pText = ProfileLinePush();
	}

	if (! bOpcodeGood)
		nOpcodeTotal = 0;

	sprintf( pText
		, "Total:  " DELIM "%9u\n"
		, sSeperator2
		, static_cast<unsigned int>(nOpcodeTotal) );
	pText = ProfileLinePush();

	sprintf( pText, "\n" );
	pText = ProfileLinePush();

// Opmode
	//	"Percent     Count  Adressing Mode\n" );
	if (bExport)
		// Note: 2 extra dummy columns are inserted to keep Addressing Mode in same column
		sprintf( pText
			, "\"Percent\"" DELIM "\"Count\"" DELIM DELIM DELIM "\"Addressing Mode\"\n"
			, sSeperator7, sSeperator2, sSeperator2, sSeperator2 );
	else
		sprintf( pText
			, "Percent" DELIM "Count" DELIM "Addressing Mode\n"
			, sSeperator7, sSeperator2 );
	pText = ProfileLinePush();

	if (nOpmodeTotal < 1)
	{
		nOpmodeTotal = 1.;
		bOpmodeGood = false;
	}

	for (iOpmode = 0; iOpmode < NUM_OPMODES; ++iOpmode )
	{
		ProfileOpmode_t tProfileOpmode = vProfileOpmode.at( iOpmode );
		Profile_t nCount  = tProfileOpmode.m_nCount;

		// Don't spam with empty data if dumping to the console
		if ((! nCount) && (! bExport))
			continue;

		int       nOpmode = tProfileOpmode.m_iOpmode;
		double    nPercent = (100. * nCount) / nOpmodeTotal;

		if (bExport)
		{
			// Note: 2 extra dummy columns are inserted to keep Addressing Mode in same column
			sprintf( sAddress, "%s%s\"%s\"", sSeperator1, sSeperator1, g_aOpmodes[ nOpmode ].m_sName );
		}
		else // not qouted if dumping to console
		{
			strcpy( sAddress, g_aOpmodes[ nOpmode ].m_sName );
		}

		// BUG: Yeah 100% is off by 1 char. Profiling only one opcode isn't worth fixing this visual alignment bug.
		sprintf( pText
			, "%7.4f%%" DELIM "%9u" DELIM "%s\n"
			, nPercent, sSeperator2
			, static_cast<unsigned int>(nCount), sSeperator2
			, sAddress
		);
		pText = ProfileLinePush();
	}

	if (! bOpmodeGood)
		nOpmodeTotal = 0;

	sprintf( pText
		, "Total:  " DELIM "%9u\n"
		, sSeperator2 
		, static_cast<unsigned int>(nOpmodeTotal) );
	pText = ProfileLinePush();

	sprintf( pText, "\n" );
	pText = ProfileLinePush();
}
#undef DELIM


//===========================================================================
void ProfileReset()
{
	int iOpcode;
	int iOpmode;

	for (iOpcode = 0; iOpcode < NUM_OPCODES; iOpcode++ )
	{
		g_aProfileOpcodes[ iOpcode ].m_iOpcode = iOpcode;
		g_aProfileOpcodes[ iOpcode ].m_nCount = 0;
	}

	for (iOpmode = 0; iOpmode < NUM_OPMODES; iOpmode++ )
	{
		g_aProfileOpmodes[ iOpmode ].m_iOpmode = iOpmode;
		g_aProfileOpmodes[ iOpmode ].m_nCount = 0;
	}
}


//===========================================================================
bool ProfileSave()
{
	bool bStatus = false;

	TCHAR filename[MAX_PATH];
	_tcscpy(filename,progdir);
	_tcscat(filename,g_FileNameProfile ); // TEXT("Profile.txt")); // =PATCH MJP

	FILE *hFile = fopen(filename,TEXT("wt"));

	if (hFile)
	{
		char *pText;
		int   nLine = g_nProfileLine;
		int   iLine;
		
		for( iLine = 0; iLine < nLine; iLine++ )
		{
			pText = ProfileLinePeek( iLine );
			if ( pText )
			{
				fputs( pText, hFile );
			}
		}
		
		fclose( hFile );
		bStatus = true;
	}
	return bStatus;
}


//===========================================================================
int DebugDrawText ( LPCTSTR pText, RECT & rRect )
{
	int nLen = _tcslen( pText );
	ExtTextOut( g_hDC,
		rRect.left, rRect.top,
		ETO_CLIPPED | ETO_OPAQUE, &rRect,
		pText, nLen,
		NULL );
	return nLen;
}


// Also moves cursor 'non proportional' font width, using FONT_INFO
//===========================================================================
int DebugDrawTextFixed ( LPCTSTR pText, RECT & rRect )
{
	int nFontWidth = g_aFontConfig[ FONT_INFO ]._nFontWidthAvg;

	int nChars = DebugDrawText( pText, rRect );
	rRect.left += (nFontWidth * nChars);
	return nChars;
}


//===========================================================================
int DebugDrawTextLine ( LPCTSTR pText, RECT & rRect )
{
	int nChars = DebugDrawText( pText, rRect );
	rRect.top    += g_nFontHeight;
	rRect.bottom += g_nFontHeight;
	return nChars;
}


// Moves cursor 'proportional' font width
//===========================================================================
int DebugDrawTextHorz ( LPCTSTR pText, RECT & rRect )
{
	int nFontWidth = g_aFontConfig[ FONT_DISASM_DEFAULT ]._nFontWidthAvg;

	SIZE size;
	int nChars = DebugDrawText( pText, rRect );
	if (GetTextExtentPoint32( g_hDC, pText, nChars,  &size ))
	{
		rRect.left += size.cx;
	}
	else
	{
		rRect.left += (nFontWidth * nChars);
	}
	return nChars;
}


//  _____________________________________________________________________________________
// |                                                                                     |
// |                           Public Functions                                          |
// |                                                                                     |
// |_____________________________________________________________________________________|

//===========================================================================
void DebugBegin ()
{
	//	ConsoleInputReset(); already called in DebugInitialize()
	TCHAR sText[ CONSOLE_WIDTH ];

	if (_tcscmp( g_aCommands[ NUM_COMMANDS ].m_sName, TEXT(__COMMANDS_VERIFY_TXT__)))
	{
		wsprintf( sText, "*** ERROR *** Commands mis-matched!" );
		MessageBox( framewindow, sText, TEXT("ERROR"), MB_OK );
	}

	if (_tcscmp( g_aParameters[ NUM_PARAMS ].m_sName, TEXT(__PARAMS_VERIFY_TXT__)))
	{
		wsprintf( sText, "*** ERROR *** Parameters mis-matched!" );
		MessageBox( framewindow, sText, TEXT("ERROR"), MB_OK );
	}

	// Check all summary help to see if it fits within the console
	for (int iCmd = 0; iCmd < NUM_COMMANDS; iCmd++ )
	{	
		char *pHelp = g_aCommands[ iCmd ].pHelpSummary;
		if (pHelp)
		{
			int nLen = _tcslen( pHelp ) + 2;
			if (nLen > CONSOLE_WIDTH)
			{
				wsprintf( sText, TEXT("Warning: %s help is %d chars"),
					pHelp, nLen );
				ConsoleBufferPush( sText );
			}
		}
	}
	

#if _DEBUG
//g_bConsoleBufferPaused = true;
#endif

	CmdMOTD(0);
	
	if (cpuemtype == CPU_FASTPAGING)
	{
		MemSetFastPaging(0);
	}

	if (!membank)
	{
		membank = mem;
	}

	mode = MODE_DEBUG;
	FrameRefreshStatus(DRAW_TITLE);

// TODO:FIXME //e uses 65C02, ][ uses 6502
	if (apple2e)
		g_aOpcodes = & g_aOpcodes65C02[ 0 ]; // Enhanced Apple //e
	else
		g_aOpcodes = & g_aOpcodes6502[ 0 ]; // Original Apple ][

	g_aOpmodes[ ADDR_INVALID2 ].m_nBytes = apple2e ? 2 : 1;
	g_aOpmodes[ ADDR_INVALID3 ].m_nBytes = apple2e ? 3 : 1;

	g_nDisasmCurAddress = regs.pc;
	DisasmCalcTopBotAddress();

	UpdateDisplay( UPDATE_ALL ); // 1
}

//===========================================================================
void DebugContinueStepping ()
{
	static unsigned nStepsTaken = 0;

	if (g_nDebugSkipLen > 0)
	{
		if ((regs.pc >= g_nDebugSkipStart) && (regs.pc < (g_nDebugSkipStart + g_nDebugSkipLen)))
		{
			// Enter go mode
			g_nDebugSteps = -1;
//			g_nDebugStepUntil = -1; // Could already be set via G
			mode = MODE_STEPPING;
		}
		else
		{
			// Enter step mode
			g_nDebugSteps = 1;
//			g_nDebugStepUntil = -1; // Could already be set via G
			mode = MODE_STEPPING;
		}
	}

	if (g_nDebugSteps)
	{
		if (g_hTraceFile)
			OutputTraceLine();
		lastpc = regs.pc;

		bool bBreak = CheckBreakpointsIO();

		InternalSingleStep();

		if (CheckBreakpointsReg())
			bBreak = true;

		if ((regs.pc == g_nDebugStepUntil) || bBreak)
			g_nDebugSteps = 0;
		else if (g_nDebugSteps > 0)
			g_nDebugSteps--;
	}

	if (g_nDebugSteps)
	{
		if (!((++nStepsTaken) & 0xFFFF))
		{
			if (nStepsTaken == 0x10000)
				VideoRedrawScreen();
			else
				VideoRefreshScreen();
		}
	}
	else
	{
		mode = MODE_DEBUG;
		FrameRefreshStatus(DRAW_TITLE);
// BUG: PageUp, Trace - doesn't center cursor

//		if ((g_nDebugStepStart < regs.pc) && (g_nDebugStepStart+3 >= regs.pc))
		// Still within current disasm "window"?
/*
		if ((regs.pc >= g_nDisasmTopAddress) && (regs.pc <= g_nDisasmBotAddress))
		{
			int eMode = g_aOpcodes[*(mem+g_nDisasmCurAddress)].addrmode;
			int nBytes = g_aOpmodes[ eMode ]._nBytes;
			g_nDisasmCurAddress += nBytes;
//			g_nDisasmTopAddress += nBytes;
//			g_nDisasmBotAddress += nBytes;
		}
		else
*/
		{
			g_nDisasmCurAddress = regs.pc;
		}

		DisasmCalcTopBotAddress();

//		g_nDisasmCurAddress += g_aOpmodes[g_aOpcodes[*(mem+g_nDisasmCurAddress)].addrmode]._nBytes;
//		DisasmCalcTopBotAddress();

		Update_t bUpdate = UPDATE_ALL;
//		if (nStepsTaken >= 0x10000) // HACK_MAGIC_NUM
//			bUpdate = UPDATE_ALL;

		UpdateDisplay( bUpdate ); // nStepsTaken >= 0x10000);
		nStepsTaken = 0;
	}
}

//===========================================================================
void DebugDestroy ()
{
	DebugEnd();

	for (int iFont = 0; iFont < NUM_FONTS; iFont++ )
	{
		DeleteObject( g_aFontConfig[ iFont ]._hFont );
		g_aFontConfig[ iFont ]._hFont = NULL;
	}
//	DeleteObject(g_hFontDisasm  );
//	DeleteObject(g_hFontDebugger);
//	DeleteObject(g_hFontWebDings);

	for( int iTable = 0; iTable < NUM_SYMBOL_TABLES; iTable++ )
	{
		_CmdSymbolsClear( (Symbols_e) iTable );
	}
}


// Sub Window _____________________________________________________________________________________

//===========================================================================
bool CanDrawDebugger()
{
	if (g_bDebuggerViewingAppleOutput)
		return false;

	if ((mode == MODE_DEBUG) || (mode == MODE_STEPPING))
		return true;

	return false;
}


//===========================================================================
void DrawWindowBottom ( Update_t bUpdate, int iWindow )
{
	if (! g_aWindowConfig[ iWindow ].bSplit)
		return;

	WindowSplit_t * pWindow = &g_aWindowConfig[ iWindow ];

//	if (pWindow->eBot == WINDOW_DATA)
//	{
//		DrawWindow_Data( bUpdate, false );
//	}
	
	if (pWindow->eBot == WINDOW_SOURCE)
		DrawSubWindow_Source2( bUpdate );
}

//===========================================================================
void DrawSubWindow_Code ( int iWindow )
{
	int nLines = g_nDisasmWinHeight;

	// Check if we have a bad disasm
	// BUG: This still doesn't catch all cases
	// G FB53, SPACE, PgDn * 
	// Note: DrawDisassemblyLine() has kludge.
//		DisasmCalcTopFromCurAddress( false );
	// These should be functionally equivalent.
	//	DisasmCalcTopFromCurAddress();
	//	DisasmCalcBotFromTopAddress();
	SelectObject( g_hDC, g_aFontConfig[ FONT_DISASM_DEFAULT ]._hFont ); // g_hFontDisasm 

	WORD nAddress = g_nDisasmTopAddress; // g_nDisasmCurAddress;
	for (int iLine = 0; iLine < nLines; iLine++ )
	{
		nAddress += DrawDisassemblyLine( g_hDC, iLine, nAddress, NULL);
	}

	SelectObject( g_hDC, g_aFontConfig[ FONT_INFO ]._hFont ); // g_hFontDebugger
}

//===========================================================================
void DrawSubWindow_Console (Update_t bUpdate)
{
	if (! CanDrawDebugger())
		return;

	SelectObject( g_hDC, g_aFontConfig[ FONT_CONSOLE ]._hFont );

//	static TCHAR sConsoleBlank[ CONSOLE_WIDTH ];
	
	if ((bUpdate & UPDATE_CONSOLE_INPUT) || (bUpdate & UPDATE_CONSOLE_DISPLAY))
	{
		SetTextColor( g_hDC, DebugGetColor( FG_CONSOLE_OUTPUT )); // COLOR_FG_CONSOLE
		SetBkColor(   g_hDC, DebugGetColor( BG_CONSOLE_OUTPUT )); // COLOR_BG_CONSOLE

//		int nLines = MIN(g_nConsoleDisplayTotal - g_iConsoleDisplayStart, g_nConsoleDisplayHeight);
		int iLine = g_iConsoleDisplayStart + CONSOLE_FIRST_LINE;
		for (int y = 0; y < g_nConsoleDisplayHeight ; y++ )
		{
			if (iLine <= (g_nConsoleDisplayTotal + CONSOLE_FIRST_LINE))
			{
				DrawConsoleLine( g_aConsoleDisplay[ iLine ], y+1 );
			}
			iLine++;
//			else
//				DrawConsoleLine( sConsoleBlank, y );
		}

		DrawConsoleInput( g_hDC );
	}
}	

//===========================================================================
void DrawSubWindow_Data (Update_t bUpdate)
{
	HDC hDC = g_hDC;
	int iBackground;	

	const int nMaxOpcodes = WINDOW_DATA_BYTES_PER_LINE;
	TCHAR sAddress  [ 5];
	TCHAR sOpcodes  [(nMaxOpcodes*3)+1] = TEXT("");
	TCHAR sImmediate[ 4 ]; // 'c'

	const int nDefaultFontWidth = 7; // g_aFontConfig[FONT_DISASM_DEFAULT]._nFontWidth or g_nFontWidthAvg
	int X_OPCODE      =  6                    * nDefaultFontWidth;
	int X_CHAR        = (6 + (nMaxOpcodes*3)) * nDefaultFontWidth;

	int iMemDump = 0;

	MemoryDump_t* pMD = &g_aMemDump[ iMemDump ];
	USHORT       nAddress = pMD->nAddress;
	DEVICE_e     eDevice  = pMD->eDevice;
	MemoryView_e iView    = pMD->eView;

	if (!pMD->bActive)
		return;

	int  iByte;
	WORD iAddress = nAddress;

	int iLine;
	int nLines = g_nDisasmWinHeight;

	for (iLine = 0; iLine < nLines; iLine++ )
	{
		iAddress = nAddress;

	// Format
		wsprintf( sAddress, TEXT("%04X"), iAddress );

		sOpcodes[0] = 0;
		for ( iByte = 0; iByte < nMaxOpcodes; iByte++ )
		{
			BYTE nData = (unsigned)*(LPBYTE)(membank + iAddress + iByte);
			wsprintf( &sOpcodes[ iByte * 3 ], TEXT("%02X "), nData );
		}
		sOpcodes[ nMaxOpcodes * 3 ] = 0;

		int nFontHeight = g_aFontConfig[ FONT_DISASM_DEFAULT ]._nLineHeight;

	// Draw
		RECT rect;
		rect.left   = 0;
		rect.top    = iLine * nFontHeight;
		rect.right  = DISPLAY_DISASM_RIGHT;
		rect.bottom = rect.top + nFontHeight;

		if (iLine & 1)
		{
			iBackground = BG_DATA_1;
		}
		else
		{
			iBackground = BG_DATA_2;
		}
		SetBkColor( hDC, DebugGetColor( iBackground ) );

		SetTextColor( hDC, DebugGetColor( FG_DISASM_ADDRESS ) );
		DebugDrawTextHorz( (LPCTSTR) sAddress, rect );

		SetTextColor( hDC, DebugGetColor( FG_DISASM_OPERATOR ) );
		if (g_bConfigDisasmAddressColon)
			DebugDrawTextHorz( TEXT(":"), rect );

		rect.left = X_OPCODE;

		SetTextColor( hDC, DebugGetColor( FG_DATA_BYTE ) );
		DebugDrawTextHorz( (LPCTSTR) sOpcodes, rect );

		rect.left = X_CHAR;

	// Seperator
		SetTextColor( hDC, DebugGetColor( FG_DISASM_OPERATOR ));
		DebugDrawTextHorz( (LPCSTR) TEXT("  |  " ), rect );


	// Plain Text
		SetTextColor( hDC, DebugGetColor( FG_DISASM_CHAR ) );

		MemoryView_e eView = pMD->eView;
		if ((eView != MEM_VIEW_ASCII) && (eView != MEM_VIEW_APPLE))
			eView = MEM_VIEW_ASCII;

		iAddress = nAddress;
		for (iByte = 0; iByte < nMaxOpcodes; iByte++ )
		{
			BYTE nImmediate = (unsigned)*(LPBYTE)(membank + iAddress);
			int iTextBackground = iBackground;
			if ((iAddress >= _6502_IO_BEGIN) && (iAddress <= _6502_IO_END))
			{
				iTextBackground = BG_INFO_IO_BYTE;
			}

			ColorizeSpecialChar( hDC, sImmediate, (BYTE) nImmediate, eView, iBackground );
			DebugDrawTextHorz( (LPCSTR) sImmediate, rect );

			iAddress++;
		}
/*
	// Colorized Text
		iAddress = nAddress;
		for (iByte = 0; iByte < nMaxOpcodes; iByte++ )
		{
			BYTE nImmediate = (unsigned)*(LPBYTE)(membank + iAddress);
			int iTextBackground = iBackground; // BG_INFO_CHAR;
//pMD->eView == MEM_VIEW_HEX
			if ((iAddress >= _6502_IO_BEGIN) && (iAddress <= _6502_IO_END))
				iTextBackground = BG_INFO_IO_BYTE;

			ColorizeSpecialChar( hDC, sImmediate, (BYTE) nImmediate, MEM_VIEW_APPLE, iBackground );
			DebugDrawTextHorz( (LPCSTR) sImmediate, rect );

			iAddress++;
		}

		SetBkColor( hDC, DebugGetColor( iBackground ) ); // Hack, colorize Char background "spills over to EOL"
		DebugDrawTextHorz( (LPCSTR) TEXT(" " ), rect );
*/
		SetBkColor( hDC, DebugGetColor( iBackground ) ); // HACK: Colorize() can "spill over" to EOL

		SetTextColor( hDC, DebugGetColor( FG_DISASM_OPERATOR ));
		DebugDrawTextHorz( (LPCSTR) TEXT("  |  " ), rect );

		nAddress += nMaxOpcodes;
	}
}


// DrawRegisters();
//===========================================================================
void DrawSubWindow_Info( int iWindow )
{
	if (g_iWindowThis == WINDOW_CONSOLE)
		return;

	const TCHAR **sReg = g_aBreakpointSource;

	DrawStack(g_hDC,0);
	DrawTargets(g_hDC,9);
	DrawRegister(g_hDC,12, sReg[ BP_SRC_REG_A ] , 1, regs.a , PARAM_REG_A  );
	DrawRegister(g_hDC,13, sReg[ BP_SRC_REG_X ] , 1, regs.x , PARAM_REG_X  );
	DrawRegister(g_hDC,14, sReg[ BP_SRC_REG_Y ] , 1, regs.y , PARAM_REG_Y  );
	DrawRegister(g_hDC,15, sReg[ BP_SRC_REG_PC] , 2, regs.pc, PARAM_REG_PC );
	DrawRegister(g_hDC,16, sReg[ BP_SRC_REG_S ] , 2, regs.sp, PARAM_REG_SP );
	DrawFlags(g_hDC,17,regs.ps,NULL);
	DrawZeroPagePointers(g_hDC,19);

#if defined(SUPPORT_Z80_EMU) && defined(OUTPUT_Z80_REGS)
	DrawRegister(g_hDC,19,TEXT("AF"),2,*(WORD*)(membank+REG_AF));
	DrawRegister(g_hDC,20,TEXT("BC"),2,*(WORD*)(membank+REG_BC));
	DrawRegister(g_hDC,21,TEXT("DE"),2,*(WORD*)(membank+REG_DE));
	DrawRegister(g_hDC,22,TEXT("HL"),2,*(WORD*)(membank+REG_HL));
	DrawRegister(g_hDC,23,TEXT("IX"),2,*(WORD*)(membank+REG_IX));
#endif

#if DEBUG_FORCE_DISPLAY
	if (true)
#else
	if (g_nBreakpoints)
#endif
		DrawBreakpoints(g_hDC,0);

#if DEBUG_FORCE_DISPLAY
	if (true)
#else
	if (g_nWatches)
#endif
		DrawWatches(g_hDC,7);

#if DEBUG_FORCE_DISPLAY
	if (true)
#else
	if (g_aMemDump[0].bActive)
#endif
		DrawMemory(g_hDC, 14, 0 ); // g_aMemDump[0].nAddress, g_aMemDump[0].eDevice);

#if DEBUG_FORCE_DISPLAY
	if (true)
#else
	if (g_aMemDump[1].bActive)
#endif
		DrawMemory(g_hDC, 19, 1 ); // g_aMemDump[1].nAddress, g_aMemDump[1].eDevice);

}

//===========================================================================
void DrawSubWindow_IO (Update_t bUpdate)
{
}

//===========================================================================
void DrawSubWindow_Source (Update_t bUpdate)
{
}


//===========================================================================
int FindSourceLine( WORD nAddress )
{
	int iAddress = 0;
	int iLine = 0;
	int iSourceLine = NO_SOURCE_LINE;

//	iterate of <address,line>
//	probably should be sorted by address
//	then can do binary search
//	iSourceLine = g_aSourceDebug.find( nAddress );
#if 0 // _DEBUG
	{
		TCHAR sText[ CONSOLE_WIDTH ];
		for (int i = 0; i < g_vSourceLines.size(); i++ )
		{
			wsprintf( sText, "%d: %s\n", i, g_vSourceLines[ i ] );
			OutputDebugString( sText );
		}
	}
#endif

	SourceAssembly_t::iterator iSource = g_aSourceDebug.begin();
	while (iSource != g_aSourceDebug.end() )
	{
		iAddress = iSource->first;
		iLine = iSource->second;

#if 0 // _DEBUG
	TCHAR sText[ CONSOLE_WIDTH ];
	wsprintf( sText, "%04X -> %d line\n", iAddress, iLine );
	OutputDebugString( sText );
#endif

		if (iAddress == nAddress)
		{
			iSourceLine = iLine;
			break;
		}

		iSource++;
	}
	// not found

	return iSourceLine;
}

//===========================================================================
void DrawSourceLine( int iSourceLine, RECT &rect )
{
	TCHAR sLine[ CONSOLE_WIDTH ];

	ZeroMemory( sLine, CONSOLE_WIDTH );
	if ((iSourceLine >=0) && (iSourceLine < g_nSourceAssemblyLines))
	{
		char * pSource = g_vSourceLines[ iSourceLine ];

//		int nLenSrc = _tcslen( pSource );
//		if (nLenSrc >= CONSOLE_WIDTH)
//			bool bStop = true;

		TextConvertTabsToSpaces( sLine, pSource, CONSOLE_WIDTH-1 ); // bugfix 2,3,1,15: fence-post error, buffer over-run

//		int nLenTab = _tcslen( sLine );
	}
	else
	{
		_tcscpy( sLine, TEXT(" "));
	}

	DebugDrawText( sLine, rect );
	rect.top += g_nFontHeight;
//	iSourceLine++;
}

//===========================================================================
void DrawSubWindow_Source2 (Update_t bUpdate)
{
//	if (! g_bSourceLevelDebugging)
//		return;

	SetTextColor( g_hDC, DebugGetColor( FG_SOURCE ));

	int iSource = g_iSourceDisplayStart;
	int nLines = g_nDisasmWinHeight;

	int y = g_nDisasmWinHeight;
	int nHeight = g_nDisasmWinHeight;

	if (g_aWindowConfig[ g_iWindowThis ].bSplit) // HACK: Split Window Height is odd, so bottom window gets +1 height
		nHeight++;

	RECT rect;
	rect.top    = (y * g_nFontHeight);
	rect.bottom = rect.top + (nHeight * g_nFontHeight);
	rect.left = 0;
	rect.right = DISPLAY_DISASM_RIGHT; // HACK: MAGIC #: 7

// Draw Title
	TCHAR sTitle[ CONSOLE_WIDTH ];
	TCHAR sText [ CONSOLE_WIDTH ];
	_tcscpy( sTitle, TEXT("   Source: "));
	_tcsncpy( sText, g_aSourceFileName, g_nConsoleDisplayWidth - _tcslen( sTitle ) - 1 );
	_tcscat( sTitle, sText );

	SetBkColor(   g_hDC, DebugGetColor( BG_SOURCE_TITLE ));
	SetTextColor( g_hDC, DebugGetColor( FG_SOURCE_TITLE ));
	DebugDrawText( sTitle, rect );
	rect.top += g_nFontHeight;

// Draw Source Lines
	int iBackground;
	int iForeground;

	int iSourceCursor = 2; // (g_nDisasmWinHeight / 2);
	int iSourceLine = FindSourceLine( regs.pc );

	if (iSourceLine == NO_SOURCE_LINE)
	{
		iSourceCursor = NO_SOURCE_LINE;
	}
	else
	{
		iSourceLine -= iSourceCursor;
		if (iSourceLine < 0)
			iSourceLine = 0;
	}

	for( int iLine = 0; iLine < nLines; iLine++ )
	{
		if (iLine != iSourceCursor)
		{
			iBackground = BG_SOURCE_1;
			if (iLine & 1)
				iBackground = BG_SOURCE_2;
			iForeground = FG_SOURCE;
		}
		else
		{
			// Hilighted cursor line
			iBackground = BG_DISASM_PC_X; // _C
			iForeground = FG_DISASM_PC_X; // _C
		}
		SetBkColor(   g_hDC, DebugGetColor( iBackground ));
		SetTextColor( g_hDC, DebugGetColor( iForeground ));

		DrawSourceLine( iSourceLine, rect );
		iSourceLine++;
	}
}

//===========================================================================
void DrawSubWindow_Symbols (Update_t bUpdate)
{
}

//===========================================================================
void DrawSubWindow_ZeroPage (Update_t bUpdate)
{
}

// Main Windows ___________________________________________________________________________________

//===========================================================================
void DrawWindow_Code( Update_t bUpdate )
{
	DrawSubWindow_Code( g_iWindowThis );

//	DrawWindowTop( g_iWindowThis );
	DrawWindowBottom( bUpdate, g_iWindowThis );

	DrawSubWindow_Info( g_iWindowThis );
}

//===========================================================================
void DrawWindow_Console( Update_t bUpdate )
{
	// Nothing to do, except draw background, since text handled by DrawSubWindow_Console()
    RECT viewportrect;
    viewportrect.left   = 0;
    viewportrect.top    = 0;
    viewportrect.right  = DISPLAY_WIDTH;
    viewportrect.bottom = DISPLAY_HEIGHT - DEFAULT_HEIGHT; // 368 = 23 lines // TODO/FIXME

// TODO/FIXME: COLOR_BG_CODE -> g_iWindowThis, once all tab backgrounds are listed first in g_aColors !
    SetBkColor(g_hDC, DebugGetColor( BG_DISASM_2 )); // COLOR_BG_CODE
	// Can't use DebugDrawText, since we don't ned the CLIPPED flag
	// TODO: add default param OPAQUE|CLIPPED
    ExtTextOut( g_hDC
		,0,0
		,ETO_OPAQUE
		,&viewportrect
		,TEXT("")
		,0
		,NULL
	);
}

//===========================================================================
void DrawWindow_Data( Update_t bUpdate )
{
	DrawSubWindow_Data( g_iWindowThis );
	DrawSubWindow_Info( g_iWindowThis );
}

//===========================================================================
void DrawWindow_IO( Update_t bUpdate )
{
	DrawSubWindow_IO( g_iWindowThis );
	DrawSubWindow_Info( g_iWindowThis );
}

//===========================================================================
void DrawWindow_Source( Update_t bUpdate )
{
	DrawSubWindow_Source( g_iWindowThis );
	DrawSubWindow_Info( g_iWindowThis );
}

//===========================================================================
void DrawWindow_Symbols( Update_t bUpdate )
{
	DrawSubWindow_Symbols( g_iWindowThis );
	DrawSubWindow_Info( g_iWindowThis );
}

void DrawWindow_ZeroPage( Update_t bUpdate )
{
	DrawSubWindow_ZeroPage( g_iWindowThis );
	DrawSubWindow_Info( g_iWindowThis );
}

//===========================================================================
void DrawWindowBackground_Main( int g_iWindowThis )
{
    RECT viewportrect;
    viewportrect.left   = 0;
    viewportrect.top    = 0;
    viewportrect.right  = SCREENSPLIT1 - 6; // HACK: MAGIC #: 14 -> 6 -> (g_nFonWidthAvg-1)
    viewportrect.bottom = DISPLAY_HEIGHT - DEFAULT_HEIGHT; // 368 = 23 lines // TODO/FIXME
// g_nFontHeight * g_nDisasmWinHeight; // 304

// TODO/FIXME: COLOR_BG_CODE -> g_iWindowThis, once all tab backgrounds are listed first in g_aColors !

    SetBkColor(g_hDC, DebugGetColor( BG_DISASM_1 )); // COLOR_BG_CODE
	// Can't use DebugDrawText, since we don't need CLIPPED
    ExtTextOut(g_hDC,0,0,ETO_OPAQUE,&viewportrect,TEXT(""),0,NULL);
}

//===========================================================================
void DrawWindowBackground_Info( int g_iWindowThis )
{
    RECT viewportrect;
    viewportrect.top    = 0;
    viewportrect.left   = SCREENSPLIT1 - 6; // 14 // HACK: MAGIC #: 14 -> (g_nFontWidthAvg-1)
    viewportrect.right  = 560;
    viewportrect.bottom = DISPLAY_HEIGHT; //g_nFontHeight * MAX_DISPLAY_INFO_LINES; // 384

	SetBkColor(g_hDC, DebugGetColor( BG_INFO )); // COLOR_BG_DATA
	// Can't use DebugDrawText, since we don't need CLIPPED
	ExtTextOut(g_hDC,0,0,ETO_OPAQUE,&viewportrect,TEXT(""),0,NULL);
}


//===========================================================================
void UpdateDisplay (Update_t bUpdate)
{
	g_hDC = FrameGetDC();

	SelectObject( g_hDC, g_aFontConfig[ FONT_INFO ]._hFont ); // g_hFontDebugger

	SetTextAlign(g_hDC,TA_TOP | TA_LEFT);

	if ((bUpdate & UPDATE_BREAKPOINTS)
		|| (bUpdate & UPDATE_DISASM)
		|| (bUpdate & UPDATE_FLAGS)
		|| (bUpdate & UPDATE_MEM_DUMP)
		|| (bUpdate & UPDATE_REGS)
		|| (bUpdate & UPDATE_STACK)
		|| (bUpdate & UPDATE_SYMBOLS)
		|| (bUpdate & UPDATE_TARGETS)
		|| (bUpdate & UPDATE_WATCH)
		|| (bUpdate & UPDATE_ZERO_PAGE))
	{
		bUpdate |= UPDATE_BACKGROUND;
		bUpdate |= UPDATE_CONSOLE_INPUT;
	}
	
	if (bUpdate & UPDATE_BACKGROUND)
	{
		if (g_iWindowThis != WINDOW_CONSOLE)
		{
			DrawWindowBackground_Main( g_iWindowThis );
			DrawWindowBackground_Info( g_iWindowThis );
		}
	}
	
	switch( g_iWindowThis )
	{
		case WINDOW_CODE:
			DrawWindow_Code( bUpdate );
			break;

		case WINDOW_CONSOLE:
			DrawWindow_Console( bUpdate );
			break;

		case WINDOW_DATA:
			DrawWindow_Data( bUpdate );
			break;

		case WINDOW_IO:
			DrawWindow_IO( bUpdate );

		case WINDOW_SOURCE:
			DrawWindow_Source( bUpdate );

		case WINDOW_SYMBOLS:
			DrawWindow_Symbols( bUpdate );
			break;

		case WINDOW_ZEROPAGE:
			DrawWindow_ZeroPage( bUpdate );

		default:
			break;
	}

	if ((bUpdate & UPDATE_CONSOLE_DISPLAY) || (bUpdate & UPDATE_CONSOLE_INPUT))
		DrawSubWindow_Console( bUpdate );

	FrameReleaseDC();
	g_hDC = 0;
}

//===========================================================================
void DebugEnd ()
{
	// Stepping ... calls us when key hit?!  FrameWndProc() ProcessButtonClick() DebugEnd()
	if (g_bProfiling)
	{
		// See: .csv / .txt note in CmdProfile()
		ProfileFormat( true, PROFILE_FORMAT_TAB ); // Export in Excel-ready text format.
		ProfileSave();
	}
	
	if (g_hTraceFile)
	{
		fclose(g_hTraceFile);
		g_hTraceFile = NULL;
	}
}

#if _DEBUG
#define DEBUG_COLOR_RAMP 0
void _SetupColorRamp( const int iPrimary, int & iColor_ )
{
	TCHAR sRamp[ CONSOLE_WIDTH*2 ] = TEXT("");
#if DEBUG_COLOR_RAMP
	TCHAR sText[ CONSOLE_WIDTH ];
#endif

	bool bR = (iPrimary & 1) ? true : false;
	bool bG = (iPrimary & 2) ? true : false;
	bool bB = (iPrimary & 4) ? true : false;
	int dStep = 32;
	int nLevels = 256 / dStep;
	for (int iLevel = nLevels; iLevel > 0; iLevel-- )
	{
		int nC = ((iLevel * dStep) - 1);
		int nR = bR ? nC : 0;
		int nG = bG ? nC : 0;
		int nB = bB ? nC : 0;
		DWORD nColor = RGB(nR,nG,nB);
		gaColorPalette[ iColor_ ] = nColor;
#if DEBUG_COLOR_RAMP
	wsprintf( sText, TEXT("RGB(%3d,%3d,%3d), "), nR, nG, nB );
	_tcscat( sRamp, sText );
#endif
		iColor_++;
	}
#if DEBUG_COLOR_RAMP
	wsprintf( sText, TEXT(" // %d%d%d\n"), bB, bG, bR );
	_tcscat( sRamp, sText );
	OutputDebugString( sRamp );
	sRamp[0] = 0;
#endif
}
#endif // _DEBUG

void _CmdColorsReset()
{
//	int iColor = 1; // black only has one level, skip it, since black levels same as white levels
//	for (int iPrimary = 1; iPrimary < 8; iPrimary++ )
//	{
//		_SetupColorRamp( iPrimary, iColor );
//	}

	// Setup default colors
	int iColor;
	for (iColor = 0; iColor < NUM_COLORS; iColor++ )
	{
		COLORREF nColor = gaColorPalette[ g_aColorIndex[ iColor ] ];

		int R = (nColor >>  0) & 0xFF;
		int G = (nColor >>  8) & 0xFF;
		int B = (nColor >> 16) & 0xFF;

		// There are many, many ways of shifting the color domain to the monochrome domain
		// NTSC uses 3x3 matrix, could map RGB -> wavelength, etc.
		int M = (R + G + B) / 3; // Monochrome component
		COLORREF nMono = RGB(M,M,M);

		DebugSetColor( SCHEME_COLOR, iColor, nColor );
		DebugSetColor( SCHEME_MONO , iColor, nMono );
	}
}


//===========================================================================
void DebugInitialize ()
{
	ZeroMemory( g_aConsoleDisplay, sizeof( g_aConsoleDisplay ) ); // CONSOLE_WIDTH * CONSOLE_HEIGHT );
	ConsoleInputReset();

	for( int iWindow = 0; iWindow < NUM_WINDOWS; iWindow++ )
	{
		WindowSplit_t *pWindow = & g_aWindowConfig[ iWindow ];

		pWindow->bSplit = false;
		pWindow->eTop = (Window_e) iWindow;
		pWindow->eBot = (Window_e) iWindow;
	}

	g_iWindowThis = WINDOW_CODE;
	g_iWindowLast = WINDOW_CODE;

	WindowUpdateDisasmSize();

	_CmdColorsReset();

	WindowUpdateConsoleDisplayedSize();

	// CLEAR THE BREAKPOINT AND WATCH TABLES
	ZeroMemory( g_aBreakpoints     , MAX_BREAKPOINTS       * sizeof(Breakpoint_t));
	ZeroMemory( g_aWatches         , MAX_WATCHES           * sizeof(Watches_t) );
	ZeroMemory( g_aZeroPagePointers, MAX_ZEROPAGE_POINTERS * sizeof(ZeroPagePointers_t));

	g_iCommand = CMD_SYMBOLS_MAIN;
	CmdSymbolsLoad(0);

	// CREATE A FONT FOR THE DEBUGGING SCREEN
	int nArgs = _Arg_1( g_sFontNameDefault );
//	CmdConfigSetFont( nArgs );

	for (int iFont = 0; iFont < NUM_FONTS; iFont++ )
	{
		g_aFontConfig[ iFont ]._hFont = NULL;
	}

	// TODO: g_aFontPitch
	_CmdConfigFont( FONT_INFO          , g_sFontNameInfo   , FIXED_PITCH | FF_MODERN      , g_nFontHeight ); // DEFAULT_CHARSET
	_CmdConfigFont( FONT_CONSOLE       , g_sFontNameConsole, FIXED_PITCH | FF_MODERN      , g_nFontHeight ); // DEFAULT_CHARSET
	_CmdConfigFont( FONT_DISASM_DEFAULT, g_sFontNameDisasm , FIXED_PITCH | FF_MODERN      , g_nFontHeight ); // OEM_CHARSET
	_CmdConfigFont( FONT_DISASM_BRANCH , g_sFontNameBranch , DEFAULT_PITCH | FF_DECORATIVE, g_nFontHeight ); // DEFAULT_CHARSET

/*
	g_hFontDebugger = CreateFont( 
		  g_nFontHeight // Height
		, 0 // Width
		, 0 // Escapement
		, 0 // Orientatin
		, FW_MEDIUM // Weight
		, 0 // Italic
		, 0 // Underline
		, 0 // Strike Out
		, DEFAULT_CHARSET // "OEM_CHARSET"  DEFAULT_CHARSET
		, OUT_DEFAULT_PRECIS
		, CLIP_DEFAULT_PRECIS
		, ANTIALIASED_QUALITY // DEFAULT_QUALITY
		, FIXED_PITCH | FF_MODERN // HACK: MAGIC #: 4 // FIXED_PITCH
		, g_sFontNameDefault );

	g_hFontWebDings = CreateFont(
		  g_nFontHeight // Height
		, 0 // Width
		, 0 // Escapement
		, 0 // Orientatin
		, FW_MEDIUM // Weight
		, 0 // Italic
		, 0 // Underline
		, 0 // Strike Out
		, DEFAULT_CHARSET // ANSI_CHARSET  // OEM_CHARSET  DEFAULT_CHARSET
		, OUT_DEFAULT_PRECIS
		, CLIP_DEFAULT_PRECIS
		, ANTIALIASED_QUALITY // DEFAULT_QUALITY
		, DEFAULT_PITCH | FF_DECORATIVE // FIXED_PITCH | 4 | FF_MODERN
		, g_sFontNameBranch );
*/
//	if (g_hFontWebDings)
	if (g_aFontConfig[ FONT_DISASM_BRANCH ]._hFont)
	{
		g_bConfigDisasmFancyBranch = true;
	}
	else
	{
		g_bConfigDisasmFancyBranch = false;
	}	
}


// Add character to the input line
//===========================================================================
void DebugProcessChar (TCHAR ch)
{
	if ((mode == MODE_STEPPING) && (ch == TEXT('\x1B'))) // HACK: ESCAPE
		g_nDebugSteps = 0;

	if (mode != MODE_DEBUG)
		return;

	if (g_bConsoleBufferPaused)
		return;

	// If don't have console input, don't pass space to the input line
	if ((ch == TEXT(' ')) && (!g_nConsoleInputChars))
		return;

	if (g_nConsoleInputChars > (g_nConsoleDisplayWidth-1))
		return;

	if (g_bConsoleInputSkip)
	{
		g_bConsoleInputSkip = false;
		return;
	}

	if ((ch >= ' ') && (ch <= 126)) // HACK MAGIC # 32 -> ' ', # 126 
	{
		if (ch == TEXT('"'))
			g_bConsoleInputQuoted = ! g_bConsoleInputQuoted;

		if (!g_bConsoleInputQuoted)
		{
			ch = (TCHAR)CharUpper((LPTSTR)ch);
		}
		ConsoleInputChar( ch );

		HDC dc = FrameGetDC();
		DrawConsoleInput( dc );
		FrameReleaseDC();
	}
}

//===========================================================================
void DebugProcessCommand (int keycode)
{
	if (mode != MODE_DEBUG)
		return;

	if (g_bDebuggerViewingAppleOutput)
	{
		// Normally any key press takes us out of "Viewing Apple Output" mode
		// VK_F# are already processed, so we can't use them to cycle next video mode
//		    if ((mode != MODE_LOGO) && (mode != MODE_DEBUG))
		g_bDebuggerViewingAppleOutput = false;
		UpdateDisplay( UPDATE_ALL ); // 1
		return;
	}

	Update_t bUpdateDisplay = UPDATE_NOTHING;

	// For long output, allow user to read it
	if (g_nConsoleBuffer)
	{
		if ((VK_SPACE == keycode) || (VK_RETURN == keycode) || (VK_TAB == keycode) || (VK_ESCAPE == keycode))
		{		
			int nLines = MIN( g_nConsoleBuffer, g_nConsoleDisplayHeight - 1 ); // was -2
			if (VK_ESCAPE == keycode) // user doesn't want to read all this stu
			{
				nLines = g_nConsoleBuffer;
			}
			ConsoleBufferTryUnpause( nLines );

			// don't really need since 'else if (keycode = VK_BACK)' but better safe then sorry
			keycode = 0; // don't single-step 
		}

		bUpdateDisplay |= UPDATE_CONSOLE_DISPLAY;
		ConsoleDisplayPause();
	}
	else
 	// If have console input, don't invoke cursor movement
	// TODO: Probably should disable all "movement" keys to map them to line editing mode
 	if ((keycode == VK_SPACE) && g_nConsoleInputChars)
		return;
	else if (keycode == VK_ESCAPE)
	{
		ConsoleInputClear();
		bUpdateDisplay |= UPDATE_CONSOLE_INPUT;
	}
	else if (keycode == VK_BACK)
	{
		if (! ConsoleInputBackSpace())
		{
			// CmdBeep();
		}
		bUpdateDisplay |= UPDATE_CONSOLE_INPUT;
	}
	else if (keycode == VK_RETURN)
	{
		ConsoleDisplayPush( ConsoleInputPeek() );

		if (g_nConsoleInputChars)
		{
			int nArgs = ParseConsoleInput( g_pConsoleInput );
			if (nArgs == ARG_SYNTAX_ERROR)
			{
				TCHAR sText[ CONSOLE_WIDTH ];
				wsprintf( sText, "Syntax error: %s", g_aArgs[0].sArg );
				bUpdateDisplay |= ConsoleDisplayError( sText );
			}
			else
				bUpdateDisplay |= ExecuteCommand( nArgs ); // ParseConsoleInput());

			if (!g_bConsoleBufferPaused)
			{
				ConsoleInputReset();
			}
		}
	}
	else if (keycode == VK_OEM_3) // Tilde ~
	{
		// Switch to Console Window
		if (g_iWindowThis != WINDOW_CONSOLE)
		{
			CmdWindowViewConsole( 0 );
		}
		else // switch back to last window
		{
			CmdWindowLast( 0 );
		}
		bUpdateDisplay |= UPDATE_ALL;
		g_bConsoleInputSkip = true; // don't pass to DebugProcessChar()
	}
	else
	{	
		switch (keycode)
		{
			case VK_TAB:
			{
				if (g_nConsoleInputChars)
				{
					// TODO: TabCompletionCommand()
					// TODO: TabCompletionSymbol()
					bUpdateDisplay |= ConsoleInputTabCompletion();
				}
				else
				if (KeybGetCtrlStatus() && KeybGetShiftStatus())
					bUpdateDisplay |= CmdWindowCyclePrev( 0 );
				else
				if (KeybGetCtrlStatus())
					bUpdateDisplay |= CmdWindowCycleNext( 0 );
				else
					bUpdateDisplay |= CmdCursorJumpPC( CURSOR_ALIGN_CENTER );
				break;
			}
			case VK_SPACE:
				if (KeybGetShiftStatus())
					bUpdateDisplay |= CmdStepOut(0);
				else
				if (KeybGetCtrlStatus())
					bUpdateDisplay |= CmdStepOver(0);
				else
					bUpdateDisplay |= CmdTrace(0);
				break;

			case VK_HOME:
				if (g_iWindowThis == WINDOW_CONSOLE)
				{
					ConsoleScrollHome();
				}
				else
				if (g_nConsoleInputChars > 0)
				{
					if (KeybGetShiftStatus())
					{
						bUpdateDisplay |= ConsoleScrollHome();
					}
					else
					{
						// Move cursor to start of console input
					}
				}
				else
				{
					// If you really want $000 at the top of the screen...
					// g_nDisasmTopAddress = _6502_BEG_MEM_ADDRESS;
					// DisasmCalcCurFromTopAddress();
					// DisasmCalcBotFromTopAddress();

					g_nDisasmCurAddress = _6502_BEG_MEM_ADDRESS;
					DisasmCalcTopBotAddress();
				}
				bUpdateDisplay |= UPDATE_DISASM;
				break;

			case VK_END:
				if (g_iWindowThis == WINDOW_CONSOLE)
				{
					ConsoleScrollEnd();
				}
				else
				if (g_nConsoleInputChars > 0)
				{
					if (KeybGetShiftStatus())
					{
						bUpdateDisplay |= ConsoleScrollEnd();
					}
					else
					{
						// Move cursor to end of console input
					}
				}
				else
				{
					// If you really want $8000 at the top of the screen...
					// g_nDisasmTopAddress =  (_6502_END_MEM_ADDRESS / 2) + 1;
					// DisasmCalcCurFromTopAddress();
					// DisasmCalcTopBotAddress();

					g_nDisasmCurAddress =  (_6502_END_MEM_ADDRESS / 2) + 1;
					DisasmCalcTopBotAddress();
				}
				bUpdateDisplay |= UPDATE_DISASM;
				break;

			case VK_PRIOR: // VK_PAGE_UP
				if (g_iWindowThis == WINDOW_CONSOLE)
				{
					bUpdateDisplay |= ConsoleScrollPageUp();
				}
				else
				if (g_nConsoleInputChars > 0)
				{
					if (KeybGetShiftStatus())
					{
						bUpdateDisplay |= ConsoleScrollPageUp();
					}
					else
					{
						// Scroll through console input history
					}
				}
				else
				{
					if (KeybGetShiftStatus())
						bUpdateDisplay |= CmdCursorPageUp256(0);
					else
					if (KeybGetCtrlStatus())
						bUpdateDisplay |= CmdCursorPageUp4K(0);
					else		
						bUpdateDisplay |= CmdCursorPageUp(0);
				}
				break;

			case VK_NEXT: // VK_PAGE_DN
				if (g_iWindowThis == WINDOW_CONSOLE)
				{
					bUpdateDisplay |= ConsoleScrollPageDn();
				}
				else
				if (g_nConsoleInputChars > 0)
				{
					if (KeybGetShiftStatus())
					{
						bUpdateDisplay |= ConsoleScrollPageDn();
					}
					else
					{
						// Scroll through console input history
					}
				}
				else
				{
					if (KeybGetShiftStatus())
						bUpdateDisplay |= CmdCursorPageDown256(0);
					else
					if (KeybGetCtrlStatus())
						bUpdateDisplay |= CmdCursorPageDown4K(0);
					else		
						bUpdateDisplay |= CmdCursorPageDown(0);
				}
				break;

	// +PATCH MJP
	//    case VK_UP:     bUpdateDisplay = CmdCursorLineUp(0);    break; // -PATCH
	//    case VK_DOWN:   bUpdateDisplay = CmdCursorLineDown(0);  break; // -PATCH
			case VK_UP:
				if (g_iWindowThis == WINDOW_CONSOLE)
				{
					bUpdateDisplay |= ConsoleScrollUp( 1 );
				}
				else
				if (g_nConsoleInputChars > 0)
				{
					if (KeybGetShiftStatus())
					{
						bUpdateDisplay |= ConsoleScrollUp( 1 );
					}
					else
					{
						// Scroll through console input history
					}
				}
				else
				{
					// Shift the Top offset up by 1 byte
					// i.e. no smart disassembly like LineUp()
					// Normally UP moves to the previous g_aOpcodes
					if (KeybGetShiftStatus())
						bUpdateDisplay |= CmdCursorLineUp(1);
					else
						bUpdateDisplay |= CmdCursorLineUp(0);
				}
				break;

			case VK_DOWN:
				if (g_iWindowThis == WINDOW_CONSOLE)
				{
					bUpdateDisplay |= ConsoleScrollDn( 1 );
				}
				else
				if (g_nConsoleInputChars > 0)
				{
					if (KeybGetShiftStatus())
					{
						bUpdateDisplay |= ConsoleScrollDn( 1 );
					}
					else
					{
						// Scroll through console input history
					}
				}
				else
				{
					if (KeybGetCtrlStatus())
						bUpdateDisplay |= CmdCursorRunUntil(0);
					else
					if (KeybGetShiftStatus())
						// Shift the Offest down by 1 byte
						// i.e. no smart disassembly like LineDown()
						bUpdateDisplay |= CmdCursorLineDown(1);
					else
						bUpdateDisplay |= CmdCursorLineDown(0);
				}
				break;

			case VK_RIGHT:
				if (KeybGetCtrlStatus())
					bUpdateDisplay |= CmdCursorSetPC( g_nDisasmCurAddress );		
				else
				if (KeybGetShiftStatus())
					bUpdateDisplay |= CmdCursorJumpPC( CURSOR_ALIGN_TOP );
				else
					bUpdateDisplay |= CmdCursorJumpPC( CURSOR_ALIGN_CENTER );
				break;

			case VK_LEFT:
				if (KeybGetShiftStatus())
					bUpdateDisplay |= CmdCursorJumpRetAddr( CURSOR_ALIGN_TOP ); // Jump to Caller
				else
					bUpdateDisplay |= CmdCursorJumpRetAddr( CURSOR_ALIGN_CENTER );
				break;

		} // switch
	}

	if (bUpdateDisplay) //  & UPDATE_BACKGROUND)
		UpdateDisplay( bUpdateDisplay );
}

// Still called from external file
void DebugDisplay( BOOL bDrawBackground )
{
	Update_t bUpdateFlags = UPDATE_ALL;

//	if (! bDrawBackground)
//		bUpdateFlags &= ~UPDATE_BACKGROUND;

	UpdateDisplay( bUpdateFlags );
}
