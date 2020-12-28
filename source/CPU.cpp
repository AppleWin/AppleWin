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
// . Answer: TRB & TSB don't have affected addressing modes
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

#include "CPU.h"
#include "Core.h"
#include "CardManager.h"
#include "Memory.h"
#include "Mockingboard.h"
#include "MouseInterface.h"
#ifdef USE_SPEECH_API
#include "Speech.h"
#endif
#include "SynchronousEventManager.h"
#include "NTSC.h"
#include "Log.h"

#include "z80emu.h"
#include "Z80VICE/z80.h"
#include "Z80VICE/z80mem.h"

#include "YamlHelper.h"

#define LOG_IRQ_TAKEN_AND_RTI 0

// 6502 Accumulator Bit Flags
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

// What is this 6502 code? Compressed 6502 code -- see: CpuSetupBenchmark()
static BYTE benchopcode[BENCHOPCODES] = {
	0x06,0x16,0x24,0x45,0x48,0x65,0x68,0x76,
	0x84,0x85,0x86,0x91,0x94,0xA4,0xA5,0xA6,
	0xB1,0xB4,0xC0,0xC4,0xC5,0xE6,
	0x19,0x6D,0x8D,0x99,0x9D,0xAD,0xB9,0xBD,
	0xDD,0xED,0xEE
};

regsrec regs;
unsigned __int64 g_nCumulativeCycles = 0;

static ULONG g_nCyclesExecuted;	// # of cycles executed up to last IO access
//static signed long g_uInternalExecutedCycles;

//

// Assume all interrupt sources assert until the device is told to stop:
// - eg by r/w to device's register or a machine reset

static bool g_bCritSectionValid = false;	// Deleting CritialSection when not valid causes crash on Win98
static CRITICAL_SECTION g_CriticalSection;	// To guard /g_bmIRQ/ & /g_bmNMI/
static volatile UINT32 g_bmIRQ = 0;
static volatile UINT32 g_bmNMI = 0;
static volatile BOOL g_bNmiFlank = FALSE; // Positive going flank on NMI line

static bool g_irqDefer1Opcode = false;

//

static eCpuType g_MainCPU = CPU_65C02;
static eCpuType g_ActiveCPU = CPU_65C02;

eCpuType GetMainCpu(void)
{
	return g_MainCPU;
}

void SetMainCpu(eCpuType cpu)
{
	_ASSERT(cpu != CPU_Z80);
	if (cpu == CPU_Z80)
		return;

	g_MainCPU = cpu;
}

static bool IsCpu65C02(eApple2Type apple2Type)
{
	// NB. All Pravets clones are 6502 (GH#307)
	return (apple2Type == A2TYPE_APPLE2EENHANCED) || (apple2Type == A2TYPE_TK30002E) || (apple2Type & A2TYPE_APPLE2C); 
}

eCpuType ProbeMainCpuDefault(eApple2Type apple2Type)
{
	return IsCpu65C02(apple2Type) ? CPU_65C02 : CPU_6502;
}

void SetMainCpuDefault(eApple2Type apple2Type)
{
	SetMainCpu( ProbeMainCpuDefault(apple2Type) );
}

eCpuType GetActiveCpu(void)
{
	return g_ActiveCPU;
}

void SetActiveCpu(eCpuType cpu)
{
	g_ActiveCPU = cpu;
}

bool Is6502InterruptEnabled(void)
{
	return !(regs.ps & AF_INTERRUPT);
}

void ResetCyclesExecutedForDebugger(void)
{
	g_nCyclesExecuted = 0;
}

//

#include "CPU/cpu_general.inl"
#include "CPU/cpu_instructions.inl"

/****************************************************************************
*
*  OPCODE TABLE
*
***/

#ifdef _DEBUG
static unsigned __int64 g_nCycleIrqStart;
static unsigned __int64 g_nCycleIrqEnd;
static UINT g_nCycleIrqTime;

static UINT g_nIdx = 0;
static const UINT BUFFER_SIZE = 4096;	// 80 secs
static UINT g_nBuffer[BUFFER_SIZE];
static UINT g_nMean = 0;
static UINT g_nMin = 0xFFFFFFFF;
static UINT g_nMax = 0;
#endif

