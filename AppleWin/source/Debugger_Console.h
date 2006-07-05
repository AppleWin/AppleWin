#ifndef DEBUGGER_CONSOLE_H
#define DEBUGGER_CONSOLE_H

	//	const int CONSOLE_HEIGHT = 384; // Lines, was 128, but need ~ 256+16 for PROFILE LIST
	//	const int CONSOLE_WIDTH  =  80;
	enum
	{
		CONSOLE_HEIGHT = 384, // Lines, was 128, but need ~ 256+16 for PROFILE LIST
		CONSOLE_WIDTH  =  80,

		CONSOLE_BUFFER_HEIGHT = 128,

		HISTORY_HEIGHT = 128,
		HISTORY_WIDTH  = 128,

		CONSOLE_FIRST_LINE = 1, // where ConsoleDisplay is pushed up from
	};

// Globals __________________________________________________________________

	typedef TCHAR conchar_t;

	// Buffer
		extern bool  g_bConsoleBufferPaused;// = false; // buffered output is waiting for user to continue
		extern int   g_nConsoleBuffer; //= 0;
		extern TCHAR g_aConsoleBuffer[ CONSOLE_BUFFER_HEIGHT ][ CONSOLE_WIDTH*2 ]; // TODO: stl::vector< line_t >

	// Cursor
		extern char g_sConsoleCursor[];

	// Display
		extern TCHAR g_aConsolePrompt[];// = TEXT(">!"); // input, assembler // NUM_PROMPTS
		extern TCHAR g_sConsolePrompt[];// = TEXT(">"); // No, NOT Integer Basic!  The nostalgic '*' "Monitor" doesn't look as good, IMHO. :-(

		extern int   g_nConsolePromptLen;

		extern bool  g_bConsoleFullWidth;// = false;

		extern int   g_iConsoleDisplayStart  ; // to allow scrolling
		extern int   g_nConsoleDisplayTotal  ; // number of lines added to console
		extern int   g_nConsoleDisplayLines  ;
		extern int   g_nConsoleDisplayWidth  ;
		extern TCHAR g_aConsoleDisplay[ CONSOLE_HEIGHT ][ CONSOLE_WIDTH*2 ];

	// Input History
		extern int   g_nHistoryLinesStart;// = 0;
		extern int   g_nHistoryLinesTotal;// = 0; // number of commands entered
		extern TCHAR g_aHistoryLines[ HISTORY_HEIGHT ][ HISTORY_WIDTH ];// = {TEXT("")};

	// Input Line
		// Raw input Line (has prompt)
		extern TCHAR * const g_aConsoleInput; // = g_aConsoleDisplay[0];

		// Cooked input line (no prompt)
		extern int           g_nConsoleInputChars  ;//= 0;
		extern TCHAR *       g_pConsoleInput       ;//= 0; // points to past prompt
		extern const TCHAR * g_pConsoleFirstArg    ;//= 0; // points to first arg
		extern bool          g_bConsoleInputQuoted ;//= false; // Allows lower-case to be entered
		extern char          g_nConsoleInputSkip   ;//= false;


// Prototypes _______________________________________________________________

// Console

	// Buffered
	bool ConsolePrint( const char * pText );
	void     ConsoleBufferToDisplay ();
	LPCSTR   ConsoleBufferPeek ();
	void     ConsoleBufferPop  ();
	bool     ConsoleBufferPush ( const TCHAR * pString );

	// Display
	Update_t ConsoleDisplayError (LPCTSTR errortext);
	void     ConsoleDisplayPause ();
	void     ConsoleDisplayPush  ( LPCSTR pText );
	Update_t ConsoleUpdate       ();

	// Input
	void     ConsoleInputToDisplay ();
	LPCSTR   ConsoleInputPeek      ();
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
