#pragma once


// Addressing _____________________________________________________________________________________

	enum
	{
//		MAX_ADDRESSING_MODE_LEN = 12

		MAX_OPMODE_FORMAT = 12,
		MAX_OPMODE_NAME   = 32

		, NO_6502_TARGET = -1
		, _6502_NUM_FLAGS = 8
	};

	enum RangeType_t
	{
		RANGE_MISSING_ARG_2 = 0, // error
		RANGE_HAS_LEN          , // valid case 1
		RANGE_HAS_END          , // valid case 2
	};

	struct AddressingMode_t
	{
		char  m_sFormat[ MAX_OPMODE_FORMAT ];
		int   m_nBytes;
		char  m_sName  [ MAX_OPMODE_NAME ];
	};

	/*
      +---------------------+--------------------------+
      | Opmode  e           |     assembler format     |
      +=====================+==========================+
      | Immediate           |          #aa             |
      | Absolute            |          aaaa            |
      | Zero Page           |          aa              |   Note:
      | Implied             |                          |
      | Indirect Absolute   |          (aaaa)          |     aa = 2 hex digits
      | Absolute Indexed,X  |          aaaa,X          |          as $FF
      | Absolute Indexed,Y  |          aaaa,Y          |
      | Zero Page Indexed,X |          aa,X            |     aaaa = 4 hex
      | Zero Page Indexed,Y |          aa,Y            |          digits as
      | Indexed Indirect    |          (aa,X)          |          $FFFF
      | Indirect Indexed    |          (aa),Y          |
      | Relative            |          aaaa            |     Can also be
      | Accumulator         |          A               |     assembler labels
      +---------------------+--------------------------+
      (Table 2-3. _6502 Software Design_, Scanlon, 1980)

	Opcode: opc aaa od
		opc...od = Mnemonic / Opcode
		...aaa.. = Addressing g_nAppMode
	od = 00
		000	#Immediate
		001	Zero page
		011	Absolute
		101	Zero page,X
		111	Absolute,X
	od = 01
		000	(Zero page,X)
		001	Zero page
		010	#Immediate
		011	Absolute
		100	(Zero page),Y
		101	Zero page,X
		110	Absolute,Y
		111	Absolute,X
	od = 10
		000	#Immediate
		001	Zero page
		010	Accumulator
		011	Absolute
		101	Zero page,X
		111	Absolute,X
	*/
	/*
		Legend:
			A = Absolute (fortunately Accumulator is implicit, leaving us to use 'A')
			I = Indexed  ( would of been X, but need reg X)
			M = iMmediate
			N = iNdirect
			R = Relative
			X = Offset X Register
			Y = Offset Y Register
			Z = Zeropage
	*/
	enum AddressingMode_e // ADDRESSING_MODES_e
	{
		  AM_IMPLIED // Note: SetDebugBreakOnInvalid() assumes this order of first 4 entries
		, AM_1    //    Invalid 1 Byte
		, AM_2    //    Invalid 2 Bytes
		, AM_3    //    Invalid 3 Bytes
		, AM_M    //  4 #Immediate
		, AM_A    //  5 $Absolute
		, AM_Z    //  6 Zeropage
		, AM_AX   //  7 Absolute, X
		, AM_AY   //  8 Absolute, Y
		, AM_ZX   //  9 Zeropage, X
		, AM_ZY   // 10 Zeropage, Y
		, AM_R    // 11 Relative
		, AM_IZX  // 12 Indexed (Zeropage Indirect, X)
		, AM_IAX  // 13 Indexed (Absolute Indirect, X)
		, AM_NZY  // 14 Indirect (Zeropage) Indexed, Y
		, AM_NZ   // 15 Indirect (Zeropage)
		, AM_NA   // 16 Indirect (Absolute) i.e. JMP
		, AM_DATA // Not an opcode! Markup as data
		, NUM_ADDRESSING_MODES
		, NUM_OPMODES = NUM_ADDRESSING_MODES
		, AM_I = NUM_ADDRESSING_MODES, // for assemler
	};


// Assembler ______________________________________________________________________________________

	enum Prompt_e
	{
		PROMPT_COMMAND,
		PROMPT_ASSEMBLER,
		NUM_PROMPTS
	};

	enum
	{
		// raised from 13 to 31 for Contiki
		MAX_SYMBOLS_LEN = 31
	};

// Bookmarks ______________________________________________________________________________________

	enum
	{
		MAX_BOOKMARKS = 10
	};

// Breakpoints ____________________________________________________________________________________

	enum
	{
		MAX_BREAKPOINTS = 16
	};

	/*
		Breakpoints are now in a tri-state.
		This allows one to set a bunch of breakpoints, and re-enable the ones you want
		without having to remember which addresses you previously added. :-)
		
		The difference between Set and Enabled breakpoints:

			Set  Enabled  Break?
			x    x        yes, listed as full brightness
			x    -        no, listed as dimmed
			-    ?        no, not listed
	*/
	// NOTE: Order must match _PARAM_REGS_*
	// NOTE: Order must match BreakpointSource_t
	// NOTE: Order must match g_aBreakpointSource
	enum BreakpointSource_t
	{
		BP_SRC_REG_A ,
		BP_SRC_REG_X ,
		BP_SRC_REG_Y ,

		BP_SRC_REG_PC, // Program Counter
		BP_SRC_REG_S , // Stack Counter

		BP_SRC_REG_P , // Processor Status
		BP_SRC_FLAG_C, // Carry
		BP_SRC_FLAG_Z, // Zero
		BP_SRC_FLAG_I, // Interrupt
		BP_SRC_FLAG_D, // Decimal
		BP_SRC_FLAG_B, // Break
		BP_SRC_FLAG_R, // Reserved
		BP_SRC_FLAG_V, // Overflow
		BP_SRC_FLAG_N, // Sign

		BP_SRC_OPCODE,
		BP_SRC_MEM_RW,
		BP_SRC_MEM_READ_ONLY,
		BP_SRC_MEM_WRITE_ONLY,

		NUM_BREAKPOINT_SOURCES
	};

	// Note: Order must match Breakpoint_Operator_t
	// Note: Order must much _PARAM_BREAKPOINT_*
	// Note: Order must match g_aBreakpointSymbols
	enum BreakpointOperator_t
	{
		BP_OP_LESS_EQUAL   , // <= REG
		BP_OP_LESS_THAN    , // <  REG
		BP_OP_EQUAL        , // =  REG
		BP_OP_NOT_EQUAL    , // != REG
//		BP_OP_NOT_EQUAL_1  , // !  REG
		BP_OP_GREATER_THAN , // >  REG
		BP_OP_GREATER_EQUAL, // >= REG
		BP_OP_READ         , // @  MEM @ ? *
		BP_OP_WRITE        , // *  MEM @ ? *
		BP_OP_READ_WRITE   , // ?  MEM @ ? *
		NUM_BREAKPOINT_OPERATORS
	};

	struct Breakpoint_t
	{
		WORD                 nAddress; // for registers, functions as nValue
		UINT                 nLength ;
		BreakpointSource_t   eSource;
		BreakpointOperator_t eOperator;
		bool                 bSet    ; // used to be called enabled pre 2.0
		bool                 bEnabled;
		bool                 bTemp;    // If true then remove BP when hit or stepping cancelled (eg. G xxxx)
	};

	typedef Breakpoint_t Bookmark_t;
	typedef Breakpoint_t Watches_t;
	typedef Breakpoint_t ZeroPagePointers_t;


// Colors ___________________________________________________________________

#include "Debugger_Color.h"

// Config _________________________________________________________________________________________
	
	enum ConfigSave_t
	{
		CONFIG_SAVE_FILE_CREATE,
		CONFIG_SAVE_FILE_APPEND
	};

