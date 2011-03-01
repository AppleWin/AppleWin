/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 1994-1996, Michael O'Brien
Copyright (C) 1999-2001, Oliver Schmidt
Copyright (C) 2002-2005, Tom Charlesworth
Copyright (C) 2006-2010, Tom Charlesworth, Michael Pohoreski

AppleWin is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

AppleWin is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with AppleWin; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/* Description: 6502/65C02 emulation
 *
 * Author: Various
 */

// TO DO:
// . All these CPP macros need to be converted to inline funcs

// TeaRex's Note about illegal opcodes:
// ------------------------------------
// . I've followed the names and descriptions given in
// . "Extra Instructions Of The 65XX Series CPU"
// . by Adam Vardy, dated Sept 27, 1996.
// . The exception is, what he calls "SKB" and "SKW" I call "NOP",
// . for consistency's sake. Several other naming conventions exist.
// . Of course, only the 6502 has illegal opcodes, the 65C02 doesn't.
// . Thus they're not emulated in Enhanced //e mode. Games relying on them
// . don't run on a real Enhanced //e either. The old mixture of 65C02
// . emulation and skipping the right number of bytes for illegal 6502
// . opcodes, while working surprisingly well in practice, was IMHO
// . ill-founded in theory and has thus been removed.


// Note about bSlowerOnPagecross:
// -------------------
// . This is used to determine if a cycle needs to be added for a page-crossing.
//
// Modes that are affected:
// . ABS,X; ABS,Y; (IND),Y
//
// The following opcodes (when indexed) add a cycle if page is crossed:
// . ADC, AND, Bxx, CMP, EOR, LDA, LDX, LDY, ORA, SBC
// . NB. Those opcode that DO NOT write to memory.
// . 65C02: JMP (ABS-INDIRECT): 65C02 fixes JMP ($xxFF) bug but needs extra cycle in that case
// . 65C02: JMP (ABS-INDIRECT,X): Probably. Currently unimplemented.
//
// The following opcodes (when indexed)	 DO NOT add a cycle if page is crossed:
// . ASL, DEC, INC, LSR, ROL, ROR, STA, STX, STY
// . NB. Those opcode that DO write to memory.
//
// What about these:
// . 65C02: STZ?, TRB?, TSB?
// . Answer: TRB & TSB don't have affected adressing modes
// .         STZ probably doesn't add a cycle since otherwise it would be slower than STA which doesn't make sense.
//
// NB. 'Zero-page indexed' opcodes wrap back to zero-page.
// .   The same goes for all the zero-page indirect modes.
//
// NB2. bSlowerOnPagecross can't be used for r/w detection, as these
// .    opcodes don't init this flag:
// . $EC CPX ABS (since there's no addressing mode of CPY which has variable cycle number)
// . $CC CPY ABS (same)
//
// 65C02 info:
// .  Read-modify-write instructions abs indexed in same page take 6 cycles (cf. 7 cycles for 6502)
// .  ASL, DEC, INC, LSR, ROL, ROR
// .  This should work now (but makes bSlowerOnPagecross even less useful for r/w detection)
//
// . Thanks to Scott Hemphill for the verified CMOS ADC and SBC algorithm! You rock.
// . And thanks to the VICE team for the NMOS ADC and SBC algorithms as well as the
// . algorithms for those illops which involve ADC or SBC. You rock too.


#include "StdAfx.h"
#include "MouseInterface.h"

#ifdef SUPPORT_CPM
#include "z80emu.h"
#include "Z80VICE\z80.h"
#include "Z80VICE\z80mem.h"
#endif

#ifdef USE_SPEECH_API
#include "Speech.h"
#endif

#define	 AF_SIGN       0x80
#define	 AF_OVERFLOW   0x40
#define	 AF_RESERVED   0x20
#define	 AF_BREAK      0x10
#define	 AF_DECIMAL    0x08
#define	 AF_INTERRUPT  0x04
#define	 AF_ZERO       0x02
#define	 AF_CARRY      0x01

#define	 SHORTOPCODES  22
#define	 BENCHOPCODES  33

// What is this 6502 code?
static BYTE benchopcode[BENCHOPCODES] = {0x06,0x16,0x24,0x45,0x48,0x65,0x68,0x76,
				  0x84,0x85,0x86,0x91,0x94,0xA4,0xA5,0xA6,
				  0xB1,0xB4,0xC0,0xC4,0xC5,0xE6,
				  0x19,0x6D,0x8D,0x99,0x9D,0xAD,0xB9,0xBD,
				  0xDD,0xED,0xEE};

