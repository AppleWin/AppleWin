#pragma once

// Win32 Debugger Font

	enum ConsoleFontSize_e
	{
		// Grid Alignment
		CONSOLE_FONT_GRID_X = 7,
		CONSOLE_FONT_GRID_Y = 8,

		// Font Char Width/Height in pixels
		CONSOLE_FONT_WIDTH  = 7,
		CONSOLE_FONT_HEIGHT = 8,

		CONSOLE_FONT_NUM_CHARS_PER_ROW = 16,
		CONSOLE_FONT_NUM_ROWS = 16,

		CONSOLE_FONT_BITMAP_WIDTH = CONSOLE_FONT_WIDTH * CONSOLE_FONT_NUM_CHARS_PER_ROW,	// 112 pixels
		CONSOLE_FONT_BITMAP_HEIGHT = CONSOLE_FONT_HEIGHT * CONSOLE_FONT_NUM_ROWS,			// 128 pixels
	};

	enum
	{
		DISPLAY_HEIGHT = 384,
		MAX_DISPLAY_LINES  = DISPLAY_HEIGHT / CONSOLE_FONT_HEIGHT,
	};
	
	int GetConsoleTopPixels( int y );

	extern FontConfig_t g_aFontConfig[ NUM_FONTS  ];

	void DebuggerSetColorFG( COLORREF nRGB );
	void DebuggerSetColorBG( COLORREF nRGB, bool bTransparent = false );

	void FillBackground(long left, long top, long right, long bottom);

	// Display ____________________________________________________________________
	void UpdateDisplay(Update_t bUpdate);

	int  PrintText       ( const char * pText, RECT & rRect );
	int  PrintTextCursorX( const char * pText, RECT & rRect );
	int  PrintTextCursorY( const char * pText, RECT & rRect );

	void PrintTextColor  ( const conchar_t * pText, RECT & rRect );

	void DrawWindow_Source      (Update_t bUpdate);

	void DrawBreakpoints      ( int line);
	void DrawConsoleInput     ();
	void DrawConsoleLine      ( const conchar_t * pText, int y);
	void DrawConsoleCursor    ();

	//

	extern HDC GetDebuggerMemDC(void);
	extern void ReleaseDebuggerMemDC(void);
	extern void StretchBltMemToFrameDC(void);
	extern HDC GetConsoleFontDC(void);
	extern void ReleaseConsoleFontDC(void);

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
		VideoScannerDisplayInfo(void) : isDecimal(false), isHorzReal(false), cycleMode(rel),
										lastCumulativeCycles(0), savedCumulativeCycles(0), cycleDelta(0) {}
		void Reset(void) { lastCumulativeCycles = savedCumulativeCycles = g_nCumulativeCycles; cycleDelta = 0; }

		bool isDecimal;
		bool isHorzReal;
		enum CYCLE_MODE {abs=0, rel, part};
		CYCLE_MODE cycleMode;

		unsigned __int64 lastCumulativeCycles;
		unsigned __int64 savedCumulativeCycles;
		UINT cycleDelta;
	};

	extern VideoScannerDisplayInfo g_videoScannerDisplayInfo;