// Commands _______________________________________________________________________________________

	enum Update_e
	{
		UPDATE_NOTHING,
		UPDATE_BACKGROUND      = (1 <<  0),
		UPDATE_BREAKPOINTS     = (1 <<  1),
		UPDATE_CONSOLE_DISPLAY = (1 <<  2),
		UPDATE_CONSOLE_INPUT   = (1 <<  3),
		UPDATE_DISASM          = (1 <<  4),
		UPDATE_FLAGS           = (1 <<  5),
		UPDATE_MEM_DUMP        = (1 <<  6),
		UPDATE_REGS            = (1 <<  7),
		UPDATE_STACK           = (1 <<  8),
		UPDATE_SYMBOLS         = (1 <<  9),
		UPDATE_TARGETS         = (1 << 10),
		UPDATE_WATCH           = (1 << 11),
		UPDATE_ZERO_PAGE       = (1 << 12),
		UPDATE_SOFTSWITCHES    = (1 << 13),
		UPDATE_VIDEOSCANNER    = (1 << 14),
		UPDATE_ALL = -1
	};

	typedef int Update_t;

	enum
	{
		MAX_COMMAND_LEN = 12,

		MAX_ARGS        = 32, // was 40
		ARG_SYNTAX_ERROR= -1,
		MAX_ARG_LEN     = 127, // extended to allow font names, GH#481, any value is good > CONSOLE_WIDTH=80
	};

	// NOTE: All Commands return flags of what needs to be redrawn
	typedef Update_t (*CmdFuncPtr_t)(int);

	struct Command_t
	{
		char         m_sName[ MAX_COMMAND_LEN ];
		CmdFuncPtr_t pFunction;
		int          iCommand;     // offset (enum) for direct command name lookup
		char        *pHelpSummary; // 1 line help summary
//		Hash_t       m_nHash; // TODO
	};

	// Commands sorted by Category
	// NOTE: Commands_e and g_aCommands[] order _MUST_ match !!! Aliases are listed at the end
	enum Commands_e
	{
// Assembler
		  CMD_ASSEMBLE
// CPU
		, CMD_CURSOR_JUMP_PC // Shift
		, CMD_CURSOR_SET_PC  // Ctrl
		, CMD_GO_NORMAL_SPEED
		, CMD_GO_FULL_SPEED
		, CMD_IN
		, CMD_INPUT_KEY
		, CMD_JSR
		, CMD_NOP
		, CMD_OUT
// CPU - Meta Info
		, CMD_PROFILE
		, CMD_REGISTER_SET
// CPU - Stack
//		, CMD_STACK_LIST
		, CMD_STACK_POP
		, CMD_STACK_POP_PSEUDO
		, CMD_STACK_PUSH
//		, CMD_STACK_RETURN
		, CMD_STEP_OVER
		, CMD_STEP_OUT
// CPU - Meta Info
		, CMD_TRACE
		, CMD_TRACE_FILE
		, CMD_TRACE_LINE
		, CMD_UNASSEMBLE
// Bookmarks
		, CMD_BOOKMARK
		, CMD_BOOKMARK_ADD
		, CMD_BOOKMARK_CLEAR
		, CMD_BOOKMARK_LIST
//		, CMD_BOOKMARK_LOAD
		, CMD_BOOKMARK_GOTO
		, CMD_BOOKMARK_SAVE
// Breakpoints
		, CMD_BREAK_INVALID
		, CMD_BREAK_OPCODE
		, CMD_BREAKPOINT
		, CMD_BREAKPOINT_ADD_SMART // smart breakpoint
		, CMD_BREAKPOINT_ADD_REG   // break on: PC == Address (fetch/execute)
		, CMD_BREAKPOINT_ADD_PC    // alias BPX = BA
//		,	CMD_BREAKPOINT_SET  = CMD_BREAKPOINT_ADD_ADDR // alias
//		,	CMD_BREAKPOINT_EXEC = CMD_BREAKPOINT_ADD_ADDR // alias
		, CMD_BREAKPOINT_ADD_IO  // break on: [$C000-$C7FF] Load/Store 
		, CMD_BREAKPOINT_ADD_MEM // break on: [$0000-$FFFF], excluding IO
		, CMD_BREAKPOINT_ADD_MEMR // break on read on: [$0000-$FFFF], excluding IO
		, CMD_BREAKPOINT_ADD_MEMW // break on write on: [$0000-$FFFF], excluding IO

		, CMD_BREAKPOINT_CLEAR
//		,	CMD_BREAKPOINT_REMOVE = CMD_BREAKPOINT_CLEAR // alias
		, CMD_BREAKPOINT_DISABLE
		, CMD_BREAKPOINT_EDIT
		, CMD_BREAKPOINT_ENABLE
		, CMD_BREAKPOINT_LIST
//		, CMD_BREAKPOINT_LOAD
		, CMD_BREAKPOINT_SAVE
// Benchmark / Timing
//		, CMD_BENCHMARK_START
//		, CMD_BENCHMARK_STOP
//		, CMD_PROFILE_START
//		, CMD_PROFILE_STOP
// Config (debugger settings)
		, CMD_BENCHMARK
		, CMD_CONFIG_BW         // BW    # rr gg bb
		, CMD_CONFIG_COLOR      // COLOR # rr gg bb

		, CMD_CONFIG_DISASM
//		, CMD_CONFIG_DISASM_BRANCH
//		, CMD_CONFIG_DISASM_COLON
//		, CMD_CONFIG_DISASM_OPCODE
//		, CMD_CONFIG_DISASM_SPACES

		, CMD_CONFIG_FONT
//		, CMD_CONFIG_FONT2 // PARAM_FONT_DISASM PARAM_FONT_INFO PARAM_FONT_SOURCE
		, CMD_CONFIG_HCOLOR     // TODO Video :: SETFRAMECOLOR(#,R,G,B)
		, CMD_CONFIG_LOAD
		, CMD_CONFIG_MONOCHROME // MONO  # rr gg bb
		, CMD_CONFIG_SAVE
		, CMD_CONFIG_GET_DEBUG_DIR
		, CMD_CONFIG_SET_DEBUG_DIR
// Cursor
		, CMD_CURSOR_JUMP_RET_ADDR
		, CMD_CURSOR_LINE_UP   // Smart Line Up
		, CMD_CURSOR_LINE_UP_1 // Shift
		, CMD_CURSOR_LINE_DOWN // Smart Line Down
		, CMD_CURSOR_LINE_DOWN_1 // Shift
//		, CMD_CURSOR_PAGE_UP
//		, CMD_CURSOR_PAGE_DOWN
		, CMD_CURSOR_PAGE_UP
		, CMD_CURSOR_PAGE_UP_256 // up to nearest page boundary
		, CMD_CURSOR_PAGE_UP_4K // Up to nearest 4K boundary

		, CMD_CURSOR_PAGE_DOWN
		, CMD_CURSOR_PAGE_DOWN_256 // Down to nearest page boundary
		, CMD_CURSOR_PAGE_DOWN_4K // Down to nearest 4K boundary
// Cycles info
		, CMD_CYCLES_INFO
// Disassembler Data
		, CMD_DISASM_DATA
		, CMD_DISASM_CODE
		, CMD_DISASM_LIST
		, CMD_DEFINE_DATA_BYTE1    // DB $00,$04,$08,$0C,$10,$14,$18,$1C
		, CMD_DEFINE_DATA_BYTE2
		, CMD_DEFINE_DATA_BYTE4
		, CMD_DEFINE_DATA_BYTE8

		, CMD_DEFINE_DATA_WORD1    // DW $300
		, CMD_DEFINE_DATA_WORD2
		, CMD_DEFINE_DATA_WORD4
		, CMD_DEFINE_DATA_STR
//		, CMD_DEFINE_DATA_FACP // FAC Packed
//		, CMD_DEFINE_DATA_FACU // FAC Unpacked
//		, CMD_DATA_DEFINE_ADDR_BYTE_L  // DB< address symbol
//		, CMD_DATA_DEFINE_ADDR_BYTE_H  // DB> address symbol
		, CMD_DEFINE_ADDR_WORD    // .DA address symbol
// Disk
		, CMD_DISK
// Flags - CPU
		, CMD_FLAG_CLEAR // Flag order must match g_aFlagNames CZIDBRVN
		, CMD_FLAG_CLR_C // 8
		, CMD_FLAG_CLR_Z // 7
		, CMD_FLAG_CLR_I // 6
		, CMD_FLAG_CLR_D // 5
		, CMD_FLAG_CLR_B // 4
		, CMD_FLAG_CLR_R // 3
		, CMD_FLAG_CLR_V // 2
		, CMD_FLAG_CLR_N // 1

		, CMD_FLAG_SET   // Flag order must match g_aFlagNames CZIDBRVN
		, CMD_FLAG_SET_C // 8
		, CMD_FLAG_SET_Z // 7
		, CMD_FLAG_SET_I // 6
		, CMD_FLAG_SET_D // 5
		, CMD_FLAG_SET_B // 4
		, CMD_FLAG_SET_R // 3
		, CMD_FLAG_SET_V // 2
		, CMD_FLAG_SET_N // 1
// Help
		, CMD_HELP_LIST
		, CMD_HELP_SPECIFIC
		, CMD_VERSION
		, CMD_MOTD // Message of the Day
// Memory		
		, CMD_MEMORY_COMPARE

		, CMD_MEM_MINI_DUMP_HEX_1 // Mini Memory Dump 1
		, CMD_MEM_MINI_DUMP_HEX_2 // Mini Memory Dump 2

		, CMD_MEM_MINI_DUMP_ASCII_1    // ASCII
		, CMD_MEM_MINI_DUMP_ASCII_2
		, CMD_MEM_MINI_DUMP_APPLE_1 // Low-Bit inverse, High-Bit normal
		, CMD_MEM_MINI_DUMP_APPLE_2
//		, CMD_MEM_MINI_DUMP_TXT_LO_1 // ASCII (Controls Chars)
//		, CMD_MEM_MINI_DUMP_TXT_LO_2
//		, CMD_MEM_MINI_DUMP_TXT_HI_1 // ASCII (High Bit)
//		, CMD_MEM_MINI_DUMP_TXT_HI_2

//		, CMD_MEMORY_DUMP = CMD_MEM_MINI_DUMP_HEX_1
		, CMD_MEMORY_EDIT
		, CMD_MEMORY_ENTER_BYTE
		, CMD_MEMORY_ENTER_WORD
		, CMD_MEMORY_LOAD
		, CMD_MEMORY_MOVE
		, CMD_MEMORY_SAVE
		, CMD_MEMORY_SEARCH
		, CMD_MEMORY_FIND_RESULTS
//		, CMD_MEMORY_SEARCH_ASCII   // Ascii Text
//		, CMD_MEMORY_SEARCH_APPLE   // Flashing Chars, Hi-Bit Set
		, CMD_MEMORY_SEARCH_HEX
		, CMD_MEMORY_FILL
		, CMD_NTSC
		, CMD_TEXT_SAVE
// Output
		, CMD_OUTPUT_CALC
		, CMD_OUTPUT_ECHO
		, CMD_OUTPUT_PRINT
		, CMD_OUTPUT_PRINTF
		, CMD_OUTPUT_RUN
// Source Level Debugging
		, CMD_SOURCE
		, CMD_SYNC
// Symbols
		, CMD_SYMBOLS_LOOKUP
//		, CMD_SYMBOLS
		, CMD_SYMBOLS_ROM
		, CMD_SYMBOLS_APPLESOFT
		, CMD_SYMBOLS_ASSEMBLY
		, CMD_SYMBOLS_USER_1
		, CMD_SYMBOLS_USER_2
		, CMD_SYMBOLS_SRC_1
		, CMD_SYMBOLS_SRC_2
		, CMD_SYMBOLS_DOS33
		, CMD_SYMBOLS_PRODOS
//		, CMD_SYMBOLS_FIND
//		, CMD_SYMBOLS_CLEAR
		, CMD_SYMBOLS_INFO
		, CMD_SYMBOLS_LIST
//		, CMD_SYMBOLS_LOAD_1
//		, CMD_SYMBOLS_LOAD_2
//		, CMD_SYMBOLS_SAVE
// Video-scanner info
		, CMD_VIDEO_SCANNER_INFO
// View
		, CMD_VIEW_TEXT4X
		, CMD_VIEW_TEXT41
		, CMD_VIEW_TEXT42
		, CMD_VIEW_TEXT8X
		, CMD_VIEW_TEXT81
		, CMD_VIEW_TEXT82
		, CMD_VIEW_GRX
		, CMD_VIEW_GR1
		, CMD_VIEW_GR2
		, CMD_VIEW_DGRX
		, CMD_VIEW_DGR1
		, CMD_VIEW_DGR2
		, CMD_VIEW_HGRX
		, CMD_VIEW_HGR1
		, CMD_VIEW_HGR2
		, CMD_VIEW_DHGRX
		, CMD_VIEW_DHGR1
		, CMD_VIEW_DHGR2
// Watch
		, CMD_WATCH // TODO: Deprecated ?
		, CMD_WATCH_ADD
		, CMD_WATCH_CLEAR
		, CMD_WATCH_DISABLE
		, CMD_WATCH_ENABLE
		, CMD_WATCH_LIST
//		, CMD_WATCH_LOAD
		, CMD_WATCH_SAVE
// Window
//		, CMD_WINDOW_COLOR_CUSTOM
		, CMD_WINDOW

		, CMD_WINDOW_CODE
		, CMD_WINDOW_CODE_1
		, CMD_WINDOW_CODE_2

		, CMD_WINDOW_CONSOLE

		, CMD_WINDOW_DATA
		, CMD_WINDOW_DATA_1
		, CMD_WINDOW_DATA_2

		// SOURCE is reserved for source level debugging
		, CMD_WINDOW_SOURCE_1
		, CMD_WINDOW_SOURCE_2

//		, CMD_WINDOW_STACK_1
//		, CMD_WINDOW_STACK_2

		// SYMBOLS is reserved for symbols lookup/commands
//		, CMD_WINDOW_SYMBOLS_1
//		, CMD_WINDOW_SYMBOLS_2

//		, CMD_WINDOW_ZEROPAGE_1
//		, CMD_WINDOW_ZEROPAGE_2

		, CMD_WINDOW_OUTPUT
//		, CMD_WINDOW_SOURCE
// ZeroPage
		, CMD_ZEROPAGE_POINTER
		, CMD_ZEROPAGE_POINTER_0
		, CMD_ZEROPAGE_POINTER_1
		, CMD_ZEROPAGE_POINTER_2
		, CMD_ZEROPAGE_POINTER_3
		, CMD_ZEROPAGE_POINTER_4
		, CMD_ZEROPAGE_POINTER_5
		, CMD_ZEROPAGE_POINTER_6
		, CMD_ZEROPAGE_POINTER_7
		, CMD_ZEROPAGE_POINTER_ADD
		, CMD_ZEROPAGE_POINTER_CLEAR
		, CMD_ZEROPAGE_POINTER_DISABLE
		, CMD_ZEROPAGE_POINTER_ENABLE
		, CMD_ZEROPAGE_POINTER_LIST
//		, CMD_ZEROPAGE_POINTER_LOAD
		, CMD_ZEROPAGE_POINTER_SAVE

		, NUM_COMMANDS

		, _CMD_MEM_MINI_DUMP_HEX_1_1 // Memory Dump
		, _CMD_MEM_MINI_DUMP_HEX_1_3 // alias M1
		, _CMD_MEM_MINI_DUMP_HEX_2_1 // alias M2
	};

