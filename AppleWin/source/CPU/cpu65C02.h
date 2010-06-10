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
