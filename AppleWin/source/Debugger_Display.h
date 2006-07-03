#ifndef DEBUGGER_DISPLAY_H
#define DEBUGGER_DISPLAY_H

// Test Colors & Glyphs
#define DEBUG_APPLE_FONT 0
// Re-route all debugger text to new font
#define USE_APPLE_FONT   0

// Win32 Debugger Font
// 1 = Use Debugger_Font_7x8.BMP
// 0 = Use CHARSET40.bmp (fg & bg colors aren't proper)
#define APPLE_FONT_NEW            1
// 7x8 Font
//#define APPLE_FONT_SCALE_ONE_HALF 1
#define APPLE_FONT_SCALE_ONE_HALF $ERROR("APPLE_FONT_SCALE_ONE_HALF")

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

	extern HDC    g_hDstDC  ;
	extern HBRUSH g_hConsoleBrushFG;
	extern HBRUSH g_hConsoleBrushBG;

	extern HDC     g_hConsoleFontDC;
	extern HBRUSH  g_hConsoleFontBrush;
	extern HBITMAP g_hConsoleFontBitmap;

	enum ConsoleColors_e
	{
		CONSOLE_COLOR_K,
		CONSOLE_COLOR_PREV = 0,
		CONSOLE_COLOR_R,
		CONSOLE_COLOR_G,
		CONSOLE_COLOR_Y,
		CONSOLE_COLOR_B,
		CONSOLE_COLOR_M,
		CONSOLE_COLOR_C,
		CONSOLE_COLOR_W,

		MAX_CONSOLE_COLORS
	};
	extern COLORREF g_anConsoleColor[ MAX_CONSOLE_COLORS ];
	extern char    *g_asConsoleColor[ MAX_CONSOLE_COLORS ];

	// ` ~ should always display ~
	#define CONSOLE_COLOR_ESCAPE_CHAR '`'
	inline bool ConsoleColorIsEscapeMeta( char c )
	{
		if (CONSOLE_COLOR_ESCAPE_CHAR == c)
			return true;
		return false;
	}

	inline bool ConsoleColorIsEscapeData( char c )
	{
		if ((c >= '0') && (c <= '7'))
			return true;
		return false;
	}

	inline COLORREF ConsoleColorGetEscapeData( char c )
	{
		int iColor = (c - '0') & (MAX_CONSOLE_COLORS - 1);
		return g_anConsoleColor[ iColor ];
	}

	inline void ConsoleColorMake( char * pText, ConsoleColors_e eColor )
	{
#if USE_APPLE_FONT
		pText[0] = CONSOLE_COLOR_ESCAPE_CHAR;
		pText[1] = eColor + '0';
		pText[2] = 0;
#else
		pText[0] = 0;
#endif
	}
	
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
