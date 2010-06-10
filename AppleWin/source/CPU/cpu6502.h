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

//===========================================================================