regsrec regs;
unsigned __int64 g_nCumulativeCycles = 0;

static ULONG g_nCyclesExecuted;	// # of cycles executed up to last IO access

//static signed long g_uInternalExecutedCycles;
// TODO: Use IRQ_CHECK_TIMEOUT=128 when running at full-speed else with IRQ_CHECK_TIMEOUT=1
// - What about when running benchmark?
static const int IRQ_CHECK_TIMEOUT = 128;
static signed int g_nIrqCheckTimeout = IRQ_CHECK_TIMEOUT;

//

// Assume all interrupt sources assert until the device is told to stop:
// - eg by r/w to device's register or a machine reset

static bool g_bCritSectionValid = false;	// Deleting CritialSection when not valid causes crash on Win98
static CRITICAL_SECTION g_CriticalSection;	// To guard /g_bmIRQ/ & /g_bmNMI/
static volatile UINT32 g_bmIRQ = 0;
static volatile UINT32 g_bmNMI = 0;
static volatile BOOL g_bNmiFlank = FALSE; // Positive going flank on NMI line

#include "cpu/cpu_general.inl"

#include "cpu/cpu_instructions.inl"

void RequestDebugger()
{
	// BUG: This causes DebugBegin to constantly be called.
	// It's as if the WM_KEYUP are auto-repeating?
	//   FrameWndProc()
	//      ProcessButtonClick()
	//         DebugBegin()
	//	PostMessage( g_hFrameWindow, WM_KEYDOWN, DEBUG_TOGGLE_KEY, 0 );
	//	PostMessage( g_hFrameWindow, WM_KEYUP  , DEBUG_TOGGLE_KEY, 0 );

	// Not a valid solution, since hitting F7 (to exit) debugger gets the debugger out of sync
	// due to EnterMessageLoop() calling ContinueExecution() after the mode has changed to DEBUG.
	//	DebugBegin();

	FrameWndProc( g_hFrameWindow, WM_KEYDOWN, DEBUG_TOGGLE_KEY, 0 );
	FrameWndProc( g_hFrameWindow, WM_KEYUP  , DEBUG_TOGGLE_KEY, 0 );
}

// Break into debugger on invalid opcodes
#define INV IsDebugBreakOnInvalid(AM_1);

/****************************************************************************
*
*  OPCODE TABLE
*
***/

unsigned __int64 g_nCycleIrqStart;
unsigned __int64 g_nCycleIrqEnd;
UINT g_nCycleIrqTime;

UINT g_nIdx = 0;
const UINT BUFFER_SIZE = 4096;	// 80 secs
UINT g_nBuffer[BUFFER_SIZE];
UINT g_nMean = 0;
UINT g_nMin = 0xFFFFFFFF;
UINT g_nMax = 0;

static __forceinline void DoIrqProfiling(DWORD uCycles)
{
#ifdef _DEBUG
	if(regs.ps & AF_INTERRUPT)
		return;		// Still in Apple's ROM

	g_nCycleIrqEnd = g_nCumulativeCycles + uCycles;
	g_nCycleIrqTime = (UINT) (g_nCycleIrqEnd - g_nCycleIrqStart);

	if(g_nCycleIrqTime > g_nMax) g_nMax = g_nCycleIrqTime;
	if(g_nCycleIrqTime < g_nMin) g_nMin = g_nCycleIrqTime;

	if(g_nIdx == BUFFER_SIZE)
		return;

	g_nBuffer[g_nIdx] = g_nCycleIrqTime;
	g_nIdx++;

	if(g_nIdx == BUFFER_SIZE)
	{
		UINT nTotal = 0;
		for(UINT i=0; i<BUFFER_SIZE; i++)
			nTotal += g_nBuffer[i];

		g_nMean = nTotal / BUFFER_SIZE;
	}
#endif
}

//===========================================================================

BYTE CpuRead(USHORT addr, ULONG uExecutedCycles)
{
	return READ;
}

void CpuWrite(USHORT addr, BYTE a, ULONG uExecutedCycles)
{
	WRITE(a);
}

//===========================================================================

#ifdef USE_SPEECH_API

