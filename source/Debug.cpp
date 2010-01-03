/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 1994-1996, Michael O'Brien
Copyright (C) 1999-2001, Oliver Schmidt
Copyright (C) 2002-2005, Tom Charlesworth
Copyright (C) 2006-2009, Tom Charlesworth, Michael Pohoreski

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
 * Author: Copyright (C) 2006-2009 Michael Pohoreski
 */

// disable warning C4786: symbol greater than 255 character:
//#pragma warning(disable: 4786)

#include "StdAfx.h"
#pragma  hdrstop

//	#define DEBUG_COMMAND_HELP  1
//	#define DEBUG_ASM_HASH 1
#define ALLOW_INPUT_LOWERCASE 1

	// See Debugger_Changelong.txt for full details
	const int DEBUGGER_VERSION = MAKE_VERSION(2,6,2,0);


// Public _________________________________________________________________________________________

// All (Global)
	bool g_bDebuggerEatKey = false;

// Bookmarks __________________________________________________________________
//	vector<int> g_aBookmarks;
	int        g_nBookmarks;
	Bookmark_t g_aBookmarks[ MAX_BOOKMARKS ];

// Breakpoints ________________________________________________________________

	// Full-Speed debugging
	int  g_nDebugOnBreakInvalid  = 0;
	int  g_iDebugOnOpcode        = 0;
	bool g_bDebugDelayBreakCheck = false;

	int          g_nBreakpoints = 0;
	Breakpoint_t g_aBreakpoints[ MAX_BREAKPOINTS ];

	// NOTE: Breakpoint_Source_t and g_aBreakpointSource must match!
	const char *g_aBreakpointSource[ NUM_BREAKPOINT_SOURCES ] =
	{	// Used to be one char, since ArgsCook also uses // TODO/FIXME: Parser use Param[] ?
		// Used for both Input & Output!
		// Regs
		"A", // Reg A
		"X", // Reg X
		"Y", // Reg Y
		// Special
		"PC", // Program Counter
		"S" , // Stack Pointer
		// Flags -- .8 Moved: Flag names from g_aFlagNames[] to "inlined" g_aBreakpointSource[]
		"P", // Processor Status
		"C", // ---- ---1 Carry
		"Z", // ---- --1- Zero
		"I", // ---- -1-- Interrupt
		"D", // ---- 1--- Decimal
		"B", // ---1 ---- Break
		"R", // --1- ---- Reserved
		"V", // -1-- ---- Overflow
		"N", // 1--- ---- Sign
		// Misc		
		"OP", // Opcode/Instruction/Mnemonic
		// Memory
		"M" // Main
		// TODO: M0 ram bank 0, M1 aux ram ?
	};

	// Note: BreakpointOperator_t, _PARAM_BREAKPOINT_, and g_aBreakpointSymbols must match!
	const char *g_aBreakpointSymbols[ NUM_BREAKPOINT_OPERATORS ] =
	{	// Output: Must be 2 chars!
		"<=", // LESS_EQAUL
		"< ", // LESS_THAN
		"= ", // EQUAL
		"!=", // NOT_EQUAL
//		"! ", // NOT_EQUAL_1
		"> ", // GREATER_THAN
		">=", // GREATER_EQUAL
		"? ", // READ   // Q. IO Read  use 'I'?  A. No, since I=Interrupt
		"@ ", // WRITE  // Q. IO Write use 'O'?  A. No, since O=Opcode
		"* ", // Read/Write
	};


// Commands _______________________________________________________________________________________

	#define __COMMANDS_VERIFY_TXT__ "\xDE\xAD\xC0\xDE"
	#define __PARAMS_VERIFY_TXT__   "\xDE\xAD\xDA\x1A"

	int g_iCommand; // last command (enum) // used for consecuitive commands

	vector<int>       g_vPotentialCommands; // global, since TAB-completion also needs
	vector<Command_t> g_vSortedCommands;

	// Setting function to NULL, allows g_aCommands arguments to be safely listed here
	// Commands should be listed alphabetically per category.
	// For the list sorted by category, check Commands_e
	// NOTE: Commands_e and g_aCommands[] must be kept in sync! Aliases are listed at the end.
	Command_t g_aCommands[] =
	{
	// Assembler
		{TEXT("A")           , CmdAssemble          , CMD_ASSEMBLE             , "Assemble instructions"      },
	// Disassembler Data 
		{TEXT("B")           , CmdDisasmDataDefByte1       , CMD_DISASM_DATA      , "Treat byte [range] as data"                },
		{TEXT("X")           , CmdDisasmDataDefCode        , CMD_DISASM_CODE      , "Treat byte [range] as code"                },
		{TEXT("DL")          , CmdDisasmDataList           , CMD_DISASM_LIST      , "List all byte ranges treated as data"      },
		// without symbol lookup
		{TEXT("DB")          , CmdDisasmDataDefByte1       , CMD_DEFINE_DATA_BYTE1, "Define byte (array)"                          },
		{TEXT("DB2")         , CmdDisasmDataDefByte2       , CMD_DEFINE_DATA_BYTE2, "Define byte array, display 2 bytes/line"    },
		{TEXT("DB4")         , CmdDisasmDataDefByte4       , CMD_DEFINE_DATA_BYTE4, "Define byte array, display 4 bytes/line"    },
		{TEXT("DB8")         , CmdDisasmDataDefByte8       , CMD_DEFINE_DATA_BYTE8, "Define byte array, display 8 bytes/line"    },
		{TEXT("DW")          , CmdDisasmDataDefWord1       , CMD_DEFINE_DATA_WORD1, "Define address array"                       },
		{TEXT("DW2")         , CmdDisasmDataDefWord2       , CMD_DEFINE_DATA_WORD2, "Define address array, display 2 words/line" },
		{TEXT("DW4")         , CmdDisasmDataDefWord4       , CMD_DEFINE_DATA_WORD4, "Define address array, display 4 words/line" },
		{TEXT("DS")          , CmdDisasmDataDefString      , CMD_DEFINE_DATA_STR  , "Define string"                              },
//		{TEXT("DF")          , CmdDisasmDataDefFloat       , CMD_DEFINE_DATA_FLOAT, "Define AppleSoft (packed) Float"            },
//		{TEXT("DFX")         , CmdDisasmDataDefFloatUnpack , CMD_DEFINE_DATA_FLOAT2,"Define AppleSoft (unpacked) Float"          },
		// with symbol lookup
//		{TEXT("DA<>")        , CmdDisasmDataDefAddress8HL  , CMD_DEFINE_ADDR_8_HL , "Define split array of addresses, high byte section followed by low byte section" },
//		{TEXT("DA><")        , CmdDisasmDataDefAddress8LH  , CMD_DEFINE_ADDR_8_LH , "Define split array of addresses, low byte section followed by high byte section" },
//		{TEXT("DA<")         , CmdDisasmDataDefAddress8H   , CMD_DEFINE_ADDR_BYTE_H   , "Define array of high byte addresses"   },
//		{TEXT("DB>")         , CmdDisasmDataDefAddress8L   , CMD_DEFINE_ADDR_BYTE_L   , "Define array of low byte addresses"    } 
		{TEXT(".DA")         , CmdDisasmDataDefAddress16   , CMD_DEFINE_ADDR_WORD , "Define array of word addresses"            },
// TODO: Rename config cmd: DISASM
//		{TEXT("UA")          , CmdDisasmDataSmart          , CMD_SMART_DISASSEMBLE, "Analyze opcodes to determine if code or data" },		
	// CPU (Main)
		{TEXT(".")           , CmdCursorJumpPC      , CMD_CURSOR_JUMP_PC       , "Locate the cursor in the disasm window" }, // centered
		{TEXT("=")           , CmdCursorSetPC       , CMD_CURSOR_SET_PC        , "Sets the PC to the current instruction" },
		{TEXT("BRK")         , CmdBreakInvalid      , CMD_BREAK_INVALID        , "Enter debugger on BRK or INVALID" },
		{TEXT("BRKOP")       , CmdBreakOpcode       , CMD_BREAK_OPCODE         , "Enter debugger on opcode"   },
		{TEXT("G")           , CmdGo                , CMD_GO                   , "Run [until PC = address]"   },
		{TEXT("IN")          , CmdIn                , CMD_IN                   , "Input byte from IO $C0xx"   },
		{TEXT("KEY")         , CmdKey               , CMD_INPUT_KEY            , "Feed key into emulator"     },
		{TEXT("JSR")         , CmdJSR               , CMD_JSR                  , "Call sub-routine"           },
		{TEXT("NOP")         , CmdNOP               , CMD_NOP                  , "Zap the current instruction with a NOP" },
		{TEXT("OUT")         , CmdOut               , CMD_OUT                  , "Output byte to IO $C0xx"    },
		{TEXT("PROFILE")     , CmdProfile           , CMD_PROFILE              , "List/Save 6502 profiling" },
		{TEXT("R")           , CmdRegisterSet       , CMD_REGISTER_SET         , "Set register" },
		{TEXT("POP")         , CmdStackPop          , CMD_STACK_POP            },
		{TEXT("PPOP")        , CmdStackPopPseudo    , CMD_STACK_POP_PSEUDO     },
		{TEXT("PUSH")        , CmdStackPop          , CMD_STACK_PUSH           },
//		{TEXT("RTS")         , CmdStackReturn       , CMD_STACK_RETURN         },
		{TEXT("P")           , CmdStepOver          , CMD_STEP_OVER            , "Step current instruction"   },
		{TEXT("RTS")         , CmdStepOut           , CMD_STEP_OUT             , "Step out of subroutine"     }, 
		{TEXT("T")           , CmdTrace             , CMD_TRACE                , "Trace current instruction"  },
		{TEXT("TF")          , CmdTraceFile         , CMD_TRACE_FILE           , "Save trace to filename" },
		{TEXT("TL")          , CmdTraceLine         , CMD_TRACE_LINE           , "Trace (with cycle counting)" },
		{TEXT("U")           , CmdUnassemble        , CMD_UNASSEMBLE           , "Disassemble instructions"   },
//		{TEXT("WAIT")        , CmdWait              , CMD_WAIT                 , "Run until
	// Bookmarks
		{TEXT("BM")          , CmdBookmark          , CMD_BOOKMARK             , "Alias for BMA (Bookmark Add)"   },
		{TEXT("BMA")         , CmdBookmarkAdd       , CMD_BOOKMARK_ADD         , "Add/Update addess to bookmark"  },
		{TEXT("BMC")         , CmdBookmarkClear     , CMD_BOOKMARK_CLEAR       , "Clear (remove) bookmark"        },
		{TEXT("BML")         , CmdBookmarkList      , CMD_BOOKMARK_LIST        , "List all bookmarks"             },
		{TEXT("BMG")         , CmdBookmarkGoto      , CMD_BOOKMARK_GOTO        , "Move cursor to bookmark"        },
//		{TEXT("BMLOAD")      , CmdBookmarkLoad      , CMD_BOOKMARK_LOAD        , "Load bookmarks"                 },
		{TEXT("BMSAVE")      , CmdBookmarkSave      , CMD_BOOKMARK_SAVE        , "Save bookmarks"                 },
	// Breakpoints
		{TEXT("BP")          , CmdBreakpoint        , CMD_BREAKPOINT           , "Alias for BPR (Breakpoint Register Add)" },
		{TEXT("BPA")         , CmdBreakpointAddSmart, CMD_BREAKPOINT_ADD_SMART , "Add (smart) breakpoint" },
//		{TEXT("BPP")         , CmdBreakpointAddFlag , CMD_BREAKPOINT_ADD_FLAG  , "Add breakpoint on flags" },
		{TEXT("BPR")         , CmdBreakpointAddReg  , CMD_BREAKPOINT_ADD_REG   , "Add breakpoint on register value"      }, // NOTE! Different from SoftICE !!!!
		{TEXT("BPX")         , CmdBreakpointAddPC   , CMD_BREAKPOINT_ADD_PC    , "Add breakpoint at current instruction" },
		{TEXT("BPIO")        , CmdBreakpointAddIO   , CMD_BREAKPOINT_ADD_IO    , "Add breakpoint for IO address $C0xx"   },
		{TEXT("BPM")         , CmdBreakpointAddMem  , CMD_BREAKPOINT_ADD_MEM   , "Add breakpoint on memory access"       },  // SoftICE

		{TEXT("BPC")         , CmdBreakpointClear   , CMD_BREAKPOINT_CLEAR     , "Clear (remove) breakpoint"             }, // SoftICE
		{TEXT("BPD")         , CmdBreakpointDisable , CMD_BREAKPOINT_DISABLE   , "Disable breakpoint- it is still in the list, just not active" }, // SoftICE
		{TEXT("BPEDIT")      , CmdBreakpointEdit    , CMD_BREAKPOINT_EDIT      , "Edit breakpoint"                       }, // SoftICE
		{TEXT("BPE")         , CmdBreakpointEnable  , CMD_BREAKPOINT_ENABLE    , "(Re)Enable disabled breakpoint"        }, // SoftICE
		{TEXT("BPL")         , CmdBreakpointList    , CMD_BREAKPOINT_LIST      , "List all breakpoints"                  }, // SoftICE
//		{TEXT("BPLOAD")      , CmdBreakpointLoad    , CMD_BREAKPOINT_LOAD      , "Loads breakpoints" },
		{TEXT("BPSAVE")      , CmdBreakpointSave    , CMD_BREAKPOINT_SAVE      , "Saves breakpoints" },
	// Config
		{TEXT("BENCHMARK")   , CmdBenchmark         , CMD_BENCHMARK            , "Benchmark the emulator" },
		{TEXT("BW")          , CmdConfigColorMono   , CMD_CONFIG_BW            , "Sets/Shows RGB for Black & White scheme" },
		{TEXT("COLOR")       , CmdConfigColorMono   , CMD_CONFIG_COLOR         , "Sets/Shows RGB for color scheme" },
//		{TEXT("OPTION")      , CmdConfigMenu        , CMD_CONFIG_MENU          , "Access config options" },
		{TEXT("DISASM")      , CmdConfigDisasm      , CMD_CONFIG_DISASM        , "Sets/Shows disassembly view options." },
		{TEXT("FONT")        , CmdConfigFont        , CMD_CONFIG_FONT          , "Shows current font or sets new one" },
		{TEXT("HCOLOR")      , CmdConfigHColor      , CMD_CONFIG_HCOLOR        , "Sets/Shows colors mapped to Apple HGR" },
		{TEXT("LOAD")        , CmdConfigLoad        , CMD_CONFIG_LOAD          , "Load debugger configuration" },
		{TEXT("MONO")        , CmdConfigColorMono   , CMD_CONFIG_MONOCHROME    , "Sets/Shows RGB for monochrome scheme" },
		{TEXT("SAVE")        , CmdConfigSave        , CMD_CONFIG_SAVE          , "Save debugger configuration" },
	// Cursor
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
		{TEXT("MEB")         , CmdMemoryEnterByte   , CMD_MEMORY_ENTER_BYTE    , "Enter byte"                   },
		{TEXT("MEW")         , CmdMemoryEnterWord   , CMD_MEMORY_ENTER_WORD    , "Enter word"                   },
		{TEXT("BLOAD")       , CmdMemoryLoad        , CMD_MEMORY_LOAD          , "Load a region of memory"      },
		{TEXT("M")           , CmdMemoryMove        , CMD_MEMORY_MOVE          , "Memory move"                  },
		{TEXT("BSAVE")       , CmdMemorySave        , CMD_MEMORY_SAVE          , "Save a region of memory"      },
		{TEXT("S")           , CmdMemorySearch      , CMD_MEMORY_SEARCH        , "Search memory for text / hex values" },
		{TEXT("@")           ,_SearchMemoryDisplay  , CMD_MEMORY_FIND_RESULTS  , "Display search memory results" },
//		{TEXT("SA")          , CmdMemorySearchAscii,  CMD_MEMORY_SEARCH_ASCII  , "Search ASCII text"            },
//		{TEXT("ST")          , CmdMemorySearchApple , CMD_MEMORY_SEARCH_APPLE  , "Search Apple text (hi-bit)"   },
		{TEXT("SH")          , CmdMemorySearchHex   , CMD_MEMORY_SEARCH_HEX    , "Search memory for hex values" },
		{TEXT("F")           , CmdMemoryFill        , CMD_MEMORY_FILL          , "Memory fill"                  },
	// Output / Scripts
		{TEXT("CALC")        , CmdOutputCalc        , CMD_OUTPUT_CALC          , "Display mini calc result"               },
		{TEXT("ECHO")        , CmdOutputEcho        , CMD_OUTPUT_ECHO          , "Echo string to console"                 }, // or toggle command echoing"
		{TEXT("PRINT")       , CmdOutputPrint       , CMD_OUTPUT_PRINT         , "Display string and/or hex values"       },
		{TEXT("PRINTF")      , CmdOutputPrintf      , CMD_OUTPUT_PRINTF        , "Display formatted string"               },
		{TEXT("RUN")         , CmdOutputRun         , CMD_OUTPUT_RUN           , "Run script file of debugger commands"   },
	// Source Level Debugging
		{TEXT("SOURCE")      , CmdSource            , CMD_SOURCE               , "Starts/Stops source level debugging" },
		{TEXT("SYNC")        , CmdSync              , CMD_SYNC                 , "Syncs the cursor to the source file" },
	// Symbols
		{TEXT("SYM")         , CmdSymbols           , CMD_SYMBOLS_LOOKUP       , "Lookup symbol or address, or define symbol" },

		{TEXT("SYMMAIN")     , CmdSymbolsCommand    , CMD_SYMBOLS_ROM          , "Main/ROM symbol table lookup/menu" }, // CLEAR,LOAD,SAVE
		{TEXT("SYMBASIC")    , CmdSymbolsCommand    , CMD_SYMBOLS_APPLESOFT    , "Applesoft symbol table lookup/menu" }, // CLEAR,LOAD,SAVE
		{TEXT("SYMASM")      , CmdSymbolsCommand    , CMD_SYMBOLS_ASSEMBLY     , "Assembly symbol table lookup/menu" }, // CLEAR,LOAD,SAVE
		{TEXT("SYMUSER")     , CmdSymbolsCommand    , CMD_SYMBOLS_USER_1       , "First user symbol table lookup/menu" }, // CLEAR,LOAD,SAVE
		{TEXT("SYMUSER2")    , CmdSymbolsCommand    , CMD_SYMBOLS_USER_2       , "Second User symbol table lookup/menu" }, // CLEAR,LOAD,SAVE
		{TEXT("SYMSRC")      , CmdSymbolsCommand    , CMD_SYMBOLS_SRC_1        , "First Source symbol table lookup/menu" }, // CLEAR,LOAD,SAVE
		{TEXT("SYMSRC2")     , CmdSymbolsCommand    , CMD_SYMBOLS_SRC_2        , "Second Source symbol table lookup/menu" }, // CLEAR,LOAD,SAVE
//		{TEXT("SYMCLEAR")    , CmdSymbolsClear      , CMD_SYMBOLS_CLEAR        }, // can't use SC = SetCarry
		{TEXT("SYMINFO")     , CmdSymbolsInfo       , CMD_SYMBOLS_INFO         , "Display summary of symbols" },
		{TEXT("SYMLIST")     , CmdSymbolsList       , CMD_SYMBOLS_LIST         , "Lookup symbol in main/user/src tables" }, // 'symbolname', can't use param '*' 
	// Variables
//		{TEXT("CLEAR")       , CmdVarsClear         , CMD_VARIABLES_CLEAR      }, 
//		{TEXT("VAR")         , CmdVarsDefine        , CMD_VARIABLES_DEFINE     },
//		{TEXT("INT8")        , CmdVarsDefineInt8    , CMD_VARIABLES_DEFINE_INT8},
//		{TEXT("INT16")       , CmdVarsDefineInt16   , CMD_VARIABLES_DEFINE_INT16},
//		{TEXT("VARS")        , CmdVarsList          , CMD_VARIABLES_LIST       }, 
//		{TEXT("VARSLOAD")    , CmdVarsLoad          , CMD_VARIABLES_LOAD       },
//		{TEXT("VARSSAVE")    , CmdVarsSave          , CMD_VARIABLES_SAVE       },
//		{TEXT("SET")         , CmdVarsSet           , CMD_VARIABLES_SET        },
	// View
		{TEXT("TEXT")        , CmdViewOutput_Text4X , CMD_VIEW_TEXT4X, "View Text screen (current page)"        },
		{TEXT("TEXT1")       , CmdViewOutput_Text41 , CMD_VIEW_TEXT41, "View Text screen Page 1"                },
		{TEXT("TEXT2")       , CmdViewOutput_Text42 , CMD_VIEW_TEXT42, "View Text screen Page 2"                },
		{TEXT("TEXT80")      , CmdViewOutput_Text8X , CMD_VIEW_TEXT8X, "View 80-col Text screen (current page)" },
		{TEXT("TEXT81")      , CmdViewOutput_Text81 , CMD_VIEW_TEXT8X, "View 80-col Text screen Page 1"         },
		{TEXT("TEXT82")      , CmdViewOutput_Text82 , CMD_VIEW_TEXT8X, "View 80-col Text screen Page 2"         },
		{TEXT("GR")          , CmdViewOutput_GRX    , CMD_VIEW_GRX   , "View Lo-Res screen (current page)"      },
		{TEXT("GR1")         , CmdViewOutput_GR1    , CMD_VIEW_GR1   , "View Lo-Res screen Page 1"              },
		{TEXT("GR2")         , CmdViewOutput_GR2    , CMD_VIEW_GR2   , "View Lo-Res screen Page 2"              },
		{TEXT("DGR")         , CmdViewOutput_DGRX   , CMD_VIEW_DGRX  , "View Double lo-res (current page)"      },
		{TEXT("DGR1")        , CmdViewOutput_DGR1   , CMD_VIEW_DGR1  , "View Double lo-res Page 1"              },
		{TEXT("DGR2")        , CmdViewOutput_DGR2   , CMD_VIEW_DGR2  , "View Double lo-res Page 2"              },
		{TEXT("HGR")         , CmdViewOutput_HGRX   , CMD_VIEW_HGRX  , "View Hi-res (current page)"             },
		{TEXT("HGR1")        , CmdViewOutput_HGR1   , CMD_VIEW_HGR1  , "View Hi-res Page 1"                     },
		{TEXT("HGR2")        , CmdViewOutput_HGR2   , CMD_VIEW_HGR2  , "View Hi-res Page 2"                     },
		{TEXT("DHGR")        , CmdViewOutput_DHGRX  , CMD_VIEW_DHGRX , "View Double Hi-res (current page)"      },
		{TEXT("DHGR1")       , CmdViewOutput_DHGR1  , CMD_VIEW_DHGR1 , "View Double Hi-res Page 1"              },
		{TEXT("DHGR2")       , CmdViewOutput_DHGR2  , CMD_VIEW_DHGR2 , "View Double Hi-res Page 2"              },
	// Watch
		{TEXT("W")           , CmdWatch             , CMD_WATCH_ADD     , "Alias for WA (Watch Add)"                      },
		{TEXT("WA")          , CmdWatchAdd          , CMD_WATCH_ADD     , "Add/Update address or symbol to watch"         },
		{TEXT("WC")          , CmdWatchClear        , CMD_WATCH_CLEAR   , "Clear (remove) watch"                          },
		{TEXT("WD")          , CmdWatchDisable      , CMD_WATCH_DISABLE , "Disable specific watch - it is still in the list, just not active" },
		{TEXT("WE")          , CmdWatchEnable       , CMD_WATCH_ENABLE  , "(Re)Enable disabled watch"                     },
		{TEXT("WL")          , CmdWatchList         , CMD_WATCH_LIST    , "List all watches"                              },
//		{TEXT("WLOAD")       , CmdWatchLoad         , CMD_WATCH_LOAD    , "Load Watches"                                  }, // Cant use as param to W
		{TEXT("WSAVE")       , CmdWatchSave         , CMD_WATCH_SAVE    , "Save Watches"                                  }, // due to symbol look-up
	// Window
		{TEXT("WIN")         , CmdWindow            , CMD_WINDOW         , "Show specified debugger window"              },
// TODO: need to rename with data disassembly
		{TEXT("CODE")        , CmdWindowViewCode    , CMD_WINDOW_CODE    , "Switch to full code window"                  },  // Can't use WC = WatchClear
		{TEXT("CODE1")       , CmdWindowShowCode1   , CMD_WINDOW_CODE_1  , "Show code on top split window"               },
		{TEXT("CODE2")       , CmdWindowShowCode2   , CMD_WINDOW_CODE_2  , "Show code on bottom split window"            },
		{TEXT("CONSOLE")     , CmdWindowViewConsole , CMD_WINDOW_CONSOLE , "Switch to full console window"               },
// TODO: need to rename with data disassembly
		{TEXT("DATA")        , CmdWindowViewData    , CMD_WINDOW_DATA    , "Switch to full data window"                  },
		{TEXT("DATA1")       , CmdWindowShowCode1   , CMD_WINDOW_CODE_1  , "Show data on top split window"               },
		{TEXT("DATA2")       , CmdWindowShowData2   , CMD_WINDOW_DATA_2  , "Show data on bottom split window"            },
		{TEXT("SOURCE1")     , CmdWindowShowSource1 , CMD_WINDOW_SOURCE_1, "Show source on top split screen"             },
		{TEXT("SOURCE2")     , CmdWindowShowSource2 , CMD_WINDOW_SOURCE_2, "Show source on bottom split screen"          },

		{TEXT("\\")          , CmdWindowViewOutput  , CMD_WINDOW_OUTPUT  , "Display Apple output until key pressed" },
//		{TEXT("INFO")        , CmdToggleInfoPanel   , CMD_WINDOW_TOGGLE },
//		{TEXT("WINSOURCE")   , CmdWindowShowSource  , CMD_WINDOW_SOURCE },
//		{TEXT("ZEROPAGE")    , CmdWindowShowZeropage, CMD_WINDOW_ZEROPAGE },
	// Zero Page
		{TEXT("ZP")          , CmdZeroPage          , CMD_ZEROPAGE_POINTER       , "Alias for ZPA (Zero Page Add)"          },
		{TEXT("ZP0")         , CmdZeroPagePointer   , CMD_ZEROPAGE_POINTER_0     , "Set/Update/Remove ZP watch 0 "          },
		{TEXT("ZP1")         , CmdZeroPagePointer   , CMD_ZEROPAGE_POINTER_1     , "Set/Update/Remove ZP watch 1"           },
		{TEXT("ZP2")         , CmdZeroPagePointer   , CMD_ZEROPAGE_POINTER_2     , "Set/Update/Remove ZP watch 2"           },
		{TEXT("ZP3")         , CmdZeroPagePointer   , CMD_ZEROPAGE_POINTER_3     , "Set/Update/Remove ZP watch 3"           },
		{TEXT("ZP4")         , CmdZeroPagePointer   , CMD_ZEROPAGE_POINTER_4     , "Set/Update/Remove ZP watch 4"           },
		{TEXT("ZP5")         , CmdZeroPagePointer   , CMD_ZEROPAGE_POINTER_5     , "Set/Update/Remove ZP watch 5 "          },
		{TEXT("ZP6")         , CmdZeroPagePointer   , CMD_ZEROPAGE_POINTER_6     , "Set/Update/Remove ZP watch 6"           },
		{TEXT("ZP7")         , CmdZeroPagePointer   , CMD_ZEROPAGE_POINTER_7     , "Set/Update/Remove ZP watch 7"           },
		{TEXT("ZPA")         , CmdZeroPageAdd       , CMD_ZEROPAGE_POINTER_ADD   , "Add/Update address to zero page pointer"},
		{TEXT("ZPC")         , CmdZeroPageClear     , CMD_ZEROPAGE_POINTER_CLEAR , "Clear (remove) zero page pointer"       },
		{TEXT("ZPD")         , CmdZeroPageDisable   , CMD_ZEROPAGE_POINTER_DISABLE,"Disable zero page pointer - it is still in the list, just not active" },
		{TEXT("ZPE")         , CmdZeroPageEnable    , CMD_ZEROPAGE_POINTER_ENABLE, "(Re)Enable disabled zero page pointer"  },
		{TEXT("ZPL")         , CmdZeroPageList      , CMD_ZEROPAGE_POINTER_LIST  , "List all zero page pointers"            },
//		{TEXT("ZPLOAD")      , CmdZeroPageLoad      , CMD_ZEROPAGE_POINTER_LOAD  , "Load zero page pointers"                }, // Cant use as param to ZP
		{TEXT("ZPSAVE")      , CmdZeroPageSave      , CMD_ZEROPAGE_POINTER_SAVE  , "Save zero page pointers"                }, // due to symbol look-up

//	{TEXT("TIMEDEMO"),CmdTimeDemo, CMD_TIMEDEMO }, // CmdBenchmarkStart(), CmdBenchmarkStop()
//	{TEXT("WC"),CmdShowCodeWindow}, // Can't use since WatchClear
//	{TEXT("WD"),CmdShowDataWindow}, //

	// Internal Consistency Check
		{TEXT(__COMMANDS_VERIFY_TXT__), NULL, NUM_COMMANDS },

	// Aliasies - Can be in any order
		{TEXT("->")          , NULL                 , CMD_CURSOR_JUMP_PC       },
		{TEXT("Ctrl ->" )    , NULL                 , CMD_CURSOR_SET_PC        },
		{TEXT("Shift ->")    , NULL                 , CMD_CURSOR_JUMP_PC       }, // at top
		{TEXT("INPUT")       , CmdIn                , CMD_IN                   },
		// Data
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

		{TEXT("ME8")         , CmdMemoryEnterByte   , CMD_MEMORY_ENTER_BYTE    }, // changed from EB -- bugfix: EB:## ##
		{TEXT("ME16")        , CmdMemoryEnterWord   , CMD_MEMORY_ENTER_WORD    },
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

		{TEXT("SYM0" )       , CmdSymbolsInfo       , CMD_SYMBOLS_ROM          },
		{TEXT("SYM1" )       , CmdSymbolsInfo       , CMD_SYMBOLS_APPLESOFT    },
		{TEXT("SYM2" )       , CmdSymbolsInfo       , CMD_SYMBOLS_ASSEMBLY     },
		{TEXT("SYM3" )       , CmdSymbolsInfo       , CMD_SYMBOLS_USER_1       },
		{TEXT("SYM4" )       , CmdSymbolsInfo       , CMD_SYMBOLS_USER_2       },
		{TEXT("SYM5" )       , CmdSymbolsInfo       , CMD_SYMBOLS_SRC_1        },
		{TEXT("SYM6" )       , CmdSymbolsInfo       , CMD_SYMBOLS_SRC_2        },

		{TEXT("TEXT40")      , CmdViewOutput_Text4X , CMD_VIEW_TEXT4X          },
		{TEXT("TEXT41")      , CmdViewOutput_Text41 , CMD_VIEW_TEXT41          },
		{TEXT("TEXT42")      , CmdViewOutput_Text42 , CMD_VIEW_TEXT42          },

		{TEXT("WATCH")       , CmdWatchAdd          , CMD_WATCH_ADD            },
		{TEXT("WINDOW")      , CmdWindow            , CMD_WINDOW               },
//		{TEXT("W?")          , CmdWatchAdd          , CMD_WATCH_ADD            },
		{TEXT("ZAP")         , CmdNOP               , CMD_NOP                  },

	// DEPRECATED  -- Probably should be removed in a future version
		{TEXT("BENCH")       , CmdBenchmarkStart    , CMD_BENCHMARK            },
		{TEXT("EXITBENCH")   , CmdBenchmarkStop     , CMD_BENCHMARK            },
		{TEXT("MDB")         , CmdMemoryMiniDumpHex , CMD_MEM_MINI_DUMP_HEX_1  }, // MemoryDumpByte  // Did anyone actually use this??
		{TEXT("MEMORY")      , CmdMemoryMiniDumpHex , CMD_MEM_MINI_DUMP_HEX_1  }, // MemoryDumpByte  // Did anyone actually use this??
};

