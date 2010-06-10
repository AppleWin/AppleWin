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

static ULONG g_nCyclesSubmitted;	// Number of cycles submitted to CpuExecute()
static ULONG g_nCyclesExecuted;

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

bool CheckDebugBreak( int iOpcode )
{
	if (g_bDebugDelayBreakCheck)
	{
		g_bDebugDelayBreakCheck = false;
		return false;
	}

	// Running at full speed? (debugger not running)
	if ((g_nAppMode != MODE_DEBUG) && (g_nAppMode != MODE_STEPPING))
	{
		if (((iOpcode == 0) && IsDebugBreakOnInvalid(0)) ||
			((g_iDebugOnOpcode) && (g_iDebugOnOpcode == iOpcode))) // User wants to enter debugger on opcode?
		{
			RequestDebugger();
			return true;
		}
	}

	return false;
}

// Break into debugger on invalid opcodes
#define INV if (IsDebugBreakOnInvalid(1)) { RequestDebugger(); bBreakOnInvalid = true; }

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

	if (CheckDebugBreak( iOpcode ))
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

static DWORD Cpu65C02 (DWORD uTotalCycles)
{
	// Optimisation:
	// . Copy the global /regs/ vars to stack-based local vars
	//   (Oliver Schmidt says this gives a performance gain, see email - The real deal: "1.10.5")
	WORD addr;
	BOOL flagc; // must always be 0 or 1, no other values allowed
	BOOL flagn; // must always be 0 or 0x80.
	BOOL flagv; // any value allowed
	BOOL flagz; // any value allowed
	WORD temp;
	WORD temp2;
	WORD val;
	AF_TO_EF
	ULONG uExecutedCycles = 0;
	BOOL bSlowerOnPagecross = 0; // Set if opcode writes to memory (eg. ASL, STA)
	WORD base;
	bool bBreakOnInvalid = false;

	do
	{
		UINT uExtraCycles = 0;
		BYTE iOpcode;

#ifdef SUPPORT_CPM
		if (g_ActiveCPU == CPU_Z80)
		{
			const UINT uZ80Cycles = z80_mainloop(uTotalCycles, uExecutedCycles); CYC(uZ80Cycles)
		}
		else
#endif
		{
		if (!Fetch(iOpcode, uExecutedCycles))
			break;

		switch (iOpcode)
		{
		case 0x00:       BRK	     CYC(7)  break;
		case 0x01:       INDX ORA	     CYC(6)  break;
		case 0x02:   INV IMM NOP	     CYC(2)  break;
		case 0x03:   INV NOP	     CYC(2)  break;
		case 0x04:       ZPG TSB	     CYC(5)  break;
		case 0x05:       ZPG ORA	     CYC(3)  break;
		case 0x06:       ZPG ASL_CMOS  CYC(5)  break;
		case 0x07:   INV NOP	     CYC(2)  break;
		case 0x08:       PHP	     CYC(3)  break;
		case 0x09:       IMM ORA	     CYC(2)  break;
		case 0x0A:       ASLA	     CYC(2)  break;
		case 0x0B:   INV NOP	     CYC(2)  break;
		case 0x0C:       ABS TSB	     CYC(6)  break;
		case 0x0D:       ABS ORA	     CYC(4)  break;
		case 0x0E:       ABS ASL_CMOS  CYC(6)  break;
		case 0x0F:   INV NOP	     CYC(2)  break;
		case 0x10:       REL BPL	     CYC(2)  break;
		case 0x11:       INDY ORA	     CYC(5)  break;
		case 0x12:       IZPG ORA	     CYC(5)  break;
		case 0x13:   INV NOP	     CYC(2)  break;
		case 0x14:       ZPG TRB	     CYC(5)  break;
		case 0x15:       ZPGX ORA	     CYC(4)  break;
		case 0x16:       ZPGX ASL_CMOS CYC(6)  break;
		case 0x17:   INV NOP	     CYC(2)  break;
		case 0x18:       CLC	     CYC(2)  break;
		case 0x19:       ABSY ORA	     CYC(4)  break;
		case 0x1A:       INA	     CYC(2)  break;
		case 0x1B:   INV NOP	     CYC(2)  break;
		case 0x1C:       ABS TRB	     CYC(6)  break;
		case 0x1D:       ABSX ORA	     CYC(4)  break;
		case 0x1E:       ABSX ASL_CMOS CYC(6)  break;
		case 0x1F:   INV NOP	     CYC(2)  break;
		case 0x20:       ABS JSR	     CYC(6)  break;
		case 0x21:       INDX AND	     CYC(6)  break;
		case 0x22:   INV IMM NOP	     CYC(2)  break;
		case 0x23:   INV NOP	     CYC(2)  break;
		case 0x24:       ZPG BIT	     CYC(3)  break;
		case 0x25:       ZPG AND	     CYC(3)  break;
		case 0x26:       ZPG ROL_CMOS  CYC(5)  break;
		case 0x27:   INV NOP	     CYC(2)  break;
		case 0x28:       PLP	     CYC(4)  break;
		case 0x29:       IMM AND	     CYC(2)  break;
		case 0x2A:       ROLA	     CYC(2)  break;
		case 0x2B:   INV NOP	     CYC(2)  break;
		case 0x2C:       ABS BIT	     CYC(4)  break;
		case 0x2D:       ABS AND	     CYC(2)  break;
		case 0x2E:       ABS ROL_CMOS  CYC(6)  break;
		case 0x2F:   INV NOP	     CYC(2)  break;
		case 0x30:       REL BMI	     CYC(2)  break;
		case 0x31:       INDY AND	     CYC(5)  break;
		case 0x32:       IZPG AND	     CYC(5)  break;
		case 0x33:   INV NOP	     CYC(2)  break;
		case 0x34:       ZPGX BIT	     CYC(4)  break;
		case 0x35:       ZPGX AND	     CYC(4)  break;
		case 0x36:       ZPGX ROL_CMOS CYC(6)  break;
		case 0x37:   INV NOP	     CYC(2)  break;
		case 0x38:       SEC	     CYC(2)  break;
		case 0x39:       ABSY AND	     CYC(4)  break;
		case 0x3A:       DEA	     CYC(2)  break;
		case 0x3B:   INV NOP	     CYC(2)  break;
		case 0x3C:       ABSX BIT	     CYC(4)  break;
		case 0x3D:       ABSX AND	     CYC(4)  break;
		case 0x3E:       ABSX ROL_CMOS CYC(6)  break;
		case 0x3F:   INV NOP	     CYC(2)  break;
		case 0x40:       RTI	     CYC(6)  DoIrqProfiling(uExecutedCycles); break;
		case 0x41:       INDX EOR	     CYC(6)  break;
		case 0x42:   INV IMM NOP	     CYC(2)  break;
		case 0x43:   INV NOP	     CYC(2)  break;
		case 0x44:   INV ZPG NOP	     CYC(3)  break;
		case 0x45:       ZPG EOR	     CYC(3)  break;
		case 0x46:       ZPG LSR_CMOS  CYC(5)  break;
		case 0x47:   INV NOP	     CYC(2)  break;
		case 0x48:       PHA	     CYC(3)  break;
		case 0x49:       IMM EOR	     CYC(2)  break;
		case 0x4A:       LSRA	     CYC(2)  break;
		case 0x4B:   INV NOP	     CYC(2)  break;
		case 0x4C:       ABS JMP	     CYC(3)  break;
		case 0x4D:       ABS EOR	     CYC(4)  break;
		case 0x4E:       ABS LSR_CMOS  CYC(6)  break;
		case 0x4F:   INV NOP	     CYC(2)  break;
		case 0x50:       REL BVC	     CYC(2)  break;
		case 0x51:       INDY EOR	     CYC(5)  break;
		case 0x52:       IZPG EOR	     CYC(5)  break;
		case 0x53:   INV NOP	     CYC(2)  break;
		case 0x54:   INV ZPGX NOP	     CYC(4)  break;
		case 0x55:       ZPGX EOR	     CYC(4)  break;
		case 0x56:       ZPGX LSR_CMOS CYC(6)  break;
		case 0x57:   INV NOP	     CYC(2)  break;
		case 0x58:       CLI	     CYC(2)  break;
		case 0x59:       ABSY EOR	     CYC(4)  break;
		case 0x5A:       PHY	     CYC(3)  break;
		case 0x5B:   INV NOP	     CYC(2)  break;
		case 0x5C:   INV ABSX NOP	     CYC(8)  break;
		case 0x5D:       ABSX EOR	     CYC(4)  break;
		case 0x5E:       ABSX LSR_CMOS CYC(6)  break;
		case 0x5F:   INV NOP	     CYC(2)  break;
		case 0x60:       RTS	     CYC(6)  break;
		case 0x61:       INDX ADC_CMOS CYC(6)  break;
		case 0x62:   INV IMM NOP	     CYC(2)  break;
		case 0x63:   INV NOP	     CYC(2)  break;
		case 0x64:       ZPG STZ	     CYC(3)  break;
		case 0x65:       ZPG ADC_CMOS  CYC(3)  break;
		case 0x66:       ZPG ROR_CMOS  CYC(5)  break;
		case 0x67:   INV NOP	     CYC(2)  break;
		case 0x68:       PLA	     CYC(4)  break;
		case 0x69:       IMM ADC_CMOS  CYC(2)  break;
		case 0x6A:       RORA	     CYC(2)  break;
		case 0x6B:   INV NOP	     CYC(2)  break;
		case 0x6C:       IABSCMOS JMP  CYC(6)  break;
		case 0x6D:       ABS ADC_CMOS  CYC(4)  break;
		case 0x6E:       ABS ROR_CMOS  CYC(6)  break;
		case 0x6F:   INV NOP	     CYC(2)  break;
		case 0x70:       REL BVS	     CYC(2)  break;
		case 0x71:       INDY ADC_CMOS CYC(5)  break;
		case 0x72:       IZPG ADC_CMOS CYC(5)  break;
		case 0x73:   INV NOP	     CYC(2)  break;
		case 0x74:       ZPGX STZ	     CYC(4)  break;
		case 0x75:       ZPGX ADC_CMOS CYC(4)  break;
		case 0x76:       ZPGX ROR_CMOS CYC(6)  break;
		case 0x77:   INV NOP	     CYC(2)  break;
		case 0x78:       SEI	     CYC(2)  break;
		case 0x79:       ABSY ADC_CMOS CYC(4)  break;
		case 0x7A:       PLY	     CYC(4)  break;
		case 0x7B:   INV NOP	     CYC(2)  break;
		case 0x7C:       IABSX JMP     CYC(6)  break;
		case 0x7D:       ABSX ADC_CMOS CYC(4)  break;
		case 0x7E:       ABSX ROR_CMOS CYC(6)  break;
		case 0x7F:   INV NOP	     CYC(2)  break;
		case 0x80:       REL BRA	     CYC(2)  break;
		case 0x81:       INDX STA	     CYC(6)  break;
		case 0x82:   INV IMM NOP	     CYC(2)  break;
		case 0x83:   INV NOP	     CYC(2)  break;
		case 0x84:       ZPG STY	     CYC(3)  break;
		case 0x85:       ZPG STA	     CYC(3)  break;
		case 0x86:       ZPG STX	     CYC(3)  break;
		case 0x87:   INV NOP	     CYC(2)  break;
		case 0x88:       DEY	     CYC(2)  break;
		case 0x89:       IMM BITI	     CYC(2)  break;
		case 0x8A:       TXA	     CYC(2)  break;
		case 0x8B:   INV NOP	     CYC(2)  break;
		case 0x8C:       ABS STY	     CYC(4)  break;
		case 0x8D:       ABS STA	     CYC(4)  break;
		case 0x8E:       ABS STX	     CYC(4)  break;
		case 0x8F:   INV NOP	     CYC(2)  break;
		case 0x90:       REL BCC	     CYC(2)  break;
		case 0x91:       INDY STA	     CYC(6)  break;
		case 0x92:       IZPG STA	     CYC(5)  break;
		case 0x93:   INV NOP	     CYC(2)  break;
		case 0x94:       ZPGX STY	     CYC(4)  break;
		case 0x95:       ZPGX STA	     CYC(4)  break;
		case 0x96:       ZPGY STX	     CYC(4)  break;
		case 0x97:   INV NOP	     CYC(2)  break;
		case 0x98:       TYA	     CYC(2)  break;
		case 0x99:       ABSY STA	     CYC(5)  break;
		case 0x9A:       TXS	     CYC(2)  break;
		case 0x9B:   INV NOP	     CYC(2)  break;
		case 0x9C:       ABS STZ	     CYC(4)  break;
		case 0x9D:       ABSX STA	     CYC(5)  break;
		case 0x9E:       ABSX STZ	     CYC(5)  break;
		case 0x9F:   INV NOP	     CYC(2)  break;
		case 0xA0:       IMM LDY	     CYC(2)  break;
		case 0xA1:       INDX LDA	     CYC(6)  break;
		case 0xA2:       IMM LDX	     CYC(2)  break;
		case 0xA3:   INV NOP	     CYC(2)  break;
		case 0xA4:       ZPG LDY	     CYC(3)  break;
		case 0xA5:       ZPG LDA	     CYC(3)  break;
		case 0xA6:       ZPG LDX	     CYC(3)  break;
		case 0xA7:   INV NOP	     CYC(2)  break;
		case 0xA8:       TAY	     CYC(2)  break;
		case 0xA9:       IMM LDA	     CYC(2)  break;
		case 0xAA:       TAX	     CYC(2)  break;
		case 0xAB:   INV NOP	     CYC(2)  break;
		case 0xAC:       ABS LDY	     CYC(4)  break;
		case 0xAD:       ABS LDA	     CYC(4)  break;
		case 0xAE:       ABS LDX	     CYC(4)  break;
		case 0xAF:   INV NOP	     CYC(2)  break;
		case 0xB0:       REL BCS	     CYC(2)  break;
		case 0xB1:       INDY LDA	     CYC(5)  break;
		case 0xB2:       IZPG LDA	     CYC(5)  break;
		case 0xB3:   INV NOP	     CYC(2)  break;
		case 0xB4:       ZPGX LDY	     CYC(4)  break;
		case 0xB5:       ZPGX LDA	     CYC(4)  break;
		case 0xB6:       ZPGY LDX	     CYC(4)  break;
		case 0xB7:   INV NOP	     CYC(2)  break;
		case 0xB8:       CLV	     CYC(2)  break;
		case 0xB9:       ABSY LDA	     CYC(4)  break;
		case 0xBA:       TSX	     CYC(2)  break;
		case 0xBB:   INV NOP	     CYC(2)  break;
		case 0xBC:       ABSX LDY	     CYC(4)  break;
		case 0xBD:       ABSX LDA	     CYC(4)  break;
		case 0xBE:       ABSY LDX	     CYC(4)  break;
		case 0xBF:   INV NOP	     CYC(2)  break;
		case 0xC0:       IMM CPY	     CYC(2)  break;
		case 0xC1:       INDX CMP	     CYC(6)  break;
		case 0xC2:   INV IMM NOP	     CYC(2)  break;
		case 0xC3:   INV NOP	     CYC(2)  break;
		case 0xC4:       ZPG CPY	     CYC(3)  break;
		case 0xC5:       ZPG CMP	     CYC(3)  break;
		case 0xC6:       ZPG DEC_CMOS  CYC(5)  break;
		case 0xC7:   INV NOP	     CYC(2)  break;
		case 0xC8:       INY	     CYC(2)  break;
		case 0xC9:       IMM CMP	     CYC(2)  break;
		case 0xCA:       DEX	     CYC(2)  break;
		case 0xCB:   INV NOP	     CYC(2)  break;
		case 0xCC:       ABS CPY	     CYC(4)  break;
		case 0xCD:       ABS CMP	     CYC(4)  break;
		case 0xCE:       ABS DEC_CMOS  CYC(5)  break;
		case 0xCF:   INV NOP	     CYC(2)  break;
		case 0xD0:       REL BNE	     CYC(2)  break;
		case 0xD1:       INDY CMP	     CYC(5)  break;
		case 0xD2:       IZPG CMP	     CYC(5)  break;
		case 0xD3:   INV NOP	     CYC(2)  break;
		case 0xD4:   INV ZPGX NOP	     CYC(4)  break;
		case 0xD5:       ZPGX CMP	     CYC(4)  break;
		case 0xD6:       ZPGX DEC_CMOS CYC(6)  break;
		case 0xD7:   INV NOP	     CYC(2)  break;
		case 0xD8:       CLD	     CYC(2)  break;
		case 0xD9:       ABSY CMP	     CYC(4)  break;
		case 0xDA:       PHX	     CYC(3)  break;
		case 0xDB:   INV NOP	     CYC(2)  break;
		case 0xDC:   INV ABSX NOP	     CYC(4)  break;
		case 0xDD:       ABSX CMP	     CYC(4)  break;
		case 0xDE:       ABSX DEC_CMOS CYC(6)  break;
		case 0xDF:   INV NOP	     CYC(2)  break;
		case 0xE0:       IMM CPX	     CYC(2)  break;
		case 0xE1:       INDX SBC_CMOS CYC(6)  break;
		case 0xE2:   INV IMM NOP	     CYC(2)  break;
		case 0xE3:   INV NOP	     CYC(2)  break;
		case 0xE4:       ZPG CPX	     CYC(3)  break;
		case 0xE5:       ZPG SBC_CMOS  CYC(3)  break;
		case 0xE6:       ZPG INC_CMOS  CYC(5)  break;
		case 0xE7:   INV NOP	     CYC(2)  break;
		case 0xE8:       INX	     CYC(2)  break;
		case 0xE9:       IMM SBC_CMOS  CYC(2)  break;
		case 0xEA:       NOP	     CYC(2)  break;
		case 0xEB:   INV NOP	     CYC(2)  break;
		case 0xEC:       ABS CPX	     CYC(4)  break;
		case 0xED:       ABS SBC_CMOS  CYC(4)  break;
		case 0xEE:       ABS INC_CMOS  CYC(6)  break;
		case 0xEF:   INV NOP	     CYC(2)  break;
		case 0xF0:       REL BEQ	     CYC(2)  break;
		case 0xF1:       INDY SBC_CMOS CYC(5)  break;
		case 0xF2:       IZPG SBC_CMOS CYC(5)  break;
		case 0xF3:   INV NOP	     CYC(2)  break;
		case 0xF4:   INV ZPGX NOP	     CYC(4)  break;
		case 0xF5:       ZPGX SBC_CMOS CYC(4)  break;
		case 0xF6:       ZPGX INC_CMOS CYC(6)  break;
		case 0xF7:   INV NOP	     CYC(2)  break;				
		case 0xF8:       SED	     CYC(2)  break;
		case 0xF9:       ABSY SBC_CMOS CYC(4)  break;
		case 0xFA:       PLX	     CYC(4)  break;
		case 0xFB:   INV NOP	     CYC(2)  break;
		case 0xFC:   INV ABSX NOP	     CYC(4)  break;
		case 0xFD:       ABSX SBC_CMOS CYC(4)  break;
		case 0xFE:       ABSX INC_CMOS CYC(6)  break;
		case 0xFF:   INV NOP	     CYC(2)  break;
		}
		}

		CheckInterruptSources(uExecutedCycles);
		NMI(uExecutedCycles, uExtraCycles, flagc, flagn, flagv, flagz);
		IRQ(uExecutedCycles, uExtraCycles, flagc, flagn, flagv, flagz);

		if (bBreakOnInvalid)
			break;

	} while (uExecutedCycles < uTotalCycles);

	EF_TO_AF
	return uExecutedCycles;
}

