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

		, NUM_ADDRESSING_MODES
		, NUM_OPMODES = NUM_ADDRESSING_MODES
		, AM_I = NUM_ADDRESSING_MODES, // for assemler

	// Deprecated
	/*
		ADDR_INVALID1  =  1,
		ADDR_INVALID2  =  2,
		ADDR_INVALID3  =  3,
		ADDR_IMM       =  4, // Immediate
		ADDR_ABS       =  5, // Absolute
		ADDR_ZP        =  6, // Zeropage
		ADDR_ABSX      =  7, // Absolute + X
		ADDR_ABSY      =  8, // Absolute + Y
		ADDR_ZP_X      =  9, // Zeropage + X
		ADDR_ZP_Y      = 10, // Zeropage + Y
		ADDR_REL       = 11, // Relative
		ADDR_INDX      = 12, // Indexed (Zeropage) Indirect
		ADDR_ABSIINDX  = 13, // Indexed (Absolute) Indirect
		ADDR_INDY      = 14, // Indirect (Zeropage) Indexed
		ADDR_IZPG      = 15, // Indirect (Zeropage)
		ADDR_IABS      = 16, // Indirect Absolute (i.e. JMP)
	*/
	};


// Assembler ______________________________________________________________________________________

	enum Prompt_e
	{
		PROMPT_COMMAND,
		PROMPT_ASSEMBLER,
		NUM_PROMPTS
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
	// NOTE: Order must match Breakpoint_Source_t
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
		BP_SRC_MEM_1 ,

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
		WORD                 nLength ;
		BreakpointSource_t   eSource;
		BreakpointOperator_t eOperator;
		bool                 bSet    ; // used to be called enabled pre 2.0
		bool                 bEnabled;
	};

	typedef Breakpoint_t Bookmark_t;
	typedef Breakpoint_t Watches_t;
	typedef Breakpoint_t ZeroPagePointers_t;