const USHORT COUT = 0xFDED;

const UINT OUTPUT_BUFFER_SIZE = 256;
char g_OutputBuffer[OUTPUT_BUFFER_SIZE+1+1];	// +1 for EOL, +1 for NULL
UINT OutputBufferIdx = 0;
bool bEscMode = false;

void CaptureCOUT(void)
{
	const char ch = regs.a & 0x7f;

	if (ch == 0x07)			// Bell
	{
		// Ignore
	}
	else if (ch == 0x08)	// Backspace
	{
		if (OutputBufferIdx)
			OutputBufferIdx--;
	}
	else if (ch == 0x0A)	// LF
	{
		// Ignore
	}
	else if (ch == 0x0D)	// CR
	{
		if (bEscMode)
		{
			bEscMode = false;
		}
		else if (OutputBufferIdx)
		{
			g_OutputBuffer[OutputBufferIdx] = 0;
			g_Speech.Speak(g_OutputBuffer);

#ifdef _DEBUG
			g_OutputBuffer[OutputBufferIdx] = '\n';
			g_OutputBuffer[OutputBufferIdx+1] = 0;
			OutputDebugString(g_OutputBuffer);
#endif

			OutputBufferIdx = 0;
		}
	}
	else if (ch == 0x1B)	// Escape
	{
		bEscMode = bEscMode ? false : true;		// Toggle mode
	}
	else if (ch >= ' ' && ch <= '~')
	{
		if (OutputBufferIdx < OUTPUT_BUFFER_SIZE && !bEscMode)
			g_OutputBuffer[OutputBufferIdx++] = ch;
	}
}

#endif

//===========================================================================

static __forceinline int Fetch(BYTE& iOpcode, ULONG uExecutedCycles)
{
	const USHORT PC = regs.pc;

	iOpcode = ((PC & 0xF000) == 0xC000)
	    ? IORead[(PC>>4) & 0xFF](PC,PC,0,0,uExecutedCycles)	// Fetch opcode from I/O memory, but params are still from mem[]
		: *(mem+PC);

	if (IsDebugBreakOpcode( iOpcode ))
		return 0;

#ifdef USE_SPEECH_API
	if (PC == COUT && g_Speech.IsEnabled() && !g_bFullSpeed)
		CaptureCOUT();
#endif

	regs.pc++;
	return 1;
}

//#define ENABLE_NMI_SUPPORT	// Not used - so don't enable
static __forceinline void NMI(ULONG& uExecutedCycles, UINT& uExtraCycles, BOOL& flagc, BOOL& flagn, BOOL& flagv, BOOL& flagz)
{
#ifdef ENABLE_NMI_SUPPORT
	if(g_bNmiFlank)
	{
		// NMI signals are only serviced once
		g_bNmiFlank = FALSE;
		g_nCycleIrqStart = g_nCumulativeCycles + uExecutedCycles;
		PUSH(regs.pc >> 8)
		PUSH(regs.pc & 0xFF)
		EF_TO_AF
		PUSH(regs.ps & ~AF_BREAK)
		regs.ps = regs.ps | AF_INTERRUPT & ~AF_DECIMAL;
		regs.pc = * (WORD*) (mem+0xFFFA);
		CYC(7)
	}
#endif
}

static __forceinline void IRQ(ULONG& uExecutedCycles, UINT& uExtraCycles, BOOL& flagc, BOOL& flagn, BOOL& flagv, BOOL& flagz)
{
	if(g_bmIRQ && !(regs.ps & AF_INTERRUPT))
	{
		// IRQ signals are deasserted when a specific r/w operation is done on device
		g_nCycleIrqStart = g_nCumulativeCycles + uExecutedCycles;
		PUSH(regs.pc >> 8)
		PUSH(regs.pc & 0xFF)
		EF_TO_AF
		PUSH(regs.ps & ~AF_BREAK)
		regs.ps = regs.ps | AF_INTERRUPT & ~AF_DECIMAL;
		regs.pc = * (WORD*) (mem+0xFFFE);
		CYC(7)
	}
}

static __forceinline void CheckInterruptSources(ULONG uExecutedCycles)
{
	if (g_nIrqCheckTimeout < 0)
	{
		MB_UpdateCycles(uExecutedCycles);
		sg_Mouse.SetVBlank(VideoGetVbl(uExecutedCycles));
		g_nIrqCheckTimeout = IRQ_CHECK_TIMEOUT;
	}
}

