#pragma once

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
	void DrawConsoleLine      ( const conchar_t * pText, int y);
	void DrawConsoleCursor    ();

	int GetDisassemblyLine(  const WORD nOffset, DisasmLine_t & line_ );
//		, int iOpcode, int iOpmode, int nOpbytes
//		char *sAddress_, char *sOpCodes_,
//		char *sTarget_, char *sTargetOffset_, int & nTargetOffset_, char *sTargetValue_,
//		char * sImmediate_, char & nImmediate_, char *sBranch_ );
	WORD DrawDisassemblyLine  ( int line, const WORD offset );
	void FormatDisassemblyLine( const DisasmLine_t & line, char *sDisassembly_, const int nBufferSize );
	void FormatOpcodeBytes    ( WORD nBaseAddress, DisasmLine_t & line_ );
	void FormatNopcodeBytes   ( WORD nBaseAddress, DisasmLine_t & line_ );

	void DrawFlags            ( int line, WORD nRegFlags, LPTSTR pFlagNames_);

	//

	extern HDC GetDebuggerMemDC(void);
	extern void ReleaseDebuggerMemDC(void);
	extern void StretchBltMemToFrameDC(void);

	enum DebugVirtualTextScreen_e
	{
		DEBUG_VIRTUAL_TEXT_WIDTH  = 80,
		DEBUG_VIRTUAL_TEXT_HEIGHT = 43
	};

	extern char g_aDebuggerVirtualTextScreen[ DEBUG_VIRTUAL_TEXT_HEIGHT ][ DEBUG_VIRTUAL_TEXT_WIDTH ];
	extern size_t Util_GetDebuggerText( char* &pText_ ); // Same API as Util_GetTextScreen()

	extern unsigned __int64 g_nCumulativeCycles;
	class VideoScannerDisplayInfo
	{
	public:
		VideoScannerDisplayInfo(void) : isDecimal(false), isHorzReal(false), isAbsCycle(false),
										lastCumulativeCycles(0), cycleDelta(0) {}
		void Reset(void) { lastCumulativeCycles = g_nCumulativeCycles; cycleDelta = 0; }

		bool isDecimal;
		bool isHorzReal;
		bool isAbsCycle;

		unsigned __int64 lastCumulativeCycles;
		UINT cycleDelta;
	};

	extern VideoScannerDisplayInfo g_videoScannerDisplayInfo;