// Colors ___________________________________________________________________

	enum Color_Schemes_e
	{
		SCHEME_COLOR, // NOTE: MUST match order in CMD_WINDOW_COLOR
		SCHEME_MONO , // NOTE: MUST match order in CMD_WINDOW_MONOCHROME
		SCHEME_BW   , // NOTE: MUST match order in CMD_WINDOW_BW
//		SCHEME_CUSTOM
		NUM_COLOR_SCHEMES
	};

	// Named, since they are easier to remember.
	// Ok, maybe RGB + CYMK is a little "too" cute. But what the hell, it works out nicely.
 	enum DebugPalette_e
	{   
		// mipmap level:   8   7   6   5   4   3   2   1   0
		// color depth:  256 224 192 160 128  96  64  32   0
		//               +32 +32 +32 +32 +32 +32 +32 +32 
		// NOTE: Levels of black are redundant.
		//                              // BGR
		K0,                             // ---  K  
		R8, R7, R6, R5, R4, R3, R2, R1, // --1  R  Red     
		G8, G7, G6, G5, G4, G3, G2, G1, // -1-  G  Green   
		Y8, Y7, Y6, Y5, Y4, Y3, Y2, Y1, // -11  Y  Yellow  
		B8, B7, B6, B5, B4, B3, B2, B1, // 1--  B  Blue    
		M8, M7, M6, M5, M4, M3, M2, M1, // 1-1  M  Magenta 
		C8, C7, C6, C5, C4, C3, C2, C1, // 11-  C  Cyan    
		W8, W7, W6, W5, W4, W3, W2, W1, // 111  W  White / Gray / Black

		COLOR_CUSTOM_01, COLOR_CUSTOM_02, COLOR_CUSTOM_03, COLOR_CUSTOM_04,
		COLOR_CUSTOM_05, COLOR_CUSTOM_06, COLOR_CUSTOM_07, COLOR_CUSTOM_08,
		COLOR_CUSTOM_09, COLOR_CUSTOM_11, CUSTOM_COLOR_11, COLOR_CUSTOM_12,
		COLOR_CUSTOM_13, COLOR_CUSTOM_14, COLOR_CUSTOM_15, COLOR_CUSTOM_16,

		NUM_PALETTE,

		// Gray Aliases		
		G000 = K0,
		G032 = W1,
		G064 = W2,
		G096 = W3,
		G128 = W4,
		G160 = W5,
		G192 = W6,
		G224 = W7,
		G256 = W8
	};

	// Yeah, this was a PITA to organize.
	enum DebugColors_e
	{
		  BG_CONSOLE_OUTPUT  // Black   Window
		, FG_CONSOLE_OUTPUT  // White
		, BG_CONSOLE_INPUT   // Black   Window
		, FG_CONSOLE_INPUT   // Light Blue

		, BG_DISASM_1        // Blue*   Odd address
		, BG_DISASM_2        // Blue*   Even address

		, BG_DISASM_BP_S_C   // Red     Breakpoint Set     (cursor)
		, FG_DISASM_BP_S_C   // White   Breakpoint Set&Ena (cursor)

		// Note: redundant BG_DISASM_BP_0_C = BG_DISASM_BP_S_C
		, BG_DISASM_BP_0_C   // DimRed  Breakpoint Disabled (cursor)
		, FG_DISASM_BP_0_C   // Gray192 Breakpoint Disabled (cursor)

		, FG_DISASM_BP_S_X   // Red     Set      (not cursor)
		, FG_DISASM_BP_0_X   // White   Disabled (not cursor)

		, BG_DISASM_C        // White (Cursor)
		, FG_DISASM_C        // Blue  (Cursor)

		, BG_DISASM_PC_C     // Yellow (not cursor)
		, FG_DISASM_PC_C     // White  (not cursor)

		, BG_DISASM_PC_X     // Dim Yellow (not cursor)
		, FG_DISASM_PC_X     // White      (not cursor)

		, BG_DISASM_BOOKMARK // Lite Blue    (always)
		, FG_DISASM_BOOKMARK // White   addr (always)

		, FG_DISASM_ADDRESS  // White   addr
		, FG_DISASM_OPERATOR // Gray192     :               $ (also around instruction addressing g_nAppMode)
		, FG_DISASM_OPCODE   // Yellow       xx xx xx
		, FG_DISASM_MNEMONIC // White                   LDA
		, FG_DISASM_TARGET   // Orange                       FAC8
		, FG_DISASM_SYMBOL   // Purple                       HOME
		, FG_DISASM_CHAR     // Cyan                               'c'
		, FG_DISASM_BRANCH   // Green                                   ^ = v

		, BG_INFO            // Cyan    Regs/Stack/BP/Watch/ZP
		, FG_INFO_TITLE      // White   Regs/Stack/BP/Watch/ZP
		, FG_INFO_BULLET     //         1
		, FG_INFO_OPERATOR   // Gray192  :    -
		, FG_INFO_ADDRESS    // Orange    FA62 FA63 (Yellow -> Orange)
		, FG_INFO_OPCODE     // Yellow              xx
		, FG_INFO_REG        // Orange (Breakpoints)
		, BG_INFO_INVERSE    // White
		, FG_INFO_INVERSE    // Cyan
		, BG_INFO_CHAR       // mid Cyan
		, FG_INFO_CHAR_HI    // White
		, FG_INFO_CHAR_LO    // Yellow
		
		, BG_INFO_IO_BYTE    // Orange (high bit)
		, FG_INFO_IO_BYTE    // Orange (non-high bit)
		                               
		, BG_DATA_1          // Cyan*   Window
		, BG_DATA_2          // Cyan*
		, FG_DATA_BYTE       // default same as FG_DISASM_OPCODE
		, FG_DATA_TEXT       // default same as FG_DISASM_NMEMONIC

		, BG_SYMBOLS_1       // window
		, BG_SYMBOLS_2        
		, FG_SYMBOLS_ADDRESS // default same as FG_DISASM_ADDRESS
		, FG_SYMBOLS_NAME    // default same as FG_DISASM_SYMBOL

		, BG_SOURCE_TITLE
		, FG_SOURCE_TITLE
		, BG_SOURCE_1        // odd
		, BG_SOURCE_2        // even
		, FG_SOURCE

		, NUM_DEBUG_COLORS
	};

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

		UPDATE_ALL = -1
	};

	typedef int Update_t;

	enum
	{
		MAX_COMMAND_LEN = 12,

		MAX_ARGS        = 32, // was 40
		ARG_SYNTAX_ERROR= -1,
		MAX_ARG_LEN     = 56, // was 12, extended to allow font names
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
// CPU
		  CMD_CURSOR_JUMP_PC // Shift
		, CMD_CURSOR_SET_PC  // Ctrl
		, CMD_ASSEMBLE
		, CMD_BREAK_INVALID
		, CMD_BREAK_OPCODE
		, CMD_GO
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
		, CMD_BREAKPOINT
		, CMD_BREAKPOINT_ADD_SMART // smart breakpoint
		, CMD_BREAKPOINT_ADD_REG   // break on: PC == Address (fetch/execute)
		, CMD_BREAKPOINT_ADD_PC    // alias BPX = BA
//		,	CMD_BREAKPOINT_SET  = CMD_BREAKPOINT_ADD_ADDR // alias
//		,	CMD_BREAKPOINT_EXEC = CMD_BREAKPOINT_ADD_ADDR // alias
		, CMD_BREAKPOINT_ADD_IO  // break on: [$C000-$C7FF] Load/Store 
		, CMD_BREAKPOINT_ADD_MEM // break on: [$0000-$FFFF], excluding IO

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

		, _CMD_MEM_MINI_DUMP_HEX_1_1 // alias MD
		, _CMD_MEM_MINI_DUMP_HEX_1_2 // alias MD = D
		, CMD_MEM_MINI_DUMP_HEX_1
		, CMD_MEM_MINI_DUMP_HEX_2  
		, _CMD_MEM_MINI_DUMP_HEX_1_3 // alias M1
		, _CMD_MEM_MINI_DUMP_HEX_2_1 // alias M2

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
		, CMD_SYMBOLS_MAIN
		, CMD_SYMBOLS_USER
		, CMD_SYMBOLS_SRC
//		, CMD_SYMBOLS_FIND
//		, CMD_SYMBOLS_CLEAR
		, CMD_SYMBOLS_INFO
		, CMD_SYMBOLS_LIST
//		, CMD_SYMBOLS_LOAD_1
//		, CMD_SYMBOLS_LOAD_2
//		, CMD_SYMBOLS_SAVE
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
		, CMD_WATCH
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
	};


