/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 1994-1996, Michael O'Brien
Copyright (C) 1999-2001, Oliver Schmidt
Copyright (C) 2002-2005, Tom Charlesworth
Copyright (C) 2006-2014, Tom Charlesworth, Michael Pohoreski

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

/* Description: Debugger Assembler
 *
 * Author: Copyright (C) 2006-2010 Michael Pohoreski
 */

#include "StdAfx.h"

#include "Debug.h"

#include "../Interface.h"
#include "../CPU.h"
#include "../Memory.h"

#define DEBUG_ASSEMBLER 0

// Globals __________________________________________________________________


// Addressing _____________________________________________________________________________________

	AddressingMode_t g_aOpmodes[ NUM_ADDRESSING_MODES ] =
	{ // Output, but eventually used for Input when Assembler is working.
		{TEXT("")        , 1 , "(implied)"              }, // (implied)
        {TEXT("")        , 1 , "n/a 1"         }, // INVALID1
        {TEXT("")        , 2 , "n/a 2"         }, // INVALID2
        {TEXT("")        , 3 , "n/a 3"         }, // INVALID3
		{TEXT("%02X")    , 2 , "Immediate"     }, // AM_M // #$%02X -> %02X
        {TEXT("%04X")    , 3 , "Absolute"      }, // AM_A
        {TEXT("%02X")    , 2 , "Zero Page"     }, // AM_Z
        {TEXT("%04X,X")  , 3 , "Absolute,X"    }, // AM_AX     // %s,X
        {TEXT("%04X,Y")  , 3 , "Absolute,Y"    }, // AM_AY     // %s,Y
        {TEXT("%02X,X")  , 2 , "Zero Page,X"   }, // AM_ZX     // %s,X
        {TEXT("%02X,Y")  , 2 , "Zero Page,Y"   }, // AM_ZY     // %s,Y
        {TEXT("%s")      , 2 , "Relative"      }, // AM_R
        {TEXT("(%02X,X)"), 2 , "(Zero Page),X" }, // AM_IZX // ($%02X,X) -> %s,X 
        {TEXT("(%04X,X)"), 3 , "(Absolute),X"  }, // AM_IAX // ($%04X,X) -> %s,X
        {TEXT("(%02X),Y"), 2 , "(Zero Page),Y" }, // AM_NZY // ($%02X),Y
        {TEXT("(%02X)")  , 2 , "(Zero Page)"   }, // AM_NZ  // ($%02X) -> $%02X
        {TEXT("(%04X)")  , 3 , "(Absolute)"    }  // AM_NA  // (%04X) -> %s
	};


// Assembler ______________________________________________________________________________________

	int    g_bAssemblerOpcodesHashed = false;
	Hash_t g_aOpcodesHash[ NUM_OPCODES ]; // for faster mnemonic lookup, for the assembler
	bool   g_bAssemblerInput = false;
	int    g_nAssemblerAddress = 0;

	const Opcodes_t *g_aOpcodes = NULL; // & g_aOpcodes65C02[ 0 ];


// Disassembler Data  _____________________________________________________________________________

	std::vector<DisasmData_t> g_aDisassemblerData;


// Instructions / Opcodes _________________________________________________________________________


// @reference: http://www.6502.org/tutorials/compare_instructions.html
// 10   signed: BPL BGE 
// B0 unsigned: BCS BGE

#define R_ MEM_R
#define _W MEM_W
#define RW MEM_R | MEM_W
#define _S MEM_S
#define im MEM_IM
#define SW MEM_S | MEM_WI
#define SR MEM_S | MEM_RI
const Opcodes_t g_aOpcodes65C02[ NUM_OPCODES ] =
{
	{"BRK", 0     , SW}, {"ORA", AM_IZX, R_}, {"nop", AM_M  , im}, {"nop", 0  , 0 }, // 00 .. 03
	{"TSB", AM_Z  , _W}, {"ORA", AM_Z  , R_}, {"ASL", AM_Z  , RW}, {"nop", 0  , 0 }, // 04 .. 07
	{"PHP", 0     , SW}, {"ORA", AM_M  , im}, {"ASL", 0     ,  0}, {"nop", 0  , 0 }, // 08 .. 0B
	{"TSB", AM_A  , _W}, {"ORA", AM_A  , R_}, {"ASL", AM_A  , RW}, {"nop", 0  , 0 }, // 0C .. 0F
	{"BPL", AM_R  ,  0}, {"ORA", AM_NZY, R_}, {"ORA", AM_NZ , R_}, {"nop", 0  , 0 }, // 10 .. 13
	{"TRB", AM_Z  , _W}, {"ORA", AM_ZX , R_}, {"ASL", AM_ZX , RW}, {"nop", 0  , 0 }, // 14 .. 17
	{"CLC", 0     ,  0}, {"ORA", AM_AY , R_}, {"INC", 0     ,  0}, {"nop", 0  , 0 }, // 18 .. 1B
	{"TRB", AM_A  , _W}, {"ORA", AM_AX , R_}, {"ASL", AM_AX , RW}, {"nop", 0  , 0 }, // 1C .. 1F

	{"JSR", AM_A  , SW}, {"AND", AM_IZX, R_}, {"nop", AM_M  , im}, {"nop", 0  , 0 }, // 20 .. 23
	{"BIT", AM_Z  , R_}, {"AND", AM_Z  , R_}, {"ROL", AM_Z  , RW}, {"nop", 0  , 0 }, // 24 .. 27
	{"PLP", 0     , SR}, {"AND", AM_M  , im}, {"ROL", 0     ,  0}, {"nop", 0  , 0 }, // 28 .. 2B
	{"BIT", AM_A  , R_}, {"AND", AM_A  , R_}, {"ROL", AM_A  , RW}, {"nop", 0  , 0 }, // 2C .. 2F
	{"BMI", AM_R  ,  0}, {"AND", AM_NZY, R_}, {"AND", AM_NZ , R_}, {"nop", 0  , 0 }, // 30 .. 33
	{"BIT", AM_ZX , R_}, {"AND", AM_ZX , R_}, {"ROL", AM_ZX , RW}, {"nop", 0  , 0 }, // 34 .. 37
	{"SEC", 0     ,  0}, {"AND", AM_AY , R_}, {"DEC", 0     ,  0}, {"nop", 0  , 0 }, // 38 .. 3B
	{"BIT", AM_AX , R_}, {"AND", AM_AX , R_}, {"ROL", AM_AX , RW}, {"nop", 0  , 0 }, // 3C .. 3F

	{"RTI", 0     , SR}, {"EOR", AM_IZX, R_}, {"nop", AM_M  , im}, {"nop", 0  , 0 }, // 40 .. 43
	{"nop", AM_Z  ,  0}, {"EOR", AM_Z  , R_}, {"LSR", AM_Z  , _W}, {"nop", 0  , 0 }, // 44 .. 47
	{"PHA", 0     , SW}, {"EOR", AM_M  , im}, {"LSR", 0     ,  0}, {"nop", 0  , 0 }, // 48 .. 4B
	{"JMP", AM_A  ,  0}, {"EOR", AM_A  , R_}, {"LSR", AM_A  , _W}, {"nop", 0  , 0 }, // 4C .. 4F
	{"BVC", AM_R  ,  0}, {"EOR", AM_NZY, R_}, {"EOR", AM_NZ , R_}, {"nop", 0  , 0 }, // 50 .. 53
	{"nop", AM_ZX ,  0}, {"EOR", AM_ZX , R_}, {"LSR", AM_ZX , _W}, {"nop", 0  , 0 }, // 54 .. 57
	{"CLI", 0     ,  0}, {"EOR", AM_AY , R_}, {"PHY", 0     , SW}, {"nop", 0  , 0 }, // 58 .. 5B
	{"nop", AM_AX ,  0}, {"EOR", AM_AX , R_}, {"LSR", AM_AX , RW}, {"nop", 0  , 0 }, // 5C .. 5F

	{"RTS", 0     , SR}, {"ADC", AM_IZX, R_}, {"nop", AM_M  , im}, {"nop", 0  , 0 }, // 60 .. 63
	{"STZ", AM_Z  , _W}, {"ADC", AM_Z  , R_}, {"ROR", AM_Z  , RW}, {"nop", 0  , 0 }, // 64 .. 67
	{"PLA", 0     , SR}, {"ADC", AM_M  , im}, {"ROR", 0     ,  0}, {"nop", 0  , 0 }, // 68 .. 6B
	{"JMP", AM_NA , R_}, {"ADC", AM_A  , R_}, {"ROR", AM_A  , RW}, {"nop", 0  , 0 }, // 6C .. 6F
	{"BVS", AM_R  ,  0}, {"ADC", AM_NZY, R_}, {"ADC", AM_NZ , R_}, {"nop", 0  , 0 }, // 70 .. 73
	{"STZ", AM_ZX , _W}, {"ADC", AM_ZX , R_}, {"ROR", AM_ZX , RW}, {"nop", 0  , 0 }, // 74 .. 77
	{"SEI", 0     ,  0}, {"ADC", AM_AY , R_}, {"PLY", 0     , SR}, {"nop", 0  , 0 }, // 78 .. 7B
	{"JMP", AM_IAX, R_}, {"ADC", AM_AX , R_}, {"ROR", AM_AX , RW}, {"nop", 0  , 0 }, // 7C .. 7F

	{"BRA", AM_R  ,  0}, {"STA", AM_IZX, _W}, {"nop", AM_M  , im}, {"nop", 0  , 0 }, // 80 .. 83
	{"STY", AM_Z  , _W}, {"STA", AM_Z  , _W}, {"STX", AM_Z  , _W}, {"nop", 0  , 0 }, // 84 .. 87
	{"DEY", 0     ,  0}, {"BIT", AM_M  , im}, {"TXA", 0     ,  0}, {"nop", 0  , 0 }, // 88 .. 8B
	{"STY", AM_A  , _W}, {"STA", AM_A  , _W}, {"STX", AM_A  , _W}, {"nop", 0  , 0 }, // 8C .. 8F
	{"BCC", AM_R  ,  0}, {"STA", AM_NZY, _W}, {"STA", AM_NZ , _W}, {"nop", 0  , 0 }, // 90 .. 93
	{"STY", AM_ZX , _W}, {"STA", AM_ZX , _W}, {"STX", AM_ZY , _W}, {"nop", 0  , 0 }, // 94 .. 97
	{"TYA", 0     ,  0}, {"STA", AM_AY , _W}, {"TXS", 0     ,  0}, {"nop", 0  , 0 }, // 98 .. 9B
	{"STZ", AM_A  , _W}, {"STA", AM_AX , _W}, {"STZ", AM_AX , _W}, {"nop", 0  , 0 }, // 9C .. 9F

	{"LDY", AM_M  , im}, {"LDA", AM_IZX, R_}, {"LDX", AM_M  , im}, {"nop", 0  , 0 }, // A0 .. A3
	{"LDY", AM_Z  , R_}, {"LDA", AM_Z  , R_}, {"LDX", AM_Z  , R_}, {"nop", 0  , 0 }, // A4 .. A7
	{"TAY", 0     ,  0}, {"LDA", AM_M  , im}, {"TAX", 0     , 0 }, {"nop", 0  , 0 }, // A8 .. AB
	{"LDY", AM_A  , R_}, {"LDA", AM_A  , R_}, {"LDX", AM_A  , R_}, {"nop", 0  , 0 }, // AC .. AF
	{"BCS", AM_R  ,  0}, {"LDA", AM_NZY, R_}, {"LDA", AM_NZ , R_}, {"nop", 0  , 0 }, // B0 .. B3
	{"LDY", AM_ZX , R_}, {"LDA", AM_ZX , R_}, {"LDX", AM_ZY , R_}, {"nop", 0  , 0 }, // B4 .. B7
	{"CLV", 0     ,  0}, {"LDA", AM_AY , R_}, {"TSX", 0     , 0 }, {"nop", 0  , 0 }, // B8 .. BB
	{"LDY", AM_AX , R_}, {"LDA", AM_AX , R_}, {"LDX", AM_AY , R_}, {"nop", 0  , 0 }, // BC .. BF

	{"CPY", AM_M  , im}, {"CMP", AM_IZX, R_}, {"nop", AM_M  , im}, {"nop", 0  , 0 }, // C0 .. C3
	{"CPY", AM_Z  , R_}, {"CMP", AM_Z  , R_}, {"DEC", AM_Z  , RW}, {"nop", 0  , 0 }, // C4 .. C7
	{"INY", 0     ,  0}, {"CMP", AM_M  , im}, {"DEX", 0     ,  0}, {"nop", 0  , 0 }, // C8 .. CB
	{"CPY", AM_A  , R_}, {"CMP", AM_A  , R_}, {"DEC", AM_A  , RW}, {"nop", 0  , 0 }, // CC .. CF
	{"BNE", AM_R  ,  0}, {"CMP", AM_NZY, R_}, {"CMP", AM_NZ ,  0}, {"nop", 0  , 0 }, // D0 .. D3
	{"nop", AM_ZX ,  0}, {"CMP", AM_ZX , R_}, {"DEC", AM_ZX , RW}, {"nop", 0  , 0 }, // D4 .. D7
	{"CLD", 0     ,  0}, {"CMP", AM_AY , R_}, {"PHX", 0     , SW}, {"nop", 0  , 0 }, // D8 .. DB
	{"nop", AM_AX ,  0}, {"CMP", AM_AX , R_}, {"DEC", AM_AX , RW}, {"nop", 0  , 0 }, // DC .. DF

	{"CPX", AM_M  , im}, {"SBC", AM_IZX, R_}, {"nop", AM_M  , im}, {"nop", 0  , 0 }, // E0 .. E3
	{"CPX", AM_Z  , R_}, {"SBC", AM_Z  , R_}, {"INC", AM_Z  , RW}, {"nop", 0  , 0 }, // E4 .. E7
	{"INX", 0     ,  0}, {"SBC", AM_M  , R_}, {"NOP", 0     ,  0}, {"nop", 0  , 0 }, // E8 .. EB
	{"CPX", AM_A  , R_}, {"SBC", AM_A  , R_}, {"INC", AM_A  , RW}, {"nop", 0  , 0 }, // EC .. EF
	{"BEQ", AM_R  ,  0}, {"SBC", AM_NZY, R_}, {"SBC", AM_NZ ,  0}, {"nop", 0  , 0 }, // F0 .. F3
	{"nop", AM_ZX ,  0}, {"SBC", AM_ZX , R_}, {"INC", AM_ZX , RW}, {"nop", 0  , 0 }, // F4 .. F7
	{"SED", 0     ,  0}, {"SBC", AM_AY , R_}, {"PLX", 0     , SR}, {"nop", 0  , 0 }, // F8 .. FB
	{"nop", AM_AX ,  0}, {"SBC", AM_AX , R_}, {"INC", AM_AX , RW}, {"nop", 0  , 0 }  // FF .. FF
};

