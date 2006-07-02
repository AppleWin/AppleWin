#ifndef DEBUGGER_DISPLAY_H
#define DEBUGGER_DISPLAY_H

// Test Colors & Glyphs
#define DEBUG_APPLE_FONT 0
// Re-route all debugger text to new font
#define USE_APPLE_FONT   0

// Win32 Debugger Font
// 1 = Use seperate BMP
// 0 = Use CHARSET40.bmp (fg & bg colors aren't proper)
#define APPLE_FONT_NEW            1
// 7x8 Font
#define APPLE_FONT_SCALE_ONE_HALF 1

#if APPLE_FONT_NEW
	#define APPLE_FONT_BITMAP_PADDED  0
	#define DEBUG_FONT_WIDTH          7
	#define DEBUG_FONT_HEIGHT         8

	#define DEBUG_FONT_CELL_WIDTH     7
	#define DEBUG_FONT_CELL_HEIGHT    8
#else
	#define APPLE_FONT_BITMAP_PADDED  1
#endif

	enum AppleFontSize_e
	{
		CW = DEBUG_FONT_CELL_WIDTH ,
		CH = DEBUG_FONT_CELL_HEIGHT,

		// Font Char Width/Height
		FW = DEBUG_FONT_WIDTH ,
		FH = DEBUG_FONT_HEIGHT,
	};

	extern HDC    g_hDstDC  ;
	extern HBRUSH g_hBrushFG;
	extern HBRUSH g_hBrushBG;

	extern HDC     g_hDebugFontDC;
	extern HBRUSH  g_hDebugFontBrush;
	extern HBITMAP g_hDebugFontBitmap;

	extern const int DISPLAY_HEIGHT;

	extern FontConfig_t g_aFontConfig[ NUM_FONTS  ];

	void DebuggerSetColorFG( COLORREF nRGB );
	void DebuggerSetColorBG( COLORREF nRGB, bool bTransparent = false );

	void DebuggerPrintChar( const int x, const int y, const int iChar );
	
	int DebugDrawText      ( LPCTSTR pText, RECT & rRect );
	int DebugDrawTextFixed ( LPCTSTR pText, RECT & rRect );
	int DebugDrawTextLine  ( LPCTSTR pText, RECT & rRect );
	int DebugDrawTextHorz  ( LPCTSTR pText, RECT & rRect );

	void DrawWindow_Source      (Update_t bUpdate);

	void DrawBreakpoints      (HDC dc, int line);
	void DrawConsoleInput     (HDC dc);
	void DrawConsoleLine      (LPCSTR pText, int y);
	WORD DrawDisassemblyLine  (HDC dc, int line, WORD offset, LPTSTR text);
	void DrawFlags            (HDC dc, int line, WORD nRegFlags, LPTSTR pFlagNames_);
	void DrawMemory           (HDC dc, int line, int iMem );
	void DrawRegister         (HDC dc, int line, LPCTSTR name, int bytes, WORD value, int iSource = 0 );
	void DrawStack            (HDC dc, int line);
	void DrawTargets          (HDC dc, int line);
	void DrawWatches          (HDC dc, int line);
	void DrawZeroPagePointers (HDC dc, int line);

#endif