static __forceinline void DoIrqProfiling(DWORD uCycles)
{
#ifdef _DEBUG
	if(regs.ps & AF_INTERRUPT)
		return;		// Still in Apple's ROM

#if LOG_IRQ_TAKEN_AND_RTI
	LogOutput("ISR-end\n\n");
#endif

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

//#define DBG_HDD_ENTRYPOINT
#if defined(_DEBUG) && defined(DBG_HDD_ENTRYPOINT)
// Output a debug msg whenever the HDD f/w is called or jump to.
static void DebugHddEntrypoint(const USHORT PC)
{
	static bool bOldPCAtC7xx = false;
	static WORD OldPC = 0;
	static UINT Count = 0;

	if ((PC >> 8) == 0xC7)
	{
		if (!bOldPCAtC7xx /*&& PC != 0xc70a*/)
		{
			Count++;
			char szDebug[100];
			sprintf(szDebug, "HDD Entrypoint: $%04X\n", PC);
			OutputDebugString(szDebug);
		}

		bOldPCAtC7xx = true;
	}
	else
	{
		bOldPCAtC7xx = false;
	}
	OldPC = PC;
}
#endif

static __forceinline void Fetch(BYTE& iOpcode, ULONG uExecutedCycles)
{
	const USHORT PC = regs.pc;

#if defined(_DEBUG) && defined(DBG_HDD_ENTRYPOINT)
	DebugHddEntrypoint(PC);
#endif

	iOpcode = ((PC & 0xF000) == 0xC000)
	    ? IORead[(PC>>4) & 0xFF](PC,PC,0,0,uExecutedCycles)	// Fetch opcode from I/O memory, but params are still from mem[]
		: *(mem+PC);

#ifdef USE_SPEECH_API
	if (PC == COUT && g_Speech.IsEnabled() && !g_bFullSpeed)
		CaptureCOUT();
#endif

	regs.pc++;
}

//#define ENABLE_NMI_SUPPORT	// Not used - so don't enable
static __forceinline void NMI(ULONG& uExecutedCycles, BOOL& flagc, BOOL& flagn, BOOL& flagv, BOOL& flagz)
{
#ifdef ENABLE_NMI_SUPPORT
	if(g_bNmiFlank)
	{
		// NMI signals are only serviced once
		g_bNmiFlank = FALSE;
#ifdef _DEBUG
		g_nCycleIrqStart = g_nCumulativeCycles + uExecutedCycles;
#endif
		PUSH(regs.pc >> 8)
		PUSH(regs.pc & 0xFF)
		EF_TO_AF
		PUSH(regs.ps & ~AF_BREAK)
		regs.ps = regs.ps | AF_INTERRUPT & ~AF_DECIMAL;
		regs.pc = * (WORD*) (mem+0xFFFA);
		UINT uExtraCycles = 0;	// Needed for CYC(a) macro
		CYC(7)
	}
#endif
}

static __forceinline void CheckSynchronousInterruptSources(UINT cycles, ULONG uExecutedCycles)
{
	g_SynchronousEventMgr.Update(cycles, uExecutedCycles);
}

// NB. No need to save to save-state, as IRQ() follows CheckSynchronousInterruptSources(), and IRQ() always sets it to false.
bool g_irqOnLastOpcodeCycle = false;

static __forceinline void IRQ(ULONG& uExecutedCycles, BOOL& flagc, BOOL& flagn, BOOL& flagv, BOOL& flagz)
{
	if(g_bmIRQ && !(regs.ps & AF_INTERRUPT))
	{
		// if 6522 interrupt occurs on opcode's last cycle, then defer IRQ by 1 opcode
		if (g_irqOnLastOpcodeCycle && !g_irqDefer1Opcode)
		{
			g_irqOnLastOpcodeCycle = false;
			g_irqDefer1Opcode = true;	// if INT occurs again on next opcode, then do NOT defer
			return;
		}

		g_irqDefer1Opcode = false;

		// IRQ signals are deasserted when a specific r/w operation is done on device
#ifdef _DEBUG
		g_nCycleIrqStart = g_nCumulativeCycles + uExecutedCycles;
#endif
		PUSH(regs.pc >> 8)
		PUSH(regs.pc & 0xFF)
		EF_TO_AF
		PUSH(regs.ps & ~AF_BREAK)
		regs.ps = (regs.ps | AF_INTERRUPT) & (~AF_DECIMAL);
		regs.pc = * (WORD*) (mem+0xFFFE);
		UINT uExtraCycles = 0;	// Needed for CYC(a) macro
		CYC(7)
#if defined(_DEBUG) && LOG_IRQ_TAKEN_AND_RTI
		LogOutput("IRQ\n");
#endif
		CheckSynchronousInterruptSources(7, uExecutedCycles);
	}

	g_irqOnLastOpcodeCycle = false;
}

//===========================================================================

#define READ _READ_WITH_IO_F8xx
#define WRITE(value) _WRITE_WITH_IO_F8xx(value)
#define HEATMAP_X(address)

#include "CPU/cpu6502.h"  // MOS 6502

#undef READ
#undef WRITE

//-------

#define READ _READ
#define WRITE(value) _WRITE(value)

#include "CPU/cpu65C02.h" // WDC 65C02

#undef READ
#undef WRITE
#undef HEATMAP_X

//-----------------

#define READ Heatmap_ReadByte_With_IO_F8xx(addr, uExecutedCycles)
#define WRITE(value) Heatmap_WriteByte_With_IO_F8xx(addr, value, uExecutedCycles);

#define HEATMAP_X(address) Heatmap_X(address)

#include "CPU/cpu_heatmap.inl"

#define Cpu6502 Cpu6502_debug
#include "CPU/cpu6502.h"  // MOS 6502
#undef Cpu6502

#undef READ
#undef WRITE

//-------

#define READ Heatmap_ReadByte(addr, uExecutedCycles)
#define WRITE(value) Heatmap_WriteByte(addr, value, uExecutedCycles);

#define Cpu65C02 Cpu65C02_debug
#include "CPU/cpu65C02.h" // WDC 65C02
#undef Cpu65C02

#undef READ
#undef WRITE
#undef HEATMAP_X

//===========================================================================

static DWORD InternalCpuExecute(const DWORD uTotalCycles, const bool bVideoUpdate)
{
	if (g_nAppMode == MODE_RUNNING || g_nAppMode == MODE_BENCHMARK)
	{
		if (GetMainCpu() == CPU_6502)
			return Cpu6502(uTotalCycles, bVideoUpdate);		// Apple ][, ][+, //e, Clones
		else
			return Cpu65C02(uTotalCycles, bVideoUpdate);	// Enhanced Apple //e
	}
	else
	{
		_ASSERT(g_nAppMode == MODE_STEPPING || g_nAppMode == MODE_DEBUG);
		if (GetMainCpu() == CPU_6502)
			return Cpu6502_debug(uTotalCycles, bVideoUpdate);	// Apple ][, ][+, //e, Clones
		else
			return Cpu65C02_debug(uTotalCycles, bVideoUpdate);	// Enhanced Apple //e
	}
}

//
// ----- ALL GLOBALLY ACCESSIBLE FUNCTIONS ARE BELOW THIS LINE -----
//

//===========================================================================

// Called by z80_RDMEM()
BYTE CpuRead(USHORT addr, ULONG uExecutedCycles)
{
	if (g_nAppMode == MODE_RUNNING)
	{
		return _READ_WITH_IO_F8xx;	// Superset of _READ
	}

	return Heatmap_ReadByte_With_IO_F8xx(addr, uExecutedCycles);
}

// Called by z80_WRMEM()
void CpuWrite(USHORT addr, BYTE value, ULONG uExecutedCycles)
{
	if (g_nAppMode == MODE_RUNNING)
	{
		_WRITE_WITH_IO_F8xx(value);	// Superset of _WRITE
		return;
	}

	Heatmap_WriteByte_With_IO_F8xx(addr, value, uExecutedCycles);
}

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
//	Call this when an IO-reg is accessed & accurate cycle info is needed
//  NB. Safe to call multiple times from the same IO function handler (as 'nExecutedCycles - g_nCyclesExecuted' will be zero the 2nd time)
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

DWORD CpuExecute(const DWORD uCycles, const bool bVideoUpdate)
{
#ifdef LOG_PERF_TIMINGS
	extern UINT64 g_timeCpu;
	PerfMarker perfMarker(g_timeCpu);
#endif

	g_nCyclesExecuted =	0;

#ifdef _DEBUG
	MB_CheckCumulativeCycles();
#endif

	// uCycles:
	//  =0  : Do single step
	//  >0  : Do multi-opcode emulation
	const DWORD uExecutedCycles = InternalCpuExecute(uCycles, bVideoUpdate);

	// NB. Required for normal-speed (even though 6522 is updated after every opcode), as may've finished on IRQ()
	MB_UpdateCycles(uExecutedCycles);	// Update 6522s (NB. Do this before updating g_nCumulativeCycles below)
										// NB. Ensures that 6522 regs are up-to-date for any potential save-state

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

	z80mem_initialize();
	z80_reset();
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

	g_irqDefer1Opcode = false;

	SetActiveCpu( GetMainCpu() );
	z80_reset();
}

//===========================================================================

#define SS_YAML_KEY_CPU_TYPE "Type"
#define SS_YAML_KEY_REGA "A"
#define SS_YAML_KEY_REGX "X"
#define SS_YAML_KEY_REGY "Y"
#define SS_YAML_KEY_REGP "P"
#define SS_YAML_KEY_REGS "S"
#define SS_YAML_KEY_REGPC "PC"
#define SS_YAML_KEY_CUMULATIVE_CYCLES "Cumulative Cycles"
#define SS_YAML_KEY_IRQ_DEFER_1_OPCODE "Defer IRQ By 1 Opcode"

#define SS_YAML_VALUE_6502 "6502"
#define SS_YAML_VALUE_65C02 "65C02"

static std::string CpuGetSnapshotStructName(void)
{
	static const std::string name("CPU");
	return name;
}

void CpuSaveSnapshot(YamlSaveHelper& yamlSaveHelper)
{
	regs.ps |= (AF_RESERVED | AF_BREAK);

	YamlSaveHelper::Label state(yamlSaveHelper, "%s:\n", CpuGetSnapshotStructName().c_str());	
	yamlSaveHelper.SaveString(SS_YAML_KEY_CPU_TYPE, GetMainCpu() == CPU_6502 ? SS_YAML_VALUE_6502 : SS_YAML_VALUE_65C02);
	yamlSaveHelper.SaveHexUint8(SS_YAML_KEY_REGA, regs.a);
	yamlSaveHelper.SaveHexUint8(SS_YAML_KEY_REGX, regs.x);
	yamlSaveHelper.SaveHexUint8(SS_YAML_KEY_REGY, regs.y);
	yamlSaveHelper.SaveHexUint8(SS_YAML_KEY_REGP, regs.ps);
	yamlSaveHelper.SaveHexUint8(SS_YAML_KEY_REGS, (BYTE) regs.sp);
	yamlSaveHelper.SaveHexUint16(SS_YAML_KEY_REGPC, regs.pc);
	yamlSaveHelper.SaveHexUint64(SS_YAML_KEY_CUMULATIVE_CYCLES, g_nCumulativeCycles);
	yamlSaveHelper.SaveBool(SS_YAML_KEY_IRQ_DEFER_1_OPCODE, g_irqDefer1Opcode);
}

void CpuLoadSnapshot(YamlLoadHelper& yamlLoadHelper, UINT version)
{
	if (!yamlLoadHelper.GetSubMap(CpuGetSnapshotStructName()))
		return;

	std::string cpuType = yamlLoadHelper.LoadString(SS_YAML_KEY_CPU_TYPE);
	eCpuType cpu;
	if (cpuType == SS_YAML_VALUE_6502) cpu = CPU_6502;
	else if (cpuType == SS_YAML_VALUE_65C02) cpu = CPU_65C02;
	else throw std::string("Load: Unknown main CPU type");
	SetMainCpu(cpu);

	regs.a  = (BYTE)     yamlLoadHelper.LoadUint(SS_YAML_KEY_REGA);
	regs.x  = (BYTE)     yamlLoadHelper.LoadUint(SS_YAML_KEY_REGX);
	regs.y  = (BYTE)     yamlLoadHelper.LoadUint(SS_YAML_KEY_REGY);
	regs.ps = (BYTE)     yamlLoadHelper.LoadUint(SS_YAML_KEY_REGP) | (AF_RESERVED | AF_BREAK);
	regs.sp = (USHORT) ((yamlLoadHelper.LoadUint(SS_YAML_KEY_REGS) & 0xff) | 0x100);
	regs.pc = (USHORT)   yamlLoadHelper.LoadUint(SS_YAML_KEY_REGPC);

	CpuIrqReset();
	CpuNmiReset();
	g_nCumulativeCycles = yamlLoadHelper.LoadUint64(SS_YAML_KEY_CUMULATIVE_CYCLES);

	if (version >= 5)
		g_irqDefer1Opcode = yamlLoadHelper.LoadBool(SS_YAML_KEY_IRQ_DEFER_1_OPCODE);

	yamlLoadHelper.PopMap();
}