// Assembler
	Update_t CmdAssemble              (int nArgs);

// Disassembler Data
	Update_t CmdDisasmDataDefCode     (int nArgs);
	Update_t CmdDisasmDataList        (int nArgs);

	Update_t CmdDisasmDataDefByte1    (int nArgs);
	Update_t CmdDisasmDataDefByte2    (int nArgs);
	Update_t CmdDisasmDataDefByte4    (int nArgs);
	Update_t CmdDisasmDataDefByte8    (int nArgs);

	Update_t CmdDisasmDataDefWord1    (int nArgs);
	Update_t CmdDisasmDataDefWord2    (int nArgs);
	Update_t CmdDisasmDataDefWord4    (int nArgs);

	Update_t CmdDisasmDataDefString   (int nArgs);

	Update_t CmdDisasmDataDefAddress8H(int nArgs);
	Update_t CmdDisasmDataDefAddress8L(int nArgs);
	Update_t CmdDisasmDataDefAddress16(int nArgs);

// CPU
	Update_t CmdCursorJumpPC       (int nArgs);
	Update_t CmdCursorSetPC        (int nArgs);
	Update_t CmdBreakInvalid       (int nArgs); // Breakpoint IFF Full-speed!
	Update_t CmdBreakOpcode        (int nArgs); // Breakpoint IFF Full-speed!
	Update_t CmdGoNormalSpeed      (int nArgs);
	Update_t CmdGoFullSpeed        (int nArgs);
	Update_t CmdIn                 (int nArgs);
	Update_t CmdKey                (int nArgs);
	Update_t CmdJSR                (int nArgs);
	Update_t CmdNOP                (int nArgs);
	Update_t CmdOut                (int nArgs);
	Update_t CmdStepOver           (int nArgs);
	Update_t CmdStepOut            (int nArgs);
	Update_t CmdTrace              (int nArgs);  // alias for CmdStepIn
	Update_t CmdTraceFile          (int nArgs);
	Update_t CmdTraceLine          (int nArgs);
	Update_t CmdUnassemble         (int nArgs); // code dump, aka, Unassemble