//===========================================================================

static DWORD Cpu6502 (DWORD uTotalCycles)
{
	WORD addr;
	BOOL flagc; // must always be 0 or 1, no other values allowed
	BOOL flagn; // must always be 0 or 0x80.
	BOOL flagv; // any value allowed
	BOOL flagz; // any value allowed
	WORD temp;
	WORD temp2;
	WORD val;
	AF_TO_EF
	ULONG uExecutedCycles = 0;
	BOOL bSlowerOnPagecross = 0; // Set if opcode writes to memory (eg. ASL, STA)
	WORD base;
	bool bBreakOnInvalid = false;  

	do
	{
		UINT uExtraCycles = 0;
		BYTE iOpcode;

#ifdef SUPPORT_CPM
		if (g_ActiveCPU == CPU_Z80)
		{
			const UINT uZ80Cycles = z80_mainloop(uTotalCycles, uExecutedCycles); CYC(uZ80Cycles)
		}
		else
#endif
		{
		if (!Fetch(iOpcode, uExecutedCycles))
			break;

		switch (iOpcode)
		{	
		case 0x00:       BRK	     CYC(7)  break;
		case 0x01:       INDX ORA	     CYC(6)  break;
		case 0x02:   INV HLT	     CYC(2)  break;
		case 0x03:   INV INDX ASO	     CYC(8)  break;
		case 0x04:   INV ZPG NOP	     CYC(3)  break;
		case 0x05:       ZPG ORA	     CYC(3)  break;
		case 0x06:       ZPG ASL_NMOS  CYC(5)  break;
		case 0x07:   INV ZPG ASO	     CYC(5)  break;
		case 0x08:       PHP	     CYC(3)  break;
		case 0x09:       IMM ORA	     CYC(2)  break;
		case 0x0A:       ASLA	     CYC(2)  break;
		case 0x0B:   INV IMM ANC	     CYC(2)  break;
		case 0x0C:   INV ABSX NOP	     CYC(4)  break;
		case 0x0D:       ABS ORA	     CYC(4)  break;
		case 0x0E:       ABS ASL_NMOS  CYC(6)  break;
		case 0x0F:   INV ABS ASO	     CYC(6)  break;
		case 0x10:       REL BPL	     CYC(2)  break;
		case 0x11:       INDY ORA	     CYC(5)  break;
		case 0x12:   INV HLT	     CYC(2)  break;
		case 0x13:   INV INDY ASO	     CYC(8)  break;
		case 0x14:   INV ZPGX NOP	     CYC(4)  break;
		case 0x15:       ZPGX ORA	     CYC(4)  break;
		case 0x16:       ZPGX ASL_NMOS CYC(6)  break;
		case 0x17:   INV ZPGX ASO	     CYC(6)  break;
		case 0x18:       CLC	     CYC(2)  break;
		case 0x19:       ABSY ORA	     CYC(4)  break;
		case 0x1A:   INV NOP	     CYC(2)  break;
		case 0x1B:   INV ABSY ASO	     CYC(7)  break;
		case 0x1C:   INV ABSX NOP	     CYC(4)  break;
		case 0x1D:       ABSX ORA	     CYC(4)  break;
		case 0x1E:       ABSX ASL_NMOS CYC(6)  break;
		case 0x1F:   INV ABSX ASO	     CYC(7)  break;
		case 0x20:       ABS JSR	     CYC(6)  break;
		case 0x21:       INDX AND	     CYC(6)  break;
		case 0x22:   INV HLT	     CYC(2)  break;
		case 0x23:   INV INDX RLA	     CYC(8)  break;
		case 0x24:       ZPG BIT	     CYC(3)  break;
		case 0x25:       ZPG AND	     CYC(3)  break;
		case 0x26:       ZPG ROL_NMOS  CYC(5)  break;
		case 0x27:   INV ZPG RLA	     CYC(5)  break;
		case 0x28:       PLP	     CYC(4)  break;
		case 0x29:       IMM AND	     CYC(2)  break;
		case 0x2A:       ROLA	     CYC(2)  break;
		case 0x2B:   INV IMM ANC	     CYC(2)  break;
		case 0x2C:       ABS BIT	     CYC(4)  break;
		case 0x2D:       ABS AND	     CYC(2)  break;
		case 0x2E:       ABS ROL_NMOS  CYC(6)  break;
		case 0x2F:   INV ABS RLA	     CYC(6)  break;
		case 0x30:       REL BMI	     CYC(2)  break;
		case 0x31:       INDY AND	     CYC(5)  break;
		case 0x32:   INV HLT	     CYC(2)  break;
		case 0x33:   INV INDY RLA	     CYC(8)  break;
		case 0x34:   INV ZPGX NOP	     CYC(4)  break;
		case 0x35:       ZPGX AND	     CYC(4)  break;
		case 0x36:       ZPGX ROL_NMOS CYC(6)  break;
		case 0x37:   INV ZPGX RLA	     CYC(6)  break;
		case 0x38:       SEC	     CYC(2)  break;
		case 0x39:       ABSY AND	     CYC(4)  break;
		case 0x3A:   INV NOP	     CYC(2)  break;
		case 0x3B:   INV ABSY RLA	     CYC(7)  break;
		case 0x3C:   INV ABSX NOP	     CYC(4)  break;
		case 0x3D:       ABSX AND	     CYC(4)  break;
		case 0x3E:       ABSX ROL_NMOS CYC(6)  break;
		case 0x3F:   INV ABSX RLA	     CYC(7)  break;
		case 0x40:       RTI	     CYC(6)  DoIrqProfiling(uExecutedCycles); break;
		case 0x41:       INDX EOR	     CYC(6)  break;
		case 0x42:   INV HLT	     CYC(2)  break;
		case 0x43:   INV INDX LSE	     CYC(8)  break;
		case 0x44:   INV ZPG NOP	     CYC(3)  break;
		case 0x45:       ZPG EOR	     CYC(3)  break;
		case 0x46:       ZPG LSR_NMOS  CYC(5)  break;
		case 0x47:   INV ZPG LSE	     CYC(5)  break;
		case 0x48:       PHA	     CYC(3)  break;
		case 0x49:       IMM EOR	     CYC(2)  break;
		case 0x4A:       LSRA	     CYC(2)  break;
		case 0x4B:   INV IMM ALR	     CYC(2)  break;
		case 0x4C:       ABS JMP	     CYC(3)  break;
		case 0x4D:       ABS EOR	     CYC(4)  break;
		case 0x4E:       ABS LSR_NMOS  CYC(6)  break;
		case 0x4F:   INV ABS LSE	     CYC(6)  break;
		case 0x50:       REL BVC	     CYC(2)  break;
		case 0x51:       INDY EOR	     CYC(5)  break;
		case 0x52:   INV HLT	     CYC(2)  break;
		case 0x53:   INV INDY LSE	     CYC(8)  break;
		case 0x54:   INV ZPGX NOP	     CYC(4)  break;
		case 0x55:       ZPGX EOR	     CYC(4)  break;
		case 0x56:       ZPGX LSR_NMOS CYC(6)  break;
		case 0x57:   INV ZPGX LSE	     CYC(6)  break;
		case 0x58:       CLI	     CYC(2)  break;
		case 0x59:       ABSY EOR	     CYC(4)  break;
		case 0x5A:   INV NOP	     CYC(2)  break;
		case 0x5B:   INV ABSY LSE	     CYC(7)  break;
		case 0x5C:   INV ABSX NOP	     CYC(4)  break;
		case 0x5D:       ABSX EOR	     CYC(4)  break;
		case 0x5E:       ABSX LSR_NMOS CYC(6)  break;
		case 0x5F:   INV ABSX LSE	     CYC(7)  break;
		case 0x60:       RTS	     CYC(6)  break;
		case 0x61:       INDX ADC_NMOS CYC(6)  break;
		case 0x62:   INV HLT	     CYC(2)  break;
		case 0x63:   INV INDX RRA	     CYC(8)  break;
		case 0x64:   INV ZPG NOP	     CYC(3)  break;
		case 0x65:       ZPG ADC_NMOS  CYC(3)  break;
		case 0x66:       ZPG ROR_NMOS  CYC(5)  break;
		case 0x67:   INV ZPG RRA	     CYC(5)  break;
		case 0x68:       PLA	     CYC(4)  break;
		case 0x69:       IMM ADC_NMOS  CYC(2)  break;
		case 0x6A:       RORA	     CYC(2)  break;
		case 0x6B:   INV IMM ARR	     CYC(2)  break;
		case 0x6C:       IABSNMOS JMP  CYC(6)  break;
		case 0x6D:       ABS ADC_NMOS  CYC(4)  break;
		case 0x6E:       ABS ROR_NMOS  CYC(6)  break;
		case 0x6F:   INV ABS RRA	     CYC(6)  break;
		case 0x70:       REL BVS	     CYC(2)  break;
		case 0x71:       INDY ADC_NMOS CYC(5)  break;
		case 0x72:   INV HLT	     CYC(2)  break;
		case 0x73:   INV INDY RRA	     CYC(8)  break;
		case 0x74:   INV ZPGX NOP	     CYC(4)  break;
		case 0x75:       ZPGX ADC_NMOS CYC(4)  break;
		case 0x76:       ZPGX ROR_NMOS CYC(6)  break;
		case 0x77:   INV ZPGX RRA	     CYC(6)  break;
		case 0x78:       SEI	     CYC(2)  break;
		case 0x79:       ABSY ADC_NMOS CYC(4)  break;
		case 0x7A:   INV NOP	     CYC(2)  break;
		case 0x7B:   INV ABSY RRA	     CYC(7)  break;
		case 0x7C:   INV ABSX NOP	     CYC(4)  break;
		case 0x7D:       ABSX ADC_NMOS CYC(4)  break;
		case 0x7E:       ABSX ROR_NMOS CYC(6)  break;
		case 0x7F:   INV ABSX RRA	     CYC(7)  break;
		case 0x80:   INV IMM NOP	     CYC(2)  break;
		case 0x81:       INDX STA	     CYC(6)  break;
		case 0x82:   INV IMM NOP	     CYC(2)  break;
		case 0x83:   INV INDX AXS	     CYC(6)  break;
		case 0x84:       ZPG STY	     CYC(3)  break;
		case 0x85:       ZPG STA	     CYC(3)  break;
		case 0x86:       ZPG STX	     CYC(3)  break;
		case 0x87:   INV ZPG AXS	     CYC(3)  break;
		case 0x88:       DEY	     CYC(2)  break;
		case 0x89:   INV IMM NOP	     CYC(2)  break;
		case 0x8A:       TXA	     CYC(2)  break;
		case 0x8B:   INV IMM XAA	     CYC(2)  break;
		case 0x8C:       ABS STY	     CYC(4)  break;
		case 0x8D:       ABS STA	     CYC(4)  break;
		case 0x8E:       ABS STX	     CYC(4)  break;
		case 0x8F:   INV ABS AXS	     CYC(4)  break;
		case 0x90:       REL BCC	     CYC(2)  break;
		case 0x91:       INDY STA	     CYC(6)  break;
		case 0x92:   INV HLT	     CYC(2)  break;
		case 0x93:   INV INDY AXA	     CYC(6)  break;
		case 0x94:       ZPGX STY	     CYC(4)  break;
		case 0x95:       ZPGX STA	     CYC(4)  break;
		case 0x96:       ZPGY STX	     CYC(4)  break;
		case 0x97:   INV ZPGY AXS	     CYC(4)  break;
		case 0x98:       TYA	     CYC(2)  break;
		case 0x99:       ABSY STA	     CYC(5)  break;
		case 0x9A:       TXS	     CYC(2)  break;
		case 0x9B:   INV ABSY TAS	     CYC(5)  break;
		case 0x9C:   INV ABSX SAY	     CYC(5)  break;
		case 0x9D:       ABSX STA	     CYC(5)  break;
		case 0x9E:   INV ABSY XAS	     CYC(5)  break;
		case 0x9F:   INV ABSY AXA	     CYC(5)  break;
		case 0xA0:       IMM LDY	     CYC(2)  break;
		case 0xA1:       INDX LDA	     CYC(6)  break;
		case 0xA2:       IMM LDX	     CYC(2)  break;
		case 0xA3:   INV INDX LAX	     CYC(6)  break;
		case 0xA4:       ZPG LDY	     CYC(3)  break;
		case 0xA5:       ZPG LDA	     CYC(3)  break;
		case 0xA6:       ZPG LDX	     CYC(3)  break;
		case 0xA7:   INV ZPG LAX	     CYC(3)  break;
		case 0xA8:       TAY	     CYC(2)  break;
		case 0xA9:       IMM LDA	     CYC(2)  break;
		case 0xAA:       TAX	     CYC(2)  break;
		case 0xAB:   INV IMM OAL	     CYC(2)  break;
		case 0xAC:       ABS LDY	     CYC(4)  break;
		case 0xAD:       ABS LDA	     CYC(4)  break;
		case 0xAE:       ABS LDX	     CYC(4)  break;
		case 0xAF:   INV ABS LAX	     CYC(4)  break;
		case 0xB0:       REL BCS	     CYC(2)  break;
		case 0xB1:       INDY LDA	     CYC(5)  break;
		case 0xB2:   INV HLT	     CYC(2)  break;
		case 0xB3:   INV INDY LAX	     CYC(5)  break;
		case 0xB4:       ZPGX LDY	     CYC(4)  break;
		case 0xB5:       ZPGX LDA	     CYC(4)  break;
		case 0xB6:       ZPGY LDX	     CYC(4)  break;
		case 0xB7:   INV ZPGY LAX	     CYC(4)  break;
		case 0xB8:       CLV	     CYC(2)  break;
		case 0xB9:       ABSY LDA	     CYC(4)  break;
		case 0xBA:       TSX	     CYC(2)  break;
		case 0xBB:   INV ABSY LAS	     CYC(4)  break;
		case 0xBC:       ABSX LDY	     CYC(4)  break;
		case 0xBD:       ABSX LDA	     CYC(4)  break;
		case 0xBE:       ABSY LDX	     CYC(4)  break;
		case 0xBF:   INV ABSY LAX	     CYC(4)  break;
		case 0xC0:       IMM CPY	     CYC(2)  break;
		case 0xC1:       INDX CMP	     CYC(6)  break;
		case 0xC2:   INV IMM NOP	     CYC(2)  break;
		case 0xC3:   INV INDX DCM	     CYC(8)  break;
		case 0xC4:       ZPG CPY	     CYC(3)  break;
		case 0xC5:       ZPG CMP	     CYC(3)  break;
		case 0xC6:       ZPG DEC_NMOS  CYC(5)  break;
		case 0xC7:   INV ZPG DCM	     CYC(5)  break;
		case 0xC8:       INY	     CYC(2)  break;
		case 0xC9:       IMM CMP	     CYC(2)  break;
		case 0xCA:       DEX	     CYC(2)  break;
		case 0xCB:   INV IMM SAX	     CYC(2)  break;
		case 0xCC:       ABS CPY	     CYC(4)  break;
		case 0xCD:       ABS CMP	     CYC(4)  break;
		case 0xCE:       ABS DEC_NMOS  CYC(5)  break;
		case 0xCF:   INV ABS DCM	     CYC(6)  break;
		case 0xD0:       REL BNE	     CYC(2)  break;
		case 0xD1:       INDY CMP	     CYC(5)  break;
		case 0xD2:   INV HLT	     CYC(2)  break;
		case 0xD3:   INV INDY DCM	     CYC(8)  break;
		case 0xD4:   INV ZPGX NOP	     CYC(4)  break;
		case 0xD5:       ZPGX CMP	     CYC(4)  break;
		case 0xD6:       ZPGX DEC_NMOS CYC(6)  break;
		case 0xD7:   INV ZPGX DCM	     CYC(6)  break;
		case 0xD8:       CLD	     CYC(2)  break;
		case 0xD9:       ABSY CMP	     CYC(4)  break;
		case 0xDA:   INV NOP	     CYC(2)  break;
		case 0xDB:   INV ABSY DCM	     CYC(7)  break;
		case 0xDC:   INV ABSX NOP	     CYC(4)  break;
		case 0xDD:       ABSX CMP	     CYC(4)  break;
		case 0xDE:       ABSX DEC_NMOS CYC(6)  break;
		case 0xDF:   INV ABSX DCM	     CYC(7)  break;
		case 0xE0:       IMM CPX	     CYC(2)  break;
		case 0xE1:       INDX SBC_NMOS CYC(6)  break;
		case 0xE2:   INV IMM NOP	     CYC(2)  break;
		case 0xE3:   INV INDX INS	     CYC(8)  break;
		case 0xE4:       ZPG CPX	     CYC(3)  break;
		case 0xE5:       ZPG SBC_NMOS  CYC(3)  break;
		case 0xE6:       ZPG INC_NMOS  CYC(5)  break;
		case 0xE7:   INV ZPG INS	     CYC(5)  break;
		case 0xE8:       INX	     CYC(2)  break;
		case 0xE9:       IMM SBC_NMOS  CYC(2)  break;
		case 0xEA:       NOP	     CYC(2)  break;
		case 0xEB:   INV IMM SBC_NMOS  CYC(2)  break;
		case 0xEC:       ABS CPX	     CYC(4)  break;
		case 0xED:       ABS SBC_NMOS  CYC(4)  break;
		case 0xEE:       ABS INC_NMOS  CYC(6)  break;
		case 0xEF:   INV ABS INS	     CYC(6)  break;
		case 0xF0:       REL BEQ	     CYC(2)  break;
		case 0xF1:       INDY SBC_NMOS CYC(5)  break;
		case 0xF2:   INV HLT	     CYC(2)  break;
		case 0xF3:   INV INDY INS	     CYC(8)  break;
		case 0xF4:   INV ZPGX NOP	     CYC(4)  break;
		case 0xF5:       ZPGX SBC_NMOS CYC(4)  break;
		case 0xF6:       ZPGX INC_NMOS CYC(6)  break;
		case 0xF7:   INV ZPGX INS	     CYC(6)  break;
		case 0xF8:       SED	     CYC(2)  break;
		case 0xF9:       ABSY SBC_NMOS CYC(4)  break;
		case 0xFA:   INV NOP	     CYC(2)  break;
		case 0xFB:   INV ABSY INS	     CYC(7)  break;
		case 0xFC:   INV ABSX NOP	     CYC(4)  break;
		case 0xFD:       ABSX SBC_NMOS CYC(4)  break;
		case 0xFE:       ABSX INC_NMOS CYC(6)  break;
		case 0xFF:   INV ABSX INS	     CYC(7)  break;
		}
		}

		CheckInterruptSources(uExecutedCycles);
		NMI(uExecutedCycles, uExtraCycles, flagc, flagn, flagv, flagz);
		IRQ(uExecutedCycles, uExtraCycles, flagc, flagn, flagv, flagz);

		if (bBreakOnInvalid)
			break;

	} while (uExecutedCycles < uTotalCycles);

	EF_TO_AF
	return uExecutedCycles;
}