const Opcodes_t g_aOpcodes6502[ NUM_OPCODES ] =
{ // Should match Cpu.cpp InternalCpuExecute() switch (*(mem+regs.pc++)) !!

/*
	Based on: http://axis.llx.com/~nparker/a2/opcodes.html

	If you really want to know what the undocumented --- (n/a) opcodes do, see
	CPU.cpp
	
	x0     x1         x2       x3   x4       x5       x6       x7   x8   x9       xA      xB   xC        xD       xE      	xF
0x	BRK    ORA (d,X)  ---      ---  tsb z    ORA d    ASL z    ---  PHP  ORA #    ASL A  ---  tsb a      ORA a    ASL a   	---
1x	BPL r  ORA (d),Y  ora (z)  ---  trb d    ORA d,X  ASL z,X  ---  CLC  ORA a,Y  ina A  ---  trb a      ORA a,X  ASL a,X 	---
2x	JSR a  AND (d,X)  ---      ---  BIT d    AND d    ROL z    ---  PLP  AND #    ROL A  ---  BIT a      AND a    ROL a   	---
3x	BMI r  AND (d),Y  and (z)  ---  bit d,X  AND d,X  ROL z,X  ---  SEC  AND a,Y  dea A  ---  bit a,X    AND a,X  ROL a,X 	---
4x	RTI    EOR (d,X)  ---      ---  ---      EOR d    LSR z    ---  PHA  EOR #    LSR A  ---  JMP a      EOR a    LSR a   	---
5x	BVC r  EOR (d),Y  eor (z)  ---  ---      EOR d,X  LSR z,X  ---  CLI  EOR a,Y  phy    ---  ---        EOR a,X  LSR a,X 	---
6x	RTS    ADC (d,X)  ---      ---  stz d    ADC d    ROR z    ---  PLA  ADC #    ROR A  ---  JMP (a)    ADC a    ROR a   	---
7x	BVS r  ADC (d),Y  adc (z)  ---  stz d,X  ADC d,X  ROR z,X  ---  SEI  ADC a,Y  ply    ---  jmp (a,X)  ADC a,X  ROR a,X 	---
8x	bra r  STA (d,X)  ---      ---  STY d    STA d    STX z    ---  DEY  bit #    TXA    ---  STY a      STA a    STX a   	---
9x	BCC r  STA (d),Y  sta (z)  ---  STY d,X  STA d,X  STX z,Y  ---  TYA  STA a,Y  TXS    ---  Stz a      STA a,X  stz a,X 	---
Ax	LDY #  LDA (d,X)  LDX #    ---  LDY d    LDA d    LDX z    ---  TAY  LDA #    TAX    ---  LDY a      LDA a    LDX a   	---
Bx	BCS r  LDA (d),Y  lda (z)  ---  LDY d,X  LDA d,X  LDX z,Y  ---  CLV  LDA a,Y  TSX    ---  LDY a,X    LDA a,X  LDX a,Y 	---
Cx	CPY #  CMP (d,X)  ---      ---  CPY d    CMP d    DEC z    ---  INY  CMP #    DEX    ---  CPY a      CMP a    DEC a   	---
Dx	BNE r  CMP (d),Y  cmp (z)  ---  ---      CMP d,X  DEC z,X  ---  CLD  CMP a,Y  phx    ---  ---        CMP a,X  DEC a,X 	---
Ex	CPX #  SBC (d,X)  ---      ---  CPX d    SBC d    INC z    ---  INX  SBC #    NOP    ---  CPX a      SBC a    INC a   	---
Fx	BEQ r  SBC (d),Y  sbc (z)  ---  ---      SBC d,X  INC z,X  ---  SED  SBC a,Y  plx    ---  ---        SBC a,X  INC a,X 	---

	Legend:
        --- illegal instruction
		UPPERCASE 6502
		lowercase 65C02
			80
			12, 32, 52, 72, 92, B2, D2, F2
			04, 14, 34, 64, 74
			89
			1A, 3A, 5A, 7A, DA, FA
			0C, 1C, 3C, 7C, 9C;
		# Immediate
		A Accumulator (implicit for mnemonic)
		a absolute
		r Relative
		d Destination 16-bit Address
		z Destination Zero Page Address
		z,x Base=Zero-Page, Offset=X
		d,x
		(d,X)
		(d),Y

*/
	{"BRK", 0     , SW}, {"ORA", AM_IZX, R_}, {"hlt", 0     , 0 }, {"aso", AM_IZX, RW}, // 00 .. 03
	{"nop", AM_Z  , R_}, {"ORA", AM_Z  , R_}, {"ASL", AM_Z  , RW}, {"aso", AM_Z  , RW}, // 04 .. 07
	{"PHP", 0     , SW}, {"ORA", AM_M  , im}, {"ASL", 0     ,  0}, {"anc", AM_M  , im}, // 08 .. 0B
	{"nop", AM_AX ,  0}, {"ORA", AM_A  , R_}, {"ASL", AM_A  , RW}, {"aso", AM_A  , RW}, // 0C .. 0F
	{"BPL", AM_R  ,  0}, {"ORA", AM_NZY, R_}, {"hlt", 0     ,  0}, {"aso", AM_NZY, RW}, // 10 .. 13
	{"nop", AM_ZX ,  0}, {"ORA", AM_ZX , R_}, {"ASL", AM_ZX , RW}, {"aso", AM_ZX , RW}, // 14 .. 17
	{"CLC", 0     ,  0}, {"ORA", AM_AY , R_}, {"nop", 0     ,  0}, {"aso", AM_AY , RW}, // 18 .. 1B
	{"nop", AM_AX ,  0}, {"ORA", AM_AX , R_}, {"ASL", AM_AX , RW}, {"aso", AM_AX , RW}, // 1C .. 1F

	{"JSR", AM_A  , SW}, {"AND", AM_IZX, R_}, {"hlt", 0     ,  0}, {"rla", AM_IZX, RW}, // 20 .. 23
	{"BIT", AM_Z  , R_}, {"AND", AM_Z  , R_}, {"ROL", AM_Z  , RW}, {"rla", AM_Z  , RW}, // 24 .. 27
	{"PLP", 0     , SR}, {"AND", AM_M  , im}, {"ROL", 0     ,  0}, {"anc", AM_M  , im}, // 28 .. 2B
	{"BIT", AM_A  , R_}, {"AND", AM_A  , R_}, {"ROL", AM_A  , RW}, {"rla", AM_A  , RW}, // 2C .. 2F
	{"BMI", AM_R  ,  0}, {"AND", AM_NZY, R_}, {"hlt", 0     ,  0}, {"rla", AM_NZY, RW}, // 30 .. 33
	{"nop", AM_ZX ,  0}, {"AND", AM_ZX , R_}, {"ROL", AM_ZX , RW}, {"rla", AM_ZX , RW}, // 34 .. 37
	{"SEC", 0     ,  0}, {"AND", AM_AY , R_}, {"nop", 0     ,  0}, {"rla", AM_AY , RW}, // 38 .. 3B
	{"nop", AM_AX ,  0}, {"AND", AM_AX , R_}, {"ROL", AM_AX , RW}, {"rla", AM_AX , RW}, // 3C .. 3F

	{"RTI", 0     , SR}, {"EOR", AM_IZX, R_}, {"hlt", 0     ,  0}, {"lse", AM_IZX, RW}, // 40 .. 43
	{"nop", AM_Z  ,  0}, {"EOR", AM_Z  , R_}, {"LSR", AM_Z  , RW}, {"lse", AM_Z  , RW}, // 44 .. 47
	{"PHA", 0     , SW}, {"EOR", AM_M  , im}, {"LSR", 0     ,  0}, {"alr", AM_M  , im}, // 48 .. 4B
	{"JMP", AM_A  ,  0}, {"EOR", AM_A  , R_}, {"LSR", AM_A  , RW}, {"lse", AM_A  , RW}, // 4C .. 4F
	{"BVC", AM_R  ,  0}, {"EOR", AM_NZY, R_}, {"hlt", 0     ,  0}, {"lse", AM_NZY, RW}, // 50 .. 53
	{"nop", AM_ZX ,  0}, {"EOR", AM_ZX , R_}, {"LSR", AM_ZX , RW}, {"lse", AM_ZX , RW}, // 54 .. 57
	{"CLI", 0     ,  0}, {"EOR", AM_AY , R_}, {"nop", 0     ,  0}, {"lse", AM_AY , RW}, // 58 .. 5B
	{"nop", AM_AX ,  0}, {"EOR", AM_AX , R_}, {"LSR", AM_AX , RW}, {"lse", AM_AX , RW}, // 5C .. 5F

	{"RTS", 0     , SR}, {"ADC", AM_IZX, R_}, {"hlt", 0     ,  0}, {"rra", AM_IZX, RW}, // 60 .. 63
	{"nop", AM_Z  ,  0}, {"ADC", AM_Z  , R_}, {"ROR", AM_Z  , RW}, {"rra", AM_Z  , RW}, // 64 .. 67
	{"PLA", 0     , SR}, {"ADC", AM_M  , im}, {"ROR", 0     ,  0}, {"arr", AM_M  , im}, // 68 .. 6B
	{"JMP", AM_NA , R_}, {"ADC", AM_A  , R_}, {"ROR", AM_A  , RW}, {"rra", AM_A  , RW}, // 6C .. 6F
	{"BVS", AM_R  ,  0}, {"ADC", AM_NZY, R_}, {"hlt", 0     ,  0}, {"rra", AM_NZY, RW}, // 70 .. 73
	{"nop", AM_ZX ,  0}, {"ADC", AM_ZX , R_}, {"ROR", AM_ZX , RW}, {"rra", AM_ZX , RW}, // 74 .. 77
	{"SEI", 0     ,  0}, {"ADC", AM_AY , R_}, {"nop", 0     ,  0}, {"rra", AM_AY , RW}, // 78 .. 7B
	{"nop", AM_AX ,  0}, {"ADC", AM_AX , R_}, {"ROR", AM_AX , RW}, {"rra", AM_AX , RW}, // 7C .. 7F

	{"nop", AM_M  , im}, {"STA", AM_IZX, _W}, {"nop", AM_M  , im}, {"axs", AM_IZX, _W}, // 80 .. 83
	{"STY", AM_Z  , _W}, {"STA", AM_Z  , _W}, {"STX", AM_Z  , _W}, {"axs", AM_Z  , _W}, // 84 .. 87
	{"DEY", 0     ,  0}, {"nop", AM_M  , im}, {"TXA", 0     ,  0}, {"xaa", AM_M  , im}, // 88 .. 8B
	{"STY", AM_A  , _W}, {"STA", AM_A  , _W}, {"STX", AM_A  , _W}, {"axs", AM_A  , _W}, // 8C .. 8F
	{"BCC", AM_R  ,  0}, {"STA", AM_NZY, _W}, {"hlt",     0 ,  0}, {"axa", AM_NZY, _W}, // 90 .. 93
	{"STY", AM_ZX , _W}, {"STA", AM_ZX , _W}, {"STX", AM_ZY , _W}, {"axs", AM_ZY , _W}, // 94 .. 97
	{"TYA", 0     ,  0}, {"STA", AM_AY , _W}, {"TXS", 0     ,  0}, {"tas", AM_AY , _W}, // 98 .. 9B
	{"say", AM_AX , _W}, {"STA", AM_AX , _W}, {"xas", AM_AX , _W}, {"axa", AM_AY , _W}, // 9C .. 9F

	{"LDY", AM_M  , im}, {"LDA", AM_IZX, R_}, {"LDX", AM_M  , im}, {"lax", AM_IZX, R_}, // A0 .. A3
	{"LDY", AM_Z  , R_}, {"LDA", AM_Z  , R_}, {"LDX", AM_Z  , R_}, {"lax", AM_Z  , R_}, // A4 .. A7
	{"TAY", 0     ,  0}, {"LDA", AM_M  , im}, {"TAX", 0     , 0 }, {"oal", AM_M  , im}, // A8 .. AB
	{"LDY", AM_A  , R_}, {"LDA", AM_A  , R_}, {"LDX", AM_A  , R_}, {"lax", AM_A  , R_}, // AC .. AF
	{"BCS", AM_R  ,  0}, {"LDA", AM_NZY, R_}, {"hlt", 0     , 0 }, {"lax", AM_NZY, R_}, // B0 .. B3
	{"LDY", AM_ZX , R_}, {"LDA", AM_ZX , R_}, {"LDX", AM_ZY , R_}, {"lax", AM_ZY , 0 }, // B4 .. B7
	{"CLV", 0     ,  0}, {"LDA", AM_AY , R_}, {"TSX", 0     , 0 }, {"las", AM_AY , R_}, // B8 .. BB
	{"LDY", AM_AX , R_}, {"LDA", AM_AX , R_}, {"LDX", AM_AY , R_}, {"lax", AM_AY , R_}, // BC .. BF

	{"CPY", AM_M  , im}, {"CMP", AM_IZX, R_}, {"nop", AM_M  , im}, {"dcm", AM_IZX, RW}, // C0 .. C3
	{"CPY", AM_Z  , R_}, {"CMP", AM_Z  , R_}, {"DEC", AM_Z  , RW}, {"dcm", AM_Z  , RW}, // C4 .. C7
	{"INY", 0     ,  0}, {"CMP", AM_M  , im}, {"DEX", 0     ,  0}, {"sax", AM_M  , im}, // C8 .. CB
	{"CPY", AM_A  , R_}, {"CMP", AM_A  , R_}, {"DEC", AM_A  , RW}, {"dcm", AM_A  , RW}, // CC .. CF
	{"BNE", AM_R  ,  0}, {"CMP", AM_NZY, R_}, {"hlt", 0     ,  0}, {"dcm", AM_NZY, RW}, // D0 .. D3
	{"nop", AM_ZX ,  0}, {"CMP", AM_ZX , R_}, {"DEC", AM_ZX , RW}, {"dcm", AM_ZX , RW}, // D4 .. D7
	{"CLD", 0     ,  0}, {"CMP", AM_AY , R_}, {"nop", 0     ,  0}, {"dcm", AM_AY , RW}, // D8 .. DB
	{"nop", AM_AX ,  0}, {"CMP", AM_AX , R_}, {"DEC", AM_AX , RW}, {"dcm", AM_AX , RW}, // DC .. DF

	{"CPX", AM_M  , im}, {"SBC", AM_IZX, R_}, {"nop", AM_M  , im}, {"ins", AM_IZX, RW}, // E0 .. E3
	{"CPX", AM_Z  , R_}, {"SBC", AM_Z  , R_}, {"INC", AM_Z  , RW}, {"ins", AM_Z  , RW}, // E4 .. E7
	{"INX", 0     ,  0}, {"SBC", AM_M  , im}, {"NOP", 0     ,  0}, {"sbc", AM_M  , im}, // E8 .. EB
	{"CPX", AM_A  , R_}, {"SBC", AM_A  , R_}, {"INC", AM_A  , RW}, {"ins", AM_A  , RW}, // EC .. EF
	{"BEQ", AM_R  ,  0}, {"SBC", AM_NZY, R_}, {"hlt", 0     ,  0}, {"ins", AM_NZY, RW}, // F0 .. F3
	{"nop", AM_ZX ,  0}, {"SBC", AM_ZX , R_}, {"INC", AM_ZX , RW}, {"ins", AM_ZX , RW}, // F4 .. F7
	{"SED", 0     ,  0}, {"SBC", AM_AY , R_}, {"nop", 0     ,  0}, {"ins", AM_AY , RW}, // F8 .. FB
	{"nop", AM_AX ,  0}, {"SBC", AM_AX , R_}, {"INC", AM_AX , RW}, {"ins", AM_AX , RW}  // FF .. FF
};

