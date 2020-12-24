/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 2009-2014, Tom Charlesworth, Michael Pohoreski

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

/* Description: Debugger commands
 *
 * Author: Copyright (C) 2011 - 2011 Michael Pohoreski
 */

#include "StdAfx.h"

#include "Debug.h"

#include "../Interface.h"

// Commands _______________________________________________________________________________________

	#define DEBUGGER__COMMANDS_VERIFY_TXT__ "\xDE\xAD\xC0\xDE"

	// Setting function to NULL, allows g_aCommands arguments to be safely listed here
	// Commands should be listed alphabetically per category.
	// For the list sorted by category, check Commands_e
	// NOTE: Keep in sync Commands_e and g_aCommands[] ! Aliases are listed at the end.
	Command_t g_aCommands[] =
	{
	// Assembler
//		{TEXT("!")           , CmdAssemberMini      , CMD_ASSEMBLER_MINI       , "Mini assembler"             },
		{TEXT("A")           , CmdAssemble          , CMD_ASSEMBLE             , "Assemble instructions"      },
	// CPU (Main)
		{TEXT(".")           , CmdCursorJumpPC      , CMD_CURSOR_JUMP_PC       , "Locate the cursor in the disasm window" }, // centered
		{TEXT("=")           , CmdCursorSetPC       , CMD_CURSOR_SET_PC        , "Sets the PC to the current instruction" },
		{TEXT("G")           , CmdGoNormalSpeed     , CMD_GO_NORMAL_SPEED      , "Run at normal speed [until PC == address]"   },
		{TEXT("GG")          , CmdGoFullSpeed       , CMD_GO_FULL_SPEED        , "Run at full speed [until PC == address]"   },
		{TEXT("IN")          , CmdIn                , CMD_IN                   , "Input byte from IO $C0xx"   },
		{TEXT("KEY")         , CmdKey               , CMD_INPUT_KEY            , "Feed key into emulator"     },
		{TEXT("JSR")         , CmdJSR               , CMD_JSR                  , "Call sub-routine"           },
		{TEXT("NOP")         , CmdNOP               , CMD_NOP                  , "Zap the current instruction with a NOP" },
		{TEXT("OUT")         , CmdOut               , CMD_OUT                  , "Output byte to IO $C0xx"    },
	// CPU - Meta Info
		{TEXT("PROFILE")     , CmdProfile           , CMD_PROFILE              , "List/Save 6502 profiling" },
		{TEXT("R")           , CmdRegisterSet       , CMD_REGISTER_SET         , "Set register" },
	// CPU - Stack
		{TEXT("POP")         , CmdStackPop          , CMD_STACK_POP            },
		{TEXT("PPOP")        , CmdStackPopPseudo    , CMD_STACK_POP_PSEUDO     },
		{TEXT("PUSH")        , CmdStackPop          , CMD_STACK_PUSH           },
//		{TEXT("RTS")         , CmdStackReturn       , CMD_STACK_RETURN         },
		{TEXT("P")           , CmdStepOver          , CMD_STEP_OVER            , "Step current instruction"   },
		{TEXT("RTS")         , CmdStepOut           , CMD_STEP_OUT             , "Step out of subroutine"     }, 
	// CPU - Meta Info
		{TEXT("T")           , CmdTrace             , CMD_TRACE                , "Trace current instruction"  },
		{TEXT("TF")          , CmdTraceFile         , CMD_TRACE_FILE           , "Save trace to filename [with video scanner info]" },
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
		{TEXT("BRK")         , CmdBreakInvalid      , CMD_BREAK_INVALID        , "Enter debugger on BRK or INVALID" },
		{TEXT("BRKOP")       , CmdBreakOpcode       , CMD_BREAK_OPCODE         , "Enter debugger on opcode"   },
		{TEXT("BP")          , CmdBreakpoint        , CMD_BREAKPOINT           , "Alias for BPR (Breakpoint Register Add)" },
		{TEXT("BPA")         , CmdBreakpointAddSmart, CMD_BREAKPOINT_ADD_SMART , "Add (smart) breakpoint" },
//		{TEXT("BPP")         , CmdBreakpointAddFlag , CMD_BREAKPOINT_ADD_FLAG  , "Add breakpoint on flags" },
		{TEXT("BPR")         , CmdBreakpointAddReg  , CMD_BREAKPOINT_ADD_REG   , "Add breakpoint on register value"      }, // NOTE! Different from SoftICE !!!!
		{TEXT("BPX")         , CmdBreakpointAddPC   , CMD_BREAKPOINT_ADD_PC    , "Add breakpoint at current instruction" },
		{TEXT("BPIO")        , CmdBreakpointAddIO   , CMD_BREAKPOINT_ADD_IO    , "Add breakpoint for IO address $C0xx"   },
		{TEXT("BPM")         , CmdBreakpointAddMemA , CMD_BREAKPOINT_ADD_MEM   , "Add breakpoint on memory access"       },  // SoftICE
		{TEXT("BPMR")        , CmdBreakpointAddMemR , CMD_BREAKPOINT_ADD_MEMR  , "Add breakpoint on memory read access"  },
		{TEXT("BPMW")        , CmdBreakpointAddMemW , CMD_BREAKPOINT_ADD_MEMW  , "Add breakpoint on memory write access" },

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
		{TEXT("PWD")         , CmdConfigGetDebugDir , CMD_CONFIG_GET_DEBUG_DIR , "Displays the current debugger directory. Used for scripts & mem load/save." },
		{TEXT("CD")          , CmdConfigSetDebugDir , CMD_CONFIG_SET_DEBUG_DIR , "Updates the current debugger directory." },
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
	// Cycles info
		{TEXT("CYCLES")      , CmdCyclesInfo        , CMD_CYCLES_INFO, "Cycles display configuration" },
		{TEXT("RCC")		 , CmdCyclesReset		, CMD_CYCLES_RESET, "Reset cycles counter" },
	// Disassembler Data 
		{TEXT("Z")           , CmdDisasmDataDefByte1       , CMD_DISASM_DATA      , "Treat byte [range] as data"                },
		{TEXT("X")           , CmdDisasmDataDefCode        , CMD_DISASM_CODE      , "Treat byte [range] as code"                },
// TODO: Conflicts with monitor command #L -> 000DL
		{TEXT("B")           , CmdDisasmDataList           , CMD_DISASM_LIST      , "List all byte ranges treated as data"      },
		// without symbol lookup
		{TEXT("DB")          , CmdDisasmDataDefByte1       , CMD_DEFINE_DATA_BYTE1, "Define byte(s)"                             },
		{TEXT("DB2")         , CmdDisasmDataDefByte2       , CMD_DEFINE_DATA_BYTE2, "Define byte array, display 2 bytes/line"    },
		{TEXT("DB4")         , CmdDisasmDataDefByte4       , CMD_DEFINE_DATA_BYTE4, "Define byte array, display 4 bytes/line"    },
		{TEXT("DB8")         , CmdDisasmDataDefByte8       , CMD_DEFINE_DATA_BYTE8, "Define byte array, display 8 bytes/line"    },
		{TEXT("DW")          , CmdDisasmDataDefWord1       , CMD_DEFINE_DATA_WORD1, "Define address array"                       },
		{TEXT("DW2")         , CmdDisasmDataDefWord2       , CMD_DEFINE_DATA_WORD2, "Define address array, display 2 words/line" },
		{TEXT("DW4")         , CmdDisasmDataDefWord4       , CMD_DEFINE_DATA_WORD4, "Define address array, display 4 words/line" },
		{TEXT("ASC")         , CmdDisasmDataDefString      , CMD_DEFINE_DATA_STR  , "Define text string"                         }, // 2.7.0.26 Changed: DS to ASC because DS is used as "Define Space" assembler directive
//		{TEXT("DF")          , CmdDisasmDataDefFloat       , CMD_DEFINE_DATA_FLOAT, "Define AppleSoft (packed) Float"            },
//		{TEXT("DFX")         , CmdDisasmDataDefFloatUnpack , CMD_DEFINE_DATA_FLOAT2,"Define AppleSoft (unpacked) Float"          },
		// with symbol lookup
//		{TEXT("DA<>")        , CmdDisasmDataDefAddress8HL  , CMD_DEFINE_ADDR_8_HL , "Define split array of addresses, high byte section followed by low byte section" },
//		{TEXT("DA><")        , CmdDisasmDataDefAddress8LH  , CMD_DEFINE_ADDR_8_LH , "Define split array of addresses, low byte section followed by high byte section" },
//		{TEXT("DA<")         , CmdDisasmDataDefAddress8H   , CMD_DEFINE_ADDR_BYTE_H   , "Define array of high byte addresses"   },
//		{TEXT("DB>")         , CmdDisasmDataDefAddress8L   , CMD_DEFINE_ADDR_BYTE_L   , "Define array of low byte addresses"    } 
		{TEXT("DA")          , CmdDisasmDataDefAddress16   , CMD_DEFINE_ADDR_WORD , "Define array of word addresses"            },
// TODO: Rename config cmd: DISASM
//		{TEXT("UA")          , CmdDisasmDataSmart          , CMD_SMART_DISASSEMBLE, "Analyze opcodes to determine if code or data" },		
	// Disk
		{TEXT("DISK")        , CmdDisk              , CMD_DISK                 , "Access Disk Drive Functions" },
	// Flags
//		{TEXT("FC")          , CmdFlag              , CMD_FLAG_CLEAR , "Clear specified Flag"           }, // NVRBDIZC see AW_CPU.cpp AF_*
// TODO: Conflicts with monitor command #L -> 000CL
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
		{TEXT("MOTD")        , CmdMOTD              , CMD_MOTD                 },							// MOTD: Message Of The Day
	// Memory
		{TEXT("MC")          , CmdMemoryCompare     , CMD_MEMORY_COMPARE       },

		{TEXT("MD1")         , CmdMemoryMiniDumpHex , CMD_MEM_MINI_DUMP_HEX_1  , "Hex dump in the mini memory area 1" },
		{TEXT("MD2")         , CmdMemoryMiniDumpHex , CMD_MEM_MINI_DUMP_HEX_2  , "Hex dump in the mini memory area 2" },

		{TEXT("MA1")         , CmdMemoryMiniDumpAscii,CMD_MEM_MINI_DUMP_ASCII_1, "ASCII text in mini memory area 1" },
		{TEXT("MA2")         , CmdMemoryMiniDumpAscii,CMD_MEM_MINI_DUMP_ASCII_2, "ASCII text in mini memory area 2" },
		{TEXT("MT1")         , CmdMemoryMiniDumpApple,CMD_MEM_MINI_DUMP_APPLE_1, "Apple Text in mini memory area 1" },
		{TEXT("MT2")         , CmdMemoryMiniDumpApple,CMD_MEM_MINI_DUMP_APPLE_2, "Apple Text in mini memory area 2" },
//		{TEXT("ML1")         , CmdMemoryMiniDumpLow , CMD_MEM_MINI_DUMP_TXT_LO_1, "Text (Ctrl) in mini memory dump area 1" },
//		{TEXT("ML2")         , CmdMemoryMiniDumpLow , CMD_MEM_MINI_DUMP_TXT_LO_2, "Text (Ctrl) in mini memory dump area 2" },
//		{TEXT("MH1")         , CmdMemoryMiniDumpHigh, CMD_MEM_MINI_DUMP_TXT_HI_1, "Text (High) in mini memory dump area 1" },
//		{TEXT("MH2")         , CmdMemoryMiniDumpHigh, CMD_MEM_MINI_DUMP_TXT_HI_2, "Text (High) in mini memory dump area 2" },

		{TEXT("ME")          , CmdMemoryEdit        , CMD_MEMORY_EDIT          , "Memory Editor - Not Implemented!" }, // TODO: like Copy ][+ Sector Edit
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

		{TEXT("NTSC")        , CmdNTSC              , CMD_NTSC                 , "Save/Load the NTSC palette"   },
		{TEXT("TSAVE")       , CmdTextSave          , CMD_TEXT_SAVE            , "Save text screen"             },
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

		{"SYMMAIN"           , CmdSymbolsCommand    , CMD_SYMBOLS_ROM          , "Main/ROM symbol table lookup/menu"      }, // CLEAR,LOAD,SAVE
		{"SYMBASIC"          , CmdSymbolsCommand    , CMD_SYMBOLS_APPLESOFT    , "Applesoft symbol table lookup/menu"     }, // CLEAR,LOAD,SAVE
		{"SYMASM"            , CmdSymbolsCommand    , CMD_SYMBOLS_ASSEMBLY     , "Assembly symbol table lookup/menu"      }, // CLEAR,LOAD,SAVE
		{"SYMUSER"           , CmdSymbolsCommand    , CMD_SYMBOLS_USER_1       , "First user symbol table lookup/menu"    }, // CLEAR,LOAD,SAVE
		{"SYMUSER2"          , CmdSymbolsCommand    , CMD_SYMBOLS_USER_2       , "Second User symbol table lookup/menu"   }, // CLEAR,LOAD,SAVE
		{"SYMSRC"            , CmdSymbolsCommand    , CMD_SYMBOLS_SRC_1        , "First Source symbol table lookup/menu"  }, // CLEAR,LOAD,SAVE
		{"SYMSRC2"           , CmdSymbolsCommand    , CMD_SYMBOLS_SRC_2        , "Second Source symbol table lookup/menu" }, // CLEAR,LOAD,SAVE
		{"SYMDOS33"          , CmdSymbolsCommand    , CMD_SYMBOLS_DOS33        , "DOS 3.3 symbol table lookup/menu"       }, // CLEAR,LOAD,SAVE
		{"SYMPRODOS"         , CmdSymbolsCommand    , CMD_SYMBOLS_PRODOS       , "ProDOS symbol table lookup/menu"        }, // CLEAR,LOAD,SAVE

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
	// Video-scanner info
		{TEXT("VIDEOINFO")   , CmdVideoScannerInfo  , CMD_VIDEO_SCANNER_INFO, "Video-scanner display configuration" },
	// View
		{TEXT("TEXT")        , CmdViewOutput_Text4X , CMD_VIEW_TEXT4X, "View Text screen (current page)"        },
		{TEXT("TEXT1")       , CmdViewOutput_Text41 , CMD_VIEW_TEXT41, "View Text screen Page 1"                },
		{TEXT("TEXT2")       , CmdViewOutput_Text42 , CMD_VIEW_TEXT42, "View Text screen Page 2"                },
		{TEXT("TEXT80")      , CmdViewOutput_Text8X , CMD_VIEW_TEXT8X, "View 80-col Text screen (current page)" },
		{TEXT("TEXT81")      , CmdViewOutput_Text81 , CMD_VIEW_TEXT81, "View 80-col Text screen Page 1"         },
		{TEXT("TEXT82")      , CmdViewOutput_Text82 , CMD_VIEW_TEXT82, "View 80-col Text screen Page 2"         },
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
		{TEXT("W")           , CmdWatch             , CMD_WATCH         , "Alias for WA (Watch Add)"                      },
		{TEXT("WA")          , CmdWatchAdd          , CMD_WATCH_ADD     , "Add/Update address or symbol to watch"         },
		{TEXT("WC")          , CmdWatchClear        , CMD_WATCH_CLEAR   , "Clear (remove) watch"                          },
		{TEXT("WD")          , CmdWatchDisable      , CMD_WATCH_DISABLE , "Disable specific watch - it is still in the list, just not active" },
		{TEXT("WE")          , CmdWatchEnable       , CMD_WATCH_ENABLE  , "(Re)Enable disabled watch"                     },
		{TEXT("WL")          , CmdWatchList         , CMD_WATCH_LIST    , "List all watches"                              },
//		{TEXT("WLOAD")       , CmdWatchLoad         , CMD_WATCH_LOAD    , "Load Watches"                                  }, // Cant use as param to W
		{TEXT("WSAVE")       , CmdWatchSave         , CMD_WATCH_SAVE    , "Save Watches"                                  }, // due to symbol look-up
	// Window
		{TEXT("WIN")         , CmdWindow            , CMD_WINDOW         , "Show specified debugger window"              },
// CODE 0, CODE 1, CODE 2 ... ???
		{TEXT("CODE")        , CmdWindowViewCode    , CMD_WINDOW_CODE    , "Switch to full code window"                  },  // Can't use WC = WatchClear
		{TEXT("CODE1")       , CmdWindowShowCode1   , CMD_WINDOW_CODE_1  , "Show code on top split window"               },
		{TEXT("CODE2")       , CmdWindowShowCode2   , CMD_WINDOW_CODE_2  , "Show code on bottom split window"            },
		{TEXT("CONSOLE")     , CmdWindowViewConsole , CMD_WINDOW_CONSOLE , "Switch to full console window"               },
		{TEXT("DATA")        , CmdWindowViewData    , CMD_WINDOW_DATA    , "Switch to full data window"                  },
		{TEXT("DATA1")       , CmdWindowShowData1   , CMD_WINDOW_DATA_1  , "Show data on top split window"               },
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
		{ DEBUGGER__COMMANDS_VERIFY_TXT__, NULL, NUM_COMMANDS },

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
	// Memory
		{TEXT("D")           , CmdMemoryMiniDumpHex , CMD_MEM_MINI_DUMP_HEX_1  , "Hex dump in the mini memory area 1" }, // FIXME: Must also work in DATA screen
		{TEXT("M1")          , CmdMemoryMiniDumpHex , CMD_MEM_MINI_DUMP_HEX_1  }, // alias
		{TEXT("M2")          , CmdMemoryMiniDumpHex , CMD_MEM_MINI_DUMP_HEX_2  }, // alias

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

//		{TEXT("SYMBOLS")     , CmdSymbols           , CMD_SYMBOLS_LOOKUP       , "Return " },
//		{TEXT("SYMBOLS1")    , CmdSymbolsInfo       , CMD_SYMBOLS_1            },
//		{TEXT("SYMBOLS2")    , CmdSymbolsInfo       , CMD_SYMBOLS_2            },
//		{"SYM0"              , CmdSymbolsInfo       , CMD_SYMBOLS_ROM          },
//		{"SYM1"              , CmdSymbolsInfo       , CMD_SYMBOLS_APPLESOFT    },
//		{"SYM2"              , CmdSymbolsInfo       , CMD_SYMBOLS_ASSEMBLY     },
//		{"SYM3"              , CmdSymbolsInfo       , CMD_SYMBOLS_USER_1       },
//		{"SYM4"              , CmdSymbolsInfo       , CMD_SYMBOLS_USER_2       },
//		{"SYM5"              , CmdSymbolsInfo       , CMD_SYMBOLS_SRC_1        },
//		{"SYM6"              , CmdSymbolsInfo       , CMD_SYMBOLS_SRC_2        },
		{"SYMDOS"            , CmdSymbolsCommand    , CMD_SYMBOLS_DOS33        },
		{"SYMPRO"            , CmdSymbolsCommand    , CMD_SYMBOLS_PRODOS       },

		{TEXT("TEXT40")      , CmdViewOutput_Text4X , CMD_VIEW_TEXT4X          },
		{TEXT("TEXT41")      , CmdViewOutput_Text41 , CMD_VIEW_TEXT41          },
		{TEXT("TEXT42")      , CmdViewOutput_Text42 , CMD_VIEW_TEXT42          },

//		{TEXT("WATCH")       , CmdWatchAdd          , CMD_WATCH_ADD            },
		{TEXT("WINDOW")      , CmdWindow            , CMD_WINDOW               },
//		{TEXT("W?")          , CmdWatchAdd          , CMD_WATCH_ADD            },
		{TEXT("ZAP")         , CmdNOP               , CMD_NOP                  },

	// DEPRECATED  -- Probably should be removed in a future version
		{TEXT("BENCH")       , CmdBenchmarkStart    , CMD_BENCHMARK            },
		{TEXT("EXITBENCH")   , NULL                 , CMD_BENCHMARK            }, // 2.8.03 was incorrectly alias with 'E' Bug #246. // CmdBenchmarkStop
		{TEXT("MDB")         , CmdMemoryMiniDumpHex , CMD_MEM_MINI_DUMP_HEX_1  }, // MemoryDumpByte  // Did anyone actually use this??
//		{TEXT("MEMORY")      , CmdMemoryMiniDumpHex , CMD_MEM_MINI_DUMP_HEX_1  }, // MemoryDumpByte  // Did anyone actually use this??
};

	const int NUM_COMMANDS_WITH_ALIASES = sizeof(g_aCommands) / sizeof (Command_t); // Determined at compile-time ;-)

