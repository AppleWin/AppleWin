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

#define $ INV // INV = Invalid -> Debugger Break
		switch (iOpcode)
		{	
		case 0x00:       BRK	CYC(7)  break;
		case 0x01:   idx ORA	CYC(6)  break;
		case 0x02: $     HLT	CYC(2)  break;
		case 0x03: $ idx ASO	CYC(8)  break;
		case 0x04: $ ZPG NOP	CYC(3)  break;
		case 0x05:   ZPG ORA	CYC(3)  break;
		case 0x06:   ZPG ASLn	CYC(5)  break;
		case 0x07: $ ZPG ASO	CYC(5)  break; // invalid
		case 0x08:       PHP	CYC(3)  break;
		case 0x09:   IMM ORA	CYC(2)  break;
		case 0x0A:       asl	CYC(2)  break;
		case 0x0B: $ IMM ANC	CYC(2)  break; // invald
		case 0x0C: $ abx NOP	CYC(4)  break;
		case 0x0D:   ABS ORA	CYC(4)  break;
		case 0x0E:   ABS ASLn	CYC(6)  break;
		case 0x0F: $ ABS ASO	CYC(6)  break; // invalid
		case 0x10:   REL BPL	CYC(2)  break;
		case 0x11:   idy ORA	CYC(5)  break;
		case 0x12: $     HLT	CYC(2)  break;
		case 0x13: $ idy ASO	CYC(8)  break; // invalid
		case 0x14: $ zpx NOP	CYC(4)  break;
		case 0x15:   zpx ORA	CYC(4)  break;
		case 0x16:   zpx ASLn	CYC(6)  break;
		case 0x17: $ zpx ASO	CYC(6)  break; // invalid
		case 0x18:       CLC	CYC(2)  break;
		case 0x19:   aby ORA	CYC(4)  break;
		case 0x1A: $     NOP	CYC(2)  break;
		case 0x1B: $ aby ASO	CYC(7)  break; // invalid
		case 0x1C: $ abx NOP	CYC(4)  break;
		case 0x1D:   abx ORA	CYC(4)  break;
		case 0x1E:   abx ASLn	CYC(6)  break;
		case 0x1F: $ abx ASO	CYC(7)  break; // invalid
		case 0x20:   ABS JSR	CYC(6)  break;
		case 0x21:   idx AND	CYC(6)  break;
		case 0x22: $     HLT	CYC(2)  break;
		case 0x23: $ idx RLA	CYC(8)  break;
		case 0x24:   ZPG BIT	CYC(3)  break;
		case 0x25:   ZPG AND	CYC(3)  break;
		case 0x26:   ZPG ROLn	CYC(5)  break;
		case 0x27: $ ZPG RLA	CYC(5)  break;
		case 0x28:       PLP	CYC(4)  break;
		case 0x29:   IMM AND	CYC(2)  break;
		case 0x2A:       rol	CYC(2)  break;
		case 0x2B: $ IMM ANC	CYC(2)  break; // invalid
		case 0x2C:   ABS BIT	CYC(4)  break;
		case 0x2D:   ABS AND	CYC(2)  break;
		case 0x2E:   ABS ROLn	CYC(6)  break;
		case 0x2F: $ ABS RLA	CYC(6)  break;
		case 0x30:   REL BMI	CYC(2)  break;
		case 0x31:   idy AND	CYC(5)  break;
		case 0x32: $     HLT	CYC(2)  break;
		case 0x33: $ idy RLA	CYC(8)  break; // invalid
		case 0x34: $ zpx NOP	CYC(4)  break;
		case 0x35:   zpx AND	CYC(4)  break;
		case 0x36:   zpx ROLn	CYC(6)  break;
		case 0x37: $ zpx RLA	CYC(6)  break;
		case 0x38:       SEC	CYC(2)  break;
		case 0x39:   aby AND	CYC(4)  break;
		case 0x3A: $     NOP	CYC(2)  break;
		case 0x3B: $ aby RLA	CYC(7)  break; // invalid
		case 0x3C: $ abx NOP	CYC(4)  break;
		case 0x3D:   abx AND	CYC(4)  break;
		case 0x3E:   abx ROLn	CYC(6)  break;
		case 0x3F: $ abx RLA	CYC(7)  break; // invalid
		case 0x40:       RTI	CYC(6)  DoIrqProfiling(uExecutedCycles); break;
		case 0x41:   idx EOR	CYC(6)  break;
		case 0x42: $     HLT	CYC(2)  break;
		case 0x43: $ idx LSE	CYC(8)  break; // invalid
		case 0x44: $ ZPG NOP	CYC(3)  break;
		case 0x45:   ZPG EOR	CYC(3)  break;
		case 0x46:   ZPG LSRn	CYC(5)  break;
		case 0x47: $ ZPG LSE	CYC(5)  break; // invalid
		case 0x48:       PHA	CYC(3)  break;
		case 0x49:   IMM EOR	CYC(2)  break;
		case 0x4A:       lsr	CYC(2)  break;
		case 0x4B: $ IMM ALR	CYC(2)  break; // invalid
		case 0x4C:   ABS JMP	CYC(3)  break;
		case 0x4D:   ABS EOR	CYC(4)  break;
		case 0x4E:   ABS LSRn	CYC(6)  break;
		case 0x4F: $ ABS LSE	CYC(6)  break;
		case 0x50:   REL BVC	CYC(2)  break;
		case 0x51:   idy EOR	CYC(5)  break;
		case 0x52: $     HLT	CYC(2)  break;
		case 0x53: $ idy LSE	CYC(8)  break;
		case 0x54: $ zpx NOP	CYC(4)  break;
		case 0x55:   zpx EOR	CYC(4)  break;
		case 0x56:   zpx LSRn	CYC(6)  break;
		case 0x57: $ zpx LSE	CYC(6)  break; // invalid
		case 0x58:       CLI	CYC(2)  break;
		case 0x59:   aby EOR	CYC(4)  break;
		case 0x5A: $     NOP	CYC(2)  break;
		case 0x5B: $ aby LSE	CYC(7)  break; // invalid
		case 0x5C: $ abx NOP	CYC(4)  break;
		case 0x5D:   abx EOR	CYC(4)  break;
		case 0x5E:   abx LSRn	CYC(6)  break;
		case 0x5F: $ abx LSE	CYC(7)  break;
		case 0x60:       RTS	CYC(6)  break;
		case 0x61:   idx ADCn	CYC(6)  break;
		case 0x62: $     HLT	CYC(2)  break;
		case 0x63: $ idx RRA	CYC(8)  break; // invalid
		case 0x64: $ ZPG NOP	CYC(3)  break;
		case 0x65:   ZPG ADCn	CYC(3)  break;
		case 0x66:   ZPG RORn	CYC(5)  break;
		case 0x67: $ ZPG RRA	CYC(5)  break;
		case 0x68:       PLA	CYC(4)  break;
		case 0x69:   IMM ADCn	CYC(2)  break;
		case 0x6A:       ror	CYC(2)  break;
		case 0x6B: $ IMM ARR	CYC(2)  break; // invalid
		case 0x6C:   IABSNMOS JMP  CYC(6)  break;
		case 0x6D:   ABS ADCn	CYC(4)  break;
		case 0x6E:   ABS RORn	CYC(6)  break;
		case 0x6F: $ ABS RRA	CYC(6)  break; // invalid
		case 0x70:   REL BVS	CYC(2)  break;
		case 0x71:   idy ADCn	CYC(5)  break;
		case 0x72: $     HLT	CYC(2)  break;
		case 0x73: $ idy RRA	CYC(8)  break; // invalid
		case 0x74: $ zpx NOP	CYC(4)  break;
		case 0x75:   zpx ADCn	CYC(4)  break;
		case 0x76:   zpx RORn	CYC(6)  break;
		case 0x77: $ zpx RRA	CYC(6)  break; // invalid
		case 0x78:       SEI	CYC(2)  break;
		case 0x79:   aby ADCn	CYC(4)  break;
		case 0x7A: $     NOP	CYC(2)  break;
		case 0x7B: $ aby RRA	CYC(7)  break; // invalid
		case 0x7C: $ abx NOP	CYC(4)  break;
		case 0x7D:   abx ADCn	CYC(4)  break;
		case 0x7E:   abx RORn	CYC(6)  break;
		case 0x7F: $ abx RRA	CYC(7)  break; // invalid
		case 0x80: $ IMM NOP	CYC(2)  break;
		case 0x81:   idx STA	CYC(6)  break;
		case 0x82: $ IMM NOP	CYC(2)  break;
		case 0x83: $ idx AXS	CYC(6)  break; // invalid
		case 0x84:   ZPG STY	CYC(3)  break;
		case 0x85:   ZPG STA	CYC(3)  break;
		case 0x86:   ZPG STX	CYC(3)  break;
		case 0x87: $ ZPG AXS	CYC(3)  break; // invalid
		case 0x88:       DEY	CYC(2)  break;
		case 0x89: $ IMM NOP	CYC(2)  break;
		case 0x8A:       TXA	CYC(2)  break;
		case 0x8B: $ IMM XAA	CYC(2)  break; // invalid
		case 0x8C:   ABS STY	CYC(4)  break;
		case 0x8D:   ABS STA	CYC(4)  break;
		case 0x8E:   ABS STX	CYC(4)  break;
		case 0x8F: $ ABS AXS	CYC(4)  break; // invalid
		case 0x90:   REL BCC	CYC(2)  break;
		case 0x91:   idy STA	CYC(6)  break;
		case 0x92: $     HLT	CYC(2)  break;
		case 0x93: $ idy AXA	CYC(6)  break; // invalid
		case 0x94:   zpx STY	CYC(4)  break;
		case 0x95:   zpx STA	CYC(4)  break;
		case 0x96:   zpy STX	CYC(4)  break;
		case 0x97: $ zpy AXS	CYC(4)  break; // invalid
		case 0x98:       TYA	CYC(2)  break;
		case 0x99:   aby STA	CYC(5)  break;
		case 0x9A:       TXS	CYC(2)  break;
		case 0x9B: $ aby TAS	CYC(5)  break; // invalid
		case 0x9C: $ abx SAY	CYC(5)  break; // invalid
		case 0x9D:   abx STA	CYC(5)  break;
		case 0x9E: $ aby XAS	CYC(5)  break;
		case 0x9F: $ aby AXA	CYC(5)  break;
		case 0xA0:   IMM LDY	CYC(2)  break;
		case 0xA1:   idx LDA	CYC(6)  break;
		case 0xA2:   IMM LDX	CYC(2)  break;
		case 0xA3: $ idx LAX	CYC(6)  break; // invalid
		case 0xA4:   ZPG LDY	CYC(3)  break;
		case 0xA5:   ZPG LDA	CYC(3)  break;
		case 0xA6:   ZPG LDX	CYC(3)  break;
		case 0xA7: $ ZPG LAX	CYC(3)  break; // invalid
		case 0xA8:       TAY	CYC(2)  break;
		case 0xA9:   IMM LDA	CYC(2)  break;
		case 0xAA:       TAX	CYC(2)  break;
		case 0xAB: $ IMM OAL	CYC(2)  break; // invalid
		case 0xAC:   ABS LDY	CYC(4)  break;
		case 0xAD:   ABS LDA	CYC(4)  break;
		case 0xAE:   ABS LDX	CYC(4)  break;
		case 0xAF: $ ABS LAX	CYC(4)  break; // invalid
		case 0xB0:   REL BCS	CYC(2)  break;
		case 0xB1:   idy LDA	CYC(5)  break;
		case 0xB2: $     HLT	CYC(2)  break;
		case 0xB3: $ idy LAX	CYC(5)  break;
		case 0xB4:   zpx LDY	CYC(4)  break;
		case 0xB5:   zpx LDA	CYC(4)  break;
		case 0xB6:   zpy LDX	CYC(4)  break;
		case 0xB7: $ zpy LAX	CYC(4)  break; // invalid
		case 0xB8:       CLV	CYC(2)  break;
		case 0xB9:   aby LDA	CYC(4)  break;
		case 0xBA:       TSX	CYC(2)  break;
		case 0xBB: $ aby LAS	CYC(4)  break; // invalid
		case 0xBC:   abx LDY	CYC(4)  break;
		case 0xBD:   abx LDA	CYC(4)  break;
		case 0xBE:   aby LDX	CYC(4)  break;
		case 0xBF: $ aby LAX	CYC(4)  break; // invalid
		case 0xC0:   IMM CPY	CYC(2)  break;
		case 0xC1:   idx CMP	CYC(6)  break;
		case 0xC2: $ IMM NOP	CYC(2)  break;
		case 0xC3: $ idx DCM	CYC(8)  break; // invalid
		case 0xC4:   ZPG CPY	CYC(3)  break;
		case 0xC5:   ZPG CMP	CYC(3)  break;
		case 0xC6:   ZPG DECn	CYC(5)  break;
		case 0xC7: $ ZPG DCM	CYC(5)  break; // invalid
		case 0xC8:       INY	CYC(2)  break;
		case 0xC9:   IMM CMP	CYC(2)  break;
		case 0xCA:       DEX	CYC(2)  break;
		case 0xCB: $ IMM SAX	CYC(2)  break; // invalid
		case 0xCC:   ABS CPY	CYC(4)  break;
		case 0xCD:   ABS CMP	CYC(4)  break;
		case 0xCE:   ABS DECn	CYC(5)  break;
		case 0xCF: $ ABS DCM	CYC(6)  break; // invalid
		case 0xD0:   REL BNE	CYC(2)  break;
		case 0xD1:   idy CMP	CYC(5)  break;
		case 0xD2: $     HLT	CYC(2)  break;
		case 0xD3: $ idy DCM	CYC(8)  break; // invalid
		case 0xD4: $ zpx NOP	CYC(4)  break;
		case 0xD5:   zpx CMP	CYC(4)  break;
		case 0xD6:   zpx DECn	CYC(6)  break;
		case 0xD7: $ zpx DCM	CYC(6)  break; // invalid
		case 0xD8:       CLD	CYC(2)  break;
		case 0xD9:   aby CMP	CYC(4)  break;
		case 0xDA: $     NOP	CYC(2)  break;
		case 0xDB: $ aby DCM	CYC(7)  break; // invalid
		case 0xDC: $ abx NOP	CYC(4)  break;
		case 0xDD:   abx CMP	CYC(4)  break;
		case 0xDE:   abx DECn	CYC(6)  break;
		case 0xDF: $ abx DCM	CYC(7)  break; // invalid
		case 0xE0:   IMM CPX	CYC(2)  break;
		case 0xE1:   idx SBCn	CYC(6)  break;
		case 0xE2: $ IMM NOP	CYC(2)  break;
		case 0xE3: $ idx INS	CYC(8)  break; // invalid
		case 0xE4:   ZPG CPX	CYC(3)  break;
		case 0xE5:   ZPG SBCn	CYC(3)  break;
		case 0xE6:   ZPG INCn	CYC(5)  break;
		case 0xE7: $ ZPG INS	CYC(5)  break; // invalid
		case 0xE8:       INX	CYC(2)  break;
		case 0xE9:   IMM SBCn	CYC(2)  break;
		case 0xEA:       NOP	CYC(2)  break;
		case 0xEB: $ IMM SBCn	CYC(2)  break;
		case 0xEC:   ABS CPX	CYC(4)  break;
		case 0xED:   ABS SBCn	CYC(4)  break;
		case 0xEE:   ABS INCn	CYC(6)  break;
		case 0xEF: $ ABS INS	CYC(6)  break; // invalid
		case 0xF0:   REL BEQ	CYC(2)  break;
		case 0xF1:   idy SBCn	CYC(5)  break;
		case 0xF2: $     HLT	CYC(2)  break;
		case 0xF3: $ idy INS	CYC(8)  break; // invalid
		case 0xF4: $ zpx NOP	CYC(4)  break;
		case 0xF5:   zpx SBCn	CYC(4)  break;
		case 0xF6:   zpx INCn	CYC(6)  break;
		case 0xF7: $ zpx INS	CYC(6)  break; // invalid
		case 0xF8:       SED	CYC(2)  break;
		case 0xF9:   aby SBCn	CYC(4)  break;
		case 0xFA: $     NOP	CYC(2)  break;
		case 0xFB: $ aby INS	CYC(7)  break;
		case 0xFC: $ abx NOP	CYC(4)  break;
		case 0xFD:   abx SBCn	CYC(4)  break;
		case 0xFE:   abx INCn	CYC(6)  break;
		case 0xFF: $ abx INS	CYC(7)  break;
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
