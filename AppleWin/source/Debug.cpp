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
//#pragma warning(disable: 4786)

#include "StdAfx.h"
#pragma  hdrstop


// NEW UI debugging
//	#define DEBUG_FORCE_DISPLAY 1
//  #define DEBUG_COMMAND_HELP  1
// #define DEBUG_ASM_HASH 1

// TODO: COLOR RESET
// TODO: COLOR SAVE ["filename"]
// TODO: COLOR LOAD ["filename"]

	// See Debugger_Changelong.txt for full details
	const int DEBUGGER_VERSION = MAKE_VERSION(2,5,3,0);


// Public _________________________________________________________________________________________


// Breakpoints ________________________________________________________________

	// Full-Speed debugging
	int g_nDebugOnBreakInvalid = 0;
	int g_iDebugOnOpcode       = 0;

	int          g_nBreakpoints = 0;
	Breakpoint_t g_aBreakpoints[ NUM_BREAKPOINTS ];

	// NOTE: Breakpoint_Source_t and g_aBreakpointSource must match!
	const TCHAR *g_aBreakpointSource[ NUM_BREAKPOINT_SOURCES ] =
	{	// Used to be one char, since ArgsCook also uses // TODO/FIXME: Parser use Param[] ?
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
	const TCHAR *g_aBreakpointSymbols[ NUM_BREAKPOINT_OPERATORS ] =
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
		{TEXT("BRK")         , CmdBreakInvalid      , CMD_BREAK_INVALID        , "Enter debugger on BRK or INVALID" },
		{TEXT("BRKOP")       , CmdBreakOpcode       , CMD_BREAK_OPCODE         , "Enter debugger on opcode"   },
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
		{TEXT("BP")          , CmdBreakpointMenu    , CMD_BREAKPOINT           , "Access breakpoint options" },
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
		{TEXT("BW")          , CmdConfigColorMono   , CMD_CONFIG_BW            , "Sets/Shows RGB for Black & White scheme" },
		{TEXT("COLOR")       , CmdConfigColorMono   , CMD_CONFIG_COLOR         , "Sets/Shows RGB for color scheme" },
		{TEXT("CONFIG")      , CmdConfigMenu        , CMD_CONFIG_MENU          , "Access config options" },
		{TEXT("ECHO")        , CmdConfigEcho        , CMD_CONFIG_ECHO          , "Echo string, or toggle command echoing" },
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
	// Disk
		{TEXT("DISK")        , CmdDisk              , CMD_DISK                 , "Access Disk Drive Functions" },
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
		{TEXT("S")           , CmdMemorySearch      , CMD_MEMORY_SEARCH        , "Search for text for hex values" },
		{TEXT("SA")          , CmdMemorySearchAscii,  CMD_MEMORY_SEARCH_ASCII  , "Search ASCII text" }, // Search ASCII
		{TEXT("ST")          , CmdMemorySearchApple , CMD_MEMORY_SEARCH_APPLE  , "Search Apple text (hi-bit)" }, // Search Apple Text
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


// Color ______________________________________________________________________

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

		W8, K0, // BG_DISASM_C        FG_DISASM_C     // B8 -> K0
		Y8, K0, // BG_DISASM_PC_C     FG_DISASM_PC_C  // K8 -> K0
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

	COLORREF DebuggerGetColor ( int iColor );


// Cursor _____________________________________________________________________

	WORD g_nDisasmTopAddress = 0;
	WORD g_nDisasmBotAddress = 0;
	WORD g_nDisasmCurAddress = 0;

	bool g_bDisasmCurBad    = false;
	int  g_nDisasmCurLine   = 0; // Aligned to Top or Center
    int  g_iDisasmCurState = CURSOR_NORMAL;

	int  g_nDisasmWinHeight = 0;

//	TCHAR g_aConfigDisasmAddressColon[] = TEXT(" :");

	extern const int WINDOW_DATA_BYTES_PER_LINE = 8;

// Font
	TCHAR     g_sFontNameDefault[ MAX_FONT_NAME ] = TEXT("Courier New");
	TCHAR     g_sFontNameConsole[ MAX_FONT_NAME ] = TEXT("Courier New");
	TCHAR     g_sFontNameDisasm [ MAX_FONT_NAME ] = TEXT("Courier New");
	TCHAR     g_sFontNameInfo   [ MAX_FONT_NAME ] = TEXT("Courier New");
	TCHAR     g_sFontNameBranch [ MAX_FONT_NAME ] = TEXT("Webdings");
	int       g_iFontSpacing = FONT_SPACING_CLEAN;
	HFONT     g_hFontWebDings  = (HFONT)0;

	// TODO: This really needs to be phased out, and use the ConfigFont[] settings
	int       g_nFontHeight = 15; // 13 -> 12 Lucida Console is readable

	const int MIN_DISPLAY_CONSOLE_LINES =  4; // doesn't include ConsoleInput

	int g_nTotalLines = 0; // DISPLAY_HEIGHT / g_nFontHeight;
	int MAX_DISPLAY_DISASM_LINES   = 0; // g_nTotalLines -  MIN_DISPLAY_CONSOLE_LINES; // 19;

	int MAX_DISPLAY_CONSOLE_LINES  =  0; // MAX_DISPLAY_DISASM_LINES + MIN_DISPLAY_CONSOLE_LINES; // 23


// Disassembly
	bool  g_bConfigDisasmOpcodeSpaces = true; // TODO: CONFIG SPACE  [0|1]
	bool  g_bConfigDisasmAddressColon = true; // TODO: CONFIG COLON  [0|1]
	int   g_iConfigDisasmBranchType = DISASM_BRANCH_FANCY; // TODO: CONFIG BRANCH [0|1]


// Display ____________________________________________________________________

	void UpdateDisplay( Update_t bUpdate );


// Memory _____________________________________________________________________

	const unsigned int _6502_ZEROPAGE_END    = 0x00FF;
	const unsigned int _6502_STACK_END       = 0x01FF;
	const unsigned int _6502_IO_BEGIN        = 0xC000;
	const unsigned int _6502_IO_END          = 0xC0FF;
	const unsigned int _6502_BEG_MEM_ADDRESS = 0x0000;
	const unsigned int _6502_END_MEM_ADDRESS = 0xFFFF;

	MemoryDump_t g_aMemDump[ NUM_MEM_DUMPS ];

	// Made global so operator @# can be used with other commands.
	MemorySearchResults_t g_vMemorySearchResults;


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
// Disk
		{TEXT("EJECT")      , NULL, PARAM_DISK_EJECT     },
		{TEXT("PROTECT")    , NULL, PARAM_DISK_PROTECT   },
		{TEXT("READ")       , NULL, PARAM_DISK_READ      },
// Font (Config)
		{TEXT("MODE")       , NULL, PARAM_FONT_MODE      }, // also INFO, CONSOLE, DISASM (from Window)
// General
		{TEXT("FIND")       , NULL, PARAM_FIND           },
		{TEXT("BRANCH")     , NULL, PARAM_BRANCH         },
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
// Memory
		{TEXT("?")          , NULL, PARAM_MEM_SEARCH_WILD },
//		{TEXT("*")          , NULL, PARAM_MEM_SEARCH_BYTE },
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

	MemoryTextFile_t g_AssemblerSourceBuffer;

	int    g_iSourceDisplayStart  = 0;
	int    g_nSourceAssembleBytes = 0;
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
	Watches_t g_aWatches[ MAX_WATCHES ]; // TODO: use vector<Watch_t> ??


// Window _________________________________________________________________________________________
	int           g_iWindowLast = WINDOW_CODE;
	int           g_iWindowThis = WINDOW_CODE;
	WindowSplit_t g_aWindowConfig[ NUM_WINDOWS ];


// Zero Page Pointers _____________________________________________________________________________
	int                g_nZeroPagePointers = 0;
	ZeroPagePointers_t g_aZeroPagePointers[ MAX_ZEROPAGE_POINTERS ]; // TODO: use vector<> ?


// TODO: // CONFIG SAVE --> VERSION #
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

	BOOL      g_bProfiling       = 0;
	int       g_nDebugSteps      = 0;
	DWORD     g_nDebugStepCycles = 0;
	int       g_nDebugStepStart  = 0;
	int       g_nDebugStepUntil  = -1; // HACK: MAGIC #

	int       g_nDebugSkipStart = 0;
	int       g_nDebugSkipLen   = 0;

	FILE     *g_hTraceFile       = NULL;

	DWORD     extbench      = 0;
	bool      g_bDebuggerViewingAppleOutput = false;

// Private ________________________________________________________________________________________


// Prototypes _______________________________________________________________

static	int ParseInput   ( LPTSTR pConsoleInput, bool bCook = true );
static	Update_t ExecuteCommand ( int nArgs );

// Colors
static	void _ConfigColorsReset();

// Drawing
static	bool DebuggerSetColor ( const int iScheme, const int iColor, const COLORREF nColor );
static	void _CmdColorGet ( const int iScheme, const int iColor );

// Font
static	void _UpdateWindowFontHeights(int nFontHeight);


	Update_t _CmdSymbolsClear      ( Symbols_e eSymbolTable );
	Update_t _CmdSymbolsCommon     ( int nArgs, int bSymbolTables );
	Update_t _CmdSymbolsListTables (int nArgs, int bSymbolTables );
	Update_t _CmdSymbolsUpdate     ( int nArgs );

	bool _CmdSymbolList_Address2Symbol( int nAddress   , int bSymbolTables );
	bool _CmdSymbolList_Symbol2Address( LPCTSTR pSymbol, int bSymbolTables );

// Source Level Debugging
static	bool BufferAssemblyListing ( TCHAR * pFileName );
static	bool ParseAssemblyListing  ( bool bBytesToMemory, bool bAddSymbols );


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
	bool  StringCat( TCHAR * pDst, LPCSTR pSrc, const int nDstSize );
	bool  TestStringCat ( TCHAR * pDst, LPCSTR pSrc, const int nDstSize );
	bool  TryStringCat ( TCHAR * pDst, LPCSTR pSrc, const int nDstSize );

	char FormatCharTxtCtrl ( const BYTE b, bool *pWasCtrl_ );
	char FormatCharTxtAsci ( const BYTE b, bool *pWasAsci_ );
	char FormatCharTxtHigh ( const BYTE b, bool *pWasHi_ );
	char FormatChar4Font ( const BYTE b, bool *pWasHi_, bool *pWasLo_ );

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


// Breakpoints ____________________________________________________________________________________


//===========================================================================
Update_t CmdBreakInvalid (int nArgs) // Breakpoint IFF Full-speed!
{
	if ((nArgs > 2) || (nArgs == 0))
		goto _Help;

	int iType = 0; // default to BRK
	int nActive;

//	if (nArgs == 2)
	iType = g_aArgs[ 1 ].nVal1;

	// Cases:
	// 0.  CMD            // display
	// 1a. CMD #          // display
	// 1b. CMD ON | OFF   //set      
	// 1c. CMD ?          // error
	// 2a. CMD # ON | OFF // set
	// 2b. CMD # ?        // error
	TCHAR sText[ CONSOLE_WIDTH ];
	bool bValidParam = true;

	int iParamArg = nArgs;
	int iParam;
	int nFound = FindParam( g_aArgs[ iParamArg ].sArg, MATCH_EXACT, iParam, _PARAM_GENERAL_BEGIN, _PARAM_GENERAL_END );

	if (nFound)
	{
		if (iParam == PARAM_ON)
			nActive = 1;
		else
		if (iParam == PARAM_OFF)
			nActive = 0;
		else
			bValidParam = false;
	}
	else
		bValidParam = false;

	if (nArgs == 1)
	{
		if (! nFound) // bValidParam) // case 1a or 1c
		{
			if ((iType < 0) || (iType > AM_3))
				goto _Help;

			if (IsDebugBreakOnInvalid( iType ))
				iParam = PARAM_ON;
			else
				iParam = PARAM_OFF;
		}
		else // case 1b
		{
			SetDebugBreakOnInvalid( iType, nActive );
		}
		
		if (iType == 0)
			wsprintf( sText, TEXT("Enter debugger on BRK opcode: %s"), g_aParameters[ iParam ].m_sName );
		else
			wsprintf( sText, TEXT("Enter debugger on INVALID %1X opcode: %s"), iType, g_aParameters[ iParam ].m_sName );
		ConsoleBufferPush( sText );
		return ConsoleUpdate();
 	}
	else	
	if (nArgs == 2)
	{
		if (! bValidParam) // case 2b
		{
			goto _Help;
		}
		else // case 2a (or not 2b ;-)
		{
			if ((iType < 0) || (iType > AM_3))
				goto _Help;

			SetDebugBreakOnInvalid( iType, nActive );

			if (iType == 0)
				wsprintf( sText, TEXT("Enter debugger on BRK opcode: %s"), g_aParameters[ iParam ].m_sName );
			else
				wsprintf( sText, TEXT("Enter debugger on INVALID %1X opcode: %s"), iType, g_aParameters[ iParam ].m_sName );
			ConsoleBufferPush( sText );
			return ConsoleUpdate();
		}
 	}

	return UPDATE_CONSOLE_DISPLAY;

_Help:
		return HelpLastCommand();
}


//===========================================================================
Update_t CmdBreakOpcode (int nArgs) // Breakpoint IFF Full-speed!
{
	TCHAR sText[ CONSOLE_WIDTH ];

	if (nArgs > 1)
		return HelpLastCommand();

	TCHAR sAction[ CONSOLE_WIDTH ] = TEXT("Current"); // default to display

	if (nArgs == 1)
	{
		int iOpcode = g_aArgs[ 1] .nVal1;
		g_iDebugOnOpcode = iOpcode & 0xFF;

		_tcscpy( sAction, TEXT("Setting") );

		if (iOpcode >= NUM_OPCODES)
		{
			wsprintf( sText, TEXT("Warning: clamping opcode: %02X"), g_iDebugOnOpcode );
			ConsoleBufferPush( sText );
			return ConsoleUpdate();
		}
	}

	if (g_iDebugOnOpcode == 0)
		// Show what the current break opcode is
		wsprintf( sText, TEXT("%s break on opcode: None")
			, sAction
			, g_iDebugOnOpcode
			, g_aOpcodes65C02[ g_iDebugOnOpcode ].sMnemonic
		);
	else
		// Show what the current break opcode is
		wsprintf( sText, TEXT("%s break on opcode: %02X %s")
			, sAction
			, g_iDebugOnOpcode
			, g_aOpcodes65C02[ g_iDebugOnOpcode ].sMnemonic
		);

	ConsoleBufferPush( sText );
	return ConsoleUpdate();
}


// bool bBP = g_nBreakpoints && CheckBreakpoint(nOffset,nOffset == regs.pc);
//===========================================================================
bool GetBreakpointInfo ( WORD nOffset, bool & bBreakpointActive_, bool & bBreakpointEnable_ )
{
	for (int iBreakpoint = 0; iBreakpoint < NUM_BREAKPOINTS; iBreakpoint++)
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


//===========================================================================
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

	Get6502Targets( regs.pc, &aTarget[0], &aTarget[1], &nBytes );
	
	if (nBytes)
	{
		for (iTarget = 0; iTarget < NUM_TARGETS; iTarget++ )
		{
			nAddress = aTarget[ iTarget ];
			if (nAddress != NO_6502_TARGET)
			{
				for (int iBreakpoint = 0; iBreakpoint < NUM_BREAKPOINTS; iBreakpoint++)
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

	for (int iBreakpoint = 0; iBreakpoint < NUM_BREAKPOINTS; iBreakpoint++)
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





// Commands _______________________________________________________________________________________


Update_t _CmdAssemble( WORD nAddress, int iArg, int nArgs )
{
	bool bHaveLabel = false;

	// if AlphaNumeric
	ArgToken_e iTokenSrc = NO_TOKEN;
	ParserFindToken( g_pConsoleInput, g_aTokens, NUM_TOKENS, &iTokenSrc );

	if (iTokenSrc == NO_TOKEN) // is TOKEN_ALPHANUMERIC
	if (g_pConsoleInput[0] != CHAR_SPACE)
	{
		bHaveLabel = true;

		// Symbol
		char *pSymbolName = g_aArgs[ iArg ].sArg; // pArg->sArg;
		SymbolUpdate( SYMBOLS_SRC, pSymbolName, nAddress, false, true ); // bool bRemoveSymbol, bool bUpdateSymbol )

		iArg++;
	}	

	bool bStatus = Assemble( iArg, nArgs, nAddress );
	if ( bStatus)
		return UPDATE_ALL;
		
	return UPDATE_CONSOLE_DISPLAY; // UPDATE_NOTHING;
}

//===========================================================================
Update_t CmdAssemble (int nArgs)
{
	if (! g_bAssemblerOpcodesHashed)
	{
		AssemblerStartup();
		g_bAssemblerOpcodesHashed = true;
	}

	// 0 : A
	// 1 : A address
	// 2+: A address mnemonic...

	if (! nArgs)
	{
//		return Help_Arg_1( CMD_ASSEMBLE );

		// Start assembler, continue with last assembled address
		AssemblerOn();
		return UPDATE_CONSOLE_DISPLAY;
	}
		
	g_nAssemblerAddress = g_aArgs[1].nVal1;

	if (nArgs == 1)
	{
		// undocumented ASM *
 		if (_tcscmp( g_aArgs[ 1 ].sArg, g_aParameters[ PARAM_WILDSTAR ].m_sName ) == 0)
		{
			_CmdAssembleHashDump();
		}

		AssemblerOn();
		return UPDATE_CONSOLE_DISPLAY;
	
//		return Help_Arg_1( CMD_ASSEMBLE );
	}

	if (nArgs > 1)
	{
		return _CmdAssemble( g_nAssemblerAddress, 2, nArgs ); // disasm, memory, watches, zeropage
	}

//		return Help_Arg_1( CMD_ASSEMBLE );
	// g_nAssemblerAddress; // g_aArgs[1].nVal1;
//	return ConsoleUpdate();

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
	int nOpbytes;

 	_6502GetOpcodeOpmodeOpbytes( iOpcode, iOpmode, nOpbytes );

	while (nOpbytes--)
	{
		*(mem+regs.pc + nOpbytes) = 0xEA;
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
Update_t CmdBreakpointMenu (int nArgs)
{
	// This is temporary until the menu is in.
	if (! nArgs)
	{
		g_aArgs[1].nVal1 = regs.pc;
		CmdBreakpointAddPC( 1 );
	}
	else
	{
		int iParam;
		int nFound = FindParam( g_aArgs[ 1 ].sArg, MATCH_EXACT, iParam, _PARAM_GENERAL_BEGIN, _PARAM_GENERAL_END );

		if (! nFound)
		{
			CmdBreakpointAddPC( 1 );
		}

		if (iParam == PARAM_SAVE)
		{
		}
		if (iParam == PARAM_LOAD)
		{
		}
		if (iParam == PARAM_RESET)
		{
		}
	}

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

	while ((iBreakpoint < NUM_BREAKPOINTS) && g_aBreakpoints[iBreakpoint].nLength)
	{
		iBreakpoint++;
		pBP++;
	}

	if (iBreakpoint >= NUM_BREAKPOINTS)
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
//		g_aArgs[1].nVal1 = regs.pc;
		g_aArgs[1].nVal1 = g_nDisasmCurAddress;
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
void _BreakWatchZero_Clear( Breakpoint_t * aBreakWatchZero, int iSlot )
{
	aBreakWatchZero[ iSlot ].bSet     = false;
	aBreakWatchZero[ iSlot ].bEnabled = false;
	aBreakWatchZero[ iSlot ].nLength  = 0;
}

void _BreakWatchZero_RemoveOne( Breakpoint_t *aBreakWatchZero, const int iSlot, int & nTotal )
{
	if (aBreakWatchZero[iSlot].bSet)
	{
		_BreakWatchZero_Clear( aBreakWatchZero, iSlot );
		nTotal--;
	}
}

void _BreakWatchZero_RemoveAll( Breakpoint_t *aBreakWatchZero, const int nMax, int & nTotal )
{
	int iSlot = nMax;

	while (iSlot--)
	{
		_BreakWatchZero_RemoveOne( aBreakWatchZero, iSlot, nTotal );
	}
}

// called by BreakpointsClear, WatchesClear, ZeroPagePointersClear
//===========================================================================
void _ClearViaArgs( int nArgs, Breakpoint_t * aBreakWatchZero, const int nMax, int & nTotal )
{
	int iSlot = 0;
	
	// Clear specified breakpoints
	while (nArgs)
	{
		iSlot = g_aArgs[nArgs].nVal1;

		if (! _tcscmp(g_aArgs[nArgs].sArg, g_aParameters[ PARAM_WILDSTAR ].m_sName))
		{
			_BreakWatchZero_RemoveAll( aBreakWatchZero, nMax, nTotal );
			break;
		}
		else
		if ((iSlot >= 1) && (iSlot <= nMax))
		{
			_BreakWatchZero_RemoveOne( aBreakWatchZero, iSlot - 1, nTotal );
		}

		nArgs--;
	}
}

// called by BreakpointsEnable, WatchesEnable, ZeroPagePointersEnable
// called by BreakpointsDisable, WatchesDisable, ZeroPagePointersDisable
void _EnableDisableViaArgs( int nArgs, Breakpoint_t * aBreakWatchZero, const int nMax, const bool bEnabled )
{
	int iSlot = 0;

	// Enable each breakpoint in the list
	while (nArgs)
	{
		iSlot = g_aArgs[nArgs].nVal1;

		if (! _tcscmp(g_aArgs[nArgs].sArg, g_aParameters[ PARAM_WILDSTAR ].m_sName))
		{
			iSlot = nMax;
			while (iSlot--)
			{
				aBreakWatchZero[ iSlot ].bEnabled = bEnabled;
			}			
			break;
		}
		else
		if ((iSlot >= 1) && (iSlot <= nMax))
		{
			aBreakWatchZero[iSlot-1].bEnabled = bEnabled;
		}

		nArgs--;
	}
}

//===========================================================================
Update_t CmdBreakpointClear (int nArgs)
{
	if (!g_nBreakpoints)
		return ConsoleDisplayError(TEXT("There are no breakpoints defined."));

	if (!nArgs)
	{
		_BreakWatchZero_RemoveAll( g_aBreakpoints, NUM_BREAKPOINTS, g_nBreakpoints );
	}
	else
	{
		_ClearViaArgs( nArgs, g_aBreakpoints, NUM_BREAKPOINTS, g_nBreakpoints );
	}

    return UPDATE_DISASM | UPDATE_BREAKPOINTS | UPDATE_CONSOLE_DISPLAY;
}

//===========================================================================
Update_t CmdBreakpointDisable (int nArgs)
{
	if (! g_nBreakpoints)
		return ConsoleDisplayError(TEXT("There are no (PC) Breakpoints defined."));

	if (! nArgs)
		return Help_Arg_1( CMD_BREAKPOINT_DISABLE );

	_EnableDisableViaArgs( nArgs, g_aBreakpoints, NUM_BREAKPOINTS, false );

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

	_EnableDisableViaArgs( nArgs, g_aBreakpoints, NUM_BREAKPOINTS, true );

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
//	int iBreakpoint = NUM_BREAKPOINTS;
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
		while (iBreakpoint < NUM_BREAKPOINTS)
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
Update_t CmdConfigEcho (int nArgs)
{
	TCHAR sText[ CONSOLE_WIDTH ] = TEXT("");

	if (g_aArgs[1].bType & TYPE_QUOTED_2)
	{
		ConsoleDisplayPush( g_aArgs[1].sArg );
	}
	else
	{
		const TCHAR *pText = g_pConsoleFirstArg; // ConsoleInputPeek();
		if (pText)
		{
			ConsoleDisplayPush( pText );
		}
	}

	return ConsoleUpdate();
}


//===========================================================================
Update_t CmdConfigMenu (int nArgs)
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
	// TODO: CmdConfigRun( gaFileNameConfig )
	
//	TCHAR sFileNameConfig[ MAX_PATH ];
	if (! nArgs)
	{

	}

//	gDebugConfigName
	// DEBUGLOAD file // load debugger setting
	return UPDATE_ALL;
}


//===========================================================================
Update_t CmdConfigRun (int nArgs)
{
	if (! nArgs)
		return Help_Arg_1( CMD_CONFIG_RUN );

	if (nArgs != 1)
		return Help_Arg_1( CMD_CONFIG_RUN );
	
	// Read in script
	// could be made global, to cache last run.
	// Opens up the possibility of:
	// CHEAT [ON | OFF] -> re-run script
	// with conditional logic
	// IF @ON ....
	MemoryTextFile_t script; 

	TCHAR * pFileName = g_aArgs[ 1 ].sArg;

	TCHAR sFileName[ MAX_PATH ];
	TCHAR sMiniFileName[ CONSOLE_WIDTH ];

//	if (g_aArgs[1].bType & TYPE_QUOTED_2)

	strcpy( sMiniFileName, pFileName );
//	strcat( sMiniFileName, ".aws" ); // HACK: MAGIC STRING

	_tcscpy(sFileName, progdir);
	_tcscat(sFileName, sMiniFileName);

	if (script.Read( sFileName ))
	{
		int iLine = 0;
		int nLine = script.GetNumLines();

		Update_t bUpdateDisplay = UPDATE_NOTHING;	

		for( int iLine = 0; iLine < nLine; iLine++ )
		{
			script.GetLine( iLine, g_pConsoleInput, CONSOLE_WIDTH-2 );
			g_nConsoleInputChars = strlen( g_pConsoleInput );
			bUpdateDisplay |= DebuggerProcessCommand( false );
		}
	}
	else
	{
		TCHAR sText[ CONSOLE_WIDTH ];
		wsprintf( sText, "Couldn't load filename: %s", sFileName );
		ConsoleBufferPush( sText );
	}	

	return ConsoleUpdate();
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

	// FIXME: Shouldn be saving in Text format, not binary!

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


// Font - Config __________________________________________________________________________________


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


// Only for FONT_DISASM_DEFAULT !
//===========================================================================
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
					if (g_aArgs[2].bType != TOKEN_QUOTE_DOUBLE)
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
			int nOpbytes;
			_6502GetOpmodeOpbytes( iAddress, iOpmode, nOpbytes );

			// .20 Fixed: DisasmCalcTopFromCurAddress()
			//if ((eMode >= ADDR_INVALID1) && (eMode <= ADDR_INVALID3))
			{
#if 0 // _DEBUG
				TCHAR sText[ CONSOLE_WIDTH ];
				wsprintf( sText, "%04X : %d bytes\n", iAddress, nOpbytes );
				OutputDebugString( sText );
#endif
				iAddress += nOpbytes;
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
			MessageBox( g_hFrameWindow, sText, "ERROR", MB_OK );
#endif
	}
 }



WORD DisasmCalcAddressFromLines( WORD iAddress, int nLines )
{
	while (nLines-- > 0)
	{
		int iOpmode;
		int nOpbytes;
		_6502GetOpmodeOpbytes( iAddress, iOpmode, nOpbytes );
		iAddress += nOpbytes;
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
	int nOpbytes;
	_6502GetOpmodeOpbytes( g_nDisasmCurAddress, iOpmode, nOpbytes ); // g_nDisasmTopAddress

	if (g_iWindowThis == WINDOW_DATA)
	{
		_CursorMoveDownAligned( WINDOW_DATA_BYTES_PER_LINE );
	}
	else
	if (nArgs)
	{
		nOpbytes = nArgs; // HACKL g_aArgs[1].val

		g_nDisasmTopAddress += nOpbytes;
		g_nDisasmCurAddress += nOpbytes;
		g_nDisasmBotAddress += nOpbytes;
	}
	else
	{
		// Works except on one case: G FB53, SPACE, DOWN
		WORD nTop = g_nDisasmTopAddress;
		WORD nCur = g_nDisasmCurAddress + nOpbytes;
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


// Disk ___________________________________________________________________________________________
Update_t CmdDisk ( int nArgs)
{
	if (! nArgs)
		goto _Help;

	if (nArgs < 2)
		goto _Help;

	int iDrive = g_aArgs[ 1 ].nVal1;

	if ((iDrive < 1) || (iDrive > 2))
		return HelpLastCommand();
	
	iDrive--;

	int iParam = 0;
	int nFound = FindParam( g_aArgs[ 2 ].sArg, MATCH_EXACT, iParam, _PARAM_DISK_BEGIN, _PARAM_DISK_END );

	if (! nFound)
		goto _Help;

	if (iParam == PARAM_DISK_EJECT)
	{
		if (nArgs > 2)
			goto _Help;

		DiskEject( iDrive );
		FrameRefreshStatus(DRAW_LEDS | DRAW_BUTTON_DRIVES);
	}
	else
	if (iParam == PARAM_DISK_PROTECT)
	{
		if (nArgs > 3)
			goto _Help;

		bool bProtect = true;

		if (nArgs == 3)
			bProtect = g_aArgs[ 3 ].nVal1 ? true : false;

		DiskSetProtect( iDrive, bProtect );
		FrameRefreshStatus(DRAW_LEDS | DRAW_BUTTON_DRIVES);
	}
	else
	{
		if (nArgs != 3)
			goto _Help;

		LPCTSTR pDiskName = g_aArgs[ 3 ].sArg;

		// DISK # "Diskname"
		DiskInsert( iDrive, pDiskName, true, false ); // write_protected, dont_create
		FrameRefreshStatus(DRAW_LEDS | DRAW_BUTTON_DRIVES);
	}

	return UPDATE_CONSOLE_DISPLAY;

_Help:
	return HelpLastCommand();
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
		nAddress = *(WORD*)(mem + REG_AF);
		bUpdate = true;
	}
	else if(strcmp(g_aArgs[1].sArg, "*BC") == 0)
	{
		nAddress = *(WORD*)(mem + REG_BC);
		bUpdate = true;
	}
	else if(strcmp(g_aArgs[1].sArg, "*DE") == 0)
	{
		nAddress = *(WORD*)(mem + REG_DE);
		bUpdate = true;
	}
	else if(strcmp(g_aArgs[1].sArg, "*HL") == 0)
	{
		nAddress = *(WORD*)(mem + REG_HL);
		bUpdate = true;
	}
	else if(strcmp(g_aArgs[1].sArg, "*IX") == 0)
	{
		nAddress = *(WORD*)(mem + REG_IX);
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
		*(mem + nAddress+nArgs-2)  = (BYTE)g_aArgs[nArgs].nVal1;
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
		*(mem + nAddress + nArgs - 2)  = (BYTE)(nData >> 0);
		*(mem + nAddress + nArgs - 1)  = (BYTE)(nData >> 8);

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
			*(mem + nAddress) = (BYTE)(g_aArgs[2].nVal1 & 0xFF); // HACK: Undocumented fill with ZERO
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
bool _GetStartEnd( WORD & nAddressStart_, WORD & nAddressEnd_ )
{
	nAddressStart_ = g_aArgs[1].nVal1; 
	int nEnd = nAddressStart_ + g_aArgs[1].nVal2;

	// .17 Bug Fix: D000,FFFF -> D000,CFFF (nothing searched!)
	if (nEnd > _6502_END_MEM_ADDRESS)
		nEnd = _6502_END_MEM_ADDRESS;

	nAddressEnd_  = nEnd;
	return true;
}


//===========================================================================
int _SearchMemoryFind(
	MemorySearchValues_t vMemorySearchValues,
	WORD nAddressStart,
	WORD nAddressEnd )
{
	int   nFound = 0;
	g_vMemorySearchResults.erase( g_vMemorySearchResults.begin(), g_vMemorySearchResults.end() );

	WORD nAddress;
	for( nAddress = nAddressStart; nAddress < nAddressEnd; nAddress++ )
	{
		bool bMatchAll = true;

		WORD nAddress2 = nAddress;

		int nMemBlocks = vMemorySearchValues.size();
		for (int iBlock = 0; iBlock < nMemBlocks; iBlock++, nAddress2++ )
		{
			MemorySearch_t ms = vMemorySearchValues.at( iBlock );
			ms.m_bFound = false;
		
			if ((ms.m_iType == MEM_SEARCH_BYTE_EXACT    ) || 
				(ms.m_iType == MEM_SEARCH_NIB_HIGH_EXACT) ||
				(ms.m_iType == MEM_SEARCH_NIB_LOW_EXACT ))
			{
				BYTE nTarget = *(mem + nAddress2);
	
				if (ms.m_iType == MEM_SEARCH_NIB_LOW_EXACT)
					nTarget &= 0x0F;

				if (ms.m_iType == MEM_SEARCH_NIB_HIGH_EXACT)
					nTarget &= 0xF0;
				
				if (ms.m_nValue == nTarget)
				{ // ms.m_nAddress = nAddress2;
					ms.m_bFound = true;
					continue;
				}
				else
				{
					bMatchAll = false;
					break;
				}
			}
			else
			if (ms.m_iType == MEM_SEARCH_BYTE_1_WILD)
			{
				// match by definition
			}
			else
			{
				// start 2ndary search
				// if next block matches, then this block matches (since we are wild)
				if ((iBlock + 1) == nMemBlocks) // there is no next block, hence we match
					continue;
					
				MemorySearch_t ms2 = vMemorySearchValues.at( iBlock + 1 );

				WORD nAddress3 = nAddress2;
				for (nAddress3 = nAddress2; nAddress3 < nAddressEnd; nAddress3++ )
				{
					if ((ms.m_iType == MEM_SEARCH_BYTE_EXACT    ) || 
						(ms.m_iType == MEM_SEARCH_NIB_HIGH_EXACT) ||
						(ms.m_iType == MEM_SEARCH_NIB_LOW_EXACT ))
					{
						BYTE nTarget = *(mem + nAddress3);
			
						if (ms.m_iType == MEM_SEARCH_NIB_LOW_EXACT)
							nTarget &= 0x0F;

						if (ms.m_iType == MEM_SEARCH_NIB_HIGH_EXACT)
							nTarget &= 0xF0;
						
						if (ms.m_nValue == nTarget)
						{
							nAddress2 = nAddress3;
							continue;
						}
						else
						{
							bMatchAll = false;
							break;
						}
					}

				}
			}
		}

		if (bMatchAll)
		{
			nFound++;

			// Save the search result
			g_vMemorySearchResults.push_back( nAddress );
		}
	}

	return nFound;
}


//===========================================================================
Update_t _SearchMemoryDisplay()
{
	int nFound = g_vMemorySearchResults.size();

	TCHAR sMatches[ CONSOLE_WIDTH ] = TEXT("");
	int   nThisLineLen = 0; // string length of matches for this line, for word-wrap

	if (nFound)
	{
		TCHAR sText[ CONSOLE_WIDTH ];

		int iFound = 0;
		while (iFound < nFound)
		{
			WORD nAddress = g_vMemorySearchResults.at( iFound );

			wsprintf( sText, "%2d:$%04X ", iFound, nAddress );
			int nLen = _tcslen( sText );

			// Fit on same line?
			if ((nThisLineLen + nLen) > (g_nConsoleDisplayWidth)) // CONSOLE_WIDTH
			{
				ConsoleDisplayPush( sMatches );
				_tcscpy( sMatches, sText );
				nThisLineLen = nLen;
			}
			else
			{
				_tcscat( sMatches, sText );
				nThisLineLen += nLen;
			}

			iFound++;
		}
		ConsoleDisplayPush( sMatches );
	}

	wsprintf( sMatches, "Total: %d  (#$%04X)", nFound, nFound );
	ConsoleDisplayPush( sMatches );

	// g_vMemorySearchResults is cleared in DebugEnd()

	return UPDATE_CONSOLE_DISPLAY;
}


//===========================================================================
Update_t _CmdMemorySearch (int nArgs, bool bTextIsAscii = true )
{
	WORD nAddressStart;
	WORD nAddressEnd;
	_GetStartEnd( nAddressStart, nAddressEnd );

	// S start,len #
	int nMinLen = nArgs - 2;

	bool bHaveWildCards = false;
	int iArg;

	MemorySearchValues_t vMemorySearchValues;
	MemorySearch_e       tLastType = MEM_SEARCH_BYTE_N_WILD;
	
	// Get search "string"
	Arg_t *pArg = & g_aArgs[ 2 ];
	
	WORD nTarget;
	for (iArg = 2; iArg <= nArgs; iArg++, pArg++ )
	{
		MemorySearch_t ms;

		nTarget = pArg->nVal1;
		ms.m_nValue = nTarget & 0xFF;
		ms.m_iType = MEM_SEARCH_BYTE_EXACT;

		if (nTarget > 0xFF) // searching for 16-bit address
		{
			vMemorySearchValues.push_back( ms );
			ms.m_nValue = (nTarget >> 8);

			tLastType = ms.m_iType;
		}
		else
		{
			TCHAR *pByte = pArg->sArg;

			
			if (pArg->bType & TYPE_QUOTED_1)
			{
				ms.m_iType = MEM_SEARCH_BYTE_EXACT;
				ms.m_bFound = false;
				ms.m_nValue = pArg->sArg[ 0 ];

				if (! bTextIsAscii) // NOTE: Single quote chars is opposite hi-bit !!!
					ms.m_nValue &= 0x7F;
				else
					ms.m_nValue |= 0x80;
			}
			else
			if (pArg->bType & TYPE_QUOTED_2)
			{
				// Convert string to hex byte(s)
				int iChar = 0;
				int nChars = pArg->nArgLen;

				if (nChars)
				{
					ms.m_iType = MEM_SEARCH_BYTE_EXACT;
					ms.m_bFound = false;

					nChars--; // last char is handle in common case below
					while (iChar < nChars)
					{
						ms.m_nValue = pArg->sArg[ iChar ];

						// Ascii (Low-Bit)
						// Apple (High-Bit)
						if (bTextIsAscii)
							ms.m_nValue &= 0x7F;
						else
							ms.m_nValue |= 0x80;

						vMemorySearchValues.push_back( ms );
						iChar++;
					}

					ms.m_nValue = pArg->sArg[ iChar ];
				}
			}
			else
			{
				// must be numeric .. make sure not too big
				if (pArg->nArgLen > 2)
				{
					vMemorySearchValues.erase( vMemorySearchValues.begin(), vMemorySearchValues.end() );
					return HelpLastCommand();
				}

				if (pArg->nArgLen == 1)
				{
					if (pByte[0] == g_aParameters[ PARAM_MEM_SEARCH_WILD ].m_sName[0]) // Hack: hard-coded one char token
					{
						ms.m_iType = MEM_SEARCH_BYTE_1_WILD;
					}
				}
				else
				{
					if (pByte[0] == g_aParameters[ PARAM_MEM_SEARCH_WILD ].m_sName[0]) // Hack: hard-coded one char token
					{
						ms.m_iType = MEM_SEARCH_NIB_LOW_EXACT;
						ms.m_nValue = pArg->nVal1 & 0x0F;
					}

					if (pByte[1] == g_aParameters[ PARAM_MEM_SEARCH_WILD ].m_sName[0]) // Hack: hard-coded one char token
					{
						if (ms.m_iType == MEM_SEARCH_NIB_LOW_EXACT)
						{
							ms.m_iType = MEM_SEARCH_BYTE_N_WILD;
						}
						else
						{
							ms.m_iType = MEM_SEARCH_NIB_HIGH_EXACT;
							ms.m_nValue = (pArg->nVal1 << 4) & 0xF0;
						}
					}
				}
			}
		}

		// skip over multiple byte_wild, since they are redundent
		// xx ?? ?? xx
		//       ^
		//       redundant
		if ((tLastType == MEM_SEARCH_BYTE_N_WILD) && (ms.m_iType == MEM_SEARCH_BYTE_N_WILD))
			continue;

		vMemorySearchValues.push_back( ms );
		tLastType = ms.m_iType;
	}

	_SearchMemoryFind( vMemorySearchValues, nAddressStart, nAddressEnd );
	vMemorySearchValues.erase( vMemorySearchValues.begin(), vMemorySearchValues.end() );

	return _SearchMemoryDisplay();
}


//===========================================================================
Update_t CmdMemorySearch (int nArgs)
{
	if (nArgs < 2)
		return HelpLastCommand();

	return _CmdMemorySearch( nArgs, true );

	return UPDATE_CONSOLE_DISPLAY;
}


// Search for ASCII text (no Hi-Bit set)
//===========================================================================
Update_t CmdMemorySearchAscii (int nArgs)
{
	if (nArgs < 2)
		return HelpLastCommand();

	return _CmdMemorySearch( nArgs, true );
}

// Search for Apple text (Hi-Bit set)
//===========================================================================
Update_t CmdMemorySearchApple (int nArgs)
{
	if (nArgs < 2)
		return HelpLastCommand();

	return _CmdMemorySearch( nArgs, false );
}

//===========================================================================
Update_t CmdMemorySearchHex (int nArgs)
{
	if (nArgs < 2)
		return HelpLastCommand();

	return _CmdMemorySearch( nArgs, true );
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

	g_AssemblerSourceBuffer.Reset();
	g_AssemblerSourceBuffer.Read( pFileName );

	if (g_AssemblerSourceBuffer.GetNumLines())
	{
		g_bSourceLevelDebugging = true;
		bStatus = true;
	}

	return bStatus;
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

	int nLines = g_AssemblerSourceBuffer.GetNumLines();
	for( int iLine = 0; iLine < nLines; iLine++ )
	{
		g_AssemblerSourceBuffer.GetLine( iLine, sText, MAX_LINE - 1 );

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
				int  iByte;
				for (iByte = 0; iByte < 4; iByte++ ) // BUG: Some assemblers also put 4 bytes on a line
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
					if (TextIsHexByte( pStart ))
					{
						BYTE nByte = TextConvert2CharsToByte( pStart );
						*(mem + ((WORD)nAddress) + iByte ) = nByte;
					}
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

				const int MAX_MINI_FILENAME = 20;
				TCHAR sMiniFileName[ MAX_MINI_FILENAME + 1 ];
				_tcsncpy( sMiniFileName, pFileName, MAX_MINI_FILENAME - 1 );
				sMiniFileName[ MAX_MINI_FILENAME ] = 0;


				if (BufferAssemblyListing( sFileName ))
				{
					_tcscpy( g_aSourceFileName, pFileName );

					if (! ParseAssemblyListing( g_bSourceAddMemory, g_bSourceAddSymbols ))
					{
						wsprintf( sFileName, "Couldn't load filename: %s", sMiniFileName );
						ConsoleBufferPush( sFileName );
					}
					else
					{
						TCHAR sText[ CONSOLE_WIDTH ];
						wsprintf( sFileName, "  Read: %d lines, %d symbols"
							, g_AssemblerSourceBuffer.GetNumLines() // g_nSourceAssemblyLines
							, g_nSourceAssemblySymbols );
						
						if (g_nSourceAssembleBytes)
						{
							wsprintf( sText, ", %d bytes", g_nSourceAssembleBytes );
							_tcscat( sFileName, sText );
						}
						ConsoleBufferPush( sFileName );
					}
				}
				else
				{
					wsprintf( sFileName, "Error reading: %s", sMiniFileName );
					ConsoleBufferPush( sFileName );
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
		if (!TextIsHexString( pText+1))
			return false;

		_tcscpy( sHexApple, TEXT("0x") );
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
void SymbolUpdate( Symbols_e eSymbolTable, char *pSymbolName, WORD nAddress, bool bRemoveSymbol, bool bUpdateSymbol )
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
			g_aSymbols[ eSymbolTable ][ nAddress ] = pSymbolName;
		}
	}
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

		SymbolUpdate( SYMBOLS_USER, pSymbolName, nAddress, bRemoveSymbol, bUpdateSymbol );
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


// Color __________________________________________________________________________________________

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
		COLORREF nColor = DebuggerGetColor( eColor );
		_ColorPrint( iColor, nColor );
	}
	else
	{
		TCHAR sText[ CONSOLE_WIDTH ];
		wsprintf( sText, "Color: %d\nOut of range!", iColor );
		MessageBox( g_hFrameWindow, sText, TEXT("ERROR"), MB_OK );
	}
}

//===========================================================================
inline COLORREF DebuggerGetColor( int iColor )
{
	COLORREF nColor = RGB(0,255,255); // 0xFFFF00; // Hot Pink! -- so we notice errors. Not that there is anything wrong with pink...

	if ((g_iColorScheme < NUM_COLOR_SCHEMES) && (iColor < NUM_COLORS))
	{
		nColor = g_aColors[ g_iColorScheme ][ iColor ];
	}

	return nColor;
}


bool DebuggerSetColor( const int iScheme, const int iColor, const COLORREF nColor )
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
	if (g_iCommand == CMD_CONFIG_BW)
		iScheme = SCHEME_BW;

	if ((iScheme < 0) || (iScheme > NUM_COLOR_SCHEMES)) // sanity check
		iScheme = SCHEME_COLOR;

	if (! nArgs)
	{
		g_iColorScheme = iScheme;
		UpdateDisplay( UPDATE_BACKGROUND );
		return UPDATE_ALL;
	}

//	if ((nArgs != 1) && (nArgs != 4))
	if (nArgs > 4)
		return HelpLastCommand();

	int iColor = g_aArgs[ 1 ].nVal1;
	if ((iColor < 0) || iColor >= NUM_COLORS)
		return HelpLastCommand();

	int iParam;
	int nFound = FindParam( g_aArgs[ 1 ].sArg, MATCH_EXACT, iParam, _PARAM_GENERAL_BEGIN, _PARAM_GENERAL_END );

	if (nFound)
	{
		if (iParam == PARAM_RESET)
		{
			_ConfigColorsReset();
			ConsoleBufferPush( TEXT(" Resetting colors." ) );
		}
		else
		if (iParam == PARAM_SAVE)
		{
		}
		else
		if (iParam == PARAM_LOAD)
		{
		}
		else
			return HelpLastCommand();
	}
	else
	{
		if (nArgs == 1)
		{	// Dump Color
			_CmdColorGet( iScheme, iColor );
			return ConsoleUpdate();
		}
		else
		if (nArgs == 4)
		{	// Set Color
			int R = g_aArgs[2].nVal1 & 0xFF;
			int G = g_aArgs[3].nVal1 & 0xFF;
			int B = g_aArgs[4].nVal1 & 0xFF;
			COLORREF nColor = RGB(R,G,B);

			DebuggerSetColor( iScheme, iColor, nColor );
		}
		else
			return HelpLastCommand();
	}

	return UPDATE_ALL;
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
//		DebuggerSetColor( iScheme, iColor );
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

	char sCommand[ CONSOLE_WIDTH ];
	strcpy( sCommand, pName );
	strupr( sCommand );

	while ((iCommand < NUM_COMMANDS_WITH_ALIASES)) // && (name[0] >= g_aCommands[iCommand].aName[0])) Command no longer in Alphabetical order
	{
		TCHAR *pCommandName = g_aCommands[iCommand].m_sName;
//		int iCmp = strcasecmp( sCommand, pCommandName, nLen )
		if (! _tcsncmp(sCommand, pCommandName, nLen))
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
	Arg_t * pArg     = & g_aArgs[ 0 ];
	TCHAR * pCommand = & pArg->sArg[0];

	CmdFuncPtr_t pFunction = NULL;
	int nFound = FindCommand( pCommand, pFunction );

	int nCookMask = (1 << NUM_TOKENS) - 1; // ArgToken_e used as bit mask!

	if (! nFound)
	{
		int nLen = _tcslen( pCommand);
		if (nLen < 6)
		{
			// verify pCommand[ 0 .. (nLen-1) ] is hex digit
			bool bIsHex = true;
			for (int iChar = 0; iChar < (nLen - 1); iChar++ )
			{
				if (isdigit(pCommand[iChar]))
					continue;
				else
				if (pCommand[iChar] >= 'A' && pCommand[iChar] <= 'F')
					continue;
				else
				{
					bIsHex = false;
					break;
				}
			}
			
			if (bIsHex)
			{
				WORD nAddress = 0;

				// Support Apple Monitor commands
				// ####G -> JMP $adress
				if (pCommand[nLen-1] == 'G')
				{
					pCommand[nLen-1] = 0;
					ArgsGetValue( pArg, & nAddress );

					regs.pc = nAddress;
					mode = MODE_RUNNING; // exit the debugger

					nFound = 1;
					g_iCommand = CMD_CONFIG_ECHO; // hack: don't cook args
				}

				// ####L -> Unassemble $address
				if (pCommand[nLen-1] == 'L')
				{
					pCommand[nLen-1] = 0;
					ArgsGetValue( pArg, & nAddress );

					g_iCommand = CMD_UNASSEMBLE;

					// replace: addrL
					// with:    comamnd addr
					pArg[1] = pArg[0];
					strcpy( pArg->sArg, g_aCommands[ g_iCommand ].m_sName );
					pArg->nArgLen = strlen( pArg->sArg );

					pArg++;
					pArg->nVal1 = nAddress;
					nArgs++;
					pFunction = g_aCommands[ g_iCommand ].pFunction;
					nFound = 1;
				}					

				// address: byte ...
				if ((pArg+1)->eToken == TOKEN_COLON)
				{
					g_iCommand = CMD_MEMORY_ENTER_BYTE;

					// replace: addr :
					// with:    comamnd addr
					pArg[1] = pArg[0];

					strcpy( pArg->sArg, g_aCommands[ g_iCommand ].m_sName );
					pArg->nArgLen = strlen( pArg->sArg );

//					nCookMask &= ~ (1 << TOKEN_COLON);
//					nArgs++;

					pFunction = g_aCommands[ g_iCommand ].pFunction;
					nFound = 1;
				}

				// TODO: display memory at address
				// addr1 [addr2] -> display byte at address
				// MDB memory display byte (is deprecated, so can be re-used)
			}
		}

		if (pArg->eToken == TOKEN_FSLASH)
		{
			pArg++;
			if (pArg->eToken == TOKEN_FSLASH)
			{
				nFound = 1;
				pFunction = NULL;
			}
		}
	}

	if (nFound > 1)
	{
// ASSERT (nFound == g_vPotentialCommands.size() );
		DisplayAmbigiousCommands( nFound );
		
		return ConsoleUpdate();
//		return ConsoleDisplayError( gaPotentialCommands );
	}

	if (nFound)
	{
		bool bCook = true;
		if (g_iCommand == CMD_CONFIG_ECHO)
			bCook = false;

		int nArgsCooked = nArgs;
		if (bCook)
			nArgsCooked = ArgsCook( nArgs, nCookMask ); // Cook them

		if (pFunction)
			return pFunction( nArgsCooked ); // Eat them

		return UPDATE_CONSOLE_DISPLAY;
	}
	else
		return ConsoleDisplayError(TEXT("Illegal Command"));
}



// ________________________________________________________________________________________________

//===========================================================================
bool Get6502Targets ( WORD nAddress, int *pTargetPartial_, int *pTargetPointer_, int * pBytes_)
{
	bool bStatus = false;

	if (! pTargetPartial_)
		return bStatus;

	if (! pTargetPointer_)
		return bStatus;

//	if (! pBytes_)
//		return bStatus;

	*pTargetPartial_   = NO_6502_TARGET;
	*pTargetPointer_  = NO_6502_TARGET;

	if (pBytes_)
		*pBytes_  = 0;	

	bStatus   = true;

//	WORD nAddress  = regs.pc;
	BYTE nOpcode   = *(LPBYTE)(mem + nAddress    );
	BYTE nTarget8  = *(LPBYTE)(mem + nAddress + 1);
	WORD nTarget16 = *(LPWORD)(mem + nAddress + 1);

	int eMode = g_aOpcodes[ nOpcode ].nAddressMode;

	switch (eMode)
	{
		case AM_A: // $Absolute
			*pTargetPointer_ = nTarget16;
			if (pBytes_)
				*pBytes_ = 2;
			break;

		case AM_IAX: // Indexed (Absolute) Indirect
			nTarget16 += regs.x;
			*pTargetPartial_    = nTarget16;
			*pTargetPointer_   = *(LPWORD)(mem + nTarget16);
			if (pBytes_)
				*pBytes_   = 2;
			break;

		case AM_AX: // Absolute, X
			nTarget16 += regs.x;
			*pTargetPointer_   = nTarget16;
			if (pBytes_)
				*pBytes_   = 2;
			break;

		case AM_AY: // Absolute, Y
			nTarget16 += regs.y;
			*pTargetPointer_   = nTarget16;
			if (pBytes_)
				*pBytes_   = 2;
			break;

		case AM_NA: // Indirect (Absolute) i.e. JMP
			*pTargetPartial_    = nTarget16;
			*pTargetPointer_   = *(LPWORD)(mem + nTarget16);
			if (pBytes_)
				*pBytes_   = 2;
			break;

		case AM_IZX: // Indexed (Zeropage Indirect, X)
			nTarget8  += regs.x;
			*pTargetPartial_    = nTarget8;
			*pTargetPointer_   = *(LPWORD)(mem + nTarget8);
			if (pBytes_)
				*pBytes_   = 2;
			break;

		case AM_NZY: // Indirect (Zeropage) Indexed, Y
			*pTargetPartial_    = nTarget8;
			*pTargetPointer_   = (*(LPWORD)(mem + nTarget8)) + regs.y;
			if (pBytes_)
				*pBytes_   = 1;
			break;

		case AM_NZ: // Indirect (Zeropage)
			*pTargetPartial_    = nTarget8;
			*pTargetPointer_   = *(LPWORD)(mem + nTarget8);
			if (pBytes_)
				*pBytes_   = 2;
			break;

		case AM_Z: // Zeropage
			*pTargetPointer_   = nTarget8;
			if (pBytes_)
				*pBytes_   = 1;
			break;

		case AM_ZX: // Zeropage, X
			*pTargetPointer_   = (nTarget8 + regs.x) & 0xFF; // .21 Bugfix: shouldn't this wrap around? Yes.
			if (pBytes_)
				*pBytes_   = 1;
			break;

		case AM_ZY: // Zeropage, Y
			*pTargetPointer_   = (nTarget8 + regs.y) & 0xFF; // .21 Bugfix: shouldn't this wrap around? Yes.
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
	if ((*pTargetPointer_ >= 0) &&
		((nOpcode == OPCODE_JSR    ) || // 0x20
		 (nOpcode == OPCODE_JMP_A  )))  // 0x4C
//		 (nOpcode == OPCODE_JMP_NA ) || // 0x6C
//		 (nOpcode == OPCODE_JMP_IAX)))  // 0x7C
	{
		*pTargetPointer_ = NO_6502_TARGET;
		if (pBytes_)
			*pBytes_ = 0;
	}
	return bStatus;
}


//===========================================================================
bool InternalSingleStep ()
{
	static DWORD dwCyclesThisFrame = 0;

	bool bResult = false;
	_try
	{
		BYTE nOpcode = *(mem+regs.pc);
		int  nOpmode = g_aOpcodes[ nOpcode ].nAddressMode;

		g_aProfileOpcodes[ nOpcode ].m_nCount++;
		g_aProfileOpmodes[ nOpmode ].m_nCount++;

		DWORD dwExecutedCycles = CpuExecute(g_nDebugStepCycles);
		dwCyclesThisFrame += dwExecutedCycles;

		if (dwCyclesThisFrame >= dwClksPerFrame)
		{
			dwCyclesThisFrame -= dwClksPerFrame;
		}
		VideoUpdateVbl( dwCyclesThisFrame );

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
int ParseInput ( LPTSTR pConsoleInput, bool bCook )
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

//	if (bCook)
//		nArg = ArgsCook( nArg ); // Cook them

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



//  _____________________________________________________________________________________
// |                                                                                     |
// |                           Public Functions                                          |
// |                                                                                     |
// |_____________________________________________________________________________________|

//===========================================================================
void DebugBegin ()
{
	// This is called every time the emulator is reset.
	// And everytime the debugger is entered.

	if (cpuemtype == CPU_FASTPAGING)
	{
		MemSetFastPaging(0);
	}

	mode = MODE_DEBUG;
	FrameRefreshStatus(DRAW_TITLE);

	if (apple2e)
		g_aOpcodes = & g_aOpcodes65C02[ 0 ]; // Enhanced Apple //e
	else
		g_aOpcodes = & g_aOpcodes6502[ 0 ]; // Original Apple ][ ][+

	g_aOpmodes[ AM_2 ].m_nBytes = apple2e ? 2 : 1;
	g_aOpmodes[ AM_3 ].m_nBytes = apple2e ? 3 : 1;

	g_nDisasmCurAddress = regs.pc;
	DisasmCalcTopBotAddress();

	g_bDebuggerViewingAppleOutput = false;

	UpdateDisplay( UPDATE_ALL );
}

//===========================================================================
void DebugContinueStepping ()
{
	static unsigned nStepsTaken = 0;

	if (g_nDebugSkipLen > 0)
	{
		if ((regs.pc >= g_nDebugSkipStart) && (regs.pc < (g_nDebugSkipStart + g_nDebugSkipLen)))
		{
			// Enter turbo debugger mode -- UI not updated, etc.
			g_nDebugSteps = -1;
			mode = MODE_STEPPING;
		}
		else
		{
			// Enter normal debugger mode -- UI updated every instruction, etc.
			g_nDebugSteps = 1;
			mode = MODE_STEPPING;
		}
	}

	if (g_nDebugSteps)
	{
		if (g_hTraceFile)
			OutputTraceLine();
		lastpc = regs.pc;

		InternalSingleStep();

		bool bBreak = CheckBreakpointsIO();

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


	g_vMemorySearchResults.erase( g_vMemorySearchResults.begin(), g_vMemorySearchResults.end() );
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

void _ConfigColorsReset()
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

		int nThreshold = 64;
		
		int BW;
		if (M < nThreshold)
			BW = 0;
		else
			BW = 255;

		COLORREF nMono = RGB(M,M,M);
		COLORREF nBW   = RGB(BW,BW,BW);

		DebuggerSetColor( SCHEME_COLOR, iColor, nColor );
		DebuggerSetColor( SCHEME_MONO , iColor, nMono );
		DebuggerSetColor( SCHEME_BW   , iColor, nBW );
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

	_ConfigColorsReset();

	WindowUpdateConsoleDisplayedSize();

	// CLEAR THE BREAKPOINT AND WATCH TABLES
	ZeroMemory( g_aBreakpoints     , NUM_BREAKPOINTS       * sizeof(Breakpoint_t));
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
	_CmdConfigFont( FONT_DISASM_BRANCH , g_sFontNameBranch , DEFAULT_PITCH | FF_DECORATIVE, g_nFontHeight+3); // DEFAULT_CHARSET

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
		g_iConfigDisasmBranchType = DISASM_BRANCH_FANCY;
	}
	else
	{
		g_iConfigDisasmBranchType = DISASM_BRANCH_PLAIN;
	}	


	//	ConsoleInputReset(); already called in DebugInitialize()
	TCHAR sText[ CONSOLE_WIDTH ];

	if (_tcscmp( g_aCommands[ NUM_COMMANDS ].m_sName, TEXT(__COMMANDS_VERIFY_TXT__)))
	{
		wsprintf( sText, "*** ERROR *** Commands mis-matched!" );
		MessageBox( g_hFrameWindow, sText, TEXT("ERROR"), MB_OK );
	}

	if (_tcscmp( g_aParameters[ NUM_PARAMS ].m_sName, TEXT(__PARAMS_VERIFY_TXT__)))
	{
		wsprintf( sText, "*** ERROR *** Parameters mis-matched!" );
		MessageBox( g_hFrameWindow, sText, TEXT("ERROR"), MB_OK );
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
}

// wparam = 0x16
// lparam = 0x002f 0x0001
// insert = VK_INSERT

// Add character to the input line
//===========================================================================
void DebuggerInputConsoleChar( TCHAR ch )
//void DebugProcessChar (TCHAR ch)
{
	if ((mode == MODE_STEPPING) && (ch == DEBUG_EXIT_KEY))
		g_nDebugSteps = 0; // Exit Debugger

	if (mode != MODE_DEBUG)
		return;

	if (g_bConsoleBufferPaused)
		return;

	if (ch == CHAR_SPACE)
	{
		// If don't have console input, don't pass space to the input line
		// exception: pass to assembler
		if ((! g_nConsoleInputChars) && (! g_bAssemblerInput))
			return;
	}
	
	if (g_nConsoleInputChars > (g_nConsoleDisplayWidth-1))
		return;

	if (g_bConsoleInputSkip)
	{
		g_bConsoleInputSkip = false;
		return;
	}

	if ((ch >= CHAR_SPACE) && (ch <= 126)) // HACK MAGIC # 32 -> ' ', # 126 
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
	else
	if (ch == 0x16) // HACK: Ctrl-V.  WTF!?
	{
		// Clipboard paste

        if (!IsClipboardFormatAvailable(CF_TEXT)) 
            return; 

        if (!OpenClipboard( g_hFrameWindow )) 
            return; 
 
		HGLOBAL hClipboard;
		LPTSTR  pData;

        hClipboard = GetClipboardData(CF_TEXT); 
        if (hClipboard != NULL) 
        { 
            pData = (char*) GlobalLock(hClipboard); 
            if (pData != NULL) 
            { 
				LPTSTR pSrc = pData;
				char c;

				while (true)
				{
					c = *pSrc++;

					if (! c)
						break;

					if (c == CHAR_CR)
					{
#if WIN32						
						// Eat char
#endif
#if MACOSX
						#pragma error( "TODO: Mac port - handle CR/LF")
#endif
					}
					else
					if (c == CHAR_LF)
					{
#if WIN32						
						DebuggerProcessCommand( true );	
#endif
#if MACOSX
						#pragma error( "TODO: Mac port - handle CR/LF")
#endif
					}
					else
					{
						// If we didn't want verbatim, we could do:
						// DebuggerInputConsoleChar( c );
						if ((c >= CHAR_SPACE) && (c <= 126)) // HACK MAGIC # 32 -> ' ', # 126 
							ConsoleInputChar( c );
					}
				}
                GlobalUnlock(hClipboard); 
            } 
        } 
        CloseClipboard(); 
 
		UpdateDisplay( UPDATE_CONSOLE_DISPLAY );
	}
}


// Triggered when ENTER is pressed, or via script
//===========================================================================
Update_t DebuggerProcessCommand( const bool bEchoConsoleInput )
{
	Update_t bUpdateDisplay = UPDATE_NOTHING;

	TCHAR sText[ CONSOLE_WIDTH ];

	if (bEchoConsoleInput)
		ConsoleDisplayPush( ConsoleInputPeek() );

	if (g_bAssemblerInput)
	{
		if (g_nConsoleInputChars)
		{
			ParseInput( g_pConsoleInput, false ); // Don't cook the args
			bUpdateDisplay |= _CmdAssemble( g_nAssemblerAddress, 0, g_nArgRaw );
		}
		else
		{
			AssemblerOff();

			int nDelayedTargets = AssemblerDelayedTargetsSize();
			if (nDelayedTargets)
			{
				wsprintf( sText, " Asm: %d sym declared, not defined", nDelayedTargets );
				ConsoleDisplayPush( sText );
				bUpdateDisplay |= UPDATE_CONSOLE_DISPLAY;
			}
		}
		ConsoleInputReset();
		bUpdateDisplay |= UPDATE_CONSOLE_DISPLAY;
		ConsoleUpdate(); // udpate console, don't pause
	}
	else
	if (g_nConsoleInputChars)
	{
		// BufferedInputPush( 
		// Handle Buffered Input
		// while ( BufferedInputPeek() )
		int nArgs = ParseInput( g_pConsoleInput );
		if (nArgs == ARG_SYNTAX_ERROR)
		{
			wsprintf( sText, "Syntax error: %s", g_aArgs[0].sArg );
			bUpdateDisplay |= ConsoleDisplayError( sText );
		}
		else
		{
			bUpdateDisplay |= ExecuteCommand( nArgs ); // ParseInput());
		}
		
		if (!g_bConsoleBufferPaused)
		{
			ConsoleInputReset();
		}
	}

	return bUpdateDisplay;
}

//===========================================================================
void DebuggerProcessKey( int keycode )
//void DebugProcessCommand (int keycode)
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
		bUpdateDisplay |= DebuggerProcessCommand( true ); // copy console input to console output

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


// Clipboard support
/*
	if (!IsClipboardFormatAvailable(CF_TEXT))
		return;
	
	if (!OpenClipboard(g_hFrameWindow))
		return;
	
	hglb = GetClipboardData(CF_TEXT);
	if (hglb == NULL)
	{	
		CloseClipboard();
		return;
	}

	lptstr = (char*) GlobalLock(hglb);
	if (lptstr == NULL)
	{	
		CloseClipboard();
		return;
	}
*/
//	else if (keycode == )
//	{
//	}

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
				if (g_bAssemblerInput)
				{
//					if (g_nConsoleInputChars)
//					{
//						ParseInput( g_pConsoleInput, false ); // Don't cook the args
//						bUpdateDisplay |= _CmdAssemble( g_nAssemblerAddress, 0, g_nArgRaw );
//					}
				}
				else
				{				
					if (KeybGetShiftStatus())
						bUpdateDisplay |= CmdStepOut(0);
					else
					if (KeybGetCtrlStatus())
						bUpdateDisplay |= CmdStepOver(0);
					else
						bUpdateDisplay |= CmdTrace(0);
				}
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