#undef R_
#undef _W
#undef RW
#undef _S
#undef im
#undef SW
#undef SR

// @reference: http://www.textfiles.com/apple/DOCUMENTATION/merlin.docs1

// Private __________________________________________________________________

	// NOTE: Keep in sync AsmDirectives_e g_aAssemblerDirectives !
	AssemblerDirective_t g_aAssemblerDirectives[ NUM_ASM_DIRECTIVES ] = 
	{
		// NULL n/a
		{""},
		// Origin, Target Address, EndProg, Equate, Data, AsciiString,HexString
		// Acme
		{"???"},
		// Big Mac
		{"???"},
		// DOS Tool Kit
		{"???"},
		// Lisa
		{"???"},
		// Merlin
		{"ASC"}, // ASC "postive" 'negative'
		{"DDB"}, // Define Double Byte (Define WORD)
		{"DFB"}, // DeFine Byte
		{"DS" }, // Define Storage
		{"HEX"}, // HEX ###### or HEX ##,##,...
		{"ORG"}, // Origin
		// MicroSparc
		{"???"},
		// ORCA/M
		{"???"},
		// SC ...
		{".OR"}, //    ORigin
		{".TA"}, //    Target Address
		{".EN"}, //    ENd of program
		{".EQ"}, //    EQuate
		{".DA"}, //    DAta
		{".AS"}, //    Ascii String
		{".HS"}, //    Hex String
		// Ted II
		{"???"},
		// Weller
		{"???"},
	// User-Defined
	// NOTE: Keep in sync AsmCustomDirective_e g_aAssemblerDirectives !
		{"db" }, // ASM_DEFINE_BYTE
		{"dw" }, // ASM_DEFINE_WORD
		{"da" }, // ASM_DEFINE_ADDRESS_16
// d    memory Dump
// da   Memory Ascii, Define Address
// ds   S = Ascii (Low),
// dt   T = Apple (High)
// dm   M = Mixed (Low,High=EndofString)
		{"ds" }, // ASM_DEFINE_ASCII_TEXT
		{"dt" }, // ASM_DEFINE_APPLE_TEXT
		{"dm" }, // ASM_DEFINE_TEXT_HI_LO

		{"df" }, // ASM_DEFINE_FLOAT
		{"dfx"}, // ASM_DEFINE_FLOAT_X
	};

	int g_iAssemblerSyntax = ASM_CUSTOM; // Which assembler syntax to use
	int g_aAssemblerFirstDirective[ NUM_ASSEMBLERS ] =
	{
		FIRST_A_DIRECTIVE,
		FIRST_B_DIRECTIVE,
		FIRST_D_DIRECTIVE,
		FIRST_L_DIRECTIVE,
		FIRST_M_DIRECTIVE,
		FIRST_u_DIRECTIVE,
		FIRST_O_DIRECTIVE,
		FIRST_S_DIRECTIVE,
		FIRST_T_DIRECTIVE,
		FIRST_W_DIRECTIVE,
		FIRST_Z_DIRECTIVE
	};