//	static const char g_aFlagNames[_6502_NUM_FLAGS+1] = TEXT("CZIDBRVN");// Reversed since arrays are from left-to-right

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
	int g_aColorIndex[ NUM_DEBUG_COLORS ] =
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

		C4, // BG_DISASM_BOOKMARK
		W8, // FG_DISASM_BOOKMARK

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

	COLORREF g_aColors[ NUM_COLOR_SCHEMES ][ NUM_DEBUG_COLORS ];

	COLORREF DebuggerGetColor ( int iColor );


// Cursor (Console Input) _____________________________________________________

//	char g_aInputCursor[] = "\|/-";
	enum InputCursor
	{
		CURSOR_INSERT,
		CURSOR_OVERSTRIKE,
		NUM_INPUT_CURSORS
	};
	
	const char g_aInputCursor[] = "_\x7F"; // insert over-write
	bool       g_bInputCursor = false;
	int        g_iInputCursor = CURSOR_OVERSTRIKE; // which cursor to use
	const int  g_nInputCursor = sizeof( g_aInputCursor );

	void DebuggerCursorUpdate();
	char DebuggerCursorGet();

// Cursor (Disasm) ____________________________________________________________

	WORD g_nDisasmTopAddress = 0;
	WORD g_nDisasmBotAddress = 0;
	WORD g_nDisasmCurAddress = 0;

	bool g_bDisasmCurBad    = false;
	int  g_nDisasmCurLine   = 0; // Aligned to Top or Center
	int  g_iDisasmCurState = CURSOR_NORMAL;

	int  g_nDisasmWinHeight = 0;

//	char g_aConfigDisasmAddressColon[] = TEXT(" :");

	extern const int WINDOW_DATA_BYTES_PER_LINE = 8;

#if OLD_FONT
// Font
	TCHAR     g_sFontNameDefault[ MAX_FONT_NAME ] = TEXT("Courier New");
	TCHAR     g_sFontNameConsole[ MAX_FONT_NAME ] = TEXT("Courier New");
	TCHAR     g_sFontNameDisasm [ MAX_FONT_NAME ] = TEXT("Courier New");
	TCHAR     g_sFontNameInfo   [ MAX_FONT_NAME ] = TEXT("Courier New");
	TCHAR     g_sFontNameBranch [ MAX_FONT_NAME ] = TEXT("Webdings");
	HFONT     g_hFontWebDings  = (HFONT)0;
#endif
	int       g_iFontSpacing = FONT_SPACING_CLEAN;

	// TODO: This really needs to be phased out, and use the ConfigFont[] settings
#if USE_APPLE_FONT
	int       g_nFontHeight = CONSOLE_FONT_HEIGHT; // 13 -> 12 Lucida Console is readable
#else
	int       g_nFontHeight = 15; // 13 -> 12 Lucida Console is readable
#endif

	const int MIN_DISPLAY_CONSOLE_LINES =  5; // doesn't include ConsoleInput

	int g_nDisasmDisplayLines  = 0;


// Config _____________________________________________________________________

// Config - Disassembly
	bool  g_bConfigDisasmAddressView   = true;
	bool  g_bConfigDisasmAddressColon  = true;
	bool  g_bConfigDisasmOpcodesView   = true;
	bool  g_bConfigDisasmOpcodeSpaces  = true;
	int   g_iConfigDisasmTargets       = DISASM_TARGET_BOTH;
	int   g_iConfigDisasmBranchType    = DISASM_BRANCH_FANCY;
	int   g_bConfigDisasmImmediateChar = DISASM_IMMED_BOTH;
	int   g_iConfigDisasmScroll        = 3; // favor 3 byte opcodes
// Config - Info
	bool  g_bConfigInfoTargetPointer   = false;

	MemoryTextFile_t g_ConfigState;

// Display ____________________________________________________________________

	void UpdateDisplay( Update_t bUpdate );


// Memory _____________________________________________________________________

	const          int _6502_BRANCH_POS      = +127;
	const          int _6502_BRANCH_NEG      = -128;
	const unsigned int _6502_ZEROPAGE_END    = 0x00FF;
	const unsigned int _6502_STACK_END       = 0x01FF;
	const unsigned int _6502_IO_BEGIN        = 0xC000;
	const unsigned int _6502_IO_END          = 0xC0FF;
	const unsigned int _6502_MEM_BEGIN = 0x0000;
	const unsigned int _6502_MEM_END   = 0xFFFF;

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
		{TEXT("!=")         , NULL, PARAM_BP_NOT_EQUAL      },
		{TEXT("!" )         , NULL, PARAM_BP_NOT_EQUAL_1    },
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
// Disasm
		{TEXT("BRANCH")     , NULL, PARAM_CONFIG_BRANCH  },
		{TEXT("COLON")      , NULL, PARAM_CONFIG_COLON   },
		{TEXT("OPCODE")     , NULL, PARAM_CONFIG_OPCODE  },
		{TEXT("POINTER")    , NULL, PARAM_CONFIG_POINTER },
		{TEXT("SPACES")		, NULL, PARAM_CONFIG_SPACES  },
		{TEXT("TARGET")     , NULL, PARAM_CONFIG_TARGET  },
// Disk
		{TEXT("EJECT")      , NULL, PARAM_DISK_EJECT     },
		{TEXT("PROTECT")    , NULL, PARAM_DISK_PROTECT   },
		{TEXT("READ")       , NULL, PARAM_DISK_READ      },
// Font (Config)
		{TEXT("MODE")       , NULL, PARAM_FONT_MODE      }, // also INFO, CONSOLE, DISASM (from Window)
// General
		{TEXT("FIND")       , NULL, PARAM_FIND           },
		{TEXT("BRANCH")     , NULL, PARAM_BRANCH         },
		{"CATEGORY"         , NULL, PARAM_CATEGORY       },
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
		{"*"           , NULL, PARAM_WILDSTAR        },
		{"BOOKMARKS"   , NULL, PARAM_CAT_BOOKMARKS   },
		{"BREAKPOINTS" , NULL, PARAM_CAT_BREAKPOINTS },
		{"CONFIG"      , NULL, PARAM_CAT_CONFIG      },
		{"CPU"         , NULL, PARAM_CAT_CPU         },
//		{TEXT("EXPRESSION") ,
		{"FLAGS"       , NULL, PARAM_CAT_FLAGS       },
		{"HELP"        , NULL, PARAM_CAT_HELP        },
		{"KEYBOARD"    , NULL, PARAM_CAT_KEYBOARD    },
		{"MEMORY"      , NULL, PARAM_CAT_MEMORY      }, // alias // SOURCE [SYMBOLS] [MEMORY] filename
		{"OUTPUT"      , NULL, PARAM_CAT_OUTPUT      },
		{"OPERATORS"   , NULL, PARAM_CAT_OPERATORS   },
		{"RANGE"       , NULL, PARAM_CAT_RANGE       },
//		{TEXT("REGISTERS")  , NULL, PARAM_CAT_REGISTERS   },
		{"SYMBOLS"     , NULL, PARAM_CAT_SYMBOLS     },
		{"VIEW"			, NULL, PARAM_CAT_VIEW        },
		{"WATCHES"     , NULL, PARAM_CAT_WATCHES     },
		{"WINDOW"      , NULL, PARAM_CAT_WINDOW      },
		{"ZEROPAGE"    , NULL, PARAM_CAT_ZEROPAGE    },
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
// View
//		{TEXT("VIEW")       , NULL, PARAM_SRC_??? },
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

	char   g_aSourceFileName[ MAX_PATH ] = "";

	MemoryTextFile_t g_AssemblerSourceBuffer;

	int    g_iSourceDisplayStart  = 0;
	int    g_nSourceAssembleBytes = 0;
	int    g_nSourceAssemblySymbols = 0;

	// TODO: Support multiple source filenames
	SourceAssembly_t g_aSourceDebug;



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

	char      g_sFileNameConfig     [] = 
#ifdef MSDOS
		"AWDEBUGR.CFG";
#else
		"AppleWinDebugger.cfg";
#endif

	char      g_sFileNameTrace      [] = "Trace.txt";

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
	bool      g_bTraceHeader     = false; // semaphore, flag header to be printed

	DWORD     extbench      = 0;
	bool      g_bDebuggerViewingAppleOutput = false;

	bool      g_bIgnoreNextKey = false;

// Private ________________________________________________________________________________________


// Prototypes _______________________________________________________________

	static	int ParseInput ( LPTSTR pConsoleInput, bool bCook = true );
	static	Update_t ExecuteCommand ( int nArgs );

// Breakpoints
	void _BWZ_List ( const Breakpoint_t * aBreakWatchZero, const int iBWZ ); // bool bZeroBased = true );
	void _BWZ_ListAll ( const Breakpoint_t * aBreakWatchZero, const int nMax );

//	bool CheckBreakpoint (WORD address, BOOL memory);
	bool CheckBreakpointsIO ();
	bool CheckBreakpointsReg ();
	bool _CmdBreakpointAddReg ( Breakpoint_t *pBP, BreakpointSource_t iSrc, BreakpointOperator_t iCmp, WORD nAddress, int nLen );
	int  _CmdBreakpointAddCommonArg ( int iArg, int nArg, BreakpointSource_t iSrc, BreakpointOperator_t iCmp );

// Config - Colors
	static	void _ConfigColorsReset ( BYTE *pPalDst = 0 );

// Config - Save
	bool ConfigSave_BufferToDisk ( char *pFileName, ConfigSave_t eConfigSave );
	void ConfigSave_PrepareHeader ( const Parameters_e eCategory, const Commands_e eCommandClear );

// Drawing
	static	bool DebuggerSetColor ( const int iScheme, const int iColor, const COLORREF nColor );
	static	void _CmdColorGet ( const int iScheme, const int iColor );

// Font
	static	void _UpdateWindowFontHeights (int nFontHeight);

// Source Level Debugging
	static	bool BufferAssemblyListing ( char * pFileName );
	static	bool ParseAssemblyListing ( bool bBytesToMemory, bool bAddSymbols );


// Window
	void _WindowJoin ();
	void _WindowSplit (Window_e eNewBottomWindow );
	void _WindowLast ();
	void _WindowSwitch ( int eNewWindow );
	int  WindowGetHeight ( int iWindow );
	void WindowUpdateDisasmSize ();
	void WindowUpdateConsoleDisplayedSize ();
	void WindowUpdateSizes                ();
	Update_t _CmdWindowViewFull   (int iNewWindow);
	Update_t _CmdWindowViewCommon (int iNewWindow);

// Utility
	char FormatCharTxtCtrl ( const BYTE b, bool *pWasCtrl_ );
	char FormatCharTxtAsci ( const BYTE b, bool *pWasAsci_ );
	char FormatCharTxtHigh ( const BYTE b, bool *pWasHi_ );
	char FormatChar4Font ( const BYTE b, bool *pWasHi_, bool *pWasLo_ );

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




// Bookmarks __________________________________________________________________


//===========================================================================
bool _Bookmark_Add( const int iBookmark, const WORD nAddress )
{
	if (iBookmark < MAX_BOOKMARKS)
	{
	//	g_aBookmarks.push_back( nAddress );
//		g_aBookmarks.at( iBookmark ) = nAddress;
		g_aBookmarks[ iBookmark ].nAddress = nAddress;
		g_aBookmarks[ iBookmark ].bSet     = true;
		g_nBookmarks++;
		return true;
	}
	
	return false;
}


//===========================================================================
bool _Bookmark_Del( const WORD nAddress )
{
	bool bDeleted = false;

//	int nSize = g_aBookmarks.size();
	int iBookmark;
	for (iBookmark = 0; iBookmark < MAX_BOOKMARKS; iBookmark++ )
	{
		if (g_aBookmarks[ iBookmark ].nAddress == nAddress)
		{
//			g_aBookmarks.at( iBookmark ) = NO_6502_TARGET;
			g_aBookmarks[ iBookmark ].bSet = false;
			bDeleted = true;
		}
	}
	return bDeleted;
}

bool Bookmark_Find( const WORD nAddress )
{
	// Ugh, linear search
//	int nSize = g_aBookmarks.size();
	int iBookmark;
	for (iBookmark = 0; iBookmark < MAX_BOOKMARKS; iBookmark++ )
	{
		if (g_aBookmarks[ iBookmark ].nAddress == nAddress)
		{
			if (g_aBookmarks[ iBookmark ].bSet)
				return true;
		}
	}
	return false;
}


//===========================================================================
bool _Bookmark_Get( const int iBookmark, WORD & nAddress )
{
//	int nSize = g_aBookmarks.size();
	if (iBookmark >= MAX_BOOKMARKS)
		return false;

	if (g_aBookmarks[ iBookmark ].bSet)
	{
		nAddress = g_aBookmarks[ iBookmark ].nAddress;
		return true;
	}

	return false;
}