// Bookmarks
	Update_t CmdBookmark           (int nArgs);
	Update_t CmdBookmarkAdd        (int nArgs);
	Update_t CmdBookmarkClear      (int nArgs);
	Update_t CmdBookmarkList       (int nArgs);
	Update_t CmdBookmarkGoto       (int nArgs);
//	Update_t CmdBookmarkLoad       (int nArgs);
	Update_t CmdBookmarkSave       (int nArgs);
// Breakpoints
	Update_t CmdBreakpoint         (int nArgs);
	Update_t CmdBreakpointAddSmart (int nArgs);
	Update_t CmdBreakpointAddReg   (int nArgs);
	Update_t CmdBreakpointAddPC    (int nArgs);
	Update_t CmdBreakpointAddIO    (int nArgs);
	Update_t CmdBreakpointAddMem   (int nArgs, BreakpointSource_t bpSrc = BP_SRC_MEM_RW);
	Update_t CmdBreakpointAddMemA  (int nArgs);
	Update_t CmdBreakpointAddMemR  (int nArgs);
	Update_t CmdBreakpointAddMemW  (int nArgs);
	Update_t CmdBreakpointClear    (int nArgs);
	Update_t CmdBreakpointDisable  (int nArgs);
	Update_t CmdBreakpointEdit     (int nArgs);
	Update_t CmdBreakpointEnable   (int nArgs);
	Update_t CmdBreakpointList     (int nArgs);
//	Update_t CmdBreakpointLoad     (int nArgs);
	Update_t CmdBreakpointSave     (int nArgs);
// Benchmark
	Update_t CmdBenchmark          (int nArgs);
	Update_t CmdBenchmarkStart     (int nArgs); //Update_t CmdSetupBenchmark (int nArgs);
	Update_t CmdBenchmarkStop      (int nArgs); //Update_t CmdExtBenchmark (int nArgs);
	Update_t CmdProfile            (int nArgs);
	Update_t CmdProfileStart       (int nArgs);
	Update_t CmdProfileStop        (int nArgs);
// Config
//	Update_t CmdConfigMenu         (int nArgs);
//	Update_t CmdConfigBase         (int nArgs);
//	Update_t CmdConfigBaseHex      (int nArgs);
//	Update_t CmdConfigBaseDec      (int nArgs);
	Update_t CmdConfigColorMono    (int nArgs);
	Update_t CmdConfigDisasm       (int nArgs);
	Update_t CmdConfigFont         (int nArgs);
	Update_t CmdConfigHColor       (int nArgs);
	Update_t CmdConfigLoad         (int nArgs);
	Update_t CmdConfigSave         (int nArgs);
	Update_t CmdConfigSetFont      (int nArgs);
	Update_t CmdConfigGetFont      (int nArgs);
	Update_t CmdConfigGetDebugDir  (int nArgs);
	Update_t CmdConfigSetDebugDir  (int nArgs);
// Cursor
	Update_t CmdCursorFollowTarget (int nArgs);
	Update_t CmdCursorLineDown     (int nArgs);
	Update_t CmdCursorLineUp       (int nArgs);
	Update_t CmdCursorJumpRetAddr  (int nArgs);
	Update_t CmdCursorRunUntil     (int nArgs);
	Update_t CmdCursorPageDown     (int nArgs);
	Update_t CmdCursorPageDown256  (int nArgs);
	Update_t CmdCursorPageDown4K   (int nArgs);
	Update_t CmdCursorPageUp       (int nArgs);
	Update_t CmdCursorPageUp256    (int nArgs);
	Update_t CmdCursorPageUp4K     (int nArgs);

// Cycles info
	Update_t CmdCyclesInfo   (int nArgs);

// Disk
	Update_t CmdDisk               (int nArgs);
// Help
	Update_t CmdHelpList           (int nArgs);
	Update_t CmdHelpSpecific       (int Argss);
	Update_t CmdVersion            (int nArgs);
	Update_t CmdMOTD               (int nArgs);
// Flags
	Update_t CmdFlag               (int nArgs);
	Update_t CmdFlagClear          (int nArgs);
	Update_t CmdFlagSet            (int nArgs);
// Memory (Data)
	Update_t CmdMemoryCompare      (int nArgs);
	Update_t CmdMemoryMiniDumpHex  (int nArgs);
	Update_t CmdMemoryMiniDumpAscii(int nArgs);
	Update_t CmdMemoryMiniDumpApple(int nArgs);
//	Update_t CmdMemoryMiniDumpLow  (int nArgs);
//	Update_t CmdMemoryMiniDumpHigh (int nArgs);

	Update_t CmdMemoryEdit         (int nArgs);
	Update_t CmdMemoryEnterByte    (int nArgs);
	Update_t CmdMemoryEnterWord    (int nArgs);
	Update_t CmdMemoryFill         (int nArgs);
	Update_t CmdNTSC               (int nArgs);
	Update_t CmdTextSave           (int nArgs);

	Update_t CmdMemoryLoad         (int nArgs);
	Update_t CmdMemoryMove         (int nArgs);
	Update_t CmdMemorySave         (int nArgs);
	Update_t CmdMemorySearch       (int nArgs);
	Update_t _SearchMemoryDisplay  (int nArgs=0); // TODO: CLEANUP
//	Update_t CmdMemorySearchLowBit (int nArgs);
//	Update_t CmdMemorySearchHiBit  (int nArgs);
	Update_t CmdMemorySearchAscii  (int nArgs);
	Update_t CmdMemorySearchApple  (int nArgs);
	Update_t CmdMemorySearchHex    (int nArgs);
// Output/Scripts
	Update_t CmdOutputCalc         (int nArgs);
	Update_t CmdOutputEcho         (int nArgs);
	Update_t CmdOutputPrint        (int nArgs);
	Update_t CmdOutputPrintf       (int nArgs);
	Update_t CmdOutputRun          (int nArgs);
// Registers
	Update_t CmdRegisterSet        (int nArgs);
// Source Level Debugging
	Update_t CmdSource             (int nArgs);
	Update_t CmdSync               (int nArgs);
// Stack
	Update_t CmdStackPush          (int nArgs);
	Update_t CmdStackPop           (int nArgs);
	Update_t CmdStackPopPseudo     (int nArgs);
	Update_t CmdStackReturn        (int nArgs);
// Symbols
	Update_t CmdSymbols            (int nArgs);
	Update_t CmdSymbolsClear       (int nArgs);
	Update_t CmdSymbolsList        (int nArgs);
	Update_t CmdSymbolsLoad        (int nArgs);
	Update_t CmdSymbolsInfo        (int nArgs);
	Update_t CmdSymbolsSave        (int nArgs);

	Update_t CmdSymbolsCommand     (int nArgs);
//	Update_t CmdSymbolsMain        (int nArgs);
//	Update_t CmdSymbolsBasic       (int nArgs);
//	Update_t CmdSymbolsUser        (int nArgs);
//	Update_t CmdSymbolsAssembly    (int nArgs);
//	Update_t CmdSymbolsSource      (int nArgs);

// Video-scanner info
	Update_t CmdVideoScannerInfo   (int nArgs);

// View
	Update_t CmdViewOutput_Text4X  (int nArgs);
	Update_t CmdViewOutput_Text41  (int nArgs);
	Update_t CmdViewOutput_Text42  (int nArgs);
	Update_t CmdViewOutput_Text8X  (int nArgs);
	Update_t CmdViewOutput_Text81  (int nArgs);
	Update_t CmdViewOutput_Text82  (int nArgs);

	Update_t CmdViewOutput_GRX     (int nArgs);
	Update_t CmdViewOutput_GR1     (int nArgs);
	Update_t CmdViewOutput_GR2     (int nArgs);
	Update_t CmdViewOutput_DGRX    (int nArgs);
	Update_t CmdViewOutput_DGR1    (int nArgs);
	Update_t CmdViewOutput_DGR2    (int nArgs);

	Update_t CmdViewOutput_HGRX    (int nArgs);
	Update_t CmdViewOutput_HGR1    (int nArgs);
	Update_t CmdViewOutput_HGR2    (int nArgs);
	Update_t CmdViewOutput_DHGRX   (int nArgs);
	Update_t CmdViewOutput_DHGR1   (int nArgs);
	Update_t CmdViewOutput_DHGR2   (int nArgs);