// Assemblers

	enum AssemblerFlags_e
	{
		AF_HaveLabel      = (1 << 0),
		AF_HaveComma      = (1 << 1),
		AF_HaveHash       = (1 << 2),
		AF_HaveImmediate  = (1 << 3),
		AF_HaveDollar     = (1 << 4),
		AF_HaveLeftParen  = (1 << 5),
		AF_HaveRightParen = (1 << 6),
		AF_HaveEitherParen= (1 << 7),
		AF_HaveBothParen  = (1 << 8),
		AF_HaveRegisterX  = (1 << 9),
		AF_HaveRegisterY  = (1 <<10),
		AF_HaveZeroPage   = (1 <<11),
		AF_HaveTarget     = (1 <<12),
	};

	enum AssemblerState_e
	{
		  AS_GET_MNEMONIC
		, AS_GET_MNEMONIC_PARM 
		, AS_GET_HASH
		, AS_GET_TARGET
		, AS_GET_PAREN
		, AS_GET_INDEX
		, AS_DONE
	};

	int         m_bAsmFlags;
	std::vector<int> m_vAsmOpcodes;
	int         m_iAsmAddressMode = AM_IMPLIED;

	struct DelayedTarget_t
	{
		char m_sAddress[ MAX_SYMBOLS_LEN + 1 ];
		WORD m_nBaseAddress; // mem address to store symbol at
		int  m_nOpcode ;
		int  m_iOpmode ; // AddressingMode_e
	};
	
	std::vector <DelayedTarget_t> m_vDelayedTargets;
	bool                     m_bDelayedTargetsDirty = false;

	int  m_nAsmBytes         = 0;
	WORD m_nAsmBaseAddress   = 0;
	WORD m_nAsmTargetAddress = 0;
	WORD m_nAsmTargetValue   = 0;

// Private
	void AssemblerHashOpcodes ();
	void AssemblerHashDirectives ();

// Implementation ___________________________________________________________


//===========================================================================
bool _6502_CalcRelativeOffset( int nOpcode, int nBaseAddress, int nTargetAddress, WORD * pTargetOffset_ )
{
	if (_6502_IsOpcodeBranch( nOpcode))
	{
		// Branch is
		//   a) relative to address+2
		//   b) in 2's compliment
		//
		// i.e.
		//   300: D0 7F -> BNE $381   0x381 - 0x300 = 0x81 +129
		//   300: D0 80 -> BNE $282   0x282 - 0x300 =      -126
		//
		// 300: D0 7E BNE $380
		// ^    ^   ^      ^
		// |    |   |      TargetAddress
		// |    |   TargetOffset
		// |    Opcode
		// BaseAddress
		int nDistance = nTargetAddress - nBaseAddress;
		if (pTargetOffset_)
			*pTargetOffset_ = (BYTE)(nDistance - 2); 

		if ((nDistance - 2) > _6502_BRANCH_POS)
			m_iAsmAddressMode = NUM_OPMODES; // signal bad

		if ((nDistance - 2) < _6502_BRANCH_NEG)
			m_iAsmAddressMode = NUM_OPMODES; // signal bad

		return true;
	}

	return false;
}