// CPU
	Update_t CmdCursorJumpPC(int nArgs);
	Update_t CmdCursorSetPC (int nArgs);
	Update_t CmdAssemble    (int nArgs);
	Update_t CmdBreakInvalid(int nArgs); // Breakpoint IFF Full-speed!
	Update_t CmdBreakOpcode (int nArgs); // Breakpoint IFF Full-speed!
	Update_t CmdGo          (int nArgs);
	Update_t CmdIn          (int nArgs);
	Update_t CmdKey         (int nArgs);
	Update_t CmdJSR         (int nArgs);
	Update_t CmdNOP         (int nArgs);
	Update_t CmdOut         (int nArgs);
	Update_t CmdStepOver    (int nArgs);
	Update_t CmdStepOut     (int nArgs);
	Update_t CmdTrace       (int nArgs);  // alias for CmdStepIn
	Update_t CmdTraceFile   (int nArgs);
	Update_t CmdTraceLine   (int nArgs);
	Update_t CmdUnassemble  (int nArgs); // code dump, aka, Unassemble
// Bookmarks
	Update_t CmdBookmark       (int nArgs);
	Update_t CmdBookmarkAdd    (int nArgs);
	Update_t CmdBookmarkClear  (int nArgs);
	Update_t CmdBookmarkList   (int nArgs);
	Update_t CmdBookmarkGoto   (int nArgs);
//	Update_t CmdBookmarkLoad   (int nArgs);
	Update_t CmdBookmarkSave   (int nArgs);
