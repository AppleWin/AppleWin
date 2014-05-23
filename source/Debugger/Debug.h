#pragma once

#include <vector>
#include <algorithm> // sort, find
#include <map>
using namespace std;

#include "Debugger_Types.h"
#include "Debugger_DisassemblerData.h"
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
//	extern vector<int> g_aBookmarks;

// Breakpoints
	enum BreakpointHit_t
	{
		BP_HIT_NONE     = 0
		,BP_HIT_INVALID = (1 << 0)
		,BP_HIT_OPCODE  = (1 << 1)
		,BP_HIT_REG     = (1 << 2)
		,BP_HIT_MEM     = (1 << 3)
	};

	extern int          g_nBreakpoints;
	extern Breakpoint_t g_aBreakpoints[ MAX_BREAKPOINTS ];

	extern const char  *g_aBreakpointSource [ NUM_BREAKPOINT_SOURCES   ];
	extern const TCHAR *g_aBreakpointSymbols[ NUM_BREAKPOINT_OPERATORS ];

	// MODE_RUNNING // Normal Speed Breakpoints
	extern bool g_bDebugNormalSpeedBreakpoints;

	// MODE_STEPPING // Full Speed Breakpoints

	// Any Speed Breakpoints
	extern int  g_nDebugBreakOnInvalid ;
	extern int  g_iDebugBreakOnOpcode  ;

	// Breakpoint Status
	extern bool g_bDebugBreakDelayCheck;
	extern int  g_bDebugBreakpointHit  ;

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
	extern char      g_sFileNameConfig[];

// Cursor
	extern WORD g_nDisasmTopAddress ;
	extern WORD g_nDisasmBotAddress ;
	extern WORD g_nDisasmCurAddress ;

	extern bool g_bDisasmCurBad   ;
	extern int  g_nDisasmCurLine  ; // Aligned to Top or Center
	extern int  g_iDisasmCurState ;

	extern int  g_nDisasmWinHeight;

	extern const int WINDOW_DATA_BYTES_PER_LINE;

// Config - Disassembly
	extern bool  g_bConfigDisasmAddressView  ;
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

// Display
	extern bool g_bDebuggerViewingAppleOutput;

// Font
	extern int g_nFontHeight;
	extern int g_iFontSpacing;

// Memory
	extern MemoryDump_t g_aMemDump[ NUM_MEM_DUMPS ];

//	extern MemorySearchArray_t g_vMemSearchMatches;
	extern vector<int> g_vMemorySearchResults;

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

// Bookmarks
	bool Bookmark_Find( const WORD nAddress );

// Breakpoints
	int CheckBreakpointsIO ();
	int CheckBreakpointsReg ();

	bool GetBreakpointInfo ( WORD nOffset, bool & bBreakpointActive_, bool & bBreakpointEnable_ );

	inline int _IsDebugBreakpointHit()
	{
		g_bDebugBreakpointHit |= CheckBreakpointsIO() || CheckBreakpointsReg();
		return g_bDebugBreakpointHit;
	}

	inline int _IsDebugBreakOnOpcode( int iOpcode )
	{
		if (g_iDebugBreakOnOpcode == iOpcode)
			g_bDebugBreakpointHit |= BP_HIT_OPCODE;
		return g_bDebugBreakpointHit;
	}

	// iOpcodeType = AM_IMPLIED (BRK), AM_1, AM_2, AM_3
	inline int IsDebugBreakOnInvalid( int iOpcodeType )
	{
		g_bDebugBreakpointHit |= ((g_nDebugBreakOnInvalid >> iOpcodeType) & 1) ? BP_HIT_INVALID : 0;
		return g_bDebugBreakpointHit;
	}

	// iOpcodeType = AM_IMPLIED (BRK), AM_1, AM_2, AM_3
	inline void SetDebugBreakOnInvalid( int iOpcodeType, int nValue )
	{
		if (iOpcodeType <= AM_3)
		{
			g_nDebugBreakOnInvalid &= ~ (          1  << iOpcodeType);
			g_nDebugBreakOnInvalid |=   ((nValue & 1) << iOpcodeType);
		}
	}

	//
	// CPU checks the Debugger breakpoints
	//   a) at opcode fetch
	//   b) after opcode execution
	//
	inline int IsDebugBreakOpcode( int iOpcode )
	{
		if (g_bDebugBreakDelayCheck)
		{
			g_bDebugBreakDelayCheck = false;
			return false;
		}

		if (! iOpcode )
			IsDebugBreakOnInvalid( AM_IMPLIED );

		if (g_iDebugBreakOnOpcode ) // User wants to enter debugger on specific opcode?
			_IsDebugBreakOnOpcode(iOpcode);

		return g_bDebugBreakpointHit;
	}
	//
	inline int IsDebugBreakpointHit()
	{
		if ( !g_bDebugNormalSpeedBreakpoints )
			return false;
		return _IsDebugBreakpointHit();
	}
	
// Source Level Debugging
	int FindSourceLine( WORD nAddress );
	const char* FormatAddress( WORD nAddress, int nBytes );

// Symbol Table / Memory
	bool FindAddressFromSymbol( const char* pSymbol, WORD * pAddress_ = NULL, int * iTable_ = NULL );
	WORD GetAddressFromSymbol ( const char* symbol); // HACK: returns 0 if symbol not found
	void SymbolUpdate( SymbolTable_Index_e eSymbolTable, char *pSymbolName, WORD nAddrss, bool bRemoveSymbol, bool bUpdateSymbol );

	const char* FindSymbolFromAddress (WORD nAdress, int * iTable_ = NULL );
	const char* GetSymbol   (WORD nAddress, int nBytes);

	Update_t DebuggerProcessCommand( const bool bEchoConsoleInput );

// Prototypes _______________________________________________________________

	enum
	{
		DEBUG_EXIT_KEY   = 0x1B, // Escape
		DEBUG_TOGGLE_KEY = VK_F1 + BTN_DEBUG
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

	void	DebuggerUpdate();
	void	DebuggerCursorNext();

	void	DebuggerMouseClick( int x, int y );
	