// Watch
	Update_t CmdWatch              (int nArgs);
	Update_t CmdWatchAdd           (int nArgs);
	Update_t CmdWatchClear         (int nArgs);
	Update_t CmdWatchDisable       (int nArgs);
	Update_t CmdWatchEnable        (int nArgs);
	Update_t CmdWatchList          (int nArgs);
//	Update_t CmdWatchLoad          (int nArgs);
	Update_t CmdWatchSave          (int nArgs);
// Window
	Update_t CmdWindow             (int nArgs);
	Update_t CmdWindowCycleNext    (int nArgs);
	Update_t CmdWindowCyclePrev    (int nArgs);
	Update_t CmdWindowLast         (int nArgs);

	Update_t CmdWindowShowCode     (int nArgs);
	Update_t CmdWindowShowCode1    (int nArgs);
	Update_t CmdWindowShowCode2    (int nArgs);
	Update_t CmdWindowShowData     (int nArgs);
	Update_t CmdWindowShowData1    (int nArgs);
	Update_t CmdWindowShowData2    (int nArgs);
	Update_t CmdWindowShowSymbols1 (int nArgs);
	Update_t CmdWindowShowSymbols2 (int nArgs);
	Update_t CmdWindowShowSource   (int nArgs);
	Update_t CmdWindowShowSource1  (int nArgs);
	Update_t CmdWindowShowSource2  (int nArgs);

	Update_t CmdWindowViewCode     (int nArgs);
	Update_t CmdWindowViewConsole  (int nArgs);
	Update_t CmdWindowViewData     (int nArgs);
	Update_t CmdWindowViewOutput   (int nArgs);
	Update_t CmdWindowViewSource   (int nArgs);
	Update_t CmdWindowViewSymbols  (int nArgs);

	Update_t CmdWindowWidthToggle  (int nArgs);

// ZeroPage
//	Update_t CmdZeroPageShow       (int nArgs);
//	Update_t CmdZeroPageHide       (int nArgs);
//	Update_t CmdZeroPageToggle     (int nArgs);
	Update_t CmdZeroPage           (int nArgs);
	Update_t CmdZeroPageAdd        (int nArgs);
	Update_t CmdZeroPageClear      (int nArgs);
	Update_t CmdZeroPageDisable    (int nArgs);
	Update_t CmdZeroPageEnable     (int nArgs);
	Update_t CmdZeroPageList       (int nArgs);
//	Update_t CmdZeroPageLoad       (int nArgs);
	Update_t CmdZeroPageSave       (int nArgs);
	Update_t CmdZeroPagePointer    (int nArgs);


// Cursor _________________________________________________________________________________________
	enum Cursor_Align_e
	{
		CURSOR_ALIGN_TOP,
		CURSOR_ALIGN_CENTER
	};

	enum CursorHiLightState_e
	{
		CURSOR_NORMAL    , // White
		CURSOR_CPU_PC    , // Yellow
		CURSOR_BREAKPOINT, // Red
	};


// Disassembly ____________________________________________________________________________________

	// Data Disassembler
	enum Nopcode_e
	{
		_NOP_REMOVED
		,NOP_BYTE_1 // 1 bytes/line
		,NOP_BYTE_2 // 2 bytes/line
		,NOP_BYTE_4 // 4 bytes/line
		,NOP_BYTE_8 // 8 bytes/line
		,NOP_WORD_1 // 1 words/line = 2 bytes (no symbol lookup)
		,NOP_WORD_2 // 2 words/line = 4 bytes
		,NOP_WORD_4 // 4 words/line = 8 bytes
		,NOP_ADDRESS// 1 word/line  = 2 bytes (with symbol lookup)
		,NOP_HEX    // hex string   =16 bytes
		,NOP_CHAR   // char string // TODO: FIXME: needed??
		,NOP_STRING_ASCII // Low Ascii
		,NOP_STRING_APPLE // High Ascii
		,NOP_STRING_APPLESOFT // Mixed Low/High
		,NOP_FAC
		,NOP_SPRITE
		,NUM_NOPCODE_TYPES
	};

	// Disassembler Data
	// type symbol[start:end]
	struct DisasmData_t
	{
		char sSymbol[ MAX_SYMBOLS_LEN+1 ];

		Nopcode_e eElementType ; // eElementType -> iNoptype
		int       iDirective   ; // iDirective   -> iNopcode

		WORD nStartAddress; // link to block [start,end)
		WORD nEndAddress  ; 
		WORD nArraySize   ; // Total bytes
//		WORD nBytePerRow  ; // 1, 8

		// with symbol lookup
		char bSymbolLookup ;
		WORD nTargetAddress;

		WORD nSpriteW;
		WORD nSpriteH;
	};

	// Disassembler ...
	enum DisasmBranch_e
	{
		  DISASM_BRANCH_OFF = 0
		, DISASM_BRANCH_PLAIN
		, DISASM_BRANCH_FANCY
		, NUM_DISASM_BRANCH_TYPES
	};

	enum DisasmFormat_e
	{
		DISASM_FORMAT_CHAR           = (1 << 0),
		DISASM_FORMAT_SYMBOL         = (1 << 1),
		DISASM_FORMAT_OFFSET         = (1 << 2),
		DISASM_FORMAT_BRANCH         = (1 << 3),
		DISASM_FORMAT_TARGET_POINTER = (1 << 4),
		DISASM_FORMAT_TARGET_VALUE   = (1 << 5),
	};

	enum DisasmImmediate_e
	{
		  DISASM_IMMED_OFF = 0
		, DISASM_IMMED_TARGET
		, DISASM_IMMED_MODE
		, DISASM_IMMED_BOTH
		, NUM_DISASM_IMMED_TYPES
	};

	enum DisasmTargets_e
	{
		  DISASM_TARGET_OFF  = 0
		, DISASM_TARGET_VAL  // Note: Also treated as bit flag !!
		, DISASM_TARGET_ADDR // Note: Also treated as bit flag !!
		, DISASM_TARGET_BOTH // Note: Also treated as bit flag !!
		, NUM_DISASM_TARGET_TYPES
	};

	enum DisasmDisplay_e // TODO: Prefix enums with DISASM_DISPLAY_
	{
		MAX_ADDRESS_LEN   = 40,
		MAX_OPCODES       =  3, // only display 3 opcode bytes -- See FormatOpcodeBytes() // TODO: FIX when showing data hex
		CHARS_FOR_ADDRESS =  8, // 4 digits + end-of-string + padding
		MAX_IMMEDIATE_LEN = 20, // Data Disassembly
		MAX_TARGET_LEN    = MAX_IMMEDIATE_LEN, // Debugger Display: pTarget = line.sTarget
	};

	struct DisasmLine_t
	{
		short iOpcode;
		short iOpmode;
		int   nOpbyte;

		char sAddress  [ CHARS_FOR_ADDRESS ];
		char sOpCodes  [(MAX_OPCODES*3)+1];

		// Added for Data Disassembler
		char sLabel    [ MAX_SYMBOLS_LEN+1 ]; // label is a symbol

		Nopcode_e iNoptype; // basic element type
		int       iNopcode; // assembler directive / pseudo opcode
		int       nSlack  ;

		char sMnemonic [ MAX_SYMBOLS_LEN+1 ]; // either the real Mnemonic or the Assembler Directive
const	DisasmData_t* pDisasmData; // If != NULL then bytes are marked up as data not code
		//

		int  nTarget; // address -> string
		char sTarget   [ MAX_ADDRESS_LEN ];

		char sTargetOffset[ CHARS_FOR_ADDRESS ]; // +/- 255, realistically +/-1
		int  nTargetOffset;

		char sTargetPointer[ CHARS_FOR_ADDRESS ];
		char sTargetValue  [ CHARS_FOR_ADDRESS ];
//		char sTargetAddress[ CHARS_FOR_ADDRESS ];

		char sImmediate[ 4 ]; // 'c'
		char nImmediate;
		char sBranch   [ 4 ]; // ^

		bool bTargetImmediate;
		bool bTargetIndirect;
		bool bTargetIndexed ;
		bool bTargetRelative;
		bool bTargetX       ;
		bool bTargetY       ;
		bool bTargetValue   ;

		void Clear()
		{
			memset( this, 0, sizeof( *this ) );
		}
		void ClearFlags()
		{
			bTargetImmediate = false; // display "#"
			bTargetIndexed   = false; // display ()
			bTargetIndirect  = false; // display ()
			bTargetRelative  = false; // display ()
			bTargetX         = false; // display ",X"
			bTargetY         = false; // display ",Y"
			bTargetValue     = false; // display $####
		}
	};
	