//===========================================================================
void _Bookmark_Reset()
{
//	g_aBookmarks.reserve( MAX_BOOKMARKS );
//	g_aBookmarks.insert( g_aBookma	int iBookmark = 0;
	int iBookmark = 0;
	for (iBookmark = 0; iBookmark < MAX_BOOKMARKS; iBookmark++ )
	{
		g_aBookmarks[ iBookmark ].bSet = false;
	}
}


//===========================================================================
int _Bookmark_Size()
{
	int g_nBookmarks = 0;

	int iBookmark;
	for (iBookmark = 0; iBookmark < MAX_BOOKMARKS; iBookmark++ )
	{
		if (g_aBookmarks[ iBookmark ].bSet)
			g_nBookmarks++;
	}

	return g_nBookmarks;
}

//===========================================================================
Update_t CmdBookmark (int nArgs)
{
	return CmdBookmarkAdd( nArgs );
}

//===========================================================================
Update_t CmdBookmarkAdd (int nArgs )
{
	// BMA [address]
	// BMA # address
	if (! nArgs)
	{
		return CmdZeroPageList( 0 );
	}

	int iArg = 1;
	int iBookmark = NO_6502_TARGET;

	if (nArgs > 1)
	{
		iBookmark = g_aArgs[ 1 ].nValue;
		iArg++;
	}

	bool bAdded = false;
	for (; iArg <= nArgs; iArg++ )
	{
		WORD nAddress = g_aArgs[ iArg ].nValue;

		if (iBookmark == NO_6502_TARGET)
		{
			iBookmark = 0;
			while ((iBookmark < MAX_BOOKMARKS) && (g_aBookmarks[iBookmark].bSet))
			{
				iBookmark++;
			}
		}

		if ((iBookmark >= MAX_BOOKMARKS) && !bAdded)
		{
			char sText[ CONSOLE_WIDTH ];
			sprintf( sText, "All bookmarks are currently in use.  (Max: %d)", MAX_BOOKMARKS );
			ConsoleDisplayPush( sText );
			return ConsoleUpdate();
		}
		
		if ((iBookmark < MAX_BOOKMARKS) && (g_nBookmarks < MAX_BOOKMARKS))
		{
			g_aBookmarks[iBookmark].bSet = true;
			g_aBookmarks[iBookmark].nAddress = nAddress;
			bAdded = true;
			g_nBookmarks++;
			iBookmark++;
		}
	}

	if (!bAdded)
		goto _Help;

	return UPDATE_DISASM | ConsoleUpdate();

_Help:
	return Help_Arg_1( CMD_BOOKMARK_ADD );

}




//===========================================================================
Update_t CmdBookmarkClear (int nArgs)
{
	int iBookmark = 0;

	bool bClearAll = false;
		
	int iArg;
	for (iArg = 1; iArg <= nArgs; iArg++ )
	{
		if (! _tcscmp(g_aArgs[nArgs].sArg, g_aParameters[ PARAM_WILDSTAR ].m_sName))
		{
			for (iBookmark = 0; iBookmark < MAX_BOOKMARKS; iBookmark++ )
			{
				if (g_aBookmarks[ iBookmark ].bSet)
					g_aBookmarks[ iBookmark ].bSet = false;
			}
			break;
		}

		iBookmark = g_aArgs[ iArg ].nValue;
		if (g_aBookmarks[ iBookmark ].bSet)
			g_aBookmarks[ iBookmark ].bSet = false;
	}

	return UPDATE_DISASM;
}


//===========================================================================
Update_t CmdBookmarkGoto ( int nArgs )
{
	if (! nArgs)
		return Help_Arg_1( CMD_BOOKMARK_GOTO );

	int iBookmark = g_aArgs[ 1 ].nValue;

	WORD nAddress;
	if (_Bookmark_Get( iBookmark, nAddress ))
	{
		g_nDisasmCurAddress = nAddress;
		g_nDisasmCurLine = 0;
		DisasmCalcTopBotAddress();
	}

	return UPDATE_DISASM;
}


//===========================================================================
Update_t CmdBookmarkList (int nArgs)
{
	if (! g_nBookmarks)
	{
		TCHAR sText[ CONSOLE_WIDTH ];
		wsprintf( sText, TEXT("  There are no current bookmarks.  (Max: %d)"), MAX_BOOKMARKS );
		ConsoleBufferPush( sText );
	}
	else
	{
		_BWZ_ListAll( g_aBookmarks, MAX_BOOKMARKS );
	}
	return ConsoleUpdate();
}


//===========================================================================
Update_t CmdBookmarkLoad (int nArgs)
{
	char sFilePath[ MAX_PATH ] = "";

	if (nArgs == 1)
	{
//		strcpy( sMiniFileName, pFileName );
	//	strcat( sMiniFileName, ".aws" ); // HACK: MAGIC STRING

//		_tcscpy(sFileName, g_sCurrentDir); // 
//		_tcscat(sFileName, sMiniFileName);
	}

	return UPDATE_CONSOLE_DISPLAY;
}


