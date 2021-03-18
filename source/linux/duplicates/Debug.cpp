#include "StdAfx.h"
#include "Common.h"

#include "Debug.h"
#include "DebugDefs.h"
#include "CPU.h"
#include "Core.h"
#include "Interface.h"

const int MIN_DISPLAY_CONSOLE_LINES =  5; // doesn't include ConsoleInput
int g_iWindowThis = WINDOW_CODE; // TODO: FIXME! should be offset into WindowConfig!!!

void WindowUpdateDisasmSize()
{
  g_nDisasmWinHeight = (MAX_DISPLAY_LINES - g_nConsoleDisplayLines) / 2;
  g_nDisasmCurLine = MAX(0, (g_nDisasmWinHeight - 1) / 2);
}

void WindowUpdateConsoleDisplayedSize()
{
  g_nConsoleDisplayLines = MIN_DISPLAY_CONSOLE_LINES;
#if USE_APPLE_FONT
  g_bConsoleFullWidth = true;
  g_nConsoleDisplayWidth = CONSOLE_WIDTH - 1;

  if (g_iWindowThis == WINDOW_CONSOLE)
  {
    g_nConsoleDisplayLines = MAX_DISPLAY_LINES;
    g_nConsoleDisplayWidth = CONSOLE_WIDTH - 1;
    g_bConsoleFullWidth = true;
  }
#else
  g_nConsoleDisplayWidth = (CONSOLE_WIDTH / 2) + 10;
  g_bConsoleFullWidth = false;

  //	g_bConsoleFullWidth = false;
  //	g_nConsoleDisplayWidth = CONSOLE_WIDTH - 10;

  if (g_iWindowThis == WINDOW_CONSOLE)
  {
    g_nConsoleDisplayLines = MAX_DISPLAY_LINES;
    g_nConsoleDisplayWidth = CONSOLE_WIDTH - 1;
    g_bConsoleFullWidth = true;
  }
#endif
}

void InitDisasm()
{
  g_nDisasmCurAddress = regs.pc;
  DisasmCalcTopBotAddress();
}

void DebugInitialize()
{
  WindowUpdateDisasmSize();
  WindowUpdateConsoleDisplayedSize();

  extern bool g_bSymbolsDisplayMissingFile;
  g_bSymbolsDisplayMissingFile = false;

  g_iCommand = CMD_SYMBOLS_ROM;
  CmdSymbolsLoad(0);

  g_iCommand = CMD_SYMBOLS_APPLESOFT;
  CmdSymbolsLoad(0);

  g_iCommand = CMD_SYMBOLS_USER_1;
  CmdSymbolsLoad(0);

  g_bSymbolsDisplayMissingFile = true;
}

void DebugReset(void)
{
}

void DebugBegin()
{
  // This is called every time the debugger is entered.
  g_nAppMode = MODE_DEBUG;
  GetFrame().FrameRefreshStatus(DRAW_TITLE | DRAW_DISK_STATUS);

  if (GetMainCpu() == CPU_6502)
  {
    g_aOpcodes = & g_aOpcodes6502[ 0 ];             // Apple ][, ][+, //e
    g_aOpmodes[ AM_2 ].m_nBytes = 1;
    g_aOpmodes[ AM_3 ].m_nBytes = 1;
  }
  else
  {
    g_aOpcodes = & g_aOpcodes65C02[ 0 ];    // Enhanced Apple //e
    g_aOpmodes[ AM_2 ].m_nBytes = 2;
    g_aOpmodes[ AM_3 ].m_nBytes = 3;
  }

  InitDisasm();
}


DWORD extbench = 0;

// NOTE: BreakpointSource_t and g_aBreakpointSource must match!
const char *g_aBreakpointSource[ NUM_BREAKPOINT_SOURCES ] =
  {     // Used to be one char, since ArgsCook also uses // TODO/FIXME: Parser use Param[] ?
   // Used for both Input & Output!
   // Regs
   "A", // Reg A
   "X", // Reg X
   "Y", // Reg Y
   // Special
   "PC", // Program Counter -- could use "$"
   "S" , // Stack Pointer
   // Flags -- .8 Moved: Flag names from g_aFlagNames[] to "inlined" g_aBreakpointSource[]
   "P", // Processor Status
   "C", // ---- ---1 Carry
   "Z", // ---- --1- Zero
   "I", // ---- -1-- Interrupt
   "D", // ---- 1--- Decimal
   "B", // ---1 ---- Break
   "R", // --1- ---- Reserved
   "V", // -1-- ---- Overflow
   "N", // 1--- ---- Sign
   // Misc
   "OP", // Opcode/Instruction/Mnemonic
   "M", // Mem RW
   "M", // Mem READ_ONLY
   "M", // Mem WRITE_ONLY
   // TODO: M0 ram bank 0, M1 aux ram ?
  };

WORD g_nDisasmTopAddress = 0;
WORD g_nDisasmBotAddress = 0;
WORD g_nDisasmCurAddress = 0;

bool g_bDisasmCurBad    = false;
int  g_nDisasmCurLine   = 0; // Aligned to Top or Center
int  g_iDisasmCurState = CURSOR_NORMAL;

int  g_nDisasmWinHeight = 0;

MemorySearchResults_t g_vMemorySearchResults;

int g_iCommand; // last command (enum) // used for consecutive commands

bool  g_bConfigDisasmAddressView   = true;
int   g_bConfigDisasmClick         = 4; // GH#462 alt=1, ctrl=2, shift=4 bitmask (default to Shift-Click)
bool  g_bConfigDisasmAddressColon  = true;
bool  g_bConfigDisasmOpcodesView   = true;
bool  g_bConfigDisasmOpcodeSpaces  = true;
int   g_iConfigDisasmTargets       = DISASM_TARGET_BOTH;
int   g_iConfigDisasmBranchType    = DISASM_BRANCH_FANCY;
int   g_bConfigDisasmImmediateChar = DISASM_IMMED_BOTH;
int   g_iConfigDisasmScroll        = 3; // favor 3 byte opcodes
// Config - Info
bool  g_bConfigInfoTargetPointer   = false;

MemoryTextFile_t g_ConfigState;

int FindParam(LPCTSTR pLookupName, Match_e eMatch, int & iParam_, int iParamBegin, int iParamEnd )
{
  throw std::runtime_error("FindParam: not implemented");
}

void DebugDisplay ( BOOL bInitDisasm )
{
  throw std::runtime_error("DebugDisplay: not implemented");
}

Update_t Help_Arg_1( int iCommandHelp )
{
  throw std::runtime_error("Help_Arg_1: not implemented");
}
