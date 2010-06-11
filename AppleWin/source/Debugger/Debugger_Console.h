#ifndef DEBUGGER_CONSOLE_H
#define DEBUGGER_CONSOLE_H

	enum
	{
		CONSOLE_HEIGHT = 384, // Lines, was 128, but need ~ 256+16 for PROFILE LIST
		CONSOLE_WIDTH  =  80,

		// need min 256+ lines for "profile list"
		CONSOLE_BUFFER_HEIGHT = 384,

		HISTORY_HEIGHT = 128,
		HISTORY_WIDTH  = 128,

		CONSOLE_FIRST_LINE = 1, // where ConsoleDisplay is pushed up from
	};

// Color ____________________________________________________________________

	// typedef unsigned char conchar_t;
	typedef short conchar_t;

	enum ConsoleColors_e
	{
		CONSOLE_COLOR_K, // 0
		CONSOLE_COLOR_x = 0, // default console foreground
		CONSOLE_COLOR_R, // 1
		CONSOLE_COLOR_G, // 2
		CONSOLE_COLOR_Y, // 3
		CONSOLE_COLOR_B, // 4
		CONSOLE_COLOR_M, // 5 Lite Blue
		CONSOLE_COLOR_C, // 6
		CONSOLE_COLOR_W, // 7
		CONSOLE_COLOR_O, // 8
		CONSOLE_COLOR_k, // 9 Grey
		NUM_CONSOLE_COLORS
	};
	extern COLORREF g_anConsoleColor[ NUM_CONSOLE_COLORS ];

	// Note: THe ` ~ key should always display ~ to prevent rendering errors
	#define CONSOLE_COLOR_ESCAPE_CHAR '`'
	#define _CONSOLE_COLOR_MASK 0x7F

/* Help Colors
*/
#if 1 // USE_APPLE_FONT
	// Console Help Color
	#define CHC_DEFAULT  "`0"
	#define CHC_USAGE    "`3"
	#define CHC_CATEGORY "`6"
	#define CHC_COMMAND  "`2"
	#define CHC_KEY      "`1"
	#define CHC_ARG_MAND "`7" // < >
	#define CHC_ARG_OPT  "`4" // [ ] 
	#define CHC_ARG_SEP  "`9" //  |  grey
	#define CHC_NUM_DEC  "`6" // cyan looks better then yellow
	#define CHC_NUM_HEX  "`3"
	#define CHC_SYMBOL   "`2"
	#define CHC_ADDRESS  "`8"
	#define CHC_ERROR    "`1"
	#define CHC_STRING   "`6"
	#define CHC_EXAMPLE  "`5"
#else
	#define CHC_DEFAULT  ""
	#define CHC_USAGE    ""
	#define CHC_COMMAND  ""
	#define CHC_KEY      ""
	#define CHC_ARG_MAND ""
	#define CHC_ARG_OPT  ""
	#define CHC_ARG_SEP  ""
	#define CHC_NUMBER   ""
	#define CHC_SYMBOL   ""
	#define CHC_ADDRESS  ""
	#define CHC_ERROR    ""
	#define CHC_STRING   ""
	#define CHC_EXAMPLE  ""
#endif

	// ascii markup
	inline bool ConsoleColor_IsCharMeta( unsigned char c )
	{
		if (CONSOLE_COLOR_ESCAPE_CHAR == c)
			return true;
		return false;
	}

	inline bool ConsoleColor_IsCharColor( unsigned char c )
	{
		if ((c >= '0') && ((c - '0') < NUM_CONSOLE_COLORS))
			return true;
		return false;
	}

	// Console "Native" Chars
	//
	// There are a few different ways of encoding color chars & mouse text
	// Simplist method is to use a user-defined ESCAPE char to shift
	// into color mode, or mouse text mode.  The other solution
	// is to use a wide-char, simulating unicode16.
	//
	//    C1C0 char16 of High Byte (c1) and Low Byte (c0)
	// 1) --??  Con: Colors chars take up extra chars.
	//          Con: String Length is complicated.
	//          Pro: simple to parse
	//
	// <-- WE USE THIS
	// 2) ccea  Pro: Efficient packing of plain text and mouse text
	//          Pro: Color is optional (only record new color)
	//          Con: need to provide char8 and char16 API 
	//          Con: little more difficult to parse/convert plain text
	//          i.e.
	//       ea = 0x20 - 0x7F ASCII
	//            0x80 - 0xFF Mouse Text  '@'-'Z' -> 0x00 - 0x1F
	//    cc    = ASCII '0' - '9' (color)
	// 3) ??cc  Con: Colors chars take up extra chars
	// 4)  f??  Con: Colors chars take up extra chars
	//
	// Legend:
	//       f Flag
	//      -- Not Applicable (n/a)
	//      ?? ASCII (0x20 - 0x7F)
	//      ea Extended ASCII with High-Bit representing Mouse Text
	//      cc Encoded Color / Mouse Text
	//
	inline bool ConsoleColor_IsColorOrMouse( conchar_t g )
	{
		if (g > _CONSOLE_COLOR_MASK)
			return true;
		return false;
	}

	inline bool ConsoleColor_IsColor( conchar_t g )
	{
		return ConsoleColor_IsCharColor (g >> 8);
	}

	inline COLORREF ConsoleColor_GetColor( conchar_t g )
	{
		const int iColor = (g >> 8) - '0';
		if (iColor < NUM_CONSOLE_COLORS)
			return g_anConsoleColor[ iColor ];

		return g_anConsoleColor[ 0 ];
	}

	inline char ConsoleColor_GetMeta( conchar_t g ) 
	{
		return ((g >> 8) & _CONSOLE_COLOR_MASK);
	}

	inline char ConsoleChar_GetChar( conchar_t g )
	{
		return (g & _CONSOLE_COLOR_MASK);
	}

	inline char ConsoleColor_MakeMouse( unsigned char c )
	{
		return ((c - '@') + (_CONSOLE_COLOR_MASK + 1));
	}

	inline conchar_t ConsoleColor_MakeMeta( unsigned char c )
	{
		conchar_t g = (ConsoleColor_MakeMouse(c) << 8);
		return g;
	}

	inline conchar_t ConsoleColor_MakeColor( unsigned char color, unsigned char text )
	{
		conchar_t g = (color << 8) | text;
		return g;
	}

