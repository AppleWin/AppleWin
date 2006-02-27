#pragma once

#include <vector>
#include <algorithm> // sort
#include <map>
using namespace std;

#include "Debugger_Types.h"
#include "Debugger_Parser.h"
#include "Debugger_Console.h"
#include "Debugger_Assembler.h"
#include "Debugger_Help.h"
#include "Debugger_Display.h"
#include "Util_MemoryTextFile.h"

// Globals __________________________________________________________________

// Benchmarking
	extern DWORD      extbench;

// Breakpoints
	extern int          g_nBreakpoints;
	extern Breakpoint_t g_aBreakpoints[ NUM_BREAKPOINTS ];

	extern const TCHAR *g_aBreakpointSource [ NUM_BREAKPOINT_SOURCES   ];
	extern const TCHAR *g_aBreakpointSymbols[ NUM_BREAKPOINT_OPERATORS ];

// Commands
	extern const int NUM_COMMANDS_WITH_ALIASES; // = sizeof(g_aCommands) / sizeof (Command_t); // Determined at compile-time ;-)
	extern       int g_iCommand; // last command

	extern Command_t g_aCommands[];
	extern Command_t g_aParameters[];

// Cursor
	extern WORD g_nDisasmTopAddress ;
	extern WORD g_nDisasmBotAddress ;
	extern WORD g_nDisasmCurAddress ;

	extern bool g_bDisasmCurBad   ;
	extern int  g_nDisasmCurLine  ; // Aligned to Top or Center
    extern int  g_iDisasmCurState ;

	extern int  g_nDisasmWinHeight;

	extern const int WINDOW_DATA_BYTES_PER_LINE;

// Disassembly
	extern int   g_iConfigDisasmBranchType;

	extern bool  g_bConfigDisasmOpcodeSpaces ;//= true; // TODO: CONFIG DISASM SPACE  [0|1]
	extern bool  g_bConfigDisasmAddressColon ;//= true; // TODO: CONFIG DISASM COLON  [0|1]
	extern bool  g_bConfigDisasmFancyBranch  ;//= true; // TODO: CONFIG DISASM BRANCH [0|1]

// Display
	extern bool g_bDebuggerViewingAppleOutput;

// Font
	extern int g_nFontHeight;
	extern int g_iFontSpacing;

// Memory
	extern MemoryDump_t g_aMemDump[ NUM_MEM_DUMPS ];

// Source Level Debugging
	extern TCHAR  g_aSourceFileName[ MAX_PATH ];
	extern MemoryTextFile_t g_AssemblerSourceBuffer;

	extern int    g_iSourceDisplayStart   ;
	extern int    g_nSourceAssembleBytes  ;
	extern int    g_nSourceAssemblySymbols;

// Version
	extern const int DEBUGGER_VERSION;

// Watches
	extern int       g_nWatches;
	extern Watches_t g_aWatches[ MAX_WATCHES ];

// Window
	extern int           g_iWindowLast;
	extern int           g_iWindowThis;
	extern WindowSplit_t g_aWindowConfig[ NUM_WINDOWS ];

// Zero Page
	extern int                g_nZeroPagePointers;
	extern ZeroPagePointers_t g_aZeroPagePointers[ MAX_ZEROPAGE_POINTERS ]; // TODO: use vector<> ?

// Prototypes _______________________________________________________________

// Breakpoints
	bool GetBreakpointInfo ( WORD nOffset, bool & bBreakpointActive_, bool & bBreakpointEnable_ );

// Color
	inline COLORREF DebuggerGetColor( int iColor );

// Source Level Debugging
	int FindSourceLine( WORD nAddress );
	LPCTSTR FormatAddress( WORD nAddress, int nBytes );

// Symbol Table / Memory
	bool FindAddressFromSymbol( LPCSTR pSymbol, WORD * pAddress_ = NULL, int * iTable_ = NULL );
	WORD GetAddressFromSymbol (LPCTSTR symbol); // HACK: returns 0 if symbol not found
	void SymbolUpdate( Symbols_e eSymbolTable, char *pSymbolName, WORD nAddrss, bool bRemoveSymbol, bool bUpdateSymbol );

	LPCTSTR FindSymbolFromAddress (WORD nAdress, int * iTable_ = NULL );
	LPCTSTR GetSymbol   (WORD nAddress, int nBytes);

	bool Get6502Targets (int *pTemp_, int *pFinal_, int *pBytes_ );

	Update_t DebuggerProcessCommand( const bool bEchoConsoleInput );

// Prototypes _______________________________________________________________

	enum
	{
		DEBUG_EXIT_KEY = 0x1B // Escape
	};

	void	DebugBegin ();
	void	DebugContinueStepping ();
	void	DebugDestroy ();
	void	DebugDisplay (BOOL);
	void	DebugEnd ();
	void	DebugInitialize ();
//	void	DebugProcessChar (TCHAR);
	void	DebuggerInputConsoleChar( TCHAR ch );
//	void	DebugProcessCommand (int);
	void	DebuggerProcessKey( int keycode );
