#ifndef DEBUGGER_DISPLAY_H
#define DEBUGGER_DISPLAY_H

	extern const int DISPLAY_HEIGHT;

	extern FontConfig_t g_aFontConfig[ NUM_FONTS  ];


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