// Parameters _____________________________________________________________________________________

	#define DEBUGGER__PARAMS_VERIFY_TXT__   "\xDE\xAD\xDA\x1A"

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
		{TEXT("CLICK")      , NULL, PARAM_CONFIG_CLICK   }, // GH#462
		{TEXT("COLON")      , NULL, PARAM_CONFIG_COLON   },
		{TEXT("OPCODE")     , NULL, PARAM_CONFIG_OPCODE  },
		{TEXT("POINTER")    , NULL, PARAM_CONFIG_POINTER },
		{TEXT("SPACES")		, NULL, PARAM_CONFIG_SPACES  },
		{TEXT("TARGET")     , NULL, PARAM_CONFIG_TARGET  },
// Disk
		{TEXT("EJECT")      , NULL, PARAM_DISK_EJECT     },
		{TEXT("INFO")       , NULL, PARAM_DISK_INFO      },
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
		{ DEBUGGER__PARAMS_VERIFY_TXT__, NULL, NUM_PARAMS     },
	};

//===========================================================================

void VerifyDebuggerCommandTable()
{
	char sText[ CONSOLE_WIDTH * 2 ];

	for (int iCmd = 0; iCmd < NUM_COMMANDS; iCmd++ )
	{
		if ( g_aCommands[ iCmd ].iCommand != iCmd)
		{
			sprintf( sText, "*** ERROR *** Enumerated Commands mis-matched at #%d!", iCmd );
			MessageBoxA(GetFrame().g_hFrameWindow, sText, TEXT("ERROR"), MB_OK );
			PostQuitMessage( 1 );
		}
	}

	// _tcscmp
	if (strcmp( g_aCommands[ NUM_COMMANDS ].m_sName, DEBUGGER__COMMANDS_VERIFY_TXT__))
	{
		sprintf( sText, "*** ERROR *** Total Commands mis-matched!" );
		MessageBoxA(GetFrame().g_hFrameWindow, sText, TEXT("ERROR"), MB_OK );
		PostQuitMessage( 1 );
	}

	if (strcmp( g_aParameters[ NUM_PARAMS ].m_sName, DEBUGGER__PARAMS_VERIFY_TXT__))
	{
		sprintf( sText, "*** ERROR *** Total Parameters mis-matched!" );
		MessageBoxA(GetFrame().g_hFrameWindow, sText, TEXT("ERROR"), MB_OK );
		PostQuitMessage( 2 );
	}
}