// Font ___________________________________________________________________________________________
	enum FontType_e
	{
//		  FONT_DEFAULT 
		  FONT_INFO 
		, FONT_CONSOLE
		, FONT_DISASM_DEFAULT
		, FONT_DISASM_BRANCH
	//	, FONT_SOURCE
		, NUM_FONTS
	};

	enum
	{
		MAX_FONT_NAME = MAX_ARG_LEN // was 64
	};

	enum FontSpacing_e
	{
		FONT_SPACING_CLASSIC   , // least lines (most spacing)
		FONT_SPACING_CLEAN     , // more lines (minimal spacing)
		FONT_SPACING_COMPRESSED, // max lines (least spacing)
		NUM_FONT_SPACING
	};

	struct FontConfig_t
	{
		char  _sFontName[ MAX_FONT_NAME ];
		HFONT _hFont;
//		int   _iFontType;
		int   _nFontWidthAvg;
		int   _nFontWidthMax;
		int   _nFontHeight;		
		int   _nLineHeight; // may or may not include spacer
	};


// Instructions / Opcodes _________________________________________________________________________

	//#define SUPPORT_Z80_EMU
	#ifdef SUPPORT_Z80_EMU
		#define OUTPUT_Z80_REGS
		#define REG_AF 0xF0
		#define REG_BC 0xF2
		#define REG_DE 0xF4
		#define REG_HL 0xF6
		#define REG_IX 0xF8
	#endif

	enum MemoryAccess_e
	{
		MEM_R  = (1 << 0), // Read
		MEM_W  = (1 << 1), // Write
		MEM_RI = (1 << 2), // Read Implicit (Implied)
		MEM_WI = (1 << 3), // Write Implicit (Implied)
		MEM_S  = (1 << 4), // Stack (Read/Write)
		MEM_IM = (1 << 5), // Immediate - Technically reads target byte

		NUM_MEM_ACCESS,

	// Alias
		MEM_READ  = (1 << 0),
		MEM_WRITE = (1 << 1),
	};

	enum
	{
		// First 256 are 6502
		// TODO: Second 256 are Directives/Pseudo Mnemonics
		NUM_OPCODES      = 256,

		MAX_MNEMONIC_LEN =   3,
	};

	struct Opcodes_t
	{
		char  sMnemonic[ MAX_MNEMONIC_LEN+1 ];
		// int16 for structure 8-byte alignment
		short nAddressMode; // TODO/FIX: nOpmode
		short nMemoryAccess;
	};
	
	struct Instruction2_t
	{
		char   sMnemonic[MAX_MNEMONIC_LEN+1];
		int    nAddressMode;
		int    iMemoryAccess;
	};

	enum Opcode_e
	{
		OPCODE_BRA     = 0x80,

		OPCODE_BRK     = 0x00,
		OPCODE_JSR     = 0x20,
		OPCODE_RTI     = 0x40,
		OPCODE_JMP_A   = 0x4C, // Absolute
		OPCODE_RTS     = 0x60,
		OPCODE_JMP_NA  = 0x6C, // Indirect Absolute
		OPCODE_JMP_IAX = 0x7C, // Indexed (Absolute Indirect, X)
		OPCODE_LDA_A   = 0xAD, // Absolute

		OPCODE_NOP     = 0xEA, // No operation
	};

	// Note: "int" causes overflow when profiling for any amount of time.
	// typedef unsigned int Profile_t;
	// i.e.
	//	double nPercent = static_cast<double>(100 * tProfileOpcode.uProfile) / nOpcodeTotal; // overflow
	typedef double Profile_t;

	struct ProfileOpcode_t
	{
		int       m_iOpcode;
		Profile_t m_nCount; // Histogram

		// functor
		bool operator () (const ProfileOpcode_t & rLHS, const ProfileOpcode_t & rRHS) const
		{
			bool bGreater = (rLHS.m_nCount > rRHS.m_nCount);
			return bGreater;
		}
	};

	struct ProfileOpmode_t
	{
		int       m_iOpmode;
		Profile_t m_nCount; // Histogram

		// functor
		bool operator () (const ProfileOpmode_t & rLHS, const ProfileOpmode_t & rRHS) const
		{
			bool bGreater = (rLHS.m_nCount > rRHS.m_nCount);
			return bGreater;
		}
	};

	enum ProfileFormat_e
	{
		PROFILE_FORMAT_SPACE,
		PROFILE_FORMAT_TAB,
		PROFILE_FORMAT_COMMA,
	};

// Memory _________________________________________________________________________________________

	extern const          int _6502_BRANCH_POS      ;//= +127
	extern const          int _6502_BRANCH_NEG      ;//= -128
	extern const unsigned int _6502_ZEROPAGE_END    ;//= 0x00FF;
	extern const unsigned int _6502_STACK_BEGIN     ;//= 0x0100;
	extern const unsigned int _6502_STACK_END       ;//= 0x01FF;
	extern const unsigned int _6502_IO_BEGIN        ;//= 0xC000;
	extern const unsigned int _6502_IO_END          ;//= 0xC0FF;
	extern const unsigned int _6502_BRK_VECTOR      ;//= 0xFFFE;
	extern const unsigned int _6502_MEM_BEGIN       ;//= 0x0000;
	extern const unsigned int _6502_MEM_END         ;//= 0xFFFF;
	extern const unsigned int _6502_MEM_LEN			;//= 0x10000;

	enum DEVICE_e
	{
		DEV_MEMORY,
		DEV_DISK2 ,
		DEV_SY6522,
		DEV_AY8910,
		NUM_DEVICES
	};

	enum MemoryView_e
	{
		MEM_VIEW_HEX   ,

		// 0x00 .. 0x1F Ctrl              (Inverse)
		// 0x20 .. 0x7F Flash / MouseText (Cyan)
		// 0x80 .. 0x9F Hi-Bit Ctrl       (Yellow)
		// 0xA0 .. 0xFF Hi-Bit Normal     (White)
		MEM_VIEW_ASCII ,
		MEM_VIEW_APPLE , // Low-Bit ASCII (Colorized Background)
//		MEM_VIEW_TXT_LO, // Ctrl Chars mapped to visible range, and inverse
//		MEM_VIEW_TXT_HI, // High Bit Ascii
		NUM_MEM_VIEWS
	};

	struct MemoryDump_t
	{
		bool         bActive;
		WORD         nAddress; // nAddressMemDump; // was USHORT
		DEVICE_e     eDevice;
		MemoryView_e eView;
	};

	enum MemoryDump_e
	{
		MEM_DUMP_1,
		MEM_DUMP_2,
		NUM_MEM_DUMPS
	};

	enum MemoryMiniDump_e
	{
		NUM_MEM_MINI_DUMPS = 2
	};

	enum MemorySearch_e
	{
		MEM_SEARCH_BYTE_EXACT    , // xx
		MEM_SEARCH_NIB_LOW_EXACT , // ?x
		MEM_SEARCH_NIB_HIGH_EXACT, // x?
		MEM_SEARCH_BYTE_1_WILD   , // ?
		MEM_SEARCH_BYTE_N_WILD   , // ??

		MEM_SEARCH_TYPE_MASK = (1 << 16) - 1,
		MEM_SEARCH_FOUND     = (1 << 16)
	};

	struct MemorySearch_t
	{
		BYTE           m_nValue  ; // search value
		MemorySearch_e m_iType   ; // 
		bool           m_bFound  ; // 
	};

	typedef std::vector<MemorySearch_t> MemorySearchValues_t;
	typedef std::vector<int>            MemorySearchResults_t;

