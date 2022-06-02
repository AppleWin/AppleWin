#pragma once

#include "../SaveState_Structs_v1.h"	// For SS_CARD_MOCKINGBOARD
#include "../Common.h"

#include "Debugger_Types.h"
#include "Debugger_DisassemblerData.h"
#include "Debugger_Disassembler.h"
#include "Debugger_Range.h"
#include "Debugger_Parser.h"
#include "Debugger_Console.h"
#include "Debugger_Assembler.h"
#include "Debugger_Help.h"
#include "Debugger_Display.h"
#include "Debugger_Symbols.h"
#include "Util_MemoryTextFile.h"

// Globals __________________________________________________________________

// All (Global)
	extern bool g_bDebuggerEatKey;

// Benchmarking
	extern DWORD      extbench;

// Bookmarks
	extern int          g_nBookmarks;
	extern Bookmark_t   g_aBookmarks[ MAX_BOOKMARKS ];

// Breakpoints
	enum BreakpointHit_t
	{
		BP_HIT_NONE = 0
		, BP_HIT_INVALID = (1 << 0)
		, BP_HIT_OPCODE = (1 << 1)
		, BP_HIT_REG = (1 << 2)
		, BP_HIT_MEM = (1 << 3)
		, BP_HIT_MEMR = (1 << 4)
		, BP_HIT_MEMW = (1 << 5)
		, BP_HIT_PC_READ_FLOATING_BUS_OR_IO_MEM = (1 << 6)
		, BP_HIT_INTERRUPT = (1 << 7)
		, BP_DMA_TO_IO_MEM = (1 << 8)
		, BP_DMA_FROM_IO_MEM = (1 << 9)
		, BP_DMA_TO_MEM = (1 << 10)
		, BP_DMA_FROM_MEM = (1 << 11)
	};

	extern int          g_nBreakpoints;
	extern Breakpoint_t g_aBreakpoints[ MAX_BREAKPOINTS ];

	extern const char  *g_aBreakpointSource [ NUM_BREAKPOINT_SOURCES   ];
	extern const TCHAR *g_aBreakpointSymbols[ NUM_BREAKPOINT_OPERATORS ];

	extern int  g_nDebugBreakOnInvalid ;
	extern int  g_iDebugBreakOnOpcode  ;

// Commands
	void VerifyDebuggerCommandTable();

	extern const int NUM_COMMANDS_WITH_ALIASES; // = sizeof(g_aCommands) / sizeof (Command_t); // Determined at compile-time ;-)
	extern       int g_iCommand; // last command

	extern Command_t g_aCommands[];
	extern Command_t g_aParameters[];

	class commands_functor_compare
	{
		public:
			bool operator() ( const Command_t & rLHS, const Command_t & rRHS ) const
			{
				// return true if lhs<rhs
				return (_tcscmp( rLHS.m_sName, rRHS.m_sName ) <= 0) ? true : false;
			}
	};

// Config - FileName
	extern std::string g_sFileNameConfig;

// Cursor
	extern WORD g_nDisasmTopAddress ;
	extern WORD g_nDisasmBotAddress ;
	extern WORD g_nDisasmCurAddress ;

	extern bool g_bDisasmCurBad   ;
	extern int  g_nDisasmCurLine  ; // Aligned to Top or Center
	extern int  g_iDisasmCurState ;

	extern int  g_nDisasmWinHeight;

	extern const int WINDOW_DATA_BYTES_PER_LINE;

	extern int g_nDisasmDisplayLines;

// Config - Disassembly
	extern bool  g_bConfigDisasmAddressView  ;
	extern int   g_bConfigDisasmClick        ; // GH#462
	extern bool  g_bConfigDisasmAddressColon ;
	extern bool  g_bConfigDisasmOpcodesView  ;
	extern bool  g_bConfigDisasmOpcodeSpaces ;
	extern int   g_iConfigDisasmTargets      ;
	extern int   g_iConfigDisasmBranchType   ;
	extern int   g_bConfigDisasmImmediateChar;
// Config - Info
	extern bool  g_bConfigInfoTargetPointer  ;

// Disassembly
	extern int g_aDisasmTargets[ MAX_DISPLAY_LINES ];

// Font
	extern int g_nFontHeight;
	extern int g_iFontSpacing;

// Memory
	extern MemoryDump_t g_aMemDump[ NUM_MEM_DUMPS ];

//	extern MemorySearchArray_t g_vMemSearchMatches;
	extern std::vector<int> g_vMemorySearchResults;

// Source Level Debugging
	extern std::string g_aSourceFileName;
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

	void WindowUpdateSizes();

// Bookmarks
	int Bookmark_Find( const WORD nAddress );

// Breakpoints
	int CheckBreakpointsIO ();
	int CheckBreakpointsReg ();

	bool GetBreakpointInfo ( WORD nOffset, bool & bBreakpointActive_, bool & bBreakpointEnable_ );

// Source Level Debugging
	int FindSourceLine( WORD nAddress );

// Memory
	size_t Util_GetTextScreen( char* &pText_ );
	void   Util_CopyTextToClipboard( const size_t nSize, const char *pText );

// Main
	Update_t DebuggerProcessCommand( const bool bEchoConsoleInput );

// Prototypes _______________________________________________________________

	bool	DebugGetVideoMode(UINT* pVideoMode);

	void	DebugBegin ();
	void	DebugExitDebugger ();
	void	DebugContinueStepping(const bool bCallerWillUpdateDisplay = false);
	void    DebugStopStepping(void);
	void	DebugDestroy ();
	void	DebugDisplay ( BOOL bInitDisasm = FALSE );
	void	DebugInitialize ();
	void	DebugReset(void);

	void	DebuggerInputConsoleChar( TCHAR ch );
	void	DebuggerProcessKey( int keycode );

	void	DebuggerUpdate();
	void	DebuggerCursorNext();

	void	DebuggerMouseClick( int x, int y );

	bool	IsDebugSteppingAtFullSpeed(void);
	void	DebuggerBreakOnDmaToOrFromIoMemory(WORD nAddress, bool isDmaToMemory);
	bool	DebuggerCheckMemBreakpoints(WORD nAddress, WORD nSize, bool isDmaToMemory);
