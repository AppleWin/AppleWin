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
// 
	enum DebugColors_e
	{
		  BG_CONSOLE_OUTPUT  // Black   Window
		, FG_CONSOLE_OUTPUT  // White
		, BG_CONSOLE_INPUT   // Black   Window
		, FG_CONSOLE_INPUT   // Light Blue

		, BG_DISASM_1        // Blue*   Odd address
		, BG_DISASM_2        // Blue*   Even address
// BreakPoint {Set}  {Cursor/Non-Cursor}
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
/*ZZZ*/	, FG_DISASM_DIRECTIVE// Purple                  db 
		, FG_DISASM_TARGET   // Orange                       FAC8
/*ZZZ*/	, FG_DISASM_SYMBOL   // Green                        HOME
		, FG_DISASM_CHAR     // Cyan                               'c'
		, FG_DISASM_BRANCH   // Green                                   ^ = v
		, FG_DISASM_SINT8    // Lite Blue

		, BG_INFO            // Cyan    Regs/Stack/BP/Watch/ZP
		, BG_INFO_WATCH      // Cyan
		, BG_INFO_ZEROPAGE   // Cyan
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

		, BG_VIDEOSCANNER_TITLE
		, FG_VIDEOSCANNER_TITLE
		, FG_VIDEOSCANNER_INVISIBLE	// yellow
		, FG_VIDEOSCANNER_VISIBLE	// green

		, BG_IRQ_TITLE
		, FG_IRQ_TITLE		// red

		, NUM_DEBUG_COLORS
	};

	extern int g_iColorScheme;
	extern COLORREF g_aColorPalette[ NUM_PALETTE ];
	extern int g_aColorIndex[ NUM_DEBUG_COLORS ];

// Color
	COLORREF DebuggerGetColor( int iColor );
	bool DebuggerSetColor ( const int iScheme, const int iColor, const COLORREF nColor );
	void ConfigColorsReset(void);