// Parameters _____________________________________________________________________________________

	/* i.e.
			SYM LOAD = $C600   (1) type: string, nVal1 = symlookup; (2) type: operator, token: EQUAL; (3) type: address, token:DOLLAR
			BP LOAD            type:  
			BP $LOAD           type: (1) = symbol, val=1adress
	*/
	enum ArgToken_e // Arg Token Type
	{
		// Single Char Tokens must come first
		  TOKEN_ALPHANUMERIC // 
		, TOKEN_AMPERSAND    // &
		, TOKEN_AT           // @  results dereference. i.e. S 0,FFFF C030; L @1
		, TOKEN_BRACE_L      // {
		, TOKEN_BRACE_R      // }
		, TOKEN_BRACKET_L    // [
		, TOKEN_BRACKET_R    // ]
		, TOKEN_BSLASH       // \xx Hex Literal
		, TOKEN_CARET        // ^
//		, TOKEN_CHAR
		, TOKEN_COLON        // : Range 
		, TOKEN_COMMA        // , Length
//		, TOKEN_DIGIT
		, TOKEN_DOLLAR       // $ Address (symbol lookup forced)
		, TOKEN_EQUAL        // = Assign Argment.n2 = Argument2
		, TOKEN_EXCLAMATION  // !
		, TOKEN_FSLASH       // /
		, TOKEN_GREATER_THAN // > 
		, TOKEN_HASH         // # Value  no symbol lookup
		, TOKEN_LESS_THAN    // <
		, TOKEN_MINUS        // - Delta  Argument1 -= Argument2
		, TOKEN_PAREN_L      // (
		, TOKEN_PAREN_R      // )
		, TOKEN_PERCENT      // %
		, TOKEN_PIPE         // |
		, TOKEN_PLUS         // + Delta  Argument1 += Argument2
		, TOKEN_QUOTE_SINGLE // '
		, TOKEN_QUOTE_DOUBLE // "
		, TOKEN_SEMI         // ; Command Seperator
		, TOKEN_SPACE        //   Token Delimiter
		, TOKEN_STAR         // *
//		, TOKEN_TAB          // '\t'
		, TOKEN_TILDE        // ~

		// Multi char tokens come last
		, TOKEN_COMMENT_EOL  // //
		,_TOKEN_FLAG_MULTI = TOKEN_COMMENT_EOL
		, TOKEN_GREATER_EQUAL// >=
		, TOKEN_LESS_EQUAL   // <=
		, TOKEN_NOT_EQUAL    // !=
//		, TOKEN_COMMENT_1    // /*
//		, TOKEN_COMMENT_2    // */

		, NUM_TOKENS // signal none, or bad
		, NO_TOKEN = NUM_TOKENS
	};

	enum ArgType_e
	{
		  TYPE_ADDRESS  = (1 << 0) // $#### or $symbolname
		, TYPE_OPERATOR = (1 << 1)
		, TYPE_QUOTED_1 = (1 << 2)
		, TYPE_QUOTED_2 = (1 << 3) // "..."
		, TYPE_STRING   = (1 << 4) // LOAD
		, TYPE_RANGE    = (1 << 5)
		, TYPE_LENGTH   = (1 << 6)
		, TYPE_VALUE    = (1 << 7)
		, TYPE_NO_REG   = (1 << 8) // Don't do register value -> Argument.nValue
		, TYPE_NO_SYM   = (1 << 9) // Don't do symbol lookup  -> Argument.nValue
	};

	struct TokenTable_t
	{
		ArgToken_e eToken;
		ArgType_e  eType ;
		char       sToken[4];
	};

	struct Arg_t
	{	
		char       sArg[ MAX_ARG_LEN+1 ]; // Array chars comes first, for alignment, GH#481 echo 55 char limit
		int        nArgLen; // Needed for TextSearch "ABC\x00"
		WORD       nValue ; // 2
//		WORD       nVal1  ; // 2
//		WORD       nVal2  ; // 2 If we have a Len (,)
		// Enums and Bools should come last for alignment
		ArgToken_e eToken ; // 1/2/4
		int        bType  ; // 1/2/4 // Flags of ArgType_e
		DEVICE_e   eDevice; // 1/2/4
		bool       bSymbol; // 1
	};

	// NOTE: Order MUST match g_aParameters[] !!!
	enum Parameters_e
	{
		// Note: Order must match Breakpoint_Operator_t
		// Note: Order must match _PARAM_BREAKPOINT_*
		// Note: Order must match g_aBreakpointSymbols
	  _PARAM_BREAKPOINT_BEGIN
		, PARAM_BP_LESS_EQUAL = _PARAM_BREAKPOINT_BEGIN   // <=
		, PARAM_BP_LESS_THAN     // <
		, PARAM_BP_EQUAL         // =
		, PARAM_BP_NOT_EQUAL     // !=
		, PARAM_BP_NOT_EQUAL_1   // !
		, PARAM_BP_GREATER_THAN  // >
		, PARAM_BP_GREATER_EQUAL // >=
		, PARAM_BP_READ          // R
		,_PARAM_BP_READ          // ? alias READ
		, PARAM_BP_WRITE         // W
		,_PARAM_BP_WRITE         // @ alias write
		, PARAM_BP_READ_WRITE    // * alias READ WRITE
	, _PARAM_BREAKPOINT_END
	,  PARAM_BREAKPOINT_NUM = _PARAM_BREAKPOINT_END - _PARAM_BREAKPOINT_BEGIN

//		, PARAM_SIZE  // TODO: used by FONT SIZE

	// Note: Order must match BreakpointSource_t
	, _PARAM_REGS_BEGIN = _PARAM_BREAKPOINT_END // Daisy Chain
// Regs
		, PARAM_REG_A = _PARAM_REGS_BEGIN
		, PARAM_REG_X
		, PARAM_REG_Y
		, PARAM_REG_PC // Program Counter
		, PARAM_REG_SP // Stack Pointer
// Flags
		, PARAM_FLAGS  // Processor Status
		, PARAM_FLAG_C // Carry
		, PARAM_FLAG_Z // Zero
		, PARAM_FLAG_I // Interrupt
		, PARAM_FLAG_D // Decimal
		, PARAM_FLAG_B // Break
		, PARAM_FLAG_R // Reserved
		, PARAM_FLAG_V // Overflow
		, PARAM_FLAG_N // Sign
	, _PARAM_REGS_END
	,  PARAM_REGS_NUM = _PARAM_REGS_END - _PARAM_REGS_BEGIN

// Disasm
	, _PARAM_CONFIG_BEGIN = _PARAM_REGS_END // Daisy Chain
		, PARAM_CONFIG_BRANCH = _PARAM_CONFIG_BEGIN // g_iConfigDisasmBranchType   [0|1|2]
		, PARAM_CONFIG_CLICK   // g_bConfigDisasmClick        [0..7] // GH#462
		, PARAM_CONFIG_COLON   // g_bConfigDisasmAddressColon [0|1]
		, PARAM_CONFIG_OPCODE  // g_bConfigDisasmOpcodesView  [0|1]
		, PARAM_CONFIG_POINTER // g_bConfigInfoTargetPointer  [0|1]
		, PARAM_CONFIG_SPACES  // g_bConfigDisasmOpcodeSpaces [0|1]
		, PARAM_CONFIG_TARGET  // g_iConfigDisasmTargets      [0|1|2]
	, _PARAM_CONFIG_END
	, PARAM_CONFIG_NUM = _PARAM_CONFIG_END - _PARAM_CONFIG_BEGIN

// Disk
	, _PARAM_DISK_BEGIN = _PARAM_CONFIG_END // Daisy Chain
		, PARAM_DISK_EJECT = _PARAM_DISK_BEGIN // DISK 1 EJECT
		, PARAM_DISK_INFO                      // DISK 1 INFO
		, PARAM_DISK_PROTECT                   // DISK 1 PROTECT
		, PARAM_DISK_READ                      // DISK 1 READ Track Sector NumSectors MemAddress
	, _PARAM_DISK_END
	,  PARAM_DISK_NUM = _PARAM_DISK_END - _PARAM_DISK_BEGIN

	, _PARAM_FONT_BEGIN = _PARAM_DISK_END // Daisy Chain
		, PARAM_FONT_MODE = _PARAM_FONT_BEGIN
	, _PARAM_FONT_END
	,  PARAM_FONT_NUM = _PARAM_FONT_END - _PARAM_FONT_BEGIN

	, _PARAM_GENERAL_BEGIN = _PARAM_FONT_END // Daisy Chain
		, PARAM_FIND = _PARAM_GENERAL_BEGIN
		, PARAM_BRANCH
		, PARAM_CATEGORY
		, PARAM_CLEAR
		, PARAM_LOAD
		, PARAM_LIST
		, PARAM_OFF
		, PARAM_ON
		, PARAM_RESET
		, PARAM_SAVE
		, PARAM_START
		, PARAM_STOP
	, _PARAM_GENERAL_END
	,  PARAM_GENERAL_NUM = _PARAM_GENERAL_END - _PARAM_GENERAL_BEGIN

	, _PARAM_HELPCATEGORIES_BEGIN = _PARAM_GENERAL_END // Daisy Chain
		, PARAM_WILDSTAR = _PARAM_HELPCATEGORIES_BEGIN
		, PARAM_CAT_BOOKMARKS   
		, PARAM_CAT_BREAKPOINTS 
		, PARAM_CAT_CONFIG      
		, PARAM_CAT_CPU         
//		, PARAM_CAT_EXPRESSION  
		, PARAM_CAT_FLAGS       
		, PARAM_CAT_HELP        
		, PARAM_CAT_KEYBOARD    
		, PARAM_CAT_MEMORY      
		, PARAM_CAT_OUTPUT      
		, PARAM_CAT_OPERATORS   
		, PARAM_CAT_RANGE       
//		, PARAM_CAT_REGISTERS   
		, PARAM_CAT_SYMBOLS     
		, PARAM_CAT_VIEW
		, PARAM_CAT_WATCHES     
		, PARAM_CAT_WINDOW      
		, PARAM_CAT_ZEROPAGE    
	, _PARAM_HELPCATEGORIES_END
	,  PARAM_HELPCATEGORIES_NUM = _PARAM_HELPCATEGORIES_END - _PARAM_HELPCATEGORIES_BEGIN

	, _PARAM_MEM_SEARCH_BEGIN = _PARAM_HELPCATEGORIES_END  // Daisy Chain
		, PARAM_MEM_SEARCH_WILD = _PARAM_MEM_SEARCH_BEGIN
//		, PARAM_MEM_SEARCH_BYTE
	, _PARAM_MEM_SEARCH_END
	,  PARAM_MEM_SEARCH_NUM = _PARAM_MEM_SEARCH_END - _PARAM_MEM_SEARCH_BEGIN

	, _PARAM_SOURCE_BEGIN = _PARAM_MEM_SEARCH_END  // Daisy Chain
		, PARAM_SRC_MEMORY = _PARAM_SOURCE_BEGIN
		,_PARAM_SRC_MEMORY  // alias MEM = MEMORY
		, PARAM_SRC_SYMBOLS
		,_PARAM_SRC_SYMBOLS // alias SYM = SYMBOLS
		, PARAM_SRC_MERLIN
		, PARAM_SRC_ORCA
	, _PARAM_SOURCE_END
	,  PARAM_SOURCE_NUM = _PARAM_SOURCE_END - _PARAM_SOURCE_BEGIN

	, _PARAM_WINDOW_BEGIN = _PARAM_SOURCE_END  // Daisy Chain
		// These are the "full screen" "windows" / Panels / Tab sheets
		, PARAM_CODE = _PARAM_WINDOW_BEGIN // disasm
//		, PARAM_CODE_1  // disasm top // removed, since can't set top window for code/data
		, PARAM_CODE_2  // disasm bot 
		, PARAM_CONSOLE
		, PARAM_DATA    // data all
//		, PARAM_DATA_1  // data top // removed, since can't set top window for code/data
		, PARAM_DATA_2  // data bot
		, PARAM_DISASM
		, PARAM_INFO    // Togle INFO on/off
		, PARAM_SOURCE
		, _PARAM_SRC     // alias SRC = SOURCE
//		, PARAM_SOURCE_1 // source top // removed, since can't set top window for code/data
		, PARAM_SOURCE_2 // source bot
		, PARAM_SYMBOLS
		, _PARAM_SYM     // alias SYM = SYMBOLS
//		, PARAM_SYMBOL_1 // symbols top // removed, since can't set top window for code/data
		, PARAM_SYMBOL_2 // symbols bot
	, _PARAM_WINDOW_END
	,  PARAM_WINDOW_NUM = _PARAM_WINDOW_END - _PARAM_WINDOW_BEGIN

		, NUM_PARAMS = _PARAM_WINDOW_END // Daisy Chain

// Aliases (actuall names)
//		,PARAM_DISASM = PARAM_CODE_1
//		, PARAM_BREAKPOINT_READ  = PARAM_READ
//		, PARAM_BREAKPOINT_WRITE = PARAM_WRITE
	};
	