//===========================================================================

#include "cpu/cpu6502.h"  // MOS 6502
#include "cpu/cpu65C02.h" // WDC 65C02
#include "cpu/cpu65d02.h" // Debug CPU Memory Visualizer

//===========================================================================

static DWORD InternalCpuExecute (DWORD uTotalCycles)
{
	if (IS_APPLE2 || (g_Apple2Type == A2TYPE_APPLE2E))
		return Cpu6502(uTotalCycles);	// Apple ][, ][+, //e
	else
		return Cpu65C02(uTotalCycles);	// Enhanced Apple //e
}

//
// ----- ALL GLOBALLY ACCESSIBLE FUNCTIONS ARE BELOW THIS LINE -----
//

//===========================================================================

void CpuDestroy ()
{
	if (g_bCritSectionValid)
	{
  		DeleteCriticalSection(&g_CriticalSection);
		g_bCritSectionValid = false;
	}
}

//===========================================================================

// Description:
//	Call this when an IO-reg is access & accurate cycle info is needed
// Pre:
//  nExecutedCycles = # of cycles executed by Cpu6502() or Cpu65C02() for this iteration of ContinueExecution()
// Post:
//	g_nCyclesExecuted
//	g_nCumulativeCycles
//
void CpuCalcCycles(const ULONG nExecutedCycles)
{
	// Calc # of cycles executed since this func was last called
	const ULONG nCycles = nExecutedCycles - g_nCyclesExecuted;
	_ASSERT( (LONG)nCycles >= 0 );
	g_nCumulativeCycles += nCycles;

	g_nCyclesExecuted = nExecutedCycles;
}

//===========================================================================

// Old method with g_uInternalExecutedCycles runs faster!
//        Old     vs    New
// - 68.0,69.0MHz vs  66.7, 67.2MHz  (with check for VBL IRQ every opcode)
// - 89.6,88.9MHz vs  87.2, 87.9MHz  (without check for VBL IRQ)
// -                  75.9, 78.5MHz  (with check for VBL IRQ every 128 cycles)
// -                 137.9,135.6MHz  (with check for VBL IRQ & MB_Update every 128 cycles)

#if 0	// TODO: Measure perf increase by using this new method
ULONG CpuGetCyclesThisVideoFrame(ULONG)	// Old func using g_uInternalExecutedCycles
{
	CpuCalcCycles(g_uInternalExecutedCycles);
	return g_dwCyclesThisFrame + g_nCyclesExecuted;
}
#else
ULONG CpuGetCyclesThisVideoFrame(const ULONG nExecutedCycles)
{
	CpuCalcCycles(nExecutedCycles);
	return g_dwCyclesThisFrame + g_nCyclesExecuted;
}
#endif

//===========================================================================

DWORD CpuExecute(const DWORD uCycles)
{
	g_nCyclesExecuted =	0;

	MB_StartOfCpuExecute();

	// uCycles:
	//  =0  : Do single step
	//  >0  : Do multi-opcode emulation
	const DWORD uExecutedCycles = InternalCpuExecute(uCycles);

	MB_UpdateCycles(uExecutedCycles);	// Update 6522s (NB. Do this before updating g_nCumulativeCycles below)

	//

	const UINT nRemainingCycles = uExecutedCycles - g_nCyclesExecuted;
	g_nCumulativeCycles	+= nRemainingCycles;

	return uExecutedCycles;
}

//===========================================================================

void CpuInitialize ()
{
	CpuDestroy();
	regs.a = regs.x = regs.y = regs.ps = 0xFF;
	regs.sp = 0x01FF;
	CpuReset();	// Init's ps & pc. Updates sp

	InitializeCriticalSection(&g_CriticalSection);
	g_bCritSectionValid = true;
	CpuIrqReset();
	CpuNmiReset();

#ifdef SUPPORT_CPM
	z80mem_initialize();
	z80_reset();
#endif
}

//===========================================================================