//===========================================================================
int  _6502_GetOpmodeOpbyte ( const int nBaseAddress, int & iOpmode_, int & nOpbyte_, const DisasmData_t** pData_ )
{
#if _DEBUG
	if (! g_aOpcodes)
	{
		MessageBox(GetFrame().g_hFrameWindow, "Debugger not properly initialized", "ERROR", MB_OK );

		g_aOpcodes = & g_aOpcodes65C02[ 0 ];	// Enhanced Apple //e
		g_aOpmodes[ AM_2 ].m_nBytes = 2;
		g_aOpmodes[ AM_3 ].m_nBytes = 3;
	}
#endif

	int iOpcode_ = *(mem + nBaseAddress);
		iOpmode_ = g_aOpcodes[ iOpcode_ ].nAddressMode;
		nOpbyte_ = g_aOpmodes[ iOpmode_ ].m_nBytes;

	// 2.6.2.25 Fixed: DB DW custom data byte sizes weren't scrolling properly in the disasm view.
    //          Changed _6502_GetOpmodeOpbyte() to be aware of data bytes.
	//
	// NOTE: _6502_GetOpmodeOpbyte() needs to (effectively) call Disassembly_GetData()
	//    a) the CmdCursorLineUp() calls us to calc for -X bytes back up how to reach the cursor (address) line below
	//    b) The disassembler view needs to know how many bytes each line is.
	int nSlack;

	// 2.7.0.0 TODO: FIXME: Opcode length that over-lap data, should be shortened ... if (nOpbyte_ > 1) if Disassembly_IsDataAddress( nBaseAddress + 1 ) nOpbyte_ = 1;
	DisasmData_t* pData = Disassembly_IsDataAddress( nBaseAddress );
	if( pData )
	{
		if( pData_ )
			*pData_ = pData;

		nSlack = pData->nEndAddress - pData->nStartAddress + 1; // *inclusive* KEEP IN SYNC: _CmdDefineByteRange() CmdDisasmDataList() _6502_GetOpmodeOpbyte() FormatNopcodeBytes()

		// Data Disassembler
		// Smart Disassembly - Data Section
		// Assemblyer Directives - Psuedo Mnemonics
		switch( pData->eElementType )
		{
			case NOP_BYTE_1: nOpbyte_ = 1; iOpmode_ = AM_M; break;
			case NOP_BYTE_2: nOpbyte_ = 2; iOpmode_ = AM_M; break;
			case NOP_BYTE_4: nOpbyte_ = 4; iOpmode_ = AM_M; break;
			case NOP_BYTE_8: nOpbyte_ = 8; iOpmode_ = AM_M; break;
			case NOP_WORD_1: nOpbyte_ = 2; iOpmode_ = AM_M; break;
			case NOP_WORD_2: nOpbyte_ = 4; iOpmode_ = AM_M; break;
			case NOP_WORD_4: nOpbyte_ = 8; iOpmode_ = AM_M; break;
			case NOP_ADDRESS:nOpbyte_ = 2; iOpmode_ = AM_A; // BUGFIX: 2.6.2.33 Define Address should be shown as Absolute mode, not Indirect Absolute mode. DA BASIC.FPTR D000:D080 // was showing as "da (END-1)" now shows as "da END-1"
				pData->nTargetAddress = *(LPWORD)(mem+nBaseAddress);
				break;
			case NOP_STRING_APPLE:
				iOpmode_ = AM_DATA;
				nOpbyte_ = nSlack;
				break;
			case NOP_STRING_APPLESOFT:
				// TODO: FIXME: scan memory for high byte
				nOpbyte_ = 8;
				iOpmode_ = AM_DATA;
				break;
			default:
#if _DEBUG // not implemented!
				int *fatal = 0;
				*fatal = 0xDEADC0DE;
#endif
				break;
		}
/*
		// REMOVED in v1.25 ... because of AppleSoft Basic:  DW NEXT1 801  DW LINE1 803
		// Check if we are not element aligned ...
		nSlack = (nOpbyte_ > 1) ? (nBaseAddress & nOpbyte_-1 ) : 0;
		if (nSlack)
		{
			nOpbyte_ = nSlack;
			iOpmode_ = AM_M;
		}
*/
		//iOpcode_ = NUM_OPCODES; // Don't have valid opcodes ... we have data !
		// iOpcode_ = (int)( pData ); // HACK: pass pData back to caller ...
		iOpcode_ = OPCODE_NOP;
	}

#if _DEBUG
	if (iOpcode_ >= NUM_OPCODES)
	{
		bool bStop = true;
	}
#endif

	return iOpcode_;
}


//===========================================================================
void _6502_GetOpcodeOpmodeOpbyte ( int & iOpcode_, int & iOpmode_, int & nOpbyte_ )
{
	iOpcode_ = _6502_GetOpmodeOpbyte( regs.pc, iOpmode_, nOpbyte_ );
}

//===========================================================================
bool _6502_GetStackReturnAddress ( WORD & nAddress_ )
{
	unsigned nStack = regs.sp;
	nStack++;

	if (nStack <= (_6502_STACK_END - 1))
	{
		nAddress_ = (unsigned)*(LPBYTE)(mem + nStack);
		nStack++;
		
		nAddress_ += ((unsigned)*(LPBYTE)(mem + nStack)) << 8;
		nAddress_++;
		return true;
	}
	return false;
}


//===========================================================================
bool _6502_GetTargets ( WORD nAddress, int *pTargetPartial_, int *pTargetPartial2_, int *pTargetPointer_, int * pTargetBytes_,
						bool bIgnoreBranch /*= true*/, bool bIncludeNextOpcodeAddress /*= true*/ )
{
	if (! pTargetPartial_)
		return false;

	if (! pTargetPartial2_)
		return false;

	if (! pTargetPointer_)
		return false;

//	if (! pTargetBytes_)
//		return false;

	*pTargetPartial_  = NO_6502_TARGET;
	*pTargetPartial2_ = NO_6502_TARGET;
	*pTargetPointer_  = NO_6502_TARGET;

	if (pTargetBytes_)
		*pTargetBytes_  = 0;	

	BYTE nOpcode   = mem[nAddress];
	BYTE nTarget8  = mem[(nAddress+1)&0xFFFF];
	WORD nTarget16 = (mem[(nAddress+2)&0xFFFF]<<8) | nTarget8;

	int eMode = g_aOpcodes[ nOpcode ].nAddressMode;

// We really need to use the values that are code and data assembler
// TODO: FIXME: _6502_GetOpmodeOpbyte( iAddress, iOpmode, nOpbytes );

	switch (eMode)
	{
		case AM_IMPLIED:
			if (g_aOpcodes[ nOpcode ].nMemoryAccess & MEM_S)	// Stack R/W?
			{
				if (nOpcode == OPCODE_RTI || nOpcode == OPCODE_RTS)	// RTI or RTS?
				{
					WORD sp = regs.sp;

					if (nOpcode == OPCODE_RTI)
					{
						//*pTargetPartial3_ = _6502_STACK_BEGIN + ((sp+1) & 0xFF);	// TODO: PLP
						++sp;
					}

					*pTargetPartial_  = _6502_STACK_BEGIN + ((sp+1) & 0xFF);
					*pTargetPartial2_ = _6502_STACK_BEGIN + ((sp+2) & 0xFF);
					nTarget16 = mem[*pTargetPartial_] + (mem[*pTargetPartial2_]<<8);

					if (nOpcode == OPCODE_RTS)
						++nTarget16;
				}
				else if (nOpcode == OPCODE_BRK)	// BRK?
				{
					*pTargetPartial_  = _6502_STACK_BEGIN + ((regs.sp+0) & 0xFF);
					*pTargetPartial2_ = _6502_STACK_BEGIN + ((regs.sp-1) & 0xFF);
					//*pTargetPartial3_ = _6502_STACK_BEGIN + ((regs.sp-2) & 0xFF);	// TODO: PHP
					//*pTargetPartial4_ = _6502_BRK_VECTOR + 0;	// TODO
					//*pTargetPartial5_ = _6502_BRK_VECTOR + 1;	// TODO
					nTarget16 = *(LPWORD)(mem + _6502_BRK_VECTOR);
				}
				else	// PHn/PLn
				{
					if (g_aOpcodes[ nOpcode ].nMemoryAccess & MEM_WI)
						nTarget16 = _6502_STACK_BEGIN + ((regs.sp+0) & 0xFF);
					else
						nTarget16 = _6502_STACK_BEGIN + ((regs.sp+1) & 0xFF);
				}

				if (bIncludeNextOpcodeAddress || (nOpcode != OPCODE_RTI && nOpcode != OPCODE_RTS && nOpcode != OPCODE_BRK))
					*pTargetPointer_ = nTarget16;

				if (pTargetBytes_)
					*pTargetBytes_ = 1;
			}
			break;

		case AM_A: // Absolute
			if (nOpcode == OPCODE_JSR)
			{
				*pTargetPartial_  = _6502_STACK_BEGIN + ((regs.sp+0) & 0xFF);
				*pTargetPartial2_ = _6502_STACK_BEGIN + ((regs.sp-1) & 0xFF);
			}

			if (bIncludeNextOpcodeAddress || (nOpcode != OPCODE_JSR && nOpcode != OPCODE_JMP_A))
				*pTargetPointer_ = nTarget16;

			if (pTargetBytes_)
				*pTargetBytes_ = 2;
			break;

		case AM_IAX: // Indexed (Absolute) Indirect - ie. JMP (abs,x)
			_ASSERT(nOpcode == OPCODE_JMP_IAX);
			nTarget16 += regs.x;
			*pTargetPartial_    = nTarget16;
			*pTargetPartial2_   = nTarget16+1;
			if (bIncludeNextOpcodeAddress)
				*pTargetPointer_ = *(LPWORD)(mem + nTarget16);
			if (pTargetBytes_)
				*pTargetBytes_ = 2;
			break;

		case AM_AX: // Absolute, X
			nTarget16 += regs.x;
			*pTargetPointer_   = nTarget16;
			if (pTargetBytes_)
				*pTargetBytes_ = 2;
			break;

		case AM_AY: // Absolute, Y
			nTarget16 += regs.y;
			*pTargetPointer_   = nTarget16;
			if (pTargetBytes_)
				*pTargetBytes_ = 2;
			break;

		case AM_NA: // Indirect (Absolute) - ie. JMP (abs)
			_ASSERT(nOpcode == OPCODE_JMP_NA);
			*pTargetPartial_    = nTarget16;
			*pTargetPartial2_   = nTarget16+1;
			if (bIncludeNextOpcodeAddress)
				*pTargetPointer_ = *(LPWORD)(mem + nTarget16);
			if (pTargetBytes_)
				*pTargetBytes_ = 2;
			break;

		case AM_IZX: // Indexed (Zeropage Indirect, X)
			nTarget8  += regs.x;
			*pTargetPartial_    = nTarget8;
			*pTargetPointer_    = *(LPWORD)(mem + nTarget8);
			if (pTargetBytes_)
				*pTargetBytes_ = 2;
			break;

		case AM_NZY: // Indirect (Zeropage) Indexed, Y
			*pTargetPartial_    = nTarget8;
			*pTargetPointer_    = ((*(LPWORD)(mem + nTarget8)) + regs.y) & _6502_MEM_END; // Bugfix: 
			if (pTargetBytes_)
				*pTargetBytes_ = 1;
			break;

		case AM_NZ: // Indirect (Zeropage)
			*pTargetPartial_    = nTarget8;
			*pTargetPointer_    = *(LPWORD)(mem + nTarget8);
			if (pTargetBytes_)
				*pTargetBytes_ = 2;
			break;

		case AM_R:
			if (!bIgnoreBranch)
			{
				*pTargetPartial_  = nTarget8;
				*pTargetPointer_ = nAddress + 2;

				if (nTarget8 <= _6502_BRANCH_POS)
					*pTargetPointer_ += nTarget8; // +
				else
					*pTargetPointer_ -= nTarget8; // -

				*pTargetPointer_ &= _6502_MEM_END;

				if (pTargetBytes_)
					*pTargetBytes_ = 1;
			}
			break;

		case AM_Z: // Zeropage
			*pTargetPointer_   = nTarget8;
			if (pTargetBytes_)
				*pTargetBytes_ = 1;
			break;

		case AM_ZX: // Zeropage, X
			*pTargetPointer_   = (nTarget8 + regs.x) & 0xFF; // .21 Bugfix: shouldn't this wrap around? Yes.
			if (pTargetBytes_)
				*pTargetBytes_ = 1;
			break;

		case AM_ZY: // Zeropage, Y
			*pTargetPointer_   = (nTarget8 + regs.y) & 0xFF; // .21 Bugfix: shouldn't this wrap around? Yes.
			if (pTargetBytes_)
				*pTargetBytes_ = 1;
			break;

		default:
			if (pTargetBytes_)
				*pTargetBytes_ = 0;
			break;
	}

	return true;
}