//===========================================================================
Update_t CmdBookmarkSave (int nArgs)
{
	TCHAR sText[ CONSOLE_WIDTH ];

	g_ConfigState.Reset();

	ConfigSave_PrepareHeader( PARAM_CAT_BOOKMARKS, CMD_BOOKMARK_CLEAR );

	int iBookmark = 0;
	while (iBookmark < MAX_BOOKMARKS)
	{
		if (g_aBookmarks[ iBookmark ].bSet)
		{
			sprintf( sText, "%s %x %04X\n"
				, g_aCommands[ CMD_BOOKMARK_ADD ].m_sName
				, iBookmark
				, g_aBookmarks[ iBookmark ].nAddress
			);
			g_ConfigState.PushLine( sText );
		}
		iBookmark++;
	}

	if (nArgs)
	{
		if (! (g_aArgs[ 1 ].bType & TYPE_QUOTED_2))
			return Help_Arg_1( CMD_BOOKMARK_SAVE );

		if (ConfigSave_BufferToDisk( g_aArgs[ 1 ].sArg, CONFIG_SAVE_FILE_CREATE ))
		{
			ConsoleBufferPush( TEXT( "Saved." ) );
			return ConsoleUpdate();
		}
	}

	return UPDATE_CONSOLE_DISPLAY;
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
	g_nAppMode = MODE_RUNNING;
	FrameRefreshStatus(DRAW_TITLE);
	VideoRedrawScreen();
	DWORD currtime = GetTickCount();
	while ((extbench = GetTickCount()) != currtime)
		; // intentional busy-waiting
	KeybQueueKeypress(TEXT(' '),1);
	g_bResetTiming = true;

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
						// ConsoleBufferPush( sText );
						ConsolePrint( sText );
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


// Breakpoints ____________________________________________________________________________________


//===========================================================================
Update_t CmdBreakInvalid (int nArgs) // Breakpoint IFF Full-speed!
{
	if ((nArgs > 2) || (nArgs == 0))
		goto _Help;

	int iType = 0; // default to BRK
	int nActive = 0;

//	if (nArgs == 2)
	iType = g_aArgs[ 1 ].nValue;

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
		int iOpcode = g_aArgs[ 1] .nValue;
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
		wsprintf( sText, TEXT("%s full speed Break on Opcode: None")
			, sAction
			, g_iDebugOnOpcode
			, g_aOpcodes65C02[ g_iDebugOnOpcode ].sMnemonic
		);
	else
		// Show what the current break opcode is
		wsprintf( sText, TEXT("%s full speed Break on Opcode: %02X %s")
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

	_6502_GetTargets( regs.pc, &aTarget[0], &aTarget[1], &nBytes );
	
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
Update_t CmdBreakpoint (int nArgs)
{
	return CmdBreakpointAddPC( nArgs );
}


// smart breakpoint
//===========================================================================
Update_t CmdBreakpointAddSmart (int nArgs)
{
	int nAddress = g_aArgs[1].nValue;

	if (! nArgs)
	{
		nArgs = 1;
		g_aArgs[ nArgs ].nValue = g_nDisasmCurAddress;		
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
				case PARAM_BP_NOT_EQUAL_1  : iCmp = BP_OP_NOT_EQUAL    ; bHaveCmp = true; break;
				case PARAM_BP_GREATER_THAN : iCmp = BP_OP_GREATER_THAN ; bHaveCmp = true; break;
				case PARAM_BP_GREATER_EQUAL: iCmp = BP_OP_GREATER_EQUAL; bHaveCmp = true; break;
				default:
					break;
			}
		}

		if ((! bHaveSrc) && (! bHaveCmp))
		{
			int dArgs = _CmdBreakpointAddCommonArg( iArg, nArgs, iSrc, iCmp );
			if (!dArgs)
			{
				return Help_Arg_1( CMD_BREAKPOINT_ADD_REG );
			}
			iArg += dArgs;
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


// @return Number of args processed
//===========================================================================
int _CmdBreakpointAddCommonArg ( int iArg, int nArg, BreakpointSource_t iSrc, BreakpointOperator_t iCmp )
{
	int dArg = 0;

	int iBreakpoint = 0;
	Breakpoint_t *pBP = & g_aBreakpoints[ iBreakpoint ];

	while ((iBreakpoint < MAX_BREAKPOINTS) && g_aBreakpoints[iBreakpoint].bSet) //g_aBreakpoints[iBreakpoint].nLength)
	{
		iBreakpoint++;
		pBP++;
	}

	if (iBreakpoint >= MAX_BREAKPOINTS)
	{
		ConsoleDisplayError(TEXT("All Breakpoints slots are currently in use."));
		return dArg;
	}

	if (iArg <= nArg)
	{
#if DEBUG_VAL_2
		int nLen = g_aArgs[iArg].nVal2;
#endif
		WORD nAddress  = 0;
		WORD nAddress2 = 0;
		WORD nEnd      = 0;
		int  nLen      = 0;

		dArg = 1;
		RangeType_t eRange = Range_Get( nAddress, nAddress2, iArg);
		if ((eRange == RANGE_HAS_END) ||
			(eRange == RANGE_HAS_LEN))
		{
			Range_CalcEndLen( eRange, nAddress, nAddress2, nEnd, nLen );
			dArg = 2;
		}

		if ( !nLen)
		{
			nLen = 1;
		}

		if (! _CmdBreakpointAddReg( pBP, iSrc, iCmp, nAddress, nLen ))
		{
			dArg = 0;
		}
		g_nBreakpoints++;
	}

	return dArg;
}


//===========================================================================
Update_t CmdBreakpointAddPC (int nArgs)
{
	BreakpointSource_t   iSrc = BP_SRC_REG_PC;
	BreakpointOperator_t iCmp = BP_OP_EQUAL  ;

	if (!nArgs)
	{
		nArgs = 1;
//		g_aArgs[1].nValue = regs.pc;
		g_aArgs[1].nValue = g_nDisasmCurAddress;
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
			int dArg = _CmdBreakpointAddCommonArg( iArg, nArgs, iSrc, iCmp );
			if (! dArg)
			{
				return Help_Arg_1( CMD_BREAKPOINT_ADD_PC );
			}
			iArg += dArg;
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
			int dArg = _CmdBreakpointAddCommonArg( iArg, nArgs, iSrc, iCmp );
			if (! dArg)
			{
				return Help_Arg_1( CMD_BREAKPOINT_ADD_MEM );
			}
			iArg += dArg;
		}
	}

	return UPDATE_BREAKPOINTS | UPDATE_CONSOLE_DISPLAY;
}


//===========================================================================
void _BWZ_Clear( Breakpoint_t * aBreakWatchZero, int iSlot )
{
	aBreakWatchZero[ iSlot ].bSet     = false;
	aBreakWatchZero[ iSlot ].bEnabled = false;
	aBreakWatchZero[ iSlot ].nLength  = 0;
}

void _BWZ_RemoveOne( Breakpoint_t *aBreakWatchZero, const int iSlot, int & nTotal )
{
	if (aBreakWatchZero[iSlot].bSet)
	{
		_BWZ_Clear( aBreakWatchZero, iSlot );
		nTotal--;
	}
}

void _BWZ_RemoveAll( Breakpoint_t *aBreakWatchZero, const int nMax, int & nTotal )
{
	for( int iSlot = 0; iSlot < nMax; iSlot++ )
	{
		_BWZ_RemoveOne( aBreakWatchZero, iSlot, nTotal );
	}
}

// called by BreakpointsClear, WatchesClear, ZeroPagePointersClear
//===========================================================================
void _BWZ_ClearViaArgs( int nArgs, Breakpoint_t * aBreakWatchZero, const int nMax, int & nTotal )
{
	int iSlot = 0;
	
	// Clear specified breakpoints
	while (nArgs)
	{
		iSlot = g_aArgs[nArgs].nValue;

		if (! _tcscmp(g_aArgs[nArgs].sArg, g_aParameters[ PARAM_WILDSTAR ].m_sName))
		{
			_BWZ_RemoveAll( aBreakWatchZero, nMax, nTotal );
			break;
		}
		else
		if ((iSlot >= 0) && (iSlot < nMax))
		{
			_BWZ_RemoveOne( aBreakWatchZero, iSlot, nTotal );
		}

		nArgs--;
	}
}

// called by BreakpointsEnable, WatchesEnable, ZeroPagePointersEnable
// called by BreakpointsDisable, WatchesDisable, ZeroPagePointersDisable
void _BWZ_EnableDisableViaArgs( int nArgs, Breakpoint_t * aBreakWatchZero, const int nMax, const bool bEnabled )
{
	int iSlot = 0;

	// Enable each breakpoint in the list
	while (nArgs)
	{
		iSlot = g_aArgs[nArgs].nValue;

		if (! _tcscmp(g_aArgs[nArgs].sArg, g_aParameters[ PARAM_WILDSTAR ].m_sName))
		{
			for( ; iSlot < nMax; iSlot++ )
			{
				aBreakWatchZero[ iSlot ].bEnabled = bEnabled;
			}			
		}
		else
		if ((iSlot >= 0) && (iSlot < nMax))
		{
			aBreakWatchZero[ iSlot ].bEnabled = bEnabled;
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
		_BWZ_RemoveAll( g_aBreakpoints, MAX_BREAKPOINTS, g_nBreakpoints );
	}
	else
	{
		_BWZ_ClearViaArgs( nArgs, g_aBreakpoints, MAX_BREAKPOINTS, g_nBreakpoints );
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

	_BWZ_EnableDisableViaArgs( nArgs, g_aBreakpoints, MAX_BREAKPOINTS, false );

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

	_BWZ_EnableDisableViaArgs( nArgs, g_aBreakpoints, MAX_BREAKPOINTS, true );

	return UPDATE_BREAKPOINTS;
}


void _BWZ_List( const Breakpoint_t * aBreakWatchZero, const int iBWZ ) //, bool bZeroBased )
{
	static       char sText[ CONSOLE_WIDTH ];
	static const char sFlags[] = "-*";
	static       char sName[ MAX_SYMBOLS_LEN+1 ];

	WORD nAddress = aBreakWatchZero[ iBWZ ].nAddress;
	LPCTSTR pSymbol = GetSymbol( nAddress, 2 );
	if (! pSymbol)
	{
		sName[0] = 0;
		pSymbol = sName;
	}

	sprintf( sText, "  #%d %c %04X %s",
//		(bZeroBased ? iBWZ + 1 : iBWZ),
		iBWZ,
		sFlags[ (int) aBreakWatchZero[ iBWZ ].bEnabled ],
		aBreakWatchZero[ iBWZ ].nAddress,
		pSymbol
	);
	ConsoleBufferPush( sText );
}

void _BWZ_ListAll( const Breakpoint_t * aBreakWatchZero, const int nMax )
{
	int iBWZ = 0;
	while (iBWZ < MAX_BOOKMARKS)
	{
		if (aBreakWatchZero[ iBWZ ].bSet)
		{
			_BWZ_List( aBreakWatchZero, iBWZ );
		}
		iBWZ++;
	}
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
		TCHAR sText[ CONSOLE_WIDTH ];
		wsprintf( sText, TEXT("  There are no current breakpoints.  (Max: %d)"), MAX_BREAKPOINTS );
		ConsoleBufferPush( sText );
	}
	else
	{	
		_BWZ_ListAll( g_aBreakpoints, MAX_BREAKPOINTS );
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
	TCHAR sText[ CONSOLE_WIDTH ];

	g_ConfigState.Reset();

	ConfigSave_PrepareHeader( PARAM_CAT_BREAKPOINTS, CMD_BREAKPOINT_CLEAR );

	int iBreakpoint = 0;
	while (iBreakpoint < MAX_BREAKPOINTS)
	{
		if (g_aBreakpoints[ iBreakpoint ].bSet)
		{
			sprintf( sText, "%s %x %04X,%04X\n"
				, g_aCommands[ CMD_BREAKPOINT_ADD_REG ].m_sName
				, iBreakpoint
				, g_aBreakpoints[ iBreakpoint ].nAddress
				, g_aBreakpoints[ iBreakpoint ].nLength
			);
			g_ConfigState.PushLine( sText );
		}
		if (! g_aBreakpoints[ iBreakpoint ].bEnabled)
		{
			sprintf( sText, "%s %x\n"
				, g_aCommands[ CMD_BREAKPOINT_DISABLE ].m_sName
				, iBreakpoint
			);
			g_ConfigState.PushLine( sText );
		}
		
		iBreakpoint++;
	}

	if (nArgs)
	{
		if (! (g_aArgs[ 1 ].bType & TYPE_QUOTED_2))
			return Help_Arg_1( CMD_BREAKPOINT_SAVE );

		if (ConfigSave_BufferToDisk( g_aArgs[ 1 ].sArg, CONFIG_SAVE_FILE_CREATE ))
		{
			ConsoleBufferPush( TEXT( "Saved." ) );
			return ConsoleUpdate();
		}
	}

	return UPDATE_CONSOLE_DISPLAY;
}

// Assembler ______________________________________________________________________________________

//===========================================================================
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
		SymbolUpdate( SYMBOLS_ASSEMBLY, pSymbolName, nAddress, false, true ); // bool bRemoveSymbol, bool bUpdateSymbol )

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
		
	g_nAssemblerAddress = g_aArgs[1].nValue;

	if (nArgs == 1)
	{
		int iArg = 1;
		
		// undocumented ASM *
		if ((! _tcscmp( g_aArgs[ iArg ].sArg, g_aParameters[ PARAM_WILDSTAR        ].m_sName )) ||
			(! _tcscmp( g_aArgs[ iArg ].sArg, g_aParameters[ PARAM_MEM_SEARCH_WILD ].m_sName )) )
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
	// g_nAssemblerAddress; // g_aArgs[1].nValue;
//	return ConsoleUpdate();

	return UPDATE_CONSOLE_DISPLAY;
}

// CPU ____________________________________________________________________________________________
// CPU Step, Trace ________________________________________________________________________________

//===========================================================================
Update_t CmdGo (int nArgs)
{
	// G StopAddress [SkipAddress,Length]
	// Example:
	//	G C600 FA00,FFFF 
	// TODO: G addr1,len   addr3,len
	// TODO: G addr1:addr2 addr3:addr4

	g_nDebugSteps = -1;
	g_nDebugStepCycles  = 0;
	g_nDebugStepStart = regs.pc;
	g_nDebugStepUntil = nArgs ? g_aArgs[1].nValue : -1;
	g_nDebugSkipStart = -1;
	g_nDebugSkipLen   = -1;

	if (nArgs > 4)
		return Help_Arg_1( CMD_GO );

	//     G StopAddress [SkipAddress,Len]
	// Old   1            2           2   
	//     G addr addr [, len]
	// New   1    2     3 4
	if (nArgs > 1)
	{
		int iArg = 2;
		g_nDebugSkipStart = g_aArgs[ iArg ].nValue;

#if DEBUG_VAL_2
		WORD nAddress     = g_aArgs[ iArg ].nVal2;
#endif
		int nLen = 0;
		int nEnd = 0;

		if (nArgs > 2)
		{
			if (g_aArgs[ iArg + 1 ].eToken == TOKEN_COMMA)
			{
				if (nArgs > 3)
				{
					nLen = g_aArgs[ iArg + 2 ].nValue;
					nEnd = g_nDebugSkipStart + nLen;
					if (nEnd > _6502_MEM_END)
						nEnd = _6502_MEM_END + 1;
				}
				else
				{
					return Help_Arg_1( CMD_GO );
				}
			}
			else
			if (g_aArgs[ iArg+ 1 ].eToken == TOKEN_COLON)
			{
				nEnd = g_aArgs[ iArg + 2 ].nValue + 1;
			}
			else
				return Help_Arg_1( CMD_GO );
		}
		else
			return Help_Arg_1( CMD_GO );
		
		nLen = nEnd - g_nDebugSkipStart;
		if (nLen < 0)
			nLen = -nLen;
		g_nDebugSkipLen = nLen;
		g_nDebugSkipLen &= _6502_MEM_END;			

#if _DEBUG
	TCHAR sText[ CONSOLE_WIDTH ];
	wsprintf( sText, TEXT("Start: %04X,%04X  End: %04X  Len: %04X"),
		g_nDebugSkipStart, g_nDebugSkipLen, nEnd, nLen );
	ConsoleBufferPush( sText );
	ConsoleBufferToDisplay();
#endif
	}

//	WORD nAddressSymbol = 0;
//	bool bFoundSymbol = FindAddressFromSymbol( g_aArgs[1].sArg, & nAddressSymbol );
//	if (bFoundSymbol)
//		g_nDebugStepUntil = nAddressSymbol;

//  if (!g_nDebugStepUntil)
//    g_nDebugStepUntil = GetAddress(g_aArgs[1].sArg);

	g_bDebuggerEatKey = true;
	g_nAppMode = MODE_STEPPING;
	FrameRefreshStatus(DRAW_TITLE);

	return UPDATE_CONSOLE_DISPLAY; // TODO: Verify // 0;
}

//===========================================================================
Update_t CmdStepOver (int nArgs)
{
	// assert( g_nDisasmCurAddress == regs.pc );

//	g_nDebugSteps = nArgs ? g_aArgs[1].nValue : 1;
	WORD nDebugSteps = nArgs ? g_aArgs[1].nValue : 1;

	while (nDebugSteps -- > 0)
	{
		int nOpcode = *(mem + regs.pc); // g_nDisasmCurAddress
	//	int eMode = g_aOpcodes[ nOpcode ].addrmode;
	//	int nByte = g_aOpmodes[eMode]._nBytes;
	//	if ((eMode ==  AM_A) && 

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
	if (_6502_GetStackReturnAddress( nAddress ))
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
	g_nDebugSteps = nArgs ? g_aArgs[1].nValue : 1;
	g_nDebugStepCycles  = 0;
	g_nDebugStepStart = regs.pc;
	g_nDebugStepUntil = -1;
	g_nAppMode = MODE_STEPPING;
	FrameRefreshStatus(DRAW_TITLE);
	DebugContinueStepping();

	return UPDATE_ALL; // TODO: Verify // 0
}

//===========================================================================
Update_t CmdTraceFile (int nArgs)
{
	char sText[ CONSOLE_WIDTH ] = "";

	if (g_hTraceFile)
	{
		fclose( g_hTraceFile );
		g_hTraceFile = NULL;

		sprintf( sText, "Trace stopped." );
	}
	else
	{
		char sFileName[MAX_PATH];

		if (nArgs)
			strcpy( sFileName, g_aArgs[1].sArg );
		else
			strcpy( sFileName, g_sFileNameTrace );


		char sFilePath[ MAX_PATH ];
		strcpy(sFilePath, g_sCurrentDir); // g_sProgramDir
		strcat(sFilePath, sFileName );

		g_hTraceFile = fopen( sFilePath, "wt" );

		if (g_hTraceFile)
		{
			sprintf( sText, "Trace started: %s", sFileName );

			g_bTraceHeader = true;
		}
		else
		{
			sprintf( sText, "Trace ERROR: %s", sFileName );
		}
	}
	
	ConsoleBufferPush( sText );
	ConsoleBufferToDisplay();

	return UPDATE_ALL; // TODO: Verify // 0
}

//===========================================================================
Update_t CmdTraceLine (int nArgs)
{
	g_nDebugSteps = nArgs ? g_aArgs[1].nValue : 1;
	g_nDebugStepCycles  = 1;
	g_nDebugStepStart = regs.pc;
	g_nDebugStepUntil = -1;

	g_nAppMode = MODE_STEPPING;
	FrameRefreshStatus(DRAW_TITLE);
	DebugContinueStepping();

	return UPDATE_ALL; // TODO: Verify // 0
}




// Unassemble
//===========================================================================
Update_t CmdUnassemble (int nArgs)
{
	if (! nArgs)
		return Help_Arg_1( CMD_UNASSEMBLE );
  
	WORD nAddress = g_aArgs[1].nValue;
	g_nDisasmTopAddress = nAddress;

	DisasmCalcCurFromTopAddress();
	DisasmCalcBotFromTopAddress();

	return UPDATE_DISASM;
}


//===========================================================================
Update_t CmdKey (int nArgs)
{
	KeybQueueKeypress(
		nArgs ? g_aArgs[1].nValue ? g_aArgs[1].nValue : g_aArgs[1].sArg[0] : TEXT(' '), 1); // FIXME!!!
	return UPDATE_CONSOLE_DISPLAY;
}

//===========================================================================
Update_t CmdIn (int nArgs)
{
	if (!nArgs)
		return Help_Arg_1( CMD_IN );
  
	WORD nAddress = g_aArgs[1].nValue;

	BYTE nPageOffset = nAddress & 0xFF;
	IORead[ (nAddress>>4) & 0xF ](regs.pc, nAddress  & 0xFF, 0, 0, 0); // g_aArgs[1].nValue 

	return UPDATE_CONSOLE_DISPLAY; // TODO: Verify // 1
}


//===========================================================================
Update_t CmdJSR (int nArgs)
{
	if (! nArgs)
		return Help_Arg_1( CMD_JSR );

	WORD nAddress = g_aArgs[1].nValue & _6502_MEM_END;

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

 	_6502_GetOpcodeOpmodeOpbyte( iOpcode, iOpmode, nOpbytes );

	while (nOpbytes--)
	{
		*(mem+regs.pc + nOpbytes) = 0xEA;
	}

	return UPDATE_ALL;
}

//===========================================================================
Update_t CmdOut (int nArgs)
{
//  if ((!nArgs) ||
//      ((g_aArgs[1].sArg[0] != TEXT('0')) && (!g_aArgs[1].nValue) && (!GetAddress(g_aArgs[1].sArg))))
//     return DisplayHelp(CmdInput);

	if (!nArgs)
		Help_Arg_1( CMD_OUT );

	WORD nAddress = g_aArgs[1].nValue;

	BYTE nPageOffset = nAddress & 0xFF;
	IOWrite[ (nAddress>>4) & 0xF ] (regs.pc, nAddress & 0xFF, 1, g_aArgs[2].nValue & 0xFF, 0);

	return UPDATE_CONSOLE_DISPLAY; // TODO: Verify // 1
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
	if (iColor < NUM_DEBUG_COLORS)
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

	if ((g_iColorScheme < NUM_COLOR_SCHEMES) && (iColor < NUM_DEBUG_COLORS))
	{
		nColor = g_aColors[ g_iColorScheme ][ iColor ];
	}

	return nColor;
}


bool DebuggerSetColor( const int iScheme, const int iColor, const COLORREF nColor )
{
	bool bStatus = false;
	if ((g_iColorScheme < NUM_COLOR_SCHEMES) && (iColor < NUM_DEBUG_COLORS))
	{
		g_aColors[ iScheme ][ iColor ] = nColor;
		bStatus = true;
	}

	// Propogate to console since it has its own copy of colors
	if (iColor == FG_CONSOLE_OUTPUT)
	{
		COLORREF nConsole = DebuggerGetColor( FG_CONSOLE_OUTPUT );
		g_anConsoleColor[ CONSOLE_COLOR_x ] = nConsole;
	}
	
	return bStatus;
}

//===========================================================================
Update_t CmdConfigColorMono (int nArgs)
{
	int iScheme = 0;
	
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

	int iColor = g_aArgs[ 1 ].nValue;
	if ((iColor < 0) || iColor >= NUM_DEBUG_COLORS)
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
			int R = g_aArgs[2].nValue & 0xFF;
			int G = g_aArgs[3].nValue & 0xFF;
			int B = g_aArgs[4].nValue & 0xFF;
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

	int iColor = g_aArgs[ 1 ].nValue;
	if ((iColor < 0) || iColor >= NUM_DEBUG_COLORS)
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

// Config _________________________________________________________________________________________


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
bool ConfigSave_BufferToDisk ( char *pFileName, ConfigSave_t eConfigSave )
{
	bool bStatus = false;

	char sModeCreate[] = "w+t";
	char sModeAppend[] = "a+t";
	char *pMode = NULL;
	if (eConfigSave == CONFIG_SAVE_FILE_CREATE)
		pMode = sModeCreate;
	else
	if (eConfigSave == CONFIG_SAVE_FILE_APPEND)
		pMode = sModeAppend;

	char sFileName[ MAX_PATH ];

	_tcscpy(sFileName, g_sCurrentDir);
	_tcscat(sFileName, pFileName    );

	FILE *hFile = fopen( pFileName, pMode );

	if (hFile)
	{
		char *pText;
		int   nLine = g_ConfigState.GetNumLines();
		int   iLine;
		
		for( iLine = 0; iLine < nLine; iLine++ )
		{
			pText = g_ConfigState.GetLine( iLine );
			if ( pText )
			{
				fputs( pText, hFile );
			}
		}
		
		fclose( hFile );
		bStatus = true;
	}
	else
	{
	}

	return bStatus;
}


//===========================================================================
void ConfigSave_PrepareHeader ( const Parameters_e eCategory, const Commands_e eCommandClear )
{
	char sText[ CONSOLE_WIDTH ];

	sprintf( sText, "%s %s = %s\n"
		, g_aTokens[ TOKEN_COMMENT_EOL  ].sToken
		, g_aParameters[ PARAM_CATEGORY ].m_sName
		, g_aParameters[ eCategory ]
		);
	g_ConfigState.PushLine( sText );

	sprintf( sText, "%s %s\n"
		, g_aCommands[ eCommandClear ].m_sName
		, g_aParameters[ PARAM_WILDSTAR ].m_sName
	);
	g_ConfigState.PushLine( sText );
}


// Save Debugger Settings
//===========================================================================
Update_t CmdConfigSave (int nArgs)
{
	TCHAR sFilename[ MAX_PATH ];
	_tcscpy( sFilename, g_sProgramDir ); // g_sCurrentDir
	_tcscat( sFilename, g_sFileNameConfig );

/*
	HANDLE hFile = CreateFile( sfilename,
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

		CloseHandle( hFile );
	}
*/
		// Bookmarks
		CmdBookmarkSave( 0 );

		// Breakpoints
		CmdBreakpointSave( 0 );
		
		// Watches
		CmdWatchSave( 0 );

		// Zeropage pointers
		CmdZeroPageSave( 0 );

		// Color Palete

		// Color Index
//		CmdColorSave( 0 );

		// UserSymbol

		// History

	return UPDATE_CONSOLE_DISPLAY;
}


// Config - Disasm ________________________________________________________________________________

//===========================================================================
Update_t CmdConfigDisasm( int nArgs )
{
	int iParam = 0;
	TCHAR sText[ CONSOLE_WIDTH ];

	bool bDisplayCurrentSettings = false;

//	if (! _tcscmp( g_aArgs[ 1 ].sArg, g_aParameters[ PARAM_WILDSTAR ].m_sName ))
	if (! nArgs)
	{
		bDisplayCurrentSettings = true;
		nArgs = PARAM_CONFIG_NUM;
	}
	else
	{
		if (nArgs > 2)
			return Help_Arg_1( CMD_CONFIG_DISASM );
	}

	for (int iArg = 1; iArg <= nArgs; iArg++ )
	{
		if (bDisplayCurrentSettings)
			iParam = _PARAM_CONFIG_BEGIN + iArg - 1;
		else
		if (FindParam( g_aArgs[iArg].sArg, MATCH_FUZZY, iParam ))
		{
		}

			switch (iParam)
			{
				case PARAM_CONFIG_BRANCH:
					if ((nArgs > 1) && (! bDisplayCurrentSettings)) // set
					{					
						iArg++;
						g_iConfigDisasmBranchType = g_aArgs[ iArg ].nValue;
						if (g_iConfigDisasmBranchType < 0)
							g_iConfigDisasmBranchType = 0;
						if (g_iConfigDisasmBranchType >= NUM_DISASM_BRANCH_TYPES)
							g_iConfigDisasmBranchType = NUM_DISASM_BRANCH_TYPES - 1;

					}
					else // show current setting
					{
						wsprintf( sText, TEXT( "Branch Type: %d" ), g_iConfigDisasmBranchType );
						ConsoleBufferPush( sText );
						ConsoleBufferToDisplay();
					}
					break;

				case PARAM_CONFIG_COLON:
					if ((nArgs > 1) && (! bDisplayCurrentSettings)) // set
					{					
						iArg++;
						g_bConfigDisasmAddressColon = (g_aArgs[ iArg ].nValue) ? true : false;
					}
					else // show current setting
					{
						int iState = g_bConfigDisasmAddressColon ? PARAM_ON : PARAM_OFF;
						wsprintf( sText, TEXT( "Colon: %s" ), g_aParameters[ iState ].m_sName );
						ConsoleBufferPush( sText );
						ConsoleBufferToDisplay();
					}
					break;

				case PARAM_CONFIG_OPCODE:
					if ((nArgs > 1) && (! bDisplayCurrentSettings)) // set
					{
						iArg++;
						g_bConfigDisasmOpcodesView = (g_aArgs[ iArg ].nValue) ? true : false;
					}
					else
					{
						int iState = g_bConfigDisasmOpcodesView ? PARAM_ON : PARAM_OFF;
						wsprintf( sText, TEXT( "Opcodes: %s" ), g_aParameters[ iState ].m_sName );
						ConsoleBufferPush( sText );
						ConsoleBufferToDisplay();
					}
					break;

				case PARAM_CONFIG_POINTER:
					if ((nArgs > 1) && (! bDisplayCurrentSettings)) // set
					{
						iArg++;
						g_bConfigInfoTargetPointer = (g_aArgs[ iArg ].nValue) ? true : false;
					}
					else
					{
						int iState = g_bConfigInfoTargetPointer ? PARAM_ON : PARAM_OFF;
						wsprintf( sText, TEXT( "Info Target Pointer: %s" ), g_aParameters[ iState ].m_sName );
						ConsoleBufferPush( sText );
						ConsoleBufferToDisplay();
					}
					break;

				case PARAM_CONFIG_SPACES:
					if ((nArgs > 1) && (! bDisplayCurrentSettings)) // set
					{
						iArg++;
						g_bConfigDisasmOpcodeSpaces = (g_aArgs[ iArg ].nValue) ? true : false;
					}
					else
					{
						int iState = g_bConfigDisasmOpcodeSpaces ? PARAM_ON : PARAM_OFF;
						wsprintf( sText, TEXT( "Opcode spaces: %s" ), g_aParameters[ iState ].m_sName );
						ConsoleBufferPush( sText );
						ConsoleBufferToDisplay();
					}
					break;
					
				case PARAM_CONFIG_TARGET:
					if ((nArgs > 1) && (! bDisplayCurrentSettings)) // set
					{					
						iArg++;
						g_iConfigDisasmTargets = g_aArgs[ iArg ].nValue;
						if (g_iConfigDisasmTargets < 0)
							g_iConfigDisasmTargets = 0;
						if (g_iConfigDisasmTargets >= NUM_DISASM_TARGET_TYPES)
							g_iConfigDisasmTargets = NUM_DISASM_TARGET_TYPES - 1;
					}
					else // show current setting
					{
						wsprintf( sText, TEXT( "Target: %d" ), g_iConfigDisasmTargets );
						ConsoleBufferPush( sText );
						ConsoleBufferToDisplay();
					}
					break;

				default:
					return Help_Arg_1( CMD_CONFIG_DISASM ); // CMD_CONFIG_DISASM_OPCODE );
			}
//		}
//		else
//			return Help_Arg_1( CMD_CONFIG_DISASM );
	}
	return UPDATE_CONSOLE_DISPLAY | UPDATE_DISASM;
}

// Config - Font __________________________________________________________________________________


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

	int nMode = g_aArgs[ 2 ].nValue;

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
	if (nArgs <= 2) // nArgs
	{
		iArg = 1;

		// FONT * is undocumented, like VERSION *
		if ((! _tcscmp( g_aArgs[ iArg ].sArg, g_aParameters[ PARAM_WILDSTAR        ].m_sName )) ||
			(! _tcscmp( g_aArgs[ iArg ].sArg, g_aParameters[ PARAM_MEM_SEARCH_WILD ].m_sName )) )
		{
			TCHAR sText[ CONSOLE_WIDTH ];
			wsprintf( sText, "Lines: %d  Font Px: %d  Line Px: %d"
				, g_nDisasmDisplayLines
				, g_aFontConfig[ FONT_DISASM_DEFAULT ]._nFontHeight
				, g_aFontConfig[ FONT_DISASM_DEFAULT ]._nLineHeight );
			ConsoleBufferPush( sText );
			ConsoleBufferToDisplay();
			return UPDATE_CONSOLE_DISPLAY;
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
		int nConsoleTopY = GetConsoleTopPixels( g_nConsoleDisplayLines );

		int nHeight = 0;

		if (g_iFontSpacing == FONT_SPACING_CLASSIC)
		{
			nHeight = nFontHeight + 1;
			g_nDisasmDisplayLines = nConsoleTopY / nHeight;
		}
		else
		if (g_iFontSpacing == FONT_SPACING_CLEAN)
		{		
			nHeight = nFontHeight;
			g_nDisasmDisplayLines = nConsoleTopY / nHeight;
		}
		else
		if (g_iFontSpacing == FONT_SPACING_COMPRESSED)
		{
			nHeight = nFontHeight - 1;
			g_nDisasmDisplayLines = (nConsoleTopY + nHeight) / nHeight; // Ceil()
		}

		g_aFontConfig[ FONT_DISASM_DEFAULT ]._nLineHeight = nHeight;
		
//		int nHeightOptimal = (nHeight0 + nHeight1) / 2;
//		int nLinesOptimal = nConsoleTopY / nHeightOptimal;
//		g_nDisasmDisplayLines = nLinesOptimal;

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
#if OLD_FONT
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
#endif
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
//		int iOpcode;
		int iOpmode;
		int nOpbytes;

		for( int iLine = 0; iLine <= nLen; iLine++ ) // min 1 opcode/instruction
		{
// a.
			_6502_GetOpmodeOpbyte( iAddress, iOpmode, nOpbytes );
// b.
//			_6502_GetOpcodeOpmodeOpbyte( iOpcode, iOpmode, nOpbytes );

			if (iLine == g_nDisasmCurLine) // && (iAddress == g_nDisasmCurAddress))
			{
				if (iAddress == g_nDisasmCurAddress)
// b.
//					&& (iOpmode != AM_1) &&
//					&& (iOpmode != AM_2) &&
//					&& (iOpmode != AM_3) &&
//					&& _6502_IsOpcodeValid( iOpcode))
				{
					g_nDisasmTopAddress = iTop;
					bFound = true;
					break;
				}
			}

			// .20 Fixed: DisasmCalcTopFromCurAddress()
			//if ((eMode >= AM_1) && (eMode <= AM_3))
#if 0 // _DEBUG
			TCHAR sText[ CONSOLE_WIDTH ];
			wsprintf( sText, "%04X : %d bytes\n", iAddress, nOpbytes );
			OutputDebugString( sText );
#endif
			iAddress += nOpbytes;
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
		// Moving the cursor line around is not really a good idea, since we're breaking consistency paradigm for the user.
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


//===========================================================================
WORD DisasmCalcAddressFromLines( WORD iAddress, int nLines )
{
	while (nLines-- > 0)
	{
		int iOpmode;
		int nOpbytes;
		_6502_GetOpmodeOpbyte( iAddress, iOpmode, nOpbytes );
		iAddress += nOpbytes;
	}
	return iAddress;
}


//===========================================================================
void DisasmCalcCurFromTopAddress()
{
	g_nDisasmCurAddress = DisasmCalcAddressFromLines( g_nDisasmTopAddress, g_nDisasmCurLine );
}


//===========================================================================
void DisasmCalcBotFromTopAddress( )
{
	g_nDisasmBotAddress = DisasmCalcAddressFromLines( g_nDisasmTopAddress, g_nDisasmWinHeight );
}


//===========================================================================
void DisasmCalcTopBotAddress ()
{
	DisasmCalcTopFromCurAddress();
	DisasmCalcBotFromTopAddress();
}


//===========================================================================
Update_t CmdCursorFollowTarget ( int nArgs )
{
	WORD nAddress = 0;
	if (_6502_GetTargetAddress( g_nDisasmCurAddress, nAddress ))
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
Update_t CmdCursorLineDown (int nArgs)
{
	int iOpmode; 
	int nOpbytes;
	_6502_GetOpmodeOpbyte( g_nDisasmCurAddress, iOpmode, nOpbytes ); // g_nDisasmTopAddress

	if (g_iWindowThis == WINDOW_DATA)
	{
		_CursorMoveDownAligned( WINDOW_DATA_BYTES_PER_LINE );
		DisasmCalcTopBotAddress();
	}
	else
	if (nArgs) // scroll down by 'n' bytes
	{
		nOpbytes = nArgs; // HACKL g_aArgs[1].val

		g_nDisasmTopAddress += nOpbytes;
		g_nDisasmCurAddress += nOpbytes;
		g_nDisasmBotAddress += nOpbytes;
		DisasmCalcTopBotAddress();
	}
	else
	{
#if DEBUG_SCROLL == 6
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
#endif
		g_nDisasmCurAddress += nOpbytes;

		_6502_GetOpmodeOpbyte( g_nDisasmTopAddress, iOpmode, nOpbytes );
		g_nDisasmTopAddress += nOpbytes;

		_6502_GetOpmodeOpbyte( g_nDisasmBotAddress, iOpmode, nOpbytes );
		g_nDisasmBotAddress += nOpbytes;

		if (g_bDisasmCurBad)
		{
//	MessageBox( NULL, TEXT("Bad Disassembly of opcodes"), TEXT("Debugger"), MB_OK );

//			g_nDisasmCurAddress = nCur;
//			g_bDisasmCurBad = false;
//			DisasmCalcTopFromCurAddress();
			DisasmCalcTopBotAddress();
//			return UPDATE_DISASM;
		}
		g_bDisasmCurBad = false;
	}

	// Can't use use + nBytes due to Disasm Singularity
//	DisasmCalcTopBotAddress();

	return UPDATE_DISASM;
}

// C++ Bug, can't have local structs used in STL containers
		struct LookAhead_t
		{
			int _nAddress;
			int _iOpcode;
			int _iOpmode;
			int _nOpbytes;
		};

//===========================================================================
Update_t CmdCursorLineUp (int nArgs)
{
	int nBytes = 1;
	
	if (g_iWindowThis == WINDOW_DATA)
	{
		_CursorMoveUpAligned( WINDOW_DATA_BYTES_PER_LINE );
	}
	else
	if (nArgs)
	{
		nBytes = nArgs; // HACK: g_aArgs[1].nValue

		g_nDisasmTopAddress--;
		DisasmCalcCurFromTopAddress();
		DisasmCalcBotFromTopAddress();
	}
	else
	{
//		if (! g_nDisasmCurLine)
//		{
//			g_nDisasmCurLine = 1;
//			DisasmCalcTopFromCurAddress( false );
			
//			g_nDisasmCurLine = 0;
//			DisasmCalcCurFromTopAddress();
//			DisasmCalcBotFromTopAddress();
//			return UPDATE_DISASM;
//		}

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
#define DEBUG_SCROLL 3

#if DEBUG_SCROLL == 1
		WORD nCur = g_nDisasmCurAddress - nBytes;

		// Adjust Top until nNewCur is at > Cur
		do
		{
			g_nDisasmTopAddress--;
			DisasmCalcCurFromTopAddress();
		} while (g_nDisasmCurAddress > nCur);
#endif

#if DEBUG_SCROLL == 2
		WORD nCur = g_nDisasmCurAddress - nBytes;

		int iOpcode;
		int iOpmode;
		int nOpbytes;

		int aOpBytes[ 4 ]; // index is relative offset from cursor
		int nLeastDesiredTopAddress = NO_6502_TARGET;

		do
		{
			g_nDisasmTopAddress--;

//			_6502_GetOpcodeOpmodeOpbyte( iOpcode, iOpmode, nOpbytes );
			iOpcode = _6502_GetOpmodeOpbyte( g_nDisasmTopAddress, iOpmode, nOpbytes );
			aOpBytes[ 1 ] = nOpbytes;

			// Disasm is kept in sync.  Maybe bad opcode, but if no other choices...
			if (nOpbytes == 1)
				nLeastDesiredTopAddress = g_nDisasmTopAddress;

			if (   (iOpmode == AM_1)
				|| (iOpmode == AM_2)
				|| (iOpmode == AM_3)
				|| ! _6502_IsOpcodeValid( iOpcode)
				|| (nOpbytes != 1) )
			{
 				g_nDisasmTopAddress--;
				DisasmCalcCurFromTopAddress();

				iOpcode = _6502_GetOpmodeOpbyte( g_nDisasmTopAddress, iOpmode, nOpbytes );
				aOpBytes[ 2 ] = nOpbytes;

				if (   (iOpmode == AM_1)
					|| (iOpmode == AM_2)
					|| (iOpmode == AM_3)
					|| ! _6502_IsOpcodeValid( iOpcode)
					|| (nOpbytes != 2) )
				{
					g_nDisasmTopAddress--;
					DisasmCalcCurFromTopAddress();

					iOpcode = _6502_GetOpmodeOpbyte( g_nDisasmTopAddress, iOpmode, nOpbytes );
					aOpBytes[ 3 ] = nOpbytes;

				if (   (iOpmode == AM_1)
					|| (iOpmode == AM_2)
					|| (iOpmode == AM_3)
					|| (nOpbytes != 3) )
					g_nDisasmTopAddress--;
					DisasmCalcCurFromTopAddress();
				}
			}

			DisasmCalcCurFromTopAddress();
		} while (g_nDisasmCurAddress > nCur);
#endif
#if DEBUG_SCROLL == 3
		// Isn't this the new DisasmCalcTopFromCurAddress() ??
		int iOpcode;
		int iOpmode;
		int nOpbytes;

		const int MAX_LOOK_AHEAD = g_nDisasmWinHeight;

		static vector<LookAhead_t> aTopCandidates;
		LookAhead_t tCandidate;

//		if (! aBestTop.capacity() )
		aTopCandidates.reserve( MAX_LOOK_AHEAD );
		aTopCandidates.erase( aTopCandidates.begin(), aTopCandidates.end() );

		WORD nTop = g_nDisasmTopAddress;
		WORD iTop = 0;
		WORD nCur = 0;

		do
		{
			nTop--;
			nCur = nTop;
			iTop = (g_nDisasmTopAddress - nTop);


			for (int iLine = 0; iLine < MAX_LOOK_AHEAD; iLine++ )
			{
				iOpcode = _6502_GetOpmodeOpbyte( nCur, iOpmode, nOpbytes );

				// If address on iLine = g_nDisasmCurLine + 1
				if (iLine == (g_nDisasmCurLine + 1))
				{
					if (nCur == (g_nDisasmCurAddress))
					{
						iOpcode = _6502_GetOpmodeOpbyte( nTop, iOpmode, nOpbytes );

						tCandidate._nAddress = nTop;
						tCandidate._iOpcode  = iOpcode;
						tCandidate._iOpmode  = iOpmode;
						tCandidate._nOpbytes = nOpbytes;

						aTopCandidates.push_back( tCandidate );
					}
				}
				nCur += nOpbytes;

				if (nCur > g_nDisasmCurAddress)
					break;
			}
		} while (iTop < MAX_LOOK_AHEAD);

		int nCandidates = aTopCandidates.size();
		if (nCandidates)
		{
			int iBest = NO_6502_TARGET;

			int iCandidate = 0;
			for ( ; iCandidate < nCandidates; iCandidate++ )
			{
				tCandidate = aTopCandidates.at( iCandidate );
				iOpcode = tCandidate._iOpcode;
				iOpmode = tCandidate._iOpmode;

				if (   (iOpmode != AM_1)
					&& (iOpmode != AM_2)
					&& (iOpmode != AM_3)
					&& _6502_IsOpcodeValid( iOpcode ) )
				{
					if (g_iConfigDisasmScroll == 1)
					{
						// Favor min opbytes
						if (iBest != NO_6502_TARGET)
							iBest = iCandidate;
					}
					else
					if (g_iConfigDisasmScroll == 3)
					{
						// Favor max opbytes
						iBest = iCandidate;
					}
				}
			}

			// All were "invalid", pick first choice
			if (iBest == NO_6502_TARGET)
				iBest = 0;

			tCandidate = aTopCandidates.at( iBest );
			g_nDisasmTopAddress = tCandidate._nAddress;

			DisasmCalcCurFromTopAddress();
			DisasmCalcBotFromTopAddress();
			g_bDisasmCurBad = false;
		}
		else
		{
			// Singularity
			g_bDisasmCurBad = true;

//			g_nDisasmTopAddress--;
			g_nDisasmCurAddress--;
//			g_nDisasmBotAddress--;
			DisasmCalcTopBotAddress();
		}
#endif

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
Update_t CmdCursorJumpRetAddr (int nArgs)
{
	WORD nAddress = 0;
	if (_6502_GetStackReturnAddress( nAddress ))
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


//===========================================================================
WORD _ClampAddress( int nAddress )
{
	if (nAddress < 0)
		nAddress = 0;
	if (nAddress > _6502_MEM_END)
		nAddress = _6502_MEM_END;

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
				g_aMemDump[0].nAddress &= _6502_MEM_END;
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

		g_nDisasmTopAddress = nNewAddress & _6502_MEM_END; // .21 Fixed: _CursorMoveUpAligned() & _CursorMoveDownAligned() not wrapping around past FF00 to 0, and wrapping around past 0 to FF00
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
				g_aMemDump[0].nAddress &= _6502_MEM_END;
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

		g_nDisasmTopAddress = nNewAddress & _6502_MEM_END; // .21 Fixed: _CursorMoveUpAligned() & _CursorMoveDownAligned() not wrapping around past FF00 to 0, and wrapping around past 0 to FF00
	}
}


//===========================================================================
Update_t CmdCursorPageDown (int nArgs)
{
	int iLines = 0; // show at least 1 line from previous display
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
// 4
//		while (++iLines < nLines)
//			CmdCursorLineDown(nArgs);

// 5
		nLines -= (g_nDisasmCurLine + 1);
		if (nLines < 1)
			nLines = 1;
			
		while (iLines++ < nLines)
		{
			CmdCursorLineDown( 0 ); // nArgs
		}
// 6

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
	int iLines = 0; // show at least 1 line from previous display
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
//		while (++iLines < nLines)
//			CmdCursorLineUp(nArgs);
		nLines -= (g_nDisasmCurLine + 1);
		if (nLines < 1)
			nLines = 1;
			
		while (iLines++ < nLines)
		{
			CmdCursorLineUp( 0 ); // smart line up
			// CmdCursorLineUp( -nLines );
		}
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

	int iDrive = g_aArgs[ 1 ].nValue;

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
			bProtect = g_aArgs[ 3 ].nValue ? true : false;

		DiskSetProtect( iDrive, bProtect );
		FrameRefreshStatus(DRAW_LEDS | DRAW_BUTTON_DRIVES);
	}
	else
	{
		if (nArgs != 3)
			goto _Help;

		LPCTSTR pDiskName = g_aArgs[ 3 ].sArg;

		// DISK # "Diskname"
		DiskInsert( iDrive, pDiskName, IMAGE_FORCE_WRITE_PROTECTED, IMAGE_DONT_CREATE );
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
	WORD nAddress = pArg->nValue;
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
		pArg->nValue = nAddress;
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

	WORD nSrcAddr = g_aArgs[1].nValue;
	WORD nLenByte = 0;
	WORD nDstAddr = g_aArgs[3].nValue;

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

// MEB addr 8_bit_value
//===========================================================================
Update_t CmdMemoryEnterByte (int nArgs)
{
	if ((nArgs < 2) ||
		((g_aArgs[2].sArg[0] != TEXT('0')) && (!g_aArgs[2].nValue))) // arg2 not numeric or not specified
	{
		Help_Arg_1( CMD_MEMORY_ENTER_WORD );
	}
	
	WORD nAddress = g_aArgs[1].nValue;
	while (nArgs >= 2)
	{
		WORD nData = g_aArgs[nArgs].nValue;
		if( nData > 0xFF)
		{
			*(mem + nAddress + nArgs - 2)  = (BYTE)(nData >> 0);
			*(mem + nAddress + nArgs - 1)  = (BYTE)(nData >> 8);
		}
		else
		{
			*(mem + nAddress+nArgs-2)  = (BYTE)nData;
		}
		*(memdirty+(nAddress >> 8)) = 1;
		nArgs--;
	}

	return UPDATE_ALL;
}

// MEW addr 16-bit_vaue
//===========================================================================
Update_t CmdMemoryEnterWord (int nArgs)
{
	if ((nArgs < 2) ||
		((g_aArgs[2].sArg[0] != TEXT('0')) && (!g_aArgs[2].nValue))) // arg2 not numeric or not specified
	{
		Help_Arg_1( CMD_MEMORY_ENTER_WORD );
	}
	
	WORD nAddress = g_aArgs[1].nValue;
	while (nArgs >= 2)
	{
		WORD nData = g_aArgs[nArgs].nValue;

		// Little Endian
		*(mem + nAddress + nArgs - 2)  = (BYTE)(nData >> 0);
		*(mem + nAddress + nArgs - 1)  = (BYTE)(nData >> 8);

		*(memdirty+(nAddress >> 8)) |= 1;
		nArgs--;
	}

	return UPDATE_ALL;
}

//===========================================================================
void MemMarkDirty( WORD nAddressStart, WORD nAddressEnd )
{
	for( int iPage = (nAddressStart >> 8); iPage <= (nAddressEnd >> 8); iPage++ )
	{
		*(memdirty+iPage) = 1;
	}
}

//===========================================================================
Update_t CmdMemoryFill (int nArgs)
{
	// F address end value
	// F address,len value
	// F address:end value
	if ((!nArgs) || (nArgs < 3) || (nArgs > 4))
		return Help_Arg_1( CMD_MEMORY_FILL );

	WORD nAddress2 = 0;
	WORD nAddressStart = 0;
	WORD nAddressEnd = 0;
	int  nAddressLen = 0;
	BYTE nValue = 0;

	if( nArgs == 3)
	{
		nAddressStart = g_aArgs[1].nValue;
		nAddressEnd   = g_aArgs[2].nValue;
		nAddressLen = MIN(_6502_MEM_END , nAddressEnd - nAddressStart + 1 );
	}
	else
	{
		RangeType_t eRange;
		eRange = Range_Get( nAddressStart, nAddress2, 1 );

		if (! Range_CalcEndLen( eRange, nAddressStart, nAddress2, nAddressEnd, nAddressLen ))
			return Help_Arg_1( CMD_MEMORY_MOVE );
	}
#if DEBUG_VAL_2
		nBytes   = MAX(1,g_aArgs[1].nVal2); // TODO: This actually work??
#endif

	if ((nAddressLen > 0) && (nAddressEnd <= _6502_MEM_END))
	{
		MemMarkDirty( nAddressStart, nAddressEnd );

		nValue = g_aArgs[nArgs].nValue & 0xFF;
		while( nAddressStart <= nAddressEnd )
		{
			// TODO: Optimize - split into pre_io, and post_io
			if ((nAddress2 < _6502_IO_BEGIN) || (nAddress2 > _6502_IO_END))
			{
				*(mem + nAddressStart) = nValue;
			}
			nAddressStart++;
		}
	}

	return UPDATE_ALL; // UPDATE_CONSOLE_DISPLAY;
}


static TCHAR g_sMemoryLoadSaveFileName[ MAX_PATH ] = TEXT("");


//===========================================================================
Update_t CmdMemoryLoad (int nArgs)
{
	// BLOAD ["Filename"] , addr[, len] 
	// BLOAD ["Filename"] , addr[: end] 
	//       1            2 3    4 5
	if (nArgs > 5)
		return Help_Arg_1( CMD_MEMORY_LOAD );

	bool bHaveFileName = false;
	int iArgAddress = 3;

	if (g_aArgs[1].bType & TYPE_QUOTED_2)
		bHaveFileName = true;

//	if (g_aArgs[2].bType & TOKEN_QUOTE_DOUBLE)
//		bHaveFileName = true;

	if (nArgs > 1)
	{
		if (g_aArgs[1].bType & TYPE_QUOTED_2)
			bHaveFileName = true;

		int iArgComma1  = 2;
		int iArgAddress = 3;
		int iArgComma2  = 4;
		int iArgLength  = 5;

		if (! bHaveFileName)
		{
			iArgComma1  = 1;
			iArgAddress = 2;
			iArgComma2  = 3;
			iArgLength  = 4;

			if (nArgs > 4)
				return Help_Arg_1( CMD_MEMORY_LOAD );
		}

		if (g_aArgs[ iArgComma1 ].eToken != TOKEN_COMMA)
			return Help_Arg_1( CMD_MEMORY_SAVE );

		TCHAR sLoadSaveFilePath[ MAX_PATH ];
		_tcscpy( sLoadSaveFilePath, g_sCurrentDir ); // g_sProgramDir

		WORD nAddressStart;
		WORD nAddress2   = 0;
		WORD nAddressEnd = 0;
		int  nAddressLen = 0;

		RangeType_t eRange;
		eRange = Range_Get( nAddressStart, nAddress2, iArgAddress );
		if (nArgs > 4)
		{
			if (eRange == RANGE_MISSING_ARG_2)
			{
				return Help_Arg_1( CMD_MEMORY_LOAD );
			}

//			if (eRange == RANGE_MISSING_ARG_2)
			if (! Range_CalcEndLen( eRange, nAddressStart, nAddress2, nAddressEnd, nAddressLen ))
			{
				return Help_Arg_1( CMD_MEMORY_SAVE );
			}
		}
		
		BYTE *pMemory = new BYTE [ _6502_MEM_END + 1 ]; // default 64K buffer
		BYTE *pDst = mem + nAddressStart;
		BYTE *pSrc = pMemory;

		if (bHaveFileName)
		{
			_tcscpy( g_sMemoryLoadSaveFileName, g_aArgs[ 1 ].sArg );
		}
		_tcscat( sLoadSaveFilePath, g_sMemoryLoadSaveFileName );
		
		FILE *hFile = fopen( sLoadSaveFilePath, "rb" );
		if (hFile)
		{
			fseek( hFile, 0, SEEK_END );
			int nFileBytes = ftell( hFile );
			fseek( hFile, 0, SEEK_SET );

			if (nFileBytes > _6502_MEM_END)
				nFileBytes = _6502_MEM_END + 1; // Bank-switched RAMR/ROM is only 16-bit

			// Caller didnt' specify how many bytes to read, default to them all
			if (nAddressLen == 0)
			{
				nAddressLen = nFileBytes;
			}

			size_t nRead = fread( pMemory, nAddressLen, 1, hFile );
			if (nRead == 1) // (size_t)nLen)
			{
				int iByte;
				for( iByte = 0; iByte < nAddressLen; iByte++ )
				{
					*pDst++ = *pSrc++;
				}
				ConsoleBufferPush( TEXT( "Loaded." ) );
			}
			fclose( hFile );
		}
		else
		{
			ConsoleBufferPush( TEXT( "ERROR: Bad filename" ) );
			TCHAR sPath[ MAX_PATH + 8 ] = "Path: ";
			_tcscat( sPath, g_sCurrentDir );
			ConsoleBufferPush( sPath );
			TCHAR sFile[ MAX_PATH + 8 ] = "File: ";
			_tcscat( sFile, g_sMemoryLoadSaveFileName );
			ConsoleBufferPush( sFile );
		}
		
		delete [] pMemory;
	}
	else
		return Help_Arg_1( CMD_MEMORY_LOAD );
	
	return ConsoleUpdate();
}

// dst src : len
//===========================================================================
Update_t CmdMemoryMove (int nArgs)
{
	if (nArgs < 3)
		return Help_Arg_1( CMD_MEMORY_MOVE );

	WORD nDst = g_aArgs[1].nValue;
//	WORD nSrc = g_aArgs[2].nValue;
//	WORD nLen = g_aArgs[3].nValue - nSrc;
	WORD nAddress2 = 0;
	WORD nAddressStart = 0;
	WORD nAddressEnd = 0;
	int  nAddressLen = 0;

	RangeType_t eRange;
	eRange = Range_Get( nAddressStart, nAddress2, 2 );

//		if (eRange == RANGE_MISSING_ARG_2)
	if (! Range_CalcEndLen( eRange, nAddressStart, nAddress2, nAddressEnd, nAddressLen ))
		return Help_Arg_1( CMD_MEMORY_MOVE );

	if ((nAddressLen > 0) && (nAddressEnd <= _6502_MEM_END))
	{
		MemMarkDirty( nAddressStart, nAddressEnd );

//			BYTE *pSrc = mem + nAddressStart;
//			BYTE *pDst = mem + nDst;
//			BYTE *pEnd = pSrc + nAddressLen;

		while( nAddressStart <= nAddressEnd )
		{
			// TODO: Optimize - split into pre_io, and post_io
			if ((nDst < _6502_IO_BEGIN) || (nDst > _6502_IO_END))
			{
				*(mem + nDst) = *(mem + nAddressStart);
			}
			nDst++;
			nAddressStart++;
		}

		return UPDATE_ALL;
	}

	return UPDATE_CONSOLE_DISPLAY;
}


//===========================================================================
Update_t CmdMemorySave (int nArgs)
{
	// BSAVE ["Filename"] , addr , len 
	// BSAVE ["Filename"] , addr : end
	//       1            2 3    4 5
	static WORD nAddressStart = 0;
	       WORD nAddress2     = 0;
	static WORD nAddressEnd   = 0;
	static int  nAddressLen   = 0;

	if (nArgs > 5)
		return Help_Arg_1( CMD_MEMORY_SAVE );

	if (! nArgs)
	{
		TCHAR sLast[ CONSOLE_WIDTH ] = TEXT("");
		if (nAddressLen)
		{
			wsprintf( sLast, TEXT("Last saved: $%04X:$%04X, %04X"),
				nAddressStart, nAddressEnd, nAddressLen );
		}
		else
		{
			wsprintf( sLast, TEXT( "Last saved: none" ) );
		}				
		ConsoleBufferPush( sLast );
	}
	else
	{
		bool bHaveFileName = false;

		if (g_aArgs[1].bType & TYPE_QUOTED_2)
			bHaveFileName = true;

//		if (g_aArgs[1].bType & TOKEN_QUOTE_DOUBLE)
//			bHaveFileName = true;

		int iArgComma1  = 2;
		int iArgAddress = 3;
		int iArgComma2  = 4;
		int iArgLength  = 5;

		if (! bHaveFileName)
		{
			iArgComma1  = 1;
			iArgAddress = 2;
			iArgComma2  = 3;
			iArgLength  = 4;

			if (nArgs > 4)
				return Help_Arg_1( CMD_MEMORY_SAVE );
		}

//		if ((g_aArgs[ iArgComma1 ].eToken != TOKEN_COMMA) || 
//			(g_aArgs[ iArgComma2 ].eToken != TOKEN_COLON))
//			return Help_Arg_1( CMD_MEMORY_SAVE );

		TCHAR sLoadSaveFilePath[ MAX_PATH ];
		_tcscpy( sLoadSaveFilePath, g_sCurrentDir ); // g_sProgramDir

		RangeType_t eRange;
		eRange = Range_Get( nAddressStart, nAddress2, iArgAddress );

//		if (eRange == RANGE_MISSING_ARG_2)
		if (! Range_CalcEndLen( eRange, nAddressStart, nAddress2, nAddressEnd, nAddressLen ))
			return Help_Arg_1( CMD_MEMORY_SAVE );

		if ((nAddressLen) && (nAddressEnd <= _6502_MEM_END))
		{
			if (! bHaveFileName)
			{
				sprintf( g_sMemoryLoadSaveFileName, "%04X.%04X.bin", nAddressStart, nAddressLen ); // nAddressEnd );
			}
			else
			{
				_tcscpy( g_sMemoryLoadSaveFileName, g_aArgs[ 1 ].sArg );
			}
			_tcscat( sLoadSaveFilePath, g_sMemoryLoadSaveFileName );

//				if (nArgs == 2)
			{
				BYTE *pMemory = new BYTE [ nAddressLen ];
				BYTE *pDst = pMemory;
				BYTE *pSrc = mem + nAddressStart;
				
				// memcpy -- copy out of active memory bank
				int iByte;
				for( iByte = 0; iByte < nAddressLen; iByte++ )
				{
					*pDst++ = *pSrc++;
				}

				FILE *hFile = fopen( sLoadSaveFilePath, "rb" );
				if (hFile)
				{
					ConsoleBufferPush( TEXT( "Warning: File already exists.  Overwriting." ) );
					fclose( hFile );
				}

				hFile = fopen( sLoadSaveFilePath, "wb" );
				if (hFile)
				{
					size_t nWrote = fwrite( pMemory, nAddressLen, 1, hFile );
					if (nWrote == 1) // (size_t)nAddressLen)
					{
						ConsoleBufferPush( TEXT( "Saved." ) );
					}
					else
					{
						ConsoleBufferPush( TEXT( "Error saving." ) );
					}
					fclose( hFile );
				}
				
				delete [] pMemory;
			}
		}
	}
	
	return ConsoleUpdate();
}


//===========================================================================
int _SearchMemoryFind(
	MemorySearchValues_t vMemorySearchValues,
	WORD nAddressStart,
	WORD nAddressEnd )
{
	int   nFound = 0;
	g_vMemorySearchResults.erase( g_vMemorySearchResults.begin(), g_vMemorySearchResults.end() );
	g_vMemorySearchResults.push_back( NO_6502_TARGET );

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
Update_t _SearchMemoryDisplay (int nArgs)
{
	const UINT nBuf = CONSOLE_WIDTH * 2;

	int nFound = g_vMemorySearchResults.size() - 1;

	int nLen = 0; // temp
	int nLineLen = 0; // string length of matches for this line, for word-wrap

	TCHAR sMatches[ nBuf ] = TEXT("");
	TCHAR sResult[ nBuf ];
	TCHAR sText[ nBuf ] = TEXT("");

	if (nFound > 0)
	{
		int iFound = 1;
		while (iFound <= nFound)
		{
			WORD nAddress = g_vMemorySearchResults.at( iFound );

//			sprintf( sText, "%2d:$%04X ", iFound, nAddress );
//			int nLen = _tcslen( sText );

			sResult[0] = 0;
			nLen = 0;

			        StringCat( sResult, CHC_COMMAND, nBuf );
			sprintf( sText, "%2d", iFound );
			nLen += StringCat( sResult, sText , nBuf );

			        StringCat( sResult, CHC_DEFAULT, nBuf );
			nLen += StringCat( sResult, ":" , nBuf );

			        StringCat( sResult, CHC_ADDRESS, nBuf );
			sprintf( sText, "$%04X", nAddress );
			nLen += StringCat( sResult, sText, nBuf );

			// Fit on same line?
			if ((nLineLen + nLen) > (g_nConsoleDisplayWidth - 1)) // CONSOLE_WIDTH
			{
				//ConsoleDisplayPush( sMatches );
				ConsolePrint( sMatches );
				_tcscpy( sMatches, sResult );
				nLineLen = nLen;
			}
			else
			{
				StringCat( sMatches, sResult, nBuf );
				nLineLen += nLen;
			}

			iFound++;
		}
		ConsolePrint( sMatches );
	}

//	wsprintf( sMatches, "Total: %d  (#$%04X)", nFound, nFound );
//	ConsoleDisplayPush( sMatches );
		sResult[0] = 0;

		        StringCat( sResult, CHC_USAGE , nBuf );
		nLen += StringCat( sResult, "Total", nBuf );

		        StringCat( sResult, CHC_DEFAULT, nBuf );
		nLen += StringCat( sResult, ": " , nBuf );

		        StringCat( sResult, CHC_NUM_DEC, nBuf );
		sprintf( sText, "%d  ", nFound );
		nLen += StringCat( sResult, sText, nBuf );

		        StringCat( sResult, CHC_ARG_OPT, nBuf );
		nLen += StringCat( sResult, "(" , nBuf );

		        StringCat( sResult, CHC_DEFAULT, nBuf );
		nLen += StringCat( sResult, "#$", nBuf );

		        StringCat( sResult, CHC_NUM_HEX, nBuf );
		sprintf( sText, "%04X", nFound );
		nLen += StringCat( sResult, sText, nBuf );

		        StringCat( sResult, CHC_ARG_OPT, nBuf );
		nLen += StringCat( sResult, ")" , nBuf );
		
		ConsolePrint( sResult );

	// g_vMemorySearchResults is cleared in DebugEnd()

//	return UPDATE_CONSOLE_DISPLAY;
	return ConsoleUpdate();
}


//===========================================================================
Update_t _CmdMemorySearch (int nArgs, bool bTextIsAscii = true )
{
	WORD nAddressStart = 0;
	WORD nAddress2   = 0;
	WORD nAddressEnd = 0;
	int  nAddressLen = 0;

	RangeType_t eRange;
	eRange = Range_Get( nAddressStart, nAddress2 );

//	if (eRange == RANGE_MISSING_ARG_2)
	if (! Range_CalcEndLen( eRange, nAddressStart, nAddress2, nAddressEnd, nAddressLen))
		return ConsoleDisplayError( TEXT("Error: Missing address seperator (comma or colon)" ) );

	int iArgFirstByte = 4;

	// S start,len #
	int nMinLen = nArgs - (iArgFirstByte - 1);

	bool bHaveWildCards = false;
	int iArg;

	MemorySearchValues_t vMemorySearchValues;
	MemorySearch_e       tLastType = MEM_SEARCH_BYTE_N_WILD;
	
	// Get search "string"
	Arg_t *pArg = & g_aArgs[ iArgFirstByte ];
	
	WORD nTarget;
	for (iArg = iArgFirstByte; iArg <= nArgs; iArg++, pArg++ )
	{
		MemorySearch_t ms;

		nTarget = pArg->nValue;
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
				// Convert string to hex byte(s)
				int iChar = 0;
				int nChars = pArg->nArgLen;

				if (nChars)
				{
					ms.m_iType = MEM_SEARCH_BYTE_EXACT;
					ms.m_bFound = false;

					while (iChar < nChars)
					{
						ms.m_nValue = pArg->sArg[ iChar ];

						// Ascii (Low-Bit)
						// Apple (High-Bit)
//						if (! bTextIsAscii) // NOTE: Single quote chars is opposite hi-bit !!!
//							ms.m_nValue &= 0x7F;
//						else
							ms.m_nValue |= 0x80;

						// last char is handle in common case below
						iChar++; 
						if (iChar < nChars)
							vMemorySearchValues.push_back( ms );
					}
				}
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

					while (iChar < nChars)
					{
						ms.m_nValue = pArg->sArg[ iChar ];

						// Ascii (Low-Bit)
						// Apple (High-Bit)
//						if (bTextIsAscii)
							ms.m_nValue &= 0x7F;
//						else
//							ms.m_nValue |= 0x80;

						iChar++; // last char is handle in common case below
						if (iChar < nChars) 
							vMemorySearchValues.push_back( ms );
					}
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
						ms.m_nValue = pArg->nValue & 0x0F;
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
							ms.m_nValue = (pArg->nValue << 4) & 0xF0;
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
	// S address,length # [,#]
	if (nArgs < 4)
		return HelpLastCommand();

	return _CmdMemorySearch( nArgs, true );

	return UPDATE_CONSOLE_DISPLAY;
}


// Search for ASCII text (no Hi-Bit set)
//===========================================================================
Update_t CmdMemorySearchAscii (int nArgs)
{
	if (nArgs < 4)
		return HelpLastCommand();

	return _CmdMemorySearch( nArgs, true );
}

// Search for Apple text (Hi-Bit set)
//===========================================================================
Update_t CmdMemorySearchApple (int nArgs)
{
	if (nArgs < 4)
		return HelpLastCommand();

	return _CmdMemorySearch( nArgs, false );
}

//===========================================================================
Update_t CmdMemorySearchHex (int nArgs)
{
	if (nArgs < 4)
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
	if (nArgs < 2) // || ((g_aArgs[2].sArg[0] != TEXT('0')) && !g_aArgs[2].nValue))
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
				
			BYTE b = (BYTE)(g_aArgs[ iArg ].nValue & 0xFF);
			WORD w = (WORD)(g_aArgs[ iArg ].nValue & 0xFFFF);

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


// Output _________________________________________________________________________________________


//===========================================================================
Update_t CmdOutputCalc (int nArgs)
{
	const int nBits = 8;

	if (! nArgs)
		return Help_Arg_1( CMD_OUTPUT_CALC );

	WORD nAddress = g_aArgs[1].nValue;
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
Update_t CmdOutputEcho (int nArgs)
{
	TCHAR sText[ CONSOLE_WIDTH ] = TEXT("");

	if (g_aArgs[1].bType & TYPE_QUOTED_2)
	{
		ConsoleDisplayPush( g_aArgs[1].sArg );
	}
	else
	{
		const char *pText = g_pConsoleFirstArg; // ConsoleInputPeek();
		if (pText)
		{
			ConsoleDisplayPush( pText );
		}
	}

	return ConsoleUpdate();
}


enum PrintState_e
{	  PS_LITERAL
	, PS_TYPE
	, PS_ESCAPE
	, PS_NEXT_ARG_BIN
	, PS_NEXT_ARG_HEX
	, PS_NEXT_ARG_DEC
	, PS_NEXT_ARG_CHR
};

struct PrintFormat_t
{
	int nValue;
	int eType;
};


//===========================================================================
Update_t CmdOutputPrint (int nArgs)
{
	// PRINT "A:",A," X:",X
	// Removed: PRINT "A:%d",A," X: %d",X
	TCHAR sText[ CONSOLE_WIDTH ] = TEXT("");
	int nLen = 0;

	int nValue;

	if (! nArgs)
		goto _Help;

	int iArg;
	for (iArg = 1; iArg <= nArgs; iArg++ )
	{
		if (g_aArgs[ iArg ].bType & TYPE_QUOTED_2)
		{
			int iChar;
			int nChar = _tcslen( g_aArgs[ iArg ].sArg );
			for( iChar = 0; iChar < nChar; iChar++ )
			{
				TCHAR c = g_aArgs[ iArg ].sArg[ iChar ];
				sText[ nLen++ ] = c;
			}

			iArg++;
//			if (iArg > nArgs)
//				goto _Help;
			if (iArg <= nArgs)
				if (g_aArgs[ iArg ].eToken != TOKEN_COMMA)
					goto _Help;
		}
		else
		{			
			nValue = g_aArgs[ iArg ].nValue;
			sprintf( &sText[ nLen ], "%04X", nValue );

			while (sText[ nLen ])
				nLen++;

			iArg++;
			if (iArg <= nArgs)
				if (g_aArgs[ iArg ].eToken != TOKEN_COMMA)
					goto _Help;
		}		
#if 0
		sprintf( &sText[ nLen ], "%04X", nValue );
		sprintf( &sText[ nLen ], "%d", nValue );
		sprintf( &sText[ nLen ], "%c", nValue );
#endif
	}

	if (nLen)
		ConsoleBufferPush( sText );

	return ConsoleUpdate();

_Help:
	return Help_Arg_1( CMD_OUTPUT_PRINT );
}


//===========================================================================
Update_t CmdOutputPrintf (int nArgs)
{
	// PRINTF "A:%d X:%d",A,X
	// PRINTF "Hex:%x  Dec:%d  Bin:%z",A,A,A

	TCHAR sText[ CONSOLE_WIDTH ] = TEXT("");

//	vector<PrintFormat_t> aValues;
//	PrintFormat_t entry;
	vector<Arg_t> aValues;
	Arg_t         entry;
	int iValue = 0;
	int nValue = 0;

	if (! nArgs)
		goto _Help;

	int nLen = 0;

	PrintState_e eThis = PS_LITERAL;
//	PrintState_e eNext = PS_NEXT_ARG_HEX; // PS_LITERAL;

	int nWidth = 0;

	int iArg;
	for (iArg = 1; iArg <= nArgs; iArg++ )
	{
		if (g_aArgs[ iArg ].bType & TYPE_QUOTED_2)
			continue;
		else
		if (g_aArgs[ iArg ].eToken == TOKEN_COMMA)
			continue;
		else
		{
//			entry.eType  = PS_LITERAL;
			entry.nValue = g_aArgs[ iArg ].nValue;
			aValues.push_back( entry );
//			nValue = g_aArgs[ iArg ].nValue;
//			aValues.push_back( nValue );
		}
	}
	const int nParamValues = (int) aValues.size();

	for (iArg = 1; iArg <= nArgs; iArg++ )
	{
		if (g_aArgs[ iArg ].bType & TYPE_QUOTED_2)
		{
			int iChar;
			int nChar = _tcslen( g_aArgs[ iArg ].sArg );
			for( iChar = 0; iChar < nChar; iChar++ )
			{
				TCHAR c = g_aArgs[ iArg ].sArg[ iChar ];
				switch ( eThis )
				{
					case PS_LITERAL:
						switch( c )
						{
							case '\\':
								eThis = PS_ESCAPE;
							case '%':
								eThis = PS_TYPE;
								break;
							default:
								sText[ nLen++ ] = c;
								break;
						}
						break;
					case PS_ESCAPE:
						switch( c )
						{
							case 'n':
							case 'r':
								eThis = PS_LITERAL;
								sText[ nLen++ ] = '\n';
								break;
						}
						break;
					case PS_TYPE:
						if (iValue >= nParamValues)
						{
							wsprintf( sText, TEXT("Error: Missing value arg: %d"), iValue + 1 );
							ConsoleBufferPush( sText );
							return ConsoleUpdate();
						}
						switch( c )
						{
							case 'X':
							case 'x': // PS_NEXT_ARG_HEX
								nValue = aValues[ iValue ].nValue;
								sprintf( &sText[ nLen ], "%04X", nValue );
								iValue++;
								break;
							case 'D':
							case 'd': // PS_NEXT_ARG_DEC
								nValue = aValues[ iValue ].nValue;
								sprintf( &sText[ nLen ], "%d", nValue );
								iValue++;
								break;
							break;
							case 'Z':
							case 'z':
							{
								nValue = aValues[ iValue ].nValue;
								if (!nWidth)
									nWidth = 8;
								int nBits = nWidth;
								while (nBits-- > 0)
								{
									if ((nValue >> nBits) & 1)
										sText[ nLen++ ] = '1';
									else
										sText[ nLen++ ] = '0';
								}
								iValue++;
								break;
							}
							case 'c': // PS_NEXT_ARG_CHR;
								nValue = aValues[ iValue ].nValue;
								sprintf( &sText[ nLen ], "%c", nValue );
								iValue++;
								break;
							case '%':
							default:
								sText[ nLen++ ] = c;
								break;
						}
						while (sText[ nLen ])
							nLen++;
						eThis = PS_LITERAL;
						break;
					default:
						break;
				}
			}
		}
		else
		if (g_aArgs[ iArg ].eToken == TOKEN_COMMA)
		{			
			iArg++;
			if (iArg > nArgs)
				goto _Help;
		}
		else
			goto _Help;
	}

	if (nLen)
		ConsoleBufferPush( sText );

	return ConsoleUpdate();

_Help:
	return Help_Arg_1( CMD_OUTPUT_PRINTF );
}


//===========================================================================
Update_t CmdOutputRun (int nArgs)
{
	if (! nArgs)
		return Help_Arg_1( CMD_OUTPUT_RUN );

	if (nArgs != 1)
		return Help_Arg_1( CMD_OUTPUT_RUN );
	
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

	_tcscpy(sFileName, g_sCurrentDir); // 
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
		char sText[ CONSOLE_WIDTH ];
		sprintf( sText, "%sCouldn't load filename: %s%s"
			, CHC_ERROR
			, CHC_STRING
			, sFileName
		);
		ConsolePrint( sText );
	}	

	return ConsoleUpdate();
}


// Source Level Debugging _________________________________________________________________________

//===========================================================================
bool BufferAssemblyListing( char *pFileName )
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

	const DWORD INVALID_ADDRESS = _6502_MEM_END + 1;

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
						g_aSymbols[ SYMBOLS_SRC_2 ][ (WORD) nAddress] = sName;
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
				_tcscpy(sFileName,g_sProgramDir);
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


// View ___________________________________________________________________________________________

// See: CmdWindowViewOutput (int nArgs)
enum ViewVideoPage_t
{
	VIEW_PAGE_X, // current page
	VIEW_PAGE_1,
	VIEW_PAGE_2
};

Update_t _ViewOutput( ViewVideoPage_t iPage, VideoUpdateFuncPtr_t pfUpdate );

Update_t _ViewOutput( ViewVideoPage_t iPage, VideoUpdateFuncPtr_t pfUpdate )
{
	g_VideoForceFullRedraw = true;
	_Video_Dirty();
	switch( iPage ) 
	{
		case VIEW_PAGE_X: _Video_SetupBanks( g_bVideoDisplayPage2 ); break; // Page Current
		case VIEW_PAGE_1: _Video_SetupBanks( false ); break; // Page 1
		case VIEW_PAGE_2: _Video_SetupBanks( true ); break; // Page 2 !
		default:
			break;
	}
	_Video_RedrawScreen( pfUpdate );
	g_bDebuggerViewingAppleOutput = true;
	return UPDATE_NOTHING; // intentional
}

// Text 40
	Update_t CmdViewOutput_Text4X (int nArgs)
	{
		return _ViewOutput( VIEW_PAGE_X, Update40ColCell );
	}
	Update_t CmdViewOutput_Text41 (int nArgs)
	{
		return _ViewOutput( VIEW_PAGE_1, Update40ColCell );
	}
	Update_t CmdViewOutput_Text42 (int nArgs)
	{
		return _ViewOutput( VIEW_PAGE_2, Update40ColCell );
	}
// Text 80
	Update_t CmdViewOutput_Text8X (int nArgs)
	{
		return _ViewOutput( VIEW_PAGE_X, Update80ColCell );
	}
	Update_t CmdViewOutput_Text81 (int nArgs)
	{
		return _ViewOutput( VIEW_PAGE_1, Update80ColCell );
	}
	Update_t CmdViewOutput_Text82 (int nArgs)
	{
		return _ViewOutput( VIEW_PAGE_2, Update80ColCell );
	}
// Lo-Res
	Update_t CmdViewOutput_GRX (int nArgs)
	{
		return _ViewOutput( VIEW_PAGE_X, UpdateLoResCell );
	}
	Update_t CmdViewOutput_GR1 (int nArgs)
	{
		return _ViewOutput( VIEW_PAGE_1, UpdateLoResCell );
	}
	Update_t CmdViewOutput_GR2 (int nArgs)
	{
		return _ViewOutput( VIEW_PAGE_2, UpdateLoResCell );
	}
// Double Lo-Res
	Update_t CmdViewOutput_DGRX (int nArgs)
	{
		return _ViewOutput( VIEW_PAGE_X, UpdateDLoResCell );
	}
	Update_t CmdViewOutput_DGR1 (int nArgs)
	{
		return _ViewOutput( VIEW_PAGE_1, UpdateDLoResCell );
	}
	Update_t CmdViewOutput_DGR2 (int nArgs)
	{
		return _ViewOutput( VIEW_PAGE_2, UpdateDLoResCell );
	}
// Hi-Res
	Update_t CmdViewOutput_HGRX (int nArgs)
	{
		return _ViewOutput( VIEW_PAGE_X, UpdateHiResCell );
	}
	Update_t CmdViewOutput_HGR1 (int nArgs)
	{
		return _ViewOutput( VIEW_PAGE_1, UpdateHiResCell );
	}
	Update_t CmdViewOutput_HGR2 (int nArgs)
	{
		return _ViewOutput( VIEW_PAGE_2, UpdateHiResCell );
	}
// Double Hi-Res
	Update_t CmdViewOutput_DHGRX (int nArgs)
	{
		return _ViewOutput( VIEW_PAGE_X, UpdateDHiResCell );
	}
	Update_t CmdViewOutput_DHGR1 (int nArgs)
	{
		return _ViewOutput( VIEW_PAGE_1, UpdateDHiResCell );
	}
	Update_t CmdViewOutput_DHGR2 (int nArgs)
	{
		return _ViewOutput( VIEW_PAGE_2, UpdateDHiResCell );
	}

// Watches ________________________________________________________________________________________


//===========================================================================
Update_t CmdWatch (int nArgs)
{
	return CmdWatchAdd( nArgs );
}


//===========================================================================
Update_t CmdWatchAdd (int nArgs)
{
	// WA [adddress]
	// WA # address
	if (! nArgs)
	{
		return CmdWatchList( 0 );
	}

	int iArg = 1;
	int iWatch = NO_6502_TARGET;
	if (nArgs > 1)
	{
		iWatch = g_aArgs[ 1 ].nValue;
		iArg++;
	}

	bool bAdded = false;
	for (; iArg <= nArgs; iArg++ )
	{
		WORD nAddress = g_aArgs[iArg].nValue;

		// Make sure address isn't an IO address
		if ((nAddress >= _6502_IO_BEGIN) && (nAddress <= _6502_IO_END))
			return ConsoleDisplayError(TEXT("You may not watch an I/O location."));

		if (iWatch == NO_6502_TARGET)
		{
			iWatch = 0;
			while ((iWatch < MAX_ZEROPAGE_POINTERS) && (g_aWatches[iWatch].bSet))
			{
				iWatch++;
			}
		}

		if ((iWatch >= MAX_WATCHES) && !bAdded)
		{
			char sText[ CONSOLE_WIDTH ];
			sprintf( sText, "All watches are currently in use.  (Max: %d)", MAX_WATCHES );
			ConsoleDisplayPush( sText );
			return ConsoleUpdate();
		}

		if ((iWatch < MAX_WATCHES) && (g_nWatches < MAX_WATCHES))
		{
			g_aWatches[iWatch].bSet = true;
			g_aWatches[iWatch].bEnabled = true;
			g_aWatches[iWatch].nAddress = (WORD) nAddress;
			bAdded = true;
			g_nWatches++;
			iWatch++;
		}
	}

	if (!bAdded)
		goto _Help;
	
	return UPDATE_WATCH;
	
_Help:
	return Help_Arg_1( CMD_WATCH_ADD );
}

//===========================================================================
Update_t CmdWatchClear (int nArgs)
{
	if (!g_nWatches)
		return ConsoleDisplayError(TEXT("There are no watches defined."));

	if (!nArgs)
		return Help_Arg_1( CMD_WATCH_CLEAR );	

	_BWZ_ClearViaArgs( nArgs, g_aWatches, MAX_WATCHES, g_nWatches );

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

	_BWZ_EnableDisableViaArgs( nArgs, g_aWatches, MAX_WATCHES, false );

	return UPDATE_WATCH;
}

//===========================================================================
Update_t CmdWatchEnable (int nArgs)
{
	if (! g_nWatches)
		return ConsoleDisplayError(TEXT("There are no watches defined."));

	if (!nArgs)
		return Help_Arg_1( CMD_WATCH_ENABLE );

	_BWZ_EnableDisableViaArgs( nArgs, g_aWatches, MAX_WATCHES, true );

	return UPDATE_WATCH;
}

//===========================================================================
Update_t CmdWatchList (int nArgs)
{
	if (! g_nWatches)
	{
		TCHAR sText[ CONSOLE_WIDTH ];
		wsprintf( sText, TEXT("  There are no current watches.  (Max: %d)"), MAX_WATCHES );
		ConsoleBufferPush( sText );
	}
	else
	{
		_BWZ_List( g_aWatches, MAX_WATCHES );
	}
	return ConsoleUpdate();
}

/*
//===========================================================================
Update_t CmdWatchLoad (int nArgs)
{
	if (!nArgs)
		return Help_Arg_1( CMD_WATCH_LOAD );

	return UPDATE_CONSOLE_DISPLAY;
}
*/

//===========================================================================
Update_t CmdWatchSave (int nArgs)
{
	if (!nArgs)
		return Help_Arg_1( CMD_WATCH_SAVE );

	return UPDATE_CONSOLE_DISPLAY;
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
	g_nConsoleDisplayLines = MIN_DISPLAY_CONSOLE_LINES;
#if USE_APPLE_FONT
	g_bConsoleFullWidth = true;
	g_nConsoleDisplayWidth = CONSOLE_WIDTH - 1;

	if (g_iWindowThis == WINDOW_CONSOLE)
	{
		g_nConsoleDisplayLines = MAX_DISPLAY_LINES;
		g_nConsoleDisplayWidth = CONSOLE_WIDTH - 1;
		g_bConsoleFullWidth = true;
	}
#else
	g_nConsoleDisplayWidth = (CONSOLE_WIDTH / 2) + 10;
	g_bConsoleFullWidth = false;

//	g_bConsoleFullWidth = false;
//	g_nConsoleDisplayWidth = CONSOLE_WIDTH - 10;

	if (g_iWindowThis == WINDOW_CONSOLE)
	{
		g_nConsoleDisplayLines = MAX_DISPLAY_LINES;
		g_nConsoleDisplayWidth = CONSOLE_WIDTH - 1;
		g_bConsoleFullWidth = true;
	}
#endif
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
		g_nDisasmWinHeight = (MAX_DISPLAY_LINES - g_nConsoleDisplayLines) / 2;
	}
	else
	{
		g_nDisasmWinHeight = MAX_DISPLAY_LINES - g_nConsoleDisplayLines;
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
Update_t CmdZeroPage (int nArgs)
{
	// ZP [address]
	// ZP # address
	return CmdZeroPageAdd( nArgs );
}

//===========================================================================
Update_t CmdZeroPageAdd     (int nArgs)
{
	// ZP [address]
	// ZP # address [address...]
	if (! nArgs)
	{
		return CmdZeroPageList( 0 );
	}

	int iArg = 1;
	int iZP = NO_6502_TARGET;

	if (nArgs > 1)
	{
		iZP = g_aArgs[ 1 ].nValue;
		iArg++;
	}
	
	bool bAdded = false;
	for (; iArg <= nArgs; iArg++ )
	{
		WORD nAddress = g_aArgs[iArg].nValue;

		if (iZP == NO_6502_TARGET)
		{
			iZP = 0;
			while ((iZP < MAX_ZEROPAGE_POINTERS) && (g_aZeroPagePointers[iZP].bSet))
			{
				iZP++;
			}
		}

		if ((iZP >= MAX_ZEROPAGE_POINTERS) && !bAdded)
		{
			char sText[ CONSOLE_WIDTH ];
			sprintf( sText, "All zero page pointers are currently in use.  (Max: %d)", MAX_ZEROPAGE_POINTERS );
			ConsoleDisplayPush( sText );
			return ConsoleUpdate();
		}
		
		if ((iZP < MAX_ZEROPAGE_POINTERS) && (g_nZeroPagePointers < MAX_ZEROPAGE_POINTERS))
		{
			g_aZeroPagePointers[iZP].bSet = true;
			g_aZeroPagePointers[iZP].bEnabled = true;
			g_aZeroPagePointers[iZP].nAddress = (BYTE) nAddress;
			bAdded = true;
			g_nZeroPagePointers++;
			iZP++;
		}
	}

	if (!bAdded)
		goto _Help;
	
	return UPDATE_ZERO_PAGE | ConsoleUpdate();

_Help:
	return Help_Arg_1( CMD_ZEROPAGE_POINTER_ADD );

}

Update_t _ZeroPage_Error()
{
//	return ConsoleDisplayError( "There are no (ZP) pointers defined." );
	char sText[ CONSOLE_WIDTH ];
	sprintf( sText, "  There are no current (ZP) pointers.  (Max: %d)", MAX_ZEROPAGE_POINTERS );
//	ConsoleBufferPush( sText );
	return ConsoleDisplayError( sText );
}

//===========================================================================
Update_t CmdZeroPageClear   (int nArgs)
{
	if (!g_nBreakpoints)
		return _ZeroPage_Error();

	// CHECK FOR ERRORS
	if (!nArgs)
		return Help_Arg_1( CMD_ZEROPAGE_POINTER_CLEAR );

	_BWZ_ClearViaArgs( nArgs, g_aZeroPagePointers, MAX_ZEROPAGE_POINTERS, g_nZeroPagePointers );

	if (! g_nZeroPagePointers)
	{
		UpdateDisplay( UPDATE_BACKGROUND );
		return UPDATE_CONSOLE_DISPLAY;
	}

	return UPDATE_CONSOLE_DISPLAY | UPDATE_ZERO_PAGE;
}

//===========================================================================
Update_t CmdZeroPageDisable (int nArgs)
{
	if (!nArgs)
		return Help_Arg_1( CMD_ZEROPAGE_POINTER_DISABLE );
	if (! g_nZeroPagePointers)
		return _ZeroPage_Error();

	_BWZ_EnableDisableViaArgs( nArgs, g_aZeroPagePointers, MAX_ZEROPAGE_POINTERS, false );

	return UPDATE_ZERO_PAGE;
}

//===========================================================================
Update_t CmdZeroPageEnable  (int nArgs)
{
	if (! g_nZeroPagePointers)
		return _ZeroPage_Error();

	if (!nArgs)
		return Help_Arg_1( CMD_ZEROPAGE_POINTER_ENABLE );

	_BWZ_EnableDisableViaArgs( nArgs, g_aZeroPagePointers, MAX_ZEROPAGE_POINTERS, true );

	return UPDATE_ZERO_PAGE;
}

//===========================================================================
Update_t CmdZeroPageList    (int nArgs)
{
	if (! g_nZeroPagePointers)
	{
		_ZeroPage_Error();
	}
	else
	{	
		_BWZ_ListAll( g_aZeroPagePointers, MAX_ZEROPAGE_POINTERS );
	}
	return ConsoleUpdate();
}

/*
//===========================================================================
Update_t CmdZeroPageLoad    (int nArgs)
{
	return UPDATE_CONSOLE_DISPLAY;

}
*/

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

		WORD nAddress = g_aArgs[1].nValue;
		g_aZeroPagePointers[iZP].nAddress = (BYTE) nAddress;
	}

	return UPDATE_ZERO_PAGE;
}


// Command Input __________________________________________________________________________________


// Note: Range is [iParamBegin,iParamEnd], not the usually (STL) expected [iParamBegin,iParamEnd)
//===========================================================================
int FindParam( LPTSTR pLookupName, Match_e eMatch, int & iParam_, int iParamBegin, int iParamEnd )
{
	int nFound = 0;
	int nLen     = _tcslen( pLookupName );
	int iParam = 0;

	if (! nLen)
		return nFound;

#if ALLOW_INPUT_LOWERCASE
	eMatch = MATCH_FUZZY;
#endif

	if (eMatch == MATCH_EXACT)
	{
//		while (iParam < NUM_PARAMS )
		for (iParam = iParamBegin; iParam <= iParamEnd; iParam++ )
		{
			TCHAR *pParamName = g_aParameters[iParam].m_sName;
			int eCompare = _tcsicmp(pLookupName, pParamName);
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
#if ALLOW_INPUT_LOWERCASE
		TCHAR aLookup[ 256 ] = "";
		for( int i = 0; i < nLen; i++ )
		{
			aLookup[ i ] = toupper( pLookupName[ i ] );
		}
#endif
		for (iParam = iParamBegin; iParam <= iParamEnd; iParam++ )
		{
			TCHAR *pParamName = g_aParameters[ iParam ].m_sName;
// _tcsnccmp

#if ALLOW_INPUT_LOWERCASE
			if (! _tcsncmp(aLookup, pParamName ,nLen))
#else
			if (! _tcsncmp(pLookupName, pParamName ,nLen))
#endif
			{
				nFound++;
				iParam_ = g_aParameters[iParam].iCommand;

				if (!_tcsicmp(pLookupName, pParamName)) // exact match?
				{
					nFound = 1; // Exact match takes precidence over fuzzy matches
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
	_strupr( sCommand );

	while ((iCommand < NUM_COMMANDS_WITH_ALIASES)) // && (name[0] >= g_aCommands[iCommand].aName[0])) Command no longer in Alphabetical order
	{
		TCHAR *pCommandName = g_aCommands[iCommand].m_sName;
//		int iCmp = strcasecmp( sCommand, pCommandName, nLen )
		if (! _tcsncmp(sCommand, pCommandName, nLen))
		{
			pFunction_ = g_aCommands[iCommand].pFunction;
			if (pFunction_)
			{
				g_iCommand = g_aCommands[iCommand].iCommand;

				// Don't push the same comamnd/alias if already on the list
				if (find( g_vPotentialCommands.begin(), g_vPotentialCommands.end(), g_iCommand) == g_vPotentialCommands.end())
				{
					nFound++;
					g_vPotentialCommands.push_back( g_iCommand );

					if (iCommand_)
						*iCommand_ = iCommand;
// !_tcscmp
					if (!_tcsicmp(pName, pCommandName)) // exact match?
					{
	//					if (iCommand_)
	//						*iCommand_ = iCommand;

						nFound = 1; // Exact match takes precidence over fuzzy matches
						g_vPotentialCommands.erase( g_vPotentialCommands.begin(), g_vPotentialCommands.end() );
						break;
					}
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
	char sText[ CONSOLE_WIDTH * 2 ];
	sprintf( sText, "Ambiguous %s%d%s Commands:"
		, CHC_NUM_DEC
		, g_vPotentialCommands.size()
		, CHC_DEFAULT
	);
	ConsolePrint( sText );

	int iCommand = 0;
	while (iCommand < nFound)
	{
		char sPotentialCommands[ CONSOLE_WIDTH ];
		sprintf( sPotentialCommands, "%s ", CHC_COMMAND );

		int iWidth = strlen( sPotentialCommands );
		while ((iCommand < nFound) && (iWidth < g_nConsoleDisplayWidth))
		{
			int   nCommand = g_vPotentialCommands[ iCommand ];
			char *pName = g_aCommands[ nCommand ].m_sName;
			int   nLen = strlen( pName );

			if ((iWidth + nLen) >= (CONSOLE_WIDTH - 1))
				break;
				
			sprintf( sText, "%s ", pName );
			strcat( sPotentialCommands, sText );
			iWidth += nLen + 1;
			iCommand++;
		}
		ConsolePrint( sPotentialCommands );
	}
}

bool IsHexDigit( char c )
{
	if ((c >= '0') && (c <= '9'))
		return true;
	else
	if ((c >= 'A') && (c <= 'F'))
		return true;
	else
	if ((c >= 'a') && (c <= 'f'))
		return true;
	return false;
}


//===========================================================================
Update_t ExecuteCommand (int nArgs) 
{
	Arg_t * pArg     = & g_aArgs[ 0 ];
	char  * pCommand = & pArg->sArg[0];

	CmdFuncPtr_t pFunction = NULL;
	int nFound = FindCommand( pCommand, pFunction );

//	int nCookMask = (1 << NUM_TOKENS) - 1; // ArgToken_e used as bit mask!

	
	// BUGFIX: commands that are also valid hex addresses
	// ####:# [#]
	// #<#.#M
	int nLen = pArg->nArgLen;

	if ((! nFound) || (nLen < 6))
	{
		{
			// verify pCommand[ 0 .. (nLen-1) ] are hex digits
			bool bIsHex = true;
			char *pChar = pCommand;
			for (int iChar = 0; iChar < (nLen - 1); iChar++, pChar++ )
			{
				bIsHex = IsHexDigit( *pChar );
				if( !bIsHex )
				{
					break;
				}
			}
			
			if (bIsHex)
			{
				WORD nAddress = 0;

				// Support old AppleWin GO commands:
				// . G      -> GO
				// . G #### -> GO until $address
				// Support Apple Monitor commands:
				// . ####G  -> JMP $address (exit debugger)
				if ((pCommand[nLen-1] == 'G') ||
					(pCommand[nLen-1] == 'g'))
				{
					if (nLen == 1)
					{
						if (nArgs)
						{
							const int iArg = 1;
							ArgsGetValue( &g_aArgs[iArg], &g_aArgs[iArg].nValue );
							_CmdBreakpointAddCommonArg(iArg, nArgs, BP_SRC_REG_PC, BP_OP_EQUAL);	// TC-TODO: Clear this temp BP when it's hit or stepping is cancelled
						}
					}
					else if (nLen > 1)
					{
						pCommand[nLen-1] = 0;
						ArgsGetValue( pArg, & nAddress );

						regs.pc = nAddress;
					}

					g_nAppMode = MODE_RUNNING; // exit the debugger

					nFound = 1;
					g_iCommand = CMD_OUTPUT_ECHO; // hack: don't cook args
				}
				else
				// ####L -> Unassemble $address
				if ((pCommand[nLen-1] == 'L') ||
					(pCommand[nLen-1] == 'l'))
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
					pArg->nValue = nAddress;
					nArgs++;
					pFunction = g_aCommands[ g_iCommand ].pFunction;
					nFound = 1;
				}					
				else
				// address: byte ...
				if ((pArg+1)->eToken == TOKEN_COLON)
				{
					g_iCommand = CMD_MEMORY_ENTER_BYTE;

					// replace: addr :
					// with:    command addr
					pArg[1] = pArg[0];

					strcpy( pArg->sArg, g_aCommands[ g_iCommand ].m_sName );
					pArg->nArgLen = strlen( pArg->sArg );

//					nCookMask &= ~ (1 << TOKEN_COLON);
//					nArgs++;

					pFunction = g_aCommands[ g_iCommand ].pFunction;
					nFound = 1;
				}
				else
				// #<#.#M
				if	(pArg[1].eToken == TOKEN_LESS_THAN)
				{
					// Look for period
					nLen = pArg[2].nArgLen;

					char *pDst = pArg[0].sArg;
					char *pSrc = pArg[2].sArg;
					char *pEnd = 0;

					bool bFoundSrc = false;
					bool bFoundLen = false;

					pChar = pSrc;
					while( *pChar )
					{
						if( *pChar == '.' )
						{
							if( pEnd ) // only allowed one period
							{
								pEnd = 0;
								break;
							}

							*pChar = 0; // ':';
							pEnd = pChar + 1;
							bFoundSrc = true;
						} else
						if( !IsHexDigit( *pChar ) )
						{
							break;
						}	
						pChar++;
					}
					if( pEnd ) {
						if(	(*pChar == 'M')
						||	(*pChar == 'm'))
						{
							*pChar++ = 0;
							if( ! *pChar )
								bFoundLen = true;
						}

						if( bFoundSrc && bFoundLen )
						{
//ArgsGetValue( pArg, & nAddress );
//char sText[ CONSOLE_WIDTH ];
//sprintf( sText, "Dst:%s  Src: %s  End: %s", pDst, pSrc, pEnd );
//ConsolePrint( sText );
							g_iCommand = CMD_MEMORY_MOVE;
							pFunction = g_aCommands[ g_iCommand ].pFunction;

							strcpy( pArg[4].sArg, pEnd );
							strcpy( pArg[3].sArg, g_aTokens[ TOKEN_COLON ].sToken );
							strcpy( pArg[2].sArg, pSrc );
							strcpy( pArg[1].sArg, pDst );
							strcpy( pArg[0].sArg, g_aCommands[ g_iCommand ].m_sName );
							// pDst moved from arg0 to arg1 !
							pArg[1].bType = TYPE_VALUE;
							pArg[2].bType = TYPE_VALUE;
							pArg[3].bType = TYPE_OPERATOR;
							pArg[4].bType = TYPE_VALUE;

							ArgsGetValue( &pArg[1], &pArg[1].nValue );
							ArgsGetValue( &pArg[2], &pArg[2].nValue );
							pArg[3].eToken = TOKEN_COLON;
							ArgsGetValue( &pArg[4], &pArg[4].nValue );

							nFound = 1;
							nArgs = 4;
						}
					}
				}

				// TODO: display memory at address
				// addr1 [addr2] -> display byte at address
				// MDB memory display byte (is deprecated, so can be re-used)
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
		if (g_iCommand == CMD_OUTPUT_ECHO)
			bCook = false;

		int nArgsCooked = nArgs;
		if (bCook)
			nArgsCooked = ArgsCook( nArgs ); // nCookMask

		if (nArgsCooked == ARG_SYNTAX_ERROR)
			return ConsoleDisplayError( "Syntax Error" );

		if (pFunction)
			return pFunction( nArgsCooked ); // Eat them

		return UPDATE_CONSOLE_DISPLAY;
	}
	else
		return ConsoleDisplayError( "Illegal Command" );
}



// ________________________________________________________________________________________________


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
	DisasmLine_t line;
	GetDisassemblyLine( regs.pc, line );

	char sDisassembly[ CONSOLE_WIDTH ]; // DrawDisassemblyLine( 0,regs.pc, sDisassembly); // Get Disasm String
	FormatDisassemblyLine( line, sDisassembly, CONSOLE_WIDTH );

	char sFlags[ _6502_NUM_FLAGS + 1 ]; DrawFlags( 0, regs.ps, sFlags ); // Get Flags String

	if (g_hTraceFile)
	{
		if (g_bTraceHeader)
		{
			g_bTraceHeader = false;

			fprintf( g_hTraceFile,
//				"00 00 00 0000 --------  0000:90 90 90  NOP"
				"A: X: Y: SP:  Flags     Addr:Opcode    Mnemonic\n"
			);
		}

		char sTarget[ 16 ];
		if (line.bTargetValue)
		{
			sprintf( sTarget, "%s:%s"
				, line.sTargetPointer
				, line.sTargetValue
			);
		}

		fprintf( g_hTraceFile,
//			"a=%02x x=%02x y=%02x sp=%03x ps=%s   %s\n",
			"%02X %02X %02X %04X %s  %s\n",
			(unsigned)regs.a,
			(unsigned)regs.x,
			(unsigned)regs.y,
			(unsigned)regs.sp,
			(char*) sFlags
			, sDisassembly
			, sTarget
		);
	}
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

	char *pColorOperator = "";
	char *pColorNumber   = "";
	char *pColorOpcode   = "";
	char *pColorMnemonic = "";
	char *pColorOpmode   = "";
	char *pColorTotal    = "";
	if (! bExport)
	{
		pColorOperator = CHC_ARG_SEP; // grey
		pColorNumber   = CHC_NUM_DEC; // cyan
		pColorOpcode   = CHC_NUM_HEX; // yellow
		pColorMnemonic = CHC_COMMAND; // green
		pColorOpmode   = CHC_USAGE  ; // yellow
		pColorTotal    = CHC_DEFAULT; // white
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
			"%s%7.4f%s%%" DELIM "%s%9u" DELIM "%s%s" DELIM "%s%s" DELIM "%s%s\n"
			, pColorNumber
			, nPercent
			, pColorOperator
			, sSeperator2
			, pColorNumber
			, static_cast<unsigned int>(nCount), sSeperator2
			, pColorOpcode
			, sOpcode, sSeperator2
			, pColorMnemonic
			, g_aOpcodes[ nOpcode ].sMnemonic, sSeperator2
			, pColorOpmode
			, sAddress
		);
		pText = ProfileLinePush();
	}

	if (! bOpcodeGood)
		nOpcodeTotal = 0;

	sprintf( pText
		, "Total:  " DELIM "%s%9u\n"
		, sSeperator2
		, pColorTotal
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
	{
		sprintf( pText
			, "Percent" DELIM "Count" DELIM "Addressing Mode\n"
			, sSeperator7, sSeperator2 );
	}
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
			, "%s%7.4f%s%%" DELIM "%s%9u" DELIM "%s%s\n"
			, pColorNumber
			, nPercent
			, pColorOperator
			, sSeperator2
			, pColorNumber
			, static_cast<unsigned int>(nCount), sSeperator2
			, pColorOpmode
			, sAddress
		);
		pText = ProfileLinePush();
	}

	if (! bOpmodeGood)
		nOpmodeTotal = 0;

	sprintf( pText
		, "Total:  " DELIM "%s%9u\n"
		, sSeperator2 
		, pColorTotal
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

	char sFilename[MAX_PATH];
	strcpy( sFilename, g_sProgramDir ); // TODO: Allow user to decide?
	strcat( sFilename, g_FileNameProfile );

	FILE *hFile = fopen( sFilename, "wt" );

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

	g_nAppMode = MODE_DEBUG;
	FrameRefreshStatus(DRAW_TITLE);

	if (IS_APPLE2 || (g_Apple2Type == A2TYPE_APPLE2E))
	{
		g_aOpcodes = & g_aOpcodes6502[ 0 ];		// Apple ][, ][+, //e
		g_aOpmodes[ AM_2 ].m_nBytes = 1;
		g_aOpmodes[ AM_3 ].m_nBytes = 1;
	}
	else
	{
		g_aOpcodes = & g_aOpcodes65C02[ 0 ];	// Enhanced Apple //e
		g_aOpmodes[ AM_2 ].m_nBytes = 2;
		g_aOpmodes[ AM_3 ].m_nBytes = 3;
	}

	g_nDisasmCurAddress = regs.pc;
	DisasmCalcTopBotAddress();

	g_bDebuggerViewingAppleOutput = false;

	UpdateDisplay( UPDATE_ALL );

#if DEBUG_APPLE_FONT
	int iFG = 7;
	int iBG = 4;

//	DebuggerSetColorFG( aColors[ iFG ] );
//	DebuggerSetColorBG( aColors[ iBG ] );

	int iChar = 0;
	int x = 0;
	int y = 0;
	for (iChar = 0; iChar < 256; iChar++)
	{
		x = (iChar % 16);
		y = (iChar / 16);

		iFG = (x >> 1); // (iChar % 8);
		iBG = (y >> 1) & 7; // (iChar / 8) & 7;
		DebuggerSetColorFG( aConsoleColors[ iFG ] );
		DebuggerSetColorBG( aConsoleColors[ iBG ] );

		DebuggerPrintChar( x * (APPLE_FONT_WIDTH / 2), y * (APPLE_FONT_HEIGHT / 2), iChar );
	}
#endif
}

//===========================================================================
void DebugContinueStepping ()
{
	static unsigned nStepsTaken = 0;

	if (g_nDebugSkipLen > 0)
	{
		if ((regs.pc >= g_nDebugSkipStart) && (regs.pc < (g_nDebugSkipStart + g_nDebugSkipLen)))
		{
			// Enter turbo debugger g_nAppMode -- UI not updated, etc.
			g_nDebugSteps = -1;
			g_nAppMode = MODE_STEPPING;
		}
		else
		{
			// Enter normal debugger g_nAppMode -- UI updated every instruction, etc.
			g_nDebugSteps = 1;
			g_nAppMode = MODE_STEPPING;
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
		g_nAppMode = MODE_DEBUG;
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

	// TODO: Symbols_Clear()
	for( int iTable = 0; iTable < NUM_SYMBOL_TABLES; iTable++ )
	{
		_CmdSymbolsClear( (SymbolTable_Index_e) iTable );
	}
	// TODO: DataDisassembly_Clear()

	SelectObject( g_hFrameDC, GetStockObject(NULL_BRUSH) );

	DeleteObject( g_hConsoleBrushFG );
	DeleteObject( g_hConsoleBrushBG );

	DeleteDC( g_hConsoleFontDC );
	DeleteObject( g_hConsoleFontBitmap );

//	ReleaseDC( g_hFrameWindow, g_hFrameDC );
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
//===========================================================================
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

// Full Screen uses the palette from g_pFramebufferinfo
// BUT DebutInitialize() is called before VideoInitialize()
// THUS this is called post-initialize to set up the global palette
//
// pPalDst is the first color in the palette that we can stick our custom debug colors in
//===========================================================================
void Debug_UpdatePalette( BYTE *pPalDst )
{
	_ConfigColorsReset( pPalDst );
}

//===========================================================================
void _ConfigColorsReset( BYTE *pPalDst )
{
//	int iColor = 1; // black only has one level, skip it, since black levels same as white levels
//	for (int iPrimary = 1; iPrimary < 8; iPrimary++ )
//	{
//		_SetupColorRamp( iPrimary, iColor );
//	}

	BYTE *pDst = pPalDst;

	// Setup default colors
	int iColor;
	for (iColor = 0; iColor < NUM_DEBUG_COLORS; iColor++ )
	{
		COLORREF nColor = gaColorPalette[ g_aColorIndex[ iColor ] ];

		int R = (nColor >>  0) & 0xFF;
		int G = (nColor >>  8) & 0xFF;
		int B = (nColor >> 16) & 0xFF;

		if( pDst )
		{
			*(pDst + 0) = B;
			*(pDst + 1) = G;
			*(pDst + 2) = R;
			*(pDst + 3) = 0;
			pDst += 4;
		}

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
	AssemblerOff(); // update prompt

#if _DEBUG
	DWORD nError = 0;
#endif

//	g_hDstDC = g_hFrameDC; //GetDC( g_hFrameWindow );
#if _DEBUG
	nError = GetLastError();
#endif

	// Must select a bitmap into the temp DC !
	HDC hTmpDC  = CreateCompatibleDC( g_hFrameDC );

#if _DEBUG
	nError = GetLastError();
#endif

	g_hConsoleFontDC = CreateCompatibleDC( g_hFrameDC );
#if _DEBUG
	nError = GetLastError();
#endif

#if APPLE_FONT_NEW
	// Pre-scaled bitmap
	g_hConsoleFontBitmap = LoadBitmap(g_hInstance,TEXT("IDB_DEBUG_FONT_7x8"));
	SelectObject( g_hConsoleFontDC, g_hConsoleFontBitmap );
#else
	// Scale at run-time

	// Black = Transparent
	// White = Opaque
	HBITMAP hTmpBitamp = LoadBitmap(g_hInstance,TEXT("CHARSET40"));
#if _DEBUG
	nError = GetLastError();
#endif

	SelectObject( hTmpDC ,hTmpBitamp);
#if _DEBUG
	nError = GetLastError();
#endif

	g_hConsoleFontBrush = GetStockBrush( WHITE_BRUSH );
	SelectObject(g_hConsoleFontDC, g_hConsoleFontBrush );

//	SelectObject(hTmpDC, g_hDebugFontBrush );

#if _DEBUG
	nError = GetLastError();
#endif

	g_hConsoleFontBitmap = CreateCompatibleBitmap(
		hTmpDC, 
		APPLE_FONT_X_REGIONSIZE/2, APPLE_FONT_Y_REGIONSIZE/2
	);
#if _DEBUG
	nError = GetLastError();
#endif
	SelectObject( g_hConsoleFontDC, g_hConsoleFontBitmap );

	StretchBlt(
		g_hConsoleFontDC,                                     // HDC hdcDest,                        // handle to destination DC
		0, 0,                                                 // int nXOriginDest, int nYOriginDest, // y-coord of destination upper-left corner
		APPLE_FONT_X_REGIONSIZE/2, APPLE_FONT_Y_REGIONSIZE/2, //  int nWidthDest,   int nHeightDest,  
		hTmpDC,                                               // HDC hdcSrc,                         // handle to source DC
		0, APPLE_FONT_Y_APPLE_80COL,                          // int nXOriginSrc,  int nYOriginSrc,
		APPLE_FONT_X_REGIONSIZE, APPLE_FONT_Y_REGIONSIZE,     // int nWidthSrc,    int nHeightSrc,
		SRCCOPY                                               // DWORD dwRop                         // raster operation code
	);

	DeleteObject( hTmpBitamp );
	DeleteObject( hTmpDC );
#endif

//	DeleteDC( g_hFrameDC ); g_hDstDC = NULL;

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
	ZeroMemory( g_aBreakpoints     , MAX_BREAKPOINTS       * sizeof(Breakpoint_t));
	ZeroMemory( g_aWatches         , MAX_WATCHES           * sizeof(Watches_t) );
	ZeroMemory( g_aZeroPagePointers, MAX_ZEROPAGE_POINTERS * sizeof(ZeroPagePointers_t));

	// Load Main, Applesoft, and User Symbols
	extern bool g_bSymbolsDisplayMissingFile;
	g_bSymbolsDisplayMissingFile = false;

	g_iCommand = CMD_SYMBOLS_ROM;
	CmdSymbolsLoad(0);

	g_iCommand = CMD_SYMBOLS_APPLESOFT;
	CmdSymbolsLoad(0);

	// ,0x7,0xFF // Treat zero-page as data
	// $00 GOWARM   JSR ...
	// $01 LOC1 DW
	// $03 GOSTROUT JSR ...
	// $07..$B0
	// $B1 CHRGET
	// $C8
	// $C9 RNDSEED DW
	// $D0..$FF

	g_iCommand = CMD_SYMBOLS_USER_1;
	CmdSymbolsLoad(0);

	g_bSymbolsDisplayMissingFile = true;

#if OLD_FONT
	// CREATE A FONT FOR THE DEBUGGING SCREEN
	int nArgs = _Arg_1( g_sFontNameDefault );
#endif

	for (int iFont = 0; iFont < NUM_FONTS; iFont++ )
	{
		g_aFontConfig[ iFont ]._hFont = NULL;
#if USE_APPLE_FONT
		g_aFontConfig[ iFont ]._nFontHeight   = CONSOLE_FONT_HEIGHT;
		g_aFontConfig[ iFont ]._nFontWidthAvg = CONSOLE_FONT_WIDTH;
		g_aFontConfig[ iFont ]._nFontWidthMax = CONSOLE_FONT_WIDTH;
		g_aFontConfig[ iFont ]._nLineHeight   = CONSOLE_FONT_HEIGHT;
#endif
	}

#if OLD_FONT
	_CmdConfigFont( FONT_INFO          , g_sFontNameInfo   , FIXED_PITCH | FF_MODERN      , g_nFontHeight ); // DEFAULT_CHARSET
	_CmdConfigFont( FONT_CONSOLE       , g_sFontNameConsole, FIXED_PITCH | FF_MODERN      , g_nFontHeight ); // DEFAULT_CHARSET
	_CmdConfigFont( FONT_DISASM_DEFAULT, g_sFontNameDisasm , FIXED_PITCH | FF_MODERN      , g_nFontHeight ); // OEM_CHARSET
	_CmdConfigFont( FONT_DISASM_BRANCH , g_sFontNameBranch , DEFAULT_PITCH | FF_DECORATIVE, g_nFontHeight+3); // DEFAULT_CHARSET
#endif
	_UpdateWindowFontHeights( g_aFontConfig[ FONT_DISASM_DEFAULT ]._nFontHeight );

	int iColor;
	
	iColor = FG_CONSOLE_OUTPUT;
	COLORREF nColor = gaColorPalette[ g_aColorIndex[ iColor ] ];
	g_anConsoleColor[ CONSOLE_COLOR_x ] = nColor;

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
#if !USE_APPLE_FONT
	if (g_aFontConfig[ FONT_DISASM_BRANCH ]._hFont)
	{
		g_iConfigDisasmBranchType = DISASM_BRANCH_FANCY;
	}
	else
	{
		g_iConfigDisasmBranchType = DISASM_BRANCH_PLAIN;
	}	
#endif

	//	ConsoleInputReset(); already called in DebugInitialize()
	TCHAR sText[ CONSOLE_WIDTH ];

	if (_tcscmp( g_aCommands[ NUM_COMMANDS ].m_sName, TEXT(__COMMANDS_VERIFY_TXT__)))
	{
		wsprintf( sText, "*** ERROR *** Commands mis-matched!" );
		MessageBox( g_hFrameWindow, sText, TEXT("ERROR"), MB_OK );
		PostQuitMessage( 1 );
	}

	if (_tcscmp( g_aParameters[ NUM_PARAMS ].m_sName, TEXT(__PARAMS_VERIFY_TXT__)))
	{
		wsprintf( sText, "*** ERROR *** Parameters mis-matched!" );
		MessageBox( g_hFrameWindow, sText, TEXT("ERROR"), MB_OK );
		PostQuitMessage( 2 );
	}

	// Check all summary help to see if it fits within the console
	for (int iCmd = 0; iCmd < NUM_COMMANDS; iCmd++ )
	{	
		char *pHelp = g_aCommands[ iCmd ].pHelpSummary;
		if (pHelp)
		{
			int nLen = _tcslen( pHelp ) + 2;
			if (nLen > (CONSOLE_WIDTH-1))
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

	_Bookmark_Reset();

	CmdMOTD(0);
}

// wparam = 0x16
// lparam = 0x002f 0x0001
// insert = VK_INSERT

// Add character to the input line
//===========================================================================
void DebuggerInputConsoleChar( TCHAR ch )
{
	if ((g_nAppMode == MODE_STEPPING) && (ch == DEBUG_EXIT_KEY))
	{
		g_nDebugSteps = 0; // Exit Debugger
	}

	if (g_nAppMode != MODE_DEBUG)
		return;

	if (g_bConsoleBufferPaused)
		return;

	if (g_bIgnoreNextKey)
	{
		g_bIgnoreNextKey = false;
		return;
	}

	if (ch == CONSOLE_COLOR_ESCAPE_CHAR)
		return;

	if (g_nConsoleInputSkip == ch)
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

	if ((ch >= CHAR_SPACE) && (ch <= 126)) // HACK MAGIC # 32 -> ' ', # 126 
	{
		if ((ch == TCHAR_QUOTE_DOUBLE) || (ch == TCHAR_QUOTE_SINGLE))
			g_bConsoleInputQuoted = ! g_bConsoleInputQuoted;

		if (!g_bConsoleInputQuoted)
		{
			// TODO: must fix param matching to ignore case
#if ALLOW_INPUT_LOWERCASE
#else
			ch = (TCHAR)CharUpper((LPTSTR)ch);
#endif
		}
		ConsoleInputChar( ch );

		DebuggerCursorNext();

		FrameGetDC();
		DrawConsoleInput();
		FrameReleaseDC();
	}
	else
	if (ch == 0x16) // HACK: Ctrl-V.  WTF!?
	{
		// Support Clipboard (paste)
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
Update_t DebuggerProcessCommand ( const bool bEchoConsoleInput )
{
	Update_t bUpdateDisplay = UPDATE_NOTHING;

	char sText[ CONSOLE_WIDTH ];

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
				sprintf( sText, " Asm: %d sym declared, not defined", nDelayedTargets );
				ConsoleDisplayPush( sText );
				bUpdateDisplay |= UPDATE_CONSOLE_DISPLAY;
			}
		}
		ConsoleInputReset();
		bUpdateDisplay |= UPDATE_CONSOLE_DISPLAY | UPDATE_CONSOLE_INPUT;
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
			sprintf( sText, "Syntax error: %s", g_aArgs[0].sArg );
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

void ToggleFullScreenConsole()
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
}

//===========================================================================
void DebuggerProcessKey( int keycode )
//void DebugProcessCommand (int keycode)
{
	if (g_nAppMode != MODE_DEBUG)
		return;

	if (g_bDebuggerViewingAppleOutput)
	{
		if ((VK_SHIFT == keycode) || (VK_CONTROL == keycode) || (VK_MENU == keycode))
		{
			return;
		}

		// Normally any key press takes us out of "Viewing Apple Output" g_nAppMode
		// VK_F# are already processed, so we can't use them to cycle next video g_nAppMode
//		    if ((g_nAppMode != MODE_LOGO) && (g_nAppMode != MODE_DEBUG))
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
			int nLines = MIN( g_nConsoleBuffer, g_nConsoleDisplayLines - 1 ); // was -2
			if (VK_ESCAPE == keycode) // user doesn't want to read all this stu
			{
				nLines = g_nConsoleBuffer;
			}
			ConsoleBufferTryUnpause( nLines );

			// don't really need since 'else if (keycode = VK_BACK)' but better safe then sorry
			keycode = 0; // don't single-step 
		}

		bUpdateDisplay |= UPDATE_CONSOLE_DISPLAY | UPDATE_CONSOLE_INPUT;
		ConsoleDisplayPause();
	}
	else
 	// If have console input, don't invoke curmovement
	// TODO: Probably should disable all "movement" keys to map them to line editing g_nAppMode
 	if ((keycode == VK_SPACE) && g_nConsoleInputChars)
		return;
	else if (keycode == VK_ESCAPE)
	{
		g_bConsoleInputQuoted = false;
		ConsoleInputReset();
		bUpdateDisplay |= UPDATE_CONSOLE_INPUT;
	}
	else if (keycode == VK_BACK)
	{
		// Note: Checks prev char if QUTOE - SINGLE or DOUBLE
//		ConsoleUpdateCursor( CHAR_SPACE );
		if (! ConsoleInputBackSpace())
		{
			// CmdBeep();
		}
		bUpdateDisplay |= UPDATE_CONSOLE_INPUT;
	}
	else if (keycode == VK_RETURN)
	{
//		ConsoleUpdateCursor( 0 );

		if (! g_nConsoleInputChars)
		{
			// bugfix: 2.6.1.35 Fixed: Pressing enter on blank line while in assembler wouldn't exit it.
			if( g_bAssemblerInput )
			{
				bUpdateDisplay |= DebuggerProcessCommand( false );
			}
			else
			{
				ToggleFullScreenConsole();
				bUpdateDisplay |= UPDATE_ALL;
			}
		}
		else
		{
			ConsoleScrollEnd();
			bUpdateDisplay |= DebuggerProcessCommand( true ); // copy console input to console output

			// BUGFIX: main disassembly listing doesn't get updated in full screen console
			//bUpdateDisplay |= UPDATE_CONSOLE_DISPLAY;
			bUpdateDisplay |= UPDATE_ALL;
		}		
	}
	else if (( keycode == VK_OEM_3 ) ||	// US: Tilde ~ (key to the immediate left of numeral 1)
			 ( keycode == VK_OEM_8 ))	// UK: Logical NOT ¬ (key to the immediate left of numeral 1)
	{
		if (KeybGetCtrlStatus())
		{
			ToggleFullScreenConsole();
			bUpdateDisplay |= UPDATE_ALL;
		}
		else
		{
			g_nConsoleInputSkip = 0; // VK_OEM_3; // don't pass to DebugProcessChar()
			DebuggerInputConsoleChar( '~' );
		}
		g_nConsoleInputSkip = '~'; // VK_OEM_3; // don't pass to DebugProcessChar()
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
					// g_nDisasmTopAddress = _6502_MEM_BEGIN;
					// DisasmCalcCurFromTopAddress();
					// DisasmCalcBotFromTopAddress();

					g_nDisasmCurAddress = _6502_MEM_BEGIN;
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
					// g_nDisasmTopAddress =  (_6502_MEM_END / 2) + 1;
					// DisasmCalcCurFromTopAddress();
					// DisasmCalcTopBotAddress();

					g_nDisasmCurAddress =  (_6502_MEM_END / 2) + 1;
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
						bUpdateDisplay |= ConsoleScrollUp( 3 );
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
						bUpdateDisplay |= ConsoleScrollDn( 3 );
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
						// TODO: FIXME: Scroll through console input history
					}
				}
				else
				{
					// Shift the Top offset up by 1 byte
					// i.e. no smart disassembly like LineUp()
					// Normally UP moves to the previous "line" which may be multiple bytes.
					if (KeybGetShiftStatus())
						bUpdateDisplay |= CmdCursorLineUp(1);
					else
						bUpdateDisplay |= CmdCursorLineUp(0); // smart disassembly
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
						// TODO: FIXME: Scroll through console input history
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
				if (KeybGetAltStatus())
					bUpdateDisplay |= CmdCursorJumpPC( CURSOR_ALIGN_CENTER );
				else
					bUpdateDisplay |= CmdCursorFollowTarget( CURSOR_ALIGN_TOP );
				break;

			case VK_LEFT:
				if (KeybGetShiftStatus())
					bUpdateDisplay |= CmdCursorJumpRetAddr( CURSOR_ALIGN_TOP ); // Jump to Caller
				else
					bUpdateDisplay |= CmdCursorJumpRetAddr( CURSOR_ALIGN_CENTER );
				break;

			default:
				if ((keycode >= '0') && (keycode <= '9'))
				{
					int nArgs = 1;
					int iBookmark = keycode - '0';
					if (KeybGetCtrlStatus() && KeybGetShiftStatus())
					{
						nArgs = 2;
						g_aArgs[ 1 ].nValue = iBookmark;
						g_aArgs[ 2 ].nValue = g_nDisasmCurAddress;
						bUpdateDisplay |= CmdBookmarkAdd( nArgs );
						g_bIgnoreNextKey = true;
					}
					else
					if (KeybGetCtrlStatus())
					{
						nArgs = 1;
						g_aArgs[ 1 ].nValue = iBookmark;
						bUpdateDisplay |= CmdBookmarkGoto( nArgs );
						g_bIgnoreNextKey = true;
					}
				}

				break;
		} // switch
	}

	if (bUpdateDisplay && !g_bDebuggerViewingAppleOutput) //  & UPDATE_BACKGROUND)
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


//===========================================================================
void DebuggerUpdate()
{
	DebuggerCursorUpdate();
}


//===========================================================================
void DebuggerCursorUpdate()
{
	if (g_nAppMode != MODE_DEBUG)
		return;

	const  int nUpdatesPerSecond = 4;
	const  DWORD nUpdateInternal_ms = 1000 / nUpdatesPerSecond;
	static DWORD nBeg = GetTickCount(); // timeGetTime();
	       DWORD nNow = GetTickCount(); // timeGetTime();

	if (((nNow - nBeg) >= nUpdateInternal_ms) && !g_bDebuggerViewingAppleOutput)
	{
		nBeg = nNow;
		
		DebuggerCursorNext();
		
		FrameGetDC();
		DrawConsoleCursor();
		FrameReleaseDC();
	}
	else
	{
		Sleep(10);		// Stop process hogging CPU
	}
}


//===========================================================================
void DebuggerCursorNext()
{
	g_bInputCursor ^= true;
	if (g_bInputCursor)
		ConsoleUpdateCursor( g_aInputCursor[ g_iInputCursor ] );
	else
		ConsoleUpdateCursor( 0 ); // show char under cursor
}


//===========================================================================
//char DebuggerCursorGet()
//{
//	return g_aInputCursor[ g_iInputCursor ];
//}


//===========================================================================
void	DebuggerMouseClick( int x, int y )
{
	if (g_nAppMode != MODE_DEBUG)
		return;

	int nFontWidth  = g_aFontConfig[ FONT_DISASM_DEFAULT ]._nFontWidthAvg;
	int nFontHeight = g_aFontConfig[ FONT_DISASM_DEFAULT ]._nLineHeight  ;

	// do picking
	FrameGetDC();

	int cx = (x - VIEWPORTX) / nFontWidth;
	int cy = (y - VIEWPORTY) / nFontHeight;
	
#if _DEBUG
	char sText[ CONSOLE_WIDTH ];
	sprintf( sText, "x:%d y:%d  cx:%d cy:%d", x, y, cx, cy );
	ConsoleDisplayPush( sText );
	DebugDisplay( UPDATE_CONSOLE_DISPLAY );
#endif

	if (g_iWindowThis == WINDOW_CODE)
	{
		// Display_AssemblyLine -- need Tabs

		if( g_bConfigDisasmAddressView )
		{
			// HACK: hard-coded from DrawDisassemblyLine::aTabs[] !!!
			if( cx < 4) // #### 
			{
				g_bConfigDisasmAddressView ^= true;
				DebugDisplay( UPDATE_DISASM );
			}
			else
			if (cx == 4) //    :
			{
				g_bConfigDisasmAddressColon ^= true;
				DebugDisplay( UPDATE_DISASM );
			}
			else         //      AD 00 00
			if ((cx > 4) & (cx <= 13))
			{
				g_bConfigDisasmOpcodesView ^= true;
				DebugDisplay( UPDATE_DISASM );
			}
			
		} else
		{
			if( cx == 0 ) //   :
			{
				// Three-way state
				//   "addr:"
				//   ":"
				//   " "
				g_bConfigDisasmAddressColon ^= true;
				if( g_bConfigDisasmAddressColon )
				{
					g_bConfigDisasmAddressView ^= true;
				}
				DebugDisplay( UPDATE_DISASM );
			}
			else
			if ((cx > 0) & (cx <= 13))
			{
				g_bConfigDisasmOpcodesView ^= true;
				DebugDisplay( UPDATE_DISASM );
			}
		}
		// Click on PC inside reg window?
		if ((cx >= 51) && (cx <= 60))
		{
			if (cy == 3)
			{
				CmdCursorJumpPC( CURSOR_ALIGN_CENTER );
				DebugDisplay( UPDATE_DISASM );
			}
			else // Click on stack
			if( cy > 3)
			{
			}
		}
	}
	
	FrameReleaseDC();
}