void CpuSetupBenchmark ()
{
	regs.a  = 0;
	regs.x  = 0;
	regs.y  = 0;
	regs.pc = 0x300;
	regs.sp = 0x1FF;

	// CREATE CODE SEGMENTS CONSISTING OF GROUPS OF COMMONLY-USED OPCODES
	{
		int addr   = 0x300;
		int opcode = 0;
		do
		{
			*(mem+addr++) = benchopcode[opcode];
			*(mem+addr++) = benchopcode[opcode];

			if (opcode >= SHORTOPCODES)
				*(mem+addr++) = 0;

			if ((++opcode >= BENCHOPCODES) || ((addr & 0x0F) >= 0x0B))
			{
				*(mem+addr++) = 0x4C;
				*(mem+addr++) = (opcode >= BENCHOPCODES) ? 0x00 : ((addr >> 4)+1) << 4;
				*(mem+addr++) = 0x03;
				while (addr & 0x0F)
					++addr;
			}
		} while (opcode < BENCHOPCODES);
	}
}

//===========================================================================

void CpuIrqReset()
{
	_ASSERT(g_bCritSectionValid);
	if (g_bCritSectionValid) EnterCriticalSection(&g_CriticalSection);
	g_bmIRQ = 0;
	if (g_bCritSectionValid) LeaveCriticalSection(&g_CriticalSection);
}

void CpuIrqAssert(eIRQSRC Device)
{
	_ASSERT(g_bCritSectionValid);
	if (g_bCritSectionValid) EnterCriticalSection(&g_CriticalSection);
	g_bmIRQ |= 1<<Device;
	if (g_bCritSectionValid) LeaveCriticalSection(&g_CriticalSection);
}

void CpuIrqDeassert(eIRQSRC Device)
{
	_ASSERT(g_bCritSectionValid);
	if (g_bCritSectionValid) EnterCriticalSection(&g_CriticalSection);
	g_bmIRQ &= ~(1<<Device);
	if (g_bCritSectionValid) LeaveCriticalSection(&g_CriticalSection);
}

//===========================================================================

void CpuNmiReset()
{
	_ASSERT(g_bCritSectionValid);
	if (g_bCritSectionValid) EnterCriticalSection(&g_CriticalSection);
	g_bmNMI = 0;
	g_bNmiFlank = FALSE;
	if (g_bCritSectionValid) LeaveCriticalSection(&g_CriticalSection);
}

void CpuNmiAssert(eIRQSRC Device)
{
	_ASSERT(g_bCritSectionValid);
	if (g_bCritSectionValid) EnterCriticalSection(&g_CriticalSection);
	if (g_bmNMI == 0) // NMI line is just becoming active
	    g_bNmiFlank = TRUE;
	g_bmNMI |= 1<<Device;
	if (g_bCritSectionValid) LeaveCriticalSection(&g_CriticalSection);
}

void CpuNmiDeassert(eIRQSRC Device)
{
	_ASSERT(g_bCritSectionValid);
	if (g_bCritSectionValid) EnterCriticalSection(&g_CriticalSection);
	g_bmNMI &= ~(1<<Device);
	if (g_bCritSectionValid) LeaveCriticalSection(&g_CriticalSection);
}

//===========================================================================

void CpuReset()
{
	// 7 cycles
	regs.ps = (regs.ps | AF_INTERRUPT) & ~AF_DECIMAL;
	regs.pc = * (WORD*) (mem+0xFFFC);
	regs.sp = 0x0100 | ((regs.sp - 3) & 0xFF);

	regs.bJammed = 0;

#ifdef SUPPORT_CPM
	g_ActiveCPU = CPU_6502;
	z80_reset();
#endif
}

//===========================================================================

DWORD CpuGetSnapshot(SS_CPU6502* pSS)
{
	pSS->A = regs.a;
	pSS->X = regs.x;
	pSS->Y = regs.y;
	pSS->P = regs.ps | AF_RESERVED | AF_BREAK;
	pSS->S = (BYTE) (regs.sp & 0xff);
	pSS->PC = regs.pc;
	pSS->g_nCumulativeCycles = g_nCumulativeCycles;

	return 0;
}

DWORD CpuSetSnapshot(SS_CPU6502* pSS)
{
	regs.a = pSS->A;
	regs.x = pSS->X;
	regs.y = pSS->Y;
	regs.ps = pSS->P | AF_RESERVED | AF_BREAK;
	regs.sp = (USHORT)pSS->S | 0x100;
	regs.pc = pSS->PC;
	CpuIrqReset();
	CpuNmiReset();
	g_nCumulativeCycles = pSS->g_nCumulativeCycles;

	return 0;
}