//===========================================================================
bool _6502_GetTargetAddress ( const WORD & nAddress, WORD & nTarget_ )
{
	int iOpcode;
	int iOpmode;
	int nOpbytes;
	iOpcode = _6502_GetOpmodeOpbyte( nAddress, iOpmode, nOpbytes );

	// Composite string that has the target nAddress

	if ((iOpmode != AM_IMPLIED) &&
		(iOpmode != AM_1) &&
		(iOpmode != AM_2) &&
		(iOpmode != AM_3))
	{
		int nTargetPartial;
		int nTargetPartial2;
		int nTargetPointer;
		int nTargetBytes;
		_6502_GetTargets( nAddress, &nTargetPartial, &nTargetPartial2, &nTargetPointer, &nTargetBytes, false );

//		if (nTargetPointer == NO_6502_TARGET)
//		{
//			if (_6502_IsOpcodeBranch( nOpcode )
//			{
//				return true;
//			}
//		}
		if (nTargetPointer != NO_6502_TARGET)
//		else
		{
			nTarget_ = nTargetPointer & _6502_MEM_END;
			return true;
		}
	}
	return false;
}

//===========================================================================
bool _6502_IsOpcodeBranch ( int iOpcode )
{
	// 76543210 Bit
	// xxx10000 Branch
	if (iOpcode == OPCODE_BRA)
		return true;

	if ((iOpcode & 0x1F) != 0x10) // low nibble not zero?
		return false;

	if ((iOpcode >> 4) & 1)
		return true;
	
//		(nOpcode == 0x10) || // BPL
//		(nOpcode == 0x30) || // BMI
//		(nOpcode == 0x50) || // BVC
//		(nOpcode == 0x70) || // BVS
//		(nOpcode == 0x90) || // BCC
//		(nOpcode == 0xB0) || // BCS
//		(nOpcode == 0xD0) || // BNE
//		(nOpcode == 0xF0) || // BEQ
	return false;
}


//===========================================================================
bool _6502_IsOpcodeValid ( int iOpcode )
{
	if ((iOpcode & 0x3) == 0x3)
		return false;

	if (islower( g_aOpcodes6502[ iOpcode ].sMnemonic[ 0 ] ))
		return false;

	return true;
}

// Assembler ________________________________________________________________


//===========================================================================
Hash_t AssemblerHashMnemonic ( const TCHAR * pMnemonic )
{
	const TCHAR *pText = pMnemonic;
	Hash_t nMnemonicHash = 0;
	int iHighBits;

	const int    NUM_LOW_BITS = 19; // 24 -> 19 prime
	const int    NUM_MSK_BITS =  5; //  4 ->  5 prime
	const Hash_t BIT_MSK_HIGH = ((1 << NUM_MSK_BITS) - 1) << NUM_LOW_BITS;

#if DEBUG_ASSEMBLER
	int nLen = strlen( pMnemonic );
	static int nMaxLen = 0;
	if (nMaxLen < nLen) {
		nMaxLen = nLen;
		char sText[CONSOLE_WIDTH * 3];
		ConsolePrintFormat( sText, "New Max Len: %d  %s", nMaxLen, pMnemonic );
	}
#endif

	while( *pText )
//	for( int iChar = 0; iChar < 4; iChar++ )
	{	
		char c = tolower( *pText ); // TODO: based on ALLOW_INPUT_LOWERCASE ??

		nMnemonicHash = (nMnemonicHash << NUM_MSK_BITS) + c;
		iHighBits = (nMnemonicHash & BIT_MSK_HIGH);
		if (iHighBits)
		{
			nMnemonicHash = (nMnemonicHash ^ (iHighBits >> NUM_LOW_BITS)) & ~ BIT_MSK_HIGH;
		}
		pText++;
	}

	return nMnemonicHash;
}


//===========================================================================
void AssemblerHashOpcodes ()
{
	Hash_t nMnemonicHash;
	int    iOpcode;

	for( iOpcode = 0; iOpcode < NUM_OPCODES; iOpcode++ )
	{
		const TCHAR *pMnemonic = g_aOpcodes65C02[ iOpcode ].sMnemonic;
		nMnemonicHash = AssemblerHashMnemonic( pMnemonic );
		g_aOpcodesHash[ iOpcode ] = nMnemonicHash;
#if DEBUG_ASSEMBLER
	   //OutputDebugString( "" );
      char sText[ 128 ];
      ConsolePrintFormat( sText, "%s : %08X  ", pMnemonic, nMnemonicHash );
	   // CLC: 002B864
#endif
	}
	ConsoleUpdate();
}

//===========================================================================
void AssemblerHashDirectives ()
{
	Hash_t nMnemonicHash;
	int    iOpcode;

	for( iOpcode = 0; iOpcode < NUM_ASM_M_DIRECTIVES; iOpcode++ )
	{
		int iNopcode = FIRST_M_DIRECTIVE + iOpcode;
//.		const TCHAR *pMnemonic = g_aAssemblerDirectivesMerlin[ iOpcode ].m_pMnemonic;
		const TCHAR *pMnemonic = g_aAssemblerDirectives[ iNopcode ].m_pMnemonic;
		nMnemonicHash = AssemblerHashMnemonic( pMnemonic );
		g_aAssemblerDirectives[ iNopcode ].m_nHash = nMnemonicHash;
	}
}

//===========================================================================
void AssemblerStartup()
{
	AssemblerHashOpcodes();
	AssemblerHashDirectives();
}
 