// Breakpoints
	Update_t CmdBreakpoint        (int nArgs);
	Update_t CmdBreakpointAddSmart(int nArgs);
	Update_t CmdBreakpointAddReg  (int nArgs);
	Update_t CmdBreakpointAddPC   (int nArgs);
	Update_t CmdBreakpointAddIO   (int nArgs);
	Update_t CmdBreakpointAddMem  (int nArgs);
	Update_t CmdBreakpointClear   (int nArgs);
	Update_t CmdBreakpointDisable (int nArgs);
	Update_t CmdBreakpointEdit    (int nArgs);
	Update_t CmdBreakpointEnable  (int nArgs);
	Update_t CmdBreakpointList    (int nArgs);
//	Update_t CmdBreakpointLoad    (int nArgs);
	Update_t CmdBreakpointSave    (int nArgs);
// Benchmark
	Update_t CmdBenchmark      (int nArgs);
	Update_t CmdBenchmarkStart (int nArgs); //Update_t CmdSetupBenchmark (int nArgs);
	Update_t CmdBenchmarkStop  (int nArgs); //Update_t CmdExtBenchmark (int nArgs);
	Update_t CmdProfile        (int nArgs);
	Update_t CmdProfileStart   (int nArgs);
	Update_t CmdProfileStop    (int nArgs);
// Config
//	Update_t CmdConfigMenu        (int nArgs);
//	Update_t CmdConfigBase        (int nArgs);
//	Update_t CmdConfigBaseHex     (int nArgs);
//	Update_t CmdConfigBaseDec     (int nArgs);
	Update_t CmdConfigColorMono   (int nArgs);
	Update_t CmdConfigDisasm      (int nArgs);
	Update_t CmdConfigFont        (int nArgs);
	Update_t CmdConfigHColor      (int nArgs);
	Update_t CmdConfigLoad        (int nArgs);
	Update_t CmdConfigSave        (int nArgs);
	Update_t CmdConfigSetFont     (int nArgs);
	Update_t CmdConfigGetFont     (int nArgs);
// Cursor
	Update_t CmdCursorFollowTarget(int nArgs);
	Update_t CmdCursorLineDown    (int nArgs);
	Update_t CmdCursorLineUp      (int nArgs);
	Update_t CmdCursorJumpRetAddr (int nArgs);
	Update_t CmdCursorRunUntil    (int nArgs);
	Update_t CmdCursorPageDown    (int nArgs);
	Update_t CmdCursorPageDown256 (int nArgs);
	Update_t CmdCursorPageDown4K  (int nArgs);
	Update_t CmdCursorPageUp      (int nArgs);
	Update_t CmdCursorPageUp256   (int nArgs);
	Update_t CmdCursorPageUp4K    (int nArgs);
// Disk
	Update_t CmdDisk              (int nArgs);
// Help
	Update_t CmdHelpList          (int nArgs);
	Update_t CmdHelpSpecific      (int Argss);
	Update_t CmdVersion           (int nArgs);
	Update_t CmdMOTD              (int nArgs);
// Flags
	Update_t CmdFlag      (int nArgs);
	Update_t CmdFlagClear (int nArgs);
	Update_t CmdFlagSet   (int nArgs);
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
	Update_t CmdMemoryLoad         (int nArgs);
	Update_t CmdMemoryMove         (int nArgs);
	Update_t CmdMemorySave         (int nArgs);
	Update_t CmdMemorySearch       (int nArgs);
	Update_t _SearchMemoryDisplay  (int nArgs=0);
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
	Update_t CmdRegisterSet     (int nArgs);
// Source Level Debugging
	Update_t CmdSource          (int nArgs);
	Update_t CmdSync            (int nArgs);
// Stack
	Update_t CmdStackPush       (int nArgs);
	Update_t CmdStackPop        (int nArgs);
	Update_t CmdStackPopPseudo  (int nArgs);
	Update_t CmdStackReturn     (int nArgs);
// Symbols
	Update_t CmdSymbols         (int nArgs);
	Update_t CmdSymbolsClear    (int nArgs);
	Update_t CmdSymbolsList     (int nArgs);
	Update_t CmdSymbolsLoad     (int nArgs);
	Update_t CmdSymbolsInfo     (int nArgs);
	Update_t CmdSymbolsMain     (int nArgs);
	Update_t CmdSymbolsUser     (int nArgs);
	Update_t CmdSymbolsSave     (int nArgs);
	Update_t CmdSymbolsSource   (int nArgs);
