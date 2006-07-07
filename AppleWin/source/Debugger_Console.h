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

	//typedef short conchar_t;
	typedef unsigned char conchar_t;

	enum ConsoleColors_e
	{
		CONSOLE_COLOR_K,
		CONSOLE_COLOR_x = 0, // default console foreground
		CONSOLE_COLOR_R,
		CONSOLE_COLOR_G,
		CONSOLE_COLOR_Y,
		CONSOLE_COLOR_B,
		CONSOLE_COLOR_M,
		CONSOLE_COLOR_C,
		CONSOLE_COLOR_W,

		MAX_CONSOLE_COLORS
	};

	extern COLORREF       g_anConsoleColor[ MAX_CONSOLE_COLORS ];

	// Note: THe ` ~ key should always display ~ to prevent rendering errors
	#define CONSOLE_COLOR_ESCAPE_CHAR '`'
	#define _CONSOLE_COLOR_MASK 0x7F

// Help Colors
/*
	Types
		Plain        White 
		Header       Yellow i.e. Usage
		Operator     Yellow
		Command      Green 
		Key          Red
        ArgMandatory Magenta  < >
        ArgOptional  Blue     [ ]
		ArgSeperator White     |

	#define CON_COLOR_DEFAULT g_asConsoleColor[ CONSOLE_COLOR_x ]
	#define CON_COLOR_DEFAULT "`0"
	
*/
#if 1 // USE_APPLE_FONT
	#define CON_COLOR_DEFAULT  "`0"
	#define CON_COLOR_USAGE    "`3"
	#define CON_COLOR_PARAM    "`2"
	#define CON_COLOR_KEY      "`1"
	#define CON_COLOR_ARG_MAND "`5"
	#define CON_COLOR_ARG_OPT  "`4"
	#define CON_COLOR_ARG_SEP  "`3"
	#define CON_COLOR_NUM      "`2"
#else
	#define CON_COLOR_DEFAULT   ""
	#define CON_COLOR_USAGE     ""
	#define CON_COLOR_PARAM     ""
	#define CON_COLOR_KEY       ""
	#define CON_COLOR_ARG_MAND  ""
	#define CON_COLOR_ARG_OPT   ""
	#define CON_COLOR_ARG_SEP   ""
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
		if ((c >= '0') && (c <= '7'))
			return true;
		return false;
	}

	// native char
	// There are a few different ways of encoding color chars & mouse text
	// 0x80 Console Color
	// 0xC0 Mouse Text
	//         High  Low
	// 1)   xx n/a   Extended ASCII (Multi-Byte to Single-Byte)
	//               0..7 = Color
	//               @..Z = Mouse Text
	//                     Con: Colors chars take up extra chars
	// 2) ccxx Color ASCII Con: need to provide char8 and char16 API
	// 3) xxcc ASCII Color Con: ""
	// 4)  fxx Flag  ASCII Con: Colors chars take up extra chars
	inline bool ConsoleColor_IsMeta( conchar_t g )
	{
		if (g > _CONSOLE_COLOR_MASK)
			return true;
		return false;
	}

//	inline bool ConsoleColor_IsCharColor( unsigned char c )
	inline bool ConsoleColor_IsColor( conchar_t g )
	{
		if (((g - '0') & _CONSOLE_COLOR_MASK) < MAX_CONSOLE_COLORS)
			return true;
		return false;
	}

	inline COLORREF ConsoleColor_GetColor( conchar_t g )
	{
		const int iColor = g & (MAX_CONSOLE_COLORS - 1);
		return g_anConsoleColor[ iColor ];
	}

	inline char ConsoleColor_GetChar( conchar_t g ) 
	{
		return (g & _CONSOLE_COLOR_MASK);
	}

	inline conchar_t ConsoleColor_Make( unsigned char c )
	{
		conchar_t g = c | (_CONSOLE_COLOR_MASK + 1);
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
		extern conchar_t * const g_aConsoleInput; // = g_aConsoleDisplay[0];

		// Cooked input line (no prompt)
		extern int           g_nConsoleInputChars  ;
		extern       char *  g_pConsoleInput       ;//= 0; // points to past prompt
		extern const char *  g_pConsoleFirstArg    ;//= 0; // points to first arg
		extern bool          g_bConsoleInputQuoted ;
		extern char          g_nConsoleInputSkip   ;


// Prototypes _______________________________________________________________

// Console

	// Buffered
	bool        ConsolePrint( const char * pText );
	void        ConsoleBufferToDisplay ();
	const conchar_t* ConsoleBufferPeek ();
	void        ConsoleBufferPop  ();
	bool        ConsoleBufferPush ( const char * pString );

	// Display
	Update_t ConsoleDisplayError ( const char * pTextError );
	void     ConsoleDisplayPause ();
	void     ConsoleDisplayPush  ( const char * pText );
	Update_t ConsoleUpdate       ();

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