//===========================================================================
void _CmdAssembleHashDump ()
{
// #if DEBUG_ASM_HASH
	std::vector<HashOpcode_t> vHashes;
	HashOpcode_t         tHash;
	TCHAR                sText[ CONSOLE_WIDTH ];

	int iOpcode;
	for( iOpcode = 0; iOpcode < NUM_OPCODES; iOpcode++ )
	{
		tHash.m_iOpcode = iOpcode;
		tHash.m_nValue  = g_aOpcodesHash[ iOpcode ]; 
		vHashes.push_back( tHash );
	}	
	
	std::sort( vHashes.begin(), vHashes.end(), HashOpcode_t() );

//	Hash_t nPrevHash = vHashes.at( 0 ).m_nValue;
	Hash_t nThisHash = 0;

	for( iOpcode = 0; iOpcode < NUM_OPCODES; iOpcode++ )
	{
		tHash = vHashes.at( iOpcode );

		Hash_t iThisHash = tHash.m_nValue;
		int    nOpcode   = tHash.m_iOpcode;
		int    nOpmode   = g_aOpcodes[ nOpcode ].nAddressMode;

		ConsoleBufferPushFormat( sText, "%08X %02X %s %s"
			, iThisHash
			, nOpcode
			, g_aOpcodes65C02[ nOpcode ].sMnemonic
			, g_aOpmodes[ nOpmode  ].m_sName
		);
		nThisHash++;
		
//		if (nPrevHash != iThisHash)
//		{
//			ConsoleBufferPushFormat( sText, "Total: %d", nThisHash );
//			nThisHash = 0;
//		}
	}

	ConsoleUpdate();
//#endif
}


 
//===========================================================================
int AssemblerPokeAddress( const int Opcode, const int nOpmode, const WORD nBaseAddress, const WORD nTargetOffset )
{
//	int nOpmode  = g_aOpcodes[ nOpcode ].nAddressMode;
	int nOpbytes = g_aOpmodes[ nOpmode ].m_nBytes;

	// if (nOpbytes != nBytes)
	//	ConsoleDisplayError( TEXT(" ERROR: Input Opcode bytes differs from actual!" ) );

	*(memdirty + (nBaseAddress >> 8)) |= 1;
//	*(mem + nBaseAddress) = (BYTE) nOpcode;

	if (nOpbytes > 1)
		*(mem + nBaseAddress + 1) = (BYTE)(nTargetOffset >> 0);

	if (nOpbytes > 2)
		*(mem + nBaseAddress + 2) = (BYTE)(nTargetOffset >> 8);

	return nOpbytes;
}

//===========================================================================
bool AssemblerPokeOpcodeAddress( const WORD nBaseAddress )
{
	int iAddressMode = m_iAsmAddressMode; // opmode detected from input
	int nTargetValue = m_nAsmTargetValue;

	int iOpcode;
	int nOpcodes = m_vAsmOpcodes.size();

	for( iOpcode = 0; iOpcode < nOpcodes; iOpcode++ )
	{
		int nOpcode = m_vAsmOpcodes.at( iOpcode ); // m_iOpcode;
		int nOpmode = g_aOpcodes[ nOpcode ].nAddressMode;

		if (nOpmode == iAddressMode)
		{
			*(mem + nBaseAddress) = (BYTE) nOpcode;
			int nOpbytes = AssemblerPokeAddress( nOpcode, nOpmode, nBaseAddress, nTargetValue );

			if (m_bDelayedTargetsDirty)
			{			
				int nDelayedTargets = m_vDelayedTargets.size();
				DelayedTarget_t *pTarget = & m_vDelayedTargets.at( nDelayedTargets - 1 );

				pTarget->m_nOpcode = nOpcode;
				pTarget->m_iOpmode = nOpmode;
			}

			g_nAssemblerAddress += nOpbytes;
			return true;
		}

	}

	return false;
}


//===========================================================================
bool TestFlag( AssemblerFlags_e eFlag )
{
	if (m_bAsmFlags & eFlag)
		return true;

	return false;
}

//===========================================================================
void SetFlag( AssemblerFlags_e eFlag, bool bValue = true )
{
	if (bValue)
		m_bAsmFlags |= eFlag;
	else
		m_bAsmFlags &= ~ eFlag;
}


/*
	Output
		AM_IMPLIED
		AM_M
		AM_A
		AM_Z
		AM_I // indexed or indirect
*/
//===========================================================================
bool AssemblerGetArgs( int iArg, int nArgs, WORD nBaseAddress )
{
	m_iAsmAddressMode = AM_IMPLIED;
	AssemblerState_e eNextState = AS_GET_MNEMONIC; 

	m_bAsmFlags  = 0;
	m_nAsmTargetAddress = 0;

	int nBase = 10;

	// Sync up to Raw Args for matching mnemonic
	// Process them instead of the cooked args, since we need the orginal tokens
	Arg_t *pArg = &g_aArgRaw[ iArg ];

	while (iArg < g_nArgRaw)
	{
		int iToken = pArg->eToken;
//		int iType  = pArg->bType;

		if (iToken == TOKEN_HASH)
		{
			if (eNextState != AS_GET_MNEMONIC_PARM)
			{
				ConsoleBufferPush( TEXT( " Syntax Error: '#'" ) );
				return false;
			}
			if (TestFlag( AF_HaveHash ))
			{
				ConsoleBufferPush( TEXT( " Syntax Error: Extra '#'" ) ); // No thanks, we already have one
				return false;
			}
			SetFlag( AF_HaveHash );

			m_iAsmAddressMode = AM_M; // Immediate
			eNextState = AS_GET_TARGET;
			m_nAsmBytes = 1;
		}
		else
		if (iToken == TOKEN_DOLLAR)
		{
			if (TestFlag( AF_HaveDollar ))
			{
				ConsoleBufferPush( TEXT( " Syntax Error: Extra '$'" ) ); // No thanks, we already have one
				return false;
			}

			nBase = 16; // switch to hex

			if (! TestFlag( AF_HaveHash))
			{
				SetFlag( AF_HaveDollar );
				m_iAsmAddressMode = AM_A; // Absolute
			}
			eNextState = AS_GET_TARGET;
			m_nAsmBytes = 2;
		}
		else
		if (iToken == TOKEN_PAREN_L)
		{
			if (TestFlag( AF_HaveLeftParen ))
			{
				ConsoleBufferPush( TEXT( " Syntax Error: Extra '('" ) ); // No thanks, we already have one
				return false;
			}
			SetFlag( AF_HaveLeftParen );

			// Indexed or Indirect
			m_iAsmAddressMode = AM_I;
		}
		else
		if (iToken == TOKEN_PAREN_R)
		{
			if (TestFlag( AF_HaveRightParen ))
			{
				ConsoleBufferPush( TEXT( " Syntax Error: Extra ')'" ) ); // No thanks, we already have one
				return false;
			}
			SetFlag( AF_HaveRightParen );

			// Indexed or Indirect
			m_iAsmAddressMode = AM_I;
		}
		else
		if (iToken == TOKEN_COMMA)
		{
			if (TestFlag( AF_HaveComma ))
			{
				ConsoleBufferPush( TEXT( " Syntax Error: Extra ','" ) ); // No thanks, we already have one
				return false;
			}
			SetFlag( AF_HaveComma );
			eNextState = AS_GET_INDEX;
			// We should have address by now
		}
		else
		if (iToken == TOKEN_LESS_THAN)
		{
		}
		else
		if (iToken == TOKEN_GREATER_THAN)
		{
		}
		else
		if (iToken == TOKEN_SEMI) // comment
		{
			break;
		}
		else
		if (iToken == TOKEN_ALPHANUMERIC)
		{
			if (eNextState == AS_GET_MNEMONIC)
			{
				eNextState = AS_GET_MNEMONIC_PARM;
			}
			else
			if (eNextState == AS_GET_MNEMONIC_PARM)
			{
				eNextState = AS_GET_TARGET;
			}

			if (eNextState == AS_GET_TARGET)
			{
				SetFlag( AF_HaveTarget );

				ArgsGetValue( pArg, & m_nAsmTargetAddress, nBase );

				// Do Symbol Lookup
				WORD nSymbolAddress;
				bool bExists = FindAddressFromSymbol( pArg->sArg, &nSymbolAddress );
				if (bExists)
				{
					m_nAsmTargetAddress = nSymbolAddress;

					if (m_iAsmAddressMode == AM_IMPLIED)
						m_iAsmAddressMode = AM_A;
				}
				else
				{
					// if valid hex address, don't have delayed target
					TCHAR sAddress[ 32 ];
					wsprintf( sAddress, "%X", m_nAsmTargetAddress);
					if (_tcscmp( sAddress, pArg->sArg))
					{
						DelayedTarget_t tDelayedTarget;

						tDelayedTarget.m_nBaseAddress = nBaseAddress;
						strncpy( tDelayedTarget.m_sAddress, pArg->sArg, MAX_SYMBOLS_LEN );
						tDelayedTarget.m_sAddress[ MAX_SYMBOLS_LEN ] = 0;

						// Flag this target that we need to update it when we have the relevent info
						m_bDelayedTargetsDirty = true;

						tDelayedTarget.m_nOpcode = 0;
						tDelayedTarget.m_iOpmode = m_iAsmAddressMode;
						
						m_vDelayedTargets.push_back( tDelayedTarget );

						m_nAsmTargetAddress = 0;
					}
				}

				if ((m_iAsmAddressMode != AM_M) &&
					(m_iAsmAddressMode != AM_IMPLIED) &&
					(! m_bDelayedTargetsDirty))
				{
					if (m_nAsmTargetAddress <= _6502_ZEROPAGE_END)
					{
						m_iAsmAddressMode = AM_Z;
						m_nAsmBytes = 1;
					}
				}
			}
			if (eNextState == AS_GET_INDEX)
			{
				if (pArg->nArgLen == 1)
				{
					if (pArg->sArg[0] == 'X')
					{
						if (! TestFlag( AF_HaveComma ))
						{
							ConsoleBufferPush( TEXT( " Syntax Error: Missing ','" ) );
							return false;
						}
						SetFlag( AF_HaveRegisterX );
					}
					if (pArg->sArg[0] == 'Y')
					{
						if (! (TestFlag( AF_HaveComma )))
						{
							ConsoleBufferPush( TEXT( " Syntax Error: Missing ','" ) );
							return false;
						}
						SetFlag( AF_HaveRegisterY );
					}
				}
			}
		}

		iArg++;
		pArg++;
	}

	return true;
}