//#include "cpu/cpu65d02.cpp"

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
// Pre:
//	Call this when an IO-reg is access & accurate cycle info is needed
// Post:
//	g_nCyclesExecuted
//	g_nCumulativeCycles
//
void CpuCalcCycles(ULONG nExecutedCycles)
{
	// Calc # of cycles executed since this func was last called
	ULONG nCycles = nExecutedCycles - g_nCyclesExecuted;
	_ASSERT( (LONG)nCycles >= 0 );

	g_nCyclesExecuted += nCycles;
	g_nCumulativeCycles += nCycles;
}

//===========================================================================

// Old method with g_uInternalExecutedCycles runs faster!
//        Old     vs    New
// - 68.0,69.0MHz vs  66.7, 67.2MHz  (with check for VBL IRQ every opcode)
// - 89.6,88.9MHz vs  87.2, 87.9MHz  (without check for VBL IRQ)
// -                  75.9, 78.5MHz  (with check for VBL IRQ every 128 cycles)
// -                 137.9,135.6MHz  (with check for VBL IRQ & MB_Update every 128 cycles)

#if 0	// TODO: Measure perf increase by using this new method
ULONG CpuGetCyclesThisFrame(ULONG)	// Old func using g_uInternalExecutedCycles
{
	CpuCalcCycles(g_uInternalExecutedCycles);
	return g_dwCyclesThisFrame + g_nCyclesExecuted;
}
#else
ULONG CpuGetCyclesThisFrame(ULONG nExecutedCycles)
{
	CpuCalcCycles(nExecutedCycles);
	return g_dwCyclesThisFrame + g_nCyclesExecuted;
}
#endif

//===========================================================================

DWORD CpuExecute (DWORD uCycles)
{
	DWORD uExecutedCycles =	0;

	g_nCyclesSubmitted = uCycles;
	g_nCyclesExecuted =	0;

	//

	MB_StartOfCpuExecute();

	if (uCycles	== 0)	// Do single step
		uExecutedCycles	= InternalCpuExecute(0);
	else				// Do multi-opcode emulation
		uExecutedCycles	= InternalCpuExecute(uCycles);

	MB_UpdateCycles(uExecutedCycles);	// Update 6522s (NB. Do this before updating g_nCumulativeCycles below)

	//

	UINT nRemainingCycles =	uExecutedCycles	- g_nCyclesExecuted;
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