// View
	Update_t CmdViewOutput_Text4X (int nArgs);
	Update_t CmdViewOutput_Text41 (int nArgs);
	Update_t CmdViewOutput_Text42 (int nArgs);
	Update_t CmdViewOutput_Text8X (int nArgs);
	Update_t CmdViewOutput_Text81 (int nArgs);
	Update_t CmdViewOutput_Text82 (int nArgs);

	Update_t CmdViewOutput_GRX (int nArgs);
	Update_t CmdViewOutput_GR1 (int nArgs);
	Update_t CmdViewOutput_GR2 (int nArgs);
	Update_t CmdViewOutput_DGRX (int nArgs);
	Update_t CmdViewOutput_DGR1 (int nArgs);
	Update_t CmdViewOutput_DGR2 (int nArgs);

	Update_t CmdViewOutput_HGRX (int nArgs);
	Update_t CmdViewOutput_HGR1 (int nArgs);
	Update_t CmdViewOutput_HGR2 (int nArgs);
	Update_t CmdViewOutput_DHGRX (int nArgs);
	Update_t CmdViewOutput_DHGR1 (int nArgs);
	Update_t CmdViewOutput_DHGR2 (int nArgs);
// Watch
	Update_t CmdWatch        (int nArgs);
	Update_t CmdWatchAdd     (int nArgs);
	Update_t CmdWatchClear   (int nArgs);
	Update_t CmdWatchDisable (int nArgs);
	Update_t CmdWatchEnable  (int nArgs);
	Update_t CmdWatchList    (int nArgs);
//	Update_t CmdWatchLoad    (int nArgs);
	Update_t CmdWatchSave    (int nArgs);
// Window
	Update_t CmdWindow            (int nArgs);
	Update_t CmdWindowCycleNext   (int nArgs);
	Update_t CmdWindowCyclePrev   (int nArgs);
	Update_t CmdWindowLast        (int nArgs);

	Update_t CmdWindowShowCode    (int nArgs);
	Update_t CmdWindowShowCode1   (int nArgs);
	Update_t CmdWindowShowCode2   (int nArgs);
	Update_t CmdWindowShowData    (int nArgs);
	Update_t CmdWindowShowData1   (int nArgs);
	Update_t CmdWindowShowData2   (int nArgs);
	Update_t CmdWindowShowSymbols1(int nArgs);
	Update_t CmdWindowShowSymbols2(int nArgs);
	Update_t CmdWindowShowSource  (int nArgs);
	Update_t CmdWindowShowSource1 (int nArgs);
	Update_t CmdWindowShowSource2 (int nArgs);

	Update_t CmdWindowViewCode    (int nArgs);
	Update_t CmdWindowViewConsole (int nArgs);
	Update_t CmdWindowViewData    (int nArgs);
	Update_t CmdWindowViewOutput  (int nArgs);
	Update_t CmdWindowViewSource  (int nArgs);
	Update_t CmdWindowViewSymbols (int nArgs);

	Update_t CmdWindowWidthToggle (int nArgs);

//	Update_t CmdZeroPageShow      (int nArgs);
//	Update_t CmdZeroPageHide      (int nArgs);
//	Update_t CmdZeroPageToggle    (int nArgs);

// ZeroPage
	Update_t CmdZeroPage        (int nArgs);
	Update_t CmdZeroPageAdd     (int nArgs);
	Update_t CmdZeroPageClear   (int nArgs);
	Update_t CmdZeroPageDisable (int nArgs);
	Update_t CmdZeroPageEnable  (int nArgs);
	Update_t CmdZeroPageList    (int nArgs);