//===========================================================================
bool AssemblerUpdateAddressingMode()
{
	SetFlag( AF_HaveEitherParen, TestFlag(AF_HaveLeftParen) || TestFlag(AF_HaveRightParen) );
	SetFlag( AF_HaveBothParen, TestFlag(AF_HaveLeftParen) && TestFlag(AF_HaveRightParen) );

	if ((TestFlag( AF_HaveLeftParen )) && (! TestFlag( AF_HaveRightParen )))
	{
		ConsoleBufferPush( TEXT( " Syntax Error: Missing ')'" ) );
		return false;
	}

	if ((! TestFlag( AF_HaveLeftParen )) && (  TestFlag( AF_HaveRightParen )))
	{
		ConsoleBufferPush( TEXT( " Syntax Error: Missing '('" ) );
		return false;
	}

	if (TestFlag( AF_HaveComma ))
	{
		if ((! TestFlag( AF_HaveRegisterX )) && (! TestFlag( AF_HaveRegisterY )))
		{
			ConsoleBufferPush( TEXT( " Syntax Error: Index 'X' or 'Y'" ) );
			return false;
		}
	}

	if (TestFlag( AF_HaveBothParen ))
	{
		if (TestFlag( AF_HaveComma ))
		{
			if (TestFlag( AF_HaveRegisterX ))
			{
				m_iAsmAddressMode = AM_AX;
				m_nAsmBytes = 2;
				if (m_nAsmTargetAddress <= _6502_ZEROPAGE_END)
				{
					m_iAsmAddressMode = AM_ZX;
					m_nAsmBytes = 1;
				}
			}
			if (TestFlag( AF_HaveRegisterY ))
			{
				m_iAsmAddressMode = AM_AY;
				m_nAsmBytes = 2;
				if (m_nAsmTargetAddress <= _6502_ZEROPAGE_END)
				{
					m_iAsmAddressMode = AM_ZY;
					m_nAsmBytes = 1;
				}
			}
		}
	}		

	if ((m_iAsmAddressMode == AM_A) || (m_iAsmAddressMode == AM_Z))
	{
		if (! TestFlag( AF_HaveEitherParen)) // if no paren
		{
			if (TestFlag( AF_HaveComma ) && TestFlag( AF_HaveRegisterX ))
			{
				if (m_iAsmAddressMode == AM_Z)
					m_iAsmAddressMode = AM_ZX;
				else
					m_iAsmAddressMode = AM_AX;
			}
			if (TestFlag( AF_HaveComma ) && TestFlag( AF_HaveRegisterY ))
			{
				if (m_iAsmAddressMode == AM_Z)
					m_iAsmAddressMode = AM_ZY;
				else
				m_iAsmAddressMode = AM_AY;
			}
		}
	}
	
	if (m_iAsmAddressMode == AM_I)
	{
		if (! TestFlag( AF_HaveEitherParen)) // if no paren
		{
			// Indirect Zero Page
			// Indirect Absolute
		}
	}

	m_nAsmTargetValue = m_nAsmTargetAddress;
	
	int nOpcode = m_vAsmOpcodes.at( 0 ); // branch opcodes don't vary (only 1 Addressing Mode)
	if (_6502_CalcRelativeOffset( nOpcode, m_nAsmBaseAddress, m_nAsmTargetAddress, & m_nAsmTargetValue ))
	{
		if (m_iAsmAddressMode == NUM_OPMODES)
			return false;

		m_iAsmAddressMode = AM_R;
	}

	return true;
}


//===========================================================================
int AssemblerDelayedTargetsSize()
{
	int nSize = m_vDelayedTargets.size();
	return nSize;
}


// The Assembler was terminated, with Symbol(s) declared, but not (yet) defined.
// i.e.
// A 300
//     BNE $DONE
// <enter>
//===========================================================================
void AssemblerProcessDelayedSymols()
{
	m_bDelayedTargetsDirty = false; // assembler set signal if new symbol was added

	bool bModified = false;
	while (! bModified)
	{
		bModified = false;
		
		std::vector<DelayedTarget_t>::iterator iSymbol;
		for( iSymbol = m_vDelayedTargets.begin(); iSymbol != m_vDelayedTargets.end(); ++iSymbol )
		{
			DelayedTarget_t *pTarget = & (*iSymbol); // m_vDelayedTargets.at( iSymbol );

			WORD nTargetAddress;
			bool bExists = FindAddressFromSymbol( pTarget->m_sAddress, & nTargetAddress );
			if (bExists)
			{
				// TODO: need to handle #<symbol, #>symbol, symbol+n, symbol-n

				bModified = true;

				int nOpcode  = pTarget->m_nOpcode;
				int nOpmode  = g_aOpcodes[ nOpcode ].nAddressMode;
//				int nOpbytes = g_aOpmodes[ nOpmode ].m_nBytes;

				// 300: D0 7E BNE $380
				// ^       ^      ^
				// |       |      TargetAddress
				// |       TargetValue
				// BaseAddress				
				WORD nTargetValue = nTargetAddress;

				if (_6502_CalcRelativeOffset( nOpcode, pTarget->m_nBaseAddress, nTargetAddress, & nTargetValue ))
				{
					if (m_iAsmAddressMode == NUM_OPMODES)
					{
						nTargetValue = 0;
						bModified = false;
					}
				}
				
				if (bModified)
				{
					AssemblerPokeAddress( nOpcode, nOpmode, pTarget->m_nBaseAddress, nTargetValue );
					*(memdirty + (pTarget->m_nBaseAddress >> 8)) |= 1;

					m_vDelayedTargets.erase( iSymbol );

					// iterators are invalid after the point of deletion
					// need to restart enumeration
					break;
				}
			}
		}

		if (! bModified)
			break;
	}
}


bool Assemble( int iArg, int nArgs, WORD nAddress )
{
	bool bGotArgs;
	bool bGotMode;
	bool bGotByte;
	
	// Since, making 2-passes is not an option,
	// we need to buffer the target address fix-ups.
	AssemblerProcessDelayedSymols();

	m_nAsmBaseAddress = nAddress;

	TCHAR *pMnemonic = g_aArgs[ iArg ].sArg;
	Hash_t nMnemonicHash = AssemblerHashMnemonic( pMnemonic );

#if DEBUG_ASSEMBLER
	char sText[ CONSOLE_WIDTH * 2 ];
	ConsolePrintFormat( sText, "%s%04X%s: %s%s%s -> %s%08X", 
		CHC_ADDRESS, nAddress,
		CHC_DEFAULT,
		CHC_STRING, pMnemonic,
		CHC_DEFAULT,
		CHC_NUM_HEX, nMnemonicHash );
#endif

	m_vAsmOpcodes.clear(); // Candiate opcodes
	int iOpcode;
	
	// Ugh! Linear search.
	for( iOpcode = 0; iOpcode < NUM_OPCODES; iOpcode++ )
	{
		if (nMnemonicHash == g_aOpcodesHash[ iOpcode ])
		{
			m_vAsmOpcodes.push_back( iOpcode );
		}
	}

	int nOpcodes = m_vAsmOpcodes.size();
	if (! nOpcodes)
	{
		// Check for assembler directive

		ConsoleBufferPush( TEXT(" Syntax Error: Invalid mnemonic") );
		return false;
	}
	else
	{
		bGotArgs  = AssemblerGetArgs( iArg, nArgs, nAddress );
		if (bGotArgs)
		{
			bGotMode = AssemblerUpdateAddressingMode();
			if (bGotMode)
			{
				bGotByte = AssemblerPokeOpcodeAddress( nAddress );
			}
		}
	}

	return true;
}


//===========================================================================
void AssemblerOn  ()
{
	g_bAssemblerInput = true;
	g_sConsolePrompt[0] = g_aConsolePrompt[ PROMPT_ASSEMBLER ];
}

//===========================================================================
void AssemblerOff ()
{
	g_bAssemblerInput = false;
	g_sConsolePrompt[0] = g_aConsolePrompt[ PROMPT_COMMAND ];
}