// Globals __________________________________________________________________

	// Buffer
		extern bool      g_bConsoleBufferPaused;
		extern int       g_nConsoleBuffer; 
		extern conchar_t g_aConsoleBuffer[ CONSOLE_BUFFER_HEIGHT ][ CONSOLE_WIDTH ]; // TODO: stl::vector< line_t >

	// Cursor
		extern char  g_sConsoleCursor[];

	// Display
		extern char  g_aConsolePrompt[];// = TEXT(">!"); // input, assembler // NUM_PROMPTS
		extern char  g_sConsolePrompt[];// = TEXT(">"); // No, NOT Integer Basic!  The nostalgic '*' "Monitor" doesn't look as good, IMHO. :-(
		extern int   g_nConsolePromptLen;

		extern bool  g_bConsoleFullWidth;// = false;

		extern int       g_iConsoleDisplayStart  ; // to allow scrolling
		extern int       g_nConsoleDisplayTotal  ; // number of lines added to console
		extern int       g_nConsoleDisplayLines  ;
		extern int       g_nConsoleDisplayWidth  ;
		extern conchar_t g_aConsoleDisplay[ CONSOLE_HEIGHT ][ CONSOLE_WIDTH ];

	// Input History
		extern int   g_nHistoryLinesStart;// = 0;
		extern int   g_nHistoryLinesTotal;// = 0; // number of commands entered
		extern char  g_aHistoryLines[ HISTORY_HEIGHT ][ HISTORY_WIDTH ];// = {TEXT("")};

	// Input Line
		// Raw input Line (has prompt)
		extern char g_aConsoleInput[ CONSOLE_WIDTH ];

		// Cooked input line (no prompt)
		extern int          g_nConsoleInputChars  ;
		extern       char * g_pConsoleInput       ; // points to past prompt
		extern const char * g_pConsoleFirstArg    ; // points to first arg
		extern bool         g_bConsoleInputQuoted ;

		extern char         g_nConsoleInputSkip   ;


// Prototypes _______________________________________________________________

// Console

	// Buffered
	bool        ConsolePrint( const char * pText );
	void        ConsoleBufferToDisplay ();
	const conchar_t* ConsoleBufferPeek ();
	void        ConsoleBufferPop  ();
	bool        ConsoleBufferPush ( const char * pString );

	void ConsoleConvertFromText( conchar_t * sText, const char * pText );

	// Display
	Update_t ConsoleDisplayError ( const char * pTextError );
	void     ConsoleDisplayPause ();
	void     ConsoleDisplayPush  ( const char * pText );
	void     ConsoleDisplayPush  ( const conchar_t * pText );
	Update_t ConsoleUpdate       ();
	void     ConsoleFlush        ();

	// Input
	void     ConsoleInputToDisplay ();
	const char *ConsoleInputPeek      ();
	bool     ConsoleInputClear     ();
	bool     ConsoleInputBackSpace ();
	bool     ConsoleInputChar      ( TCHAR ch );
	void     ConsoleInputReset     ();
	int      ConsoleInputTabCompletion ();

	void     ConsoleUpdateCursor( char ch );

	Update_t ConsoleBufferTryUnpause (int nLines);

	// Scrolling
	Update_t ConsoleScrollHome   ();
	Update_t ConsoleScrollEnd    ();
	Update_t ConsoleScrollUp     ( int nLines );
	Update_t ConsoleScrollDn     ( int nLines );
	Update_t ConsoleScrollPageUp ();
	Update_t ConsoleScrollPageDn ();

#endif