//	Update_t CmdZeroPageLoad    (int nArgs);
	Update_t CmdZeroPageSave    (int nArgs);
	Update_t CmdZeroPagePointer (int nArgs);


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

	enum DisasmText_e
	{
		nMaxAddressLen    = 40,
		nMaxOpcodes       =  3,
		CHARS_FOR_ADDRESS =  8, // 4 digits plus null
	};

	struct DisasmLine_t
	{
		int iOpcode;
		int iOpmode;
		int nOpbyte;

		char sAddress  [ CHARS_FOR_ADDRESS ];
		char sOpCodes  [(nMaxOpcodes*3)+1];

		int  nTarget;
		char sTarget   [nMaxAddressLen];

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
			sAddress  [ 0 ] = 0;
			sOpCodes  [ 0 ] = 0;

			nTarget = 0;
			sTarget   [ 0 ] = 0;

			sTargetOffset[ 0 ] = 0;
			nTargetOffset = 0;

			sTargetPointer[ 0 ] = 0;
			sTargetValue  [ 0 ] = 0;

			sImmediate[ 0 ] = 0;
			nImmediate = 0;

			sBranch   [ 0 ] = 0;

			bTargetImmediate = false;
			bTargetIndexed   = false;
			bTargetIndirect  = false;
//			bTargetInside    = false;
//			bTargetOutside   = false;
			bTargetRelative  = false;
			bTargetX         = false;
			bTargetY         = false; // need to dislay ",Y"
			bTargetValue     = false;
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

		OPCODE_JSR     = 0x20,
		OPCODE_JMP_A   = 0x4C, // Absolute
		OPCODE_JMP_NA  = 0x6C, // Indirect Absolute
		OPCODE_JMP_IAX = 0x7C, // Indexed (Absolute Indirect, X)
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
	extern const unsigned int _6502_STACK_END       ;//= 0x01FF;
	extern const unsigned int _6502_IO_BEGIN        ;//= 0xC000;
	extern const unsigned int _6502_IO_END          ;//= 0xC0FF;
	extern const unsigned int _6502_MEM_BEGIN       ;//= 0x0000;
	extern const unsigned int _6502_MEM_END         ;//= 0xFFFF;


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

	typedef vector<MemorySearch_t> MemorySearchValues_t;
	typedef vector<int>            MemorySearchResults_t;

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
		char       sArg[ MAX_ARG_LEN ]; // Array chars comes first, for alignment
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

	// Note: Order must match Breakpoint_Source_t
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

	typedef map<WORD, int> SourceAssembly_t; // Address -> Line #  &  FileName


// Symbols ________________________________________________________________________________________

	enum
	{
		// raised from 13 to 31 for Contiki
		MAX_SYMBOLS_LEN = 31
	};

	// ****************************************
	// WARNING: This is the simple enumeration.
	// See: g_aSymbols[]
	// ****************************************
	enum Symbols_e
	{
		SYMBOLS_MAIN,
		SYMBOLS_USER,
		SYMBOLS_SRC ,
		NUM_SYMBOL_TABLES = 3
	};

	// ****************************************
	// WARNING: This is the bit-flags to select which table.
	// See: CmdSymbolsListTable()
	// ****************************************
	enum SymbolTable_e
	{
		SYMBOL_TABLE_MAIN = (1 << 0),
		SYMBOL_TABLE_USER = (1 << 1),
		SYMBOL_TABLE_SRC  = (1 << 2),
	};

	typedef map<WORD, string> SymbolTable_t;


// Watches ________________________________________________________________________________________

	enum
	{
		MAX_WATCHES = 16
	};


// Window _________________________________________________________________________________________

	enum Window_e
	{
		WINDOW_CODE    ,
		WINDOW_DATA    ,
		WINDOW_CONSOLE ,
		NUM_WINDOWS    ,
// Not implemented yet
		WINDOW_IO      , // soft switches   $addr  name   state
		WINDOW_SYMBOLS ,
		WINDOW_ZEROPAGE,
		WINDOW_SOURCE  ,
	};
	
	struct WindowSplit_t
	{
		bool     bSplit;
		Window_e eTop;
		Window_e eBot;	
		// TODO: nTopHeight
		// TODO: nBotHeight
	};


// Zero Page ______________________________________________________________________________________

	enum
	{
		MAX_ZEROPAGE_POINTERS = 8
	};