// Source Level Debugging _________________________________________________________________________

	enum
	{
		NO_SOURCE_LINE = -1
	};

	typedef std::map<WORD, int> SourceAssembly_t; // Address -> Line #  &  FileName


// Symbols ________________________________________________________________________________________

	// ****************************************
	// Note: This is the index (enumeration) to select which table
	// See: g_aSymbols[]
	// ****************************************
	enum SymbolTable_Index_e // Symbols_e -> SymbolTable_Index_e
	{
		SYMBOLS_MAIN,
		SYMBOLS_APPLESOFT,
		SYMBOLS_ASSEMBLY,
		SYMBOLS_USER_1,
		SYMBOLS_USER_2,
		SYMBOLS_SRC_1,
		SYMBOLS_SRC_2,
		SYMBOLS_DOS33,
		SYMBOLS_PRODOS,
		NUM_SYMBOL_TABLES
	};

	// ****************************************
	// Note: This is the bit-flags to select which table.
	// See: CmdSymbolsListTable(), g_bDisplaySymbolTables
	// ****************************************
	enum SymbolTable_Masks_e // SymbolTable_e -> 
	{
		SYMBOL_TABLE_MAIN      = (1 << 0),
		SYMBOL_TABLE_APPLESOFT = (1 << 1),
		SYMBOL_TABLE_ASSEMBLY  = (1 << 2),
		SYMBOL_TABLE_USER_1    = (1 << 3),
		SYMBOL_TABLE_USER_2    = (1 << 4),
		SYMBOL_TABLE_SRC_1     = (1 << 5),
		SYMBOL_TABLE_SRC_2     = (1 << 6),
		SYMBOL_TABLE_DOS33     = (1 << 7),
		SYMBOL_TABLE_PRODOS    = (1 << 8),
	};

	typedef std::map<WORD, std::string> SymbolTable_t;


// Watches ________________________________________________________________________________________

	enum
	{
		MAX_WATCHES = 6
	};


// Window _________________________________________________________________________________________

	enum Window_e
	{
		WINDOW_NULL    , // no window data!
		WINDOW_CONSOLE ,
		WINDOW_CODE    ,
		WINDOW_DATA    , // memory view

		WINDOW_INFO    , // WINDOW_REGS, WINDOW_STACK, WINDOW_BREAKPOINTS, WINDOW_WATCHES, WINDOW,

		NUM_WINDOWS    ,
// Not implemented yet
		WINDOW_REGS    ,
		WINDOW_STACK   ,
		WINDOW_SYMBOLS ,
		WINDOW_TARGETS ,

		WINDOW_IO      , // soft switches   $addr  name   state
		WINDOW_ZEROPAGE,
		WINDOW_SOURCE  ,
		WINDOW_OUTPUT  ,


	};

	enum WindowSplit_e
	{
		WIN_SPLIT_HORZ = (1 << 0),
		WIN_SPLIT_VERT = (1 << 1),
		WIN_SPLIT_PARENT_TOP = (1 << 2),
	};
	
	struct WindowSplit_t
	{
		RECT tBoundingBox; // 

		int  nWidth ; // Width & Height are always valid 
		int  nHeight; // If window is split/join, then auto-updated (right,bottom)

		int  nCursorY; // Address
		int  nCursorX; // or line,col of text file ...

		int  bSplit ; 

		int  iParent;// index into g_aWindowConfig
		int  iChild ; // index into g_aWindowConfig

		Window_e eTop;
		Window_e eBot;	
	};

// Debuger_PanelInit();
// WindowsRemoveAll()
// int iNext = 0;
// iNext = Panel_Add( iNext, WINDOW_CONSOLE );
// iNext = Panel_AutoSplit( iNext, WINDOW_CODE );
// iNext = Panel_AutoSplit( iNext, WINDOW_REGS );


	// g_bWindowDisplayShowChild = false;
	// g_bWindowDisplayShowRoot  = WINDOW_CODE;


// Zero Page ______________________________________________________________________________________

	enum
	{
		MAX_ZEROPAGE_POINTERS = 8
	};

