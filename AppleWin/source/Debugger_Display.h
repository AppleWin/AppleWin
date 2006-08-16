#ifndef DEBUGGER_DISPLAY_H
#define DEBUGGER_DISPLAY_H

// use the new Debugger Font (Apple Font)
#define USE_APPLE_FONT   1

// Test Colors & Glyphs
#define DEBUG_APPLE_FONT 0

// Win32 Debugger Font
// 1 = Use Debugger_Font.BMP (7x8)
// 0 = Use CHARSET40.bmp (fg & bg colors aren't proper)
#define APPLE_FONT_NEW            1

#if APPLE_FONT_NEW
	#define APPLE_FONT_BITMAP_PADDED  0
#else
	#define APPLE_FONT_BITMAP_PADDED  1
#endif

	enum ConsoleFontSize_e
	{
#if APPLE_FONT_NEW
		// Grid Alignment
		CONSOLE_FONT_GRID_X = 7,
		CONSOLE_FONT_GRID_Y = 8,

		// Font Char Width/Height in pixels
		CONSOLE_FONT_WIDTH  = 7,
		CONSOLE_FONT_HEIGHT = 8,
#else
		CONSOLE_FONT_GRID_X = 8,
		CONSOLE_FONT_GRID_Y = 8,

		// Font Char Width/Height in pixels
		CONSOLE_FONT_WIDTH  = 7,
		CONSOLE_FONT_HEIGHT = 8,
#endif
	};

//	extern HDC    g_hDstDC  ;
	extern HBRUSH g_hConsoleBrushFG;
	extern HBRUSH g_hConsoleBrushBG;

	extern HDC     g_hConsoleFontDC;
	extern HBRUSH  g_hConsoleFontBrush;
	extern HBITMAP g_hConsoleFontBitmap;

	enum
	{
		DISPLAY_HEIGHT = 384,
		MAX_DISPLAY_LINES  = DISPLAY_HEIGHT / CONSOLE_FONT_HEIGHT,
	};
	
	int GetConsoleTopPixels( int y );

	extern FontConfig_t g_aFontConfig[ NUM_FONTS  ];

	void DebuggerSetColorFG( COLORREF nRGB );
	void DebuggerSetColorBG( COLORREF nRGB, bool bTransparent = false );

	void PrintGlyph      ( const int x, const int y, const int iChar );
	int  PrintText       ( const char * pText, RECT & rRect );
	int  PrintTextCursorX( const char * pText, RECT & rRect );
	int  PrintTextCursorY( const char * pText, RECT & rRect );

	void PrintTextColor  ( const conchar_t * pText, RECT & rRect );

	void DrawWindow_Source      (Update_t bUpdate);

	void DrawBreakpoints      ( int line);
	void DrawConsoleInput     ();
	void DrawConsoleLine      ( const char * pText, int y);
	void DrawConsoleCursor    ();

	int GetDisassemblyLine(  const WORD nOffset, DisasmLine_t & line_ );
//		, int iOpcode, int iOpmode, int nOpbytes
//		char *sAddress_, char *sOpCodes_,
//		char *sTarget_, char *sTargetOffset_, int & nTargetOffset_, char *sTargetValue_,
//		char * sImmediate_, char & nImmediate_, char *sBranch_ );
	WORD DrawDisassemblyLine  ( int line, const WORD offset );
	void FormatDisassemblyLine( const DisasmLine_t & line, char *sDisassembly_, const int nBufferSize );

	void DrawFlags            ( int line, WORD nRegFlags, LPTSTR pFlagNames_);
	void DrawMemory           ( int line, int iMem );
	void DrawRegister         ( int line, LPCTSTR name, int bytes, WORD value, int iSource = 0 );
	void DrawStack            ( int line);
	void DrawTargets          ( int line);
	void DrawWatches          ( int line);
	void DrawZeroPagePointers ( int line);

#endif
