/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 1994-1996, Michael O'Brien
Copyright (C) 1999-2001, Oliver Schmidt
Copyright (C) 2002-2005, Tom Charlesworth
Copyright (C) 2006-2011, Tom Charlesworth, Michael Pohoreski

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

static DWORD Cpu65C02(DWORD uTotalCycles, const bool bVideoUpdate)
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
	WORD base;

	do
	{
		UINT uExtraCycles = 0;
		BYTE iOpcode;

// NTSC_BEGIN
		ULONG uPreviousCycles = uExecutedCycles;
// NTSC_END

		if (GetActiveCpu() == CPU_Z80)
		{
			const UINT uZ80Cycles = z80_mainloop(uTotalCycles, uExecutedCycles); CYC(uZ80Cycles)
		}
		else if (NMI(uExecutedCycles, flagc, flagn, flagv, flagz) || IRQ(uExecutedCycles, flagc, flagn, flagv, flagz))
		{
			// Allow AppleWin debugger's single-stepping to just step the pending IRQ
		}
		else
		{
			HEATMAP_X( regs.pc );
			Fetch(iOpcode, uExecutedCycles);

			switch (iOpcode)
			{
// TODO-MP Optimization Note: ?? Move CYC(#) to array ??
			case 0x00:            BRK  CYC(7)  break;
			case 0x01: idx        ORA  CYC(6)  break;
			case 0x02: IMM        NOP  CYC(2)  break;	// invalid
			case 0x03:            NOP  CYC(1)  break;	// invalid
			case 0x04: ZPG        TSB  CYC(5)  break;
			case 0x05: ZPG        ORA  CYC(3)  break;
			case 0x06: ZPG        ASLc CYC(5)  break;
			case 0x07:            NOP  CYC(1)  break;	// invalid
			case 0x08:            PHP  CYC(3)  break;
			case 0x09: IMM        ORA  CYC(2)  break;
			case 0x0A:            asl  CYC(2)  break;
			case 0x0B:            NOP  CYC(1)  break;	// invalid
			case 0x0C: ABS        TSB  CYC(6)  break;
			case 0x0D: ABS        ORA  CYC(4)  break;
			case 0x0E: ABS        ASLc CYC(6)  break;
			case 0x0F:            NOP  CYC(1)  break;	// invalid
			case 0x10: REL        BPL  CYC(2)  break;
			case 0x11: INDY_OPT   ORA  CYC(5)  break;
			case 0x12: izp        ORA  CYC(5)  break;
			case 0x13:            NOP  CYC(1)  break;	// invalid
			case 0x14: ZPG        TRB  CYC(5)  break;
			case 0x15: zpx        ORA  CYC(4)  break;
			case 0x16: zpx        ASLc CYC(6)  break;
			case 0x17:            NOP  CYC(1)  break;	// invalid
			case 0x18:            CLC  CYC(2)  break;
			case 0x19: ABSY_OPT   ORA  CYC(4)  break;
			case 0x1A:            INA  CYC(2)  break;
			case 0x1B:            NOP  CYC(1)  break;	// invalid
			case 0x1C: ABS        TRB  CYC(6)  break;
			case 0x1D: ABSX_OPT   ORA  CYC(4)  break;
			case 0x1E: ABSX_OPT   ASLc CYC(6)  break;
			case 0x1F:            NOP  CYC(1)  break;	// invalid
			case 0x20: ABS        JSR  CYC(6)  break;
			case 0x21: idx        AND  CYC(6)  break;
			case 0x22: IMM        NOP  CYC(2)  break;	// invalid
			case 0x23:            NOP  CYC(1)  break;	// invalid
			case 0x24: ZPG        BIT  CYC(3)  break;
			case 0x25: ZPG        AND  CYC(3)  break;
			case 0x26: ZPG        ROLc CYC(5)  break;
			case 0x27:            NOP  CYC(1)  break;	// invalid
			case 0x28:            PLP  CYC(4)  break;
			case 0x29: IMM        AND  CYC(2)  break;
			case 0x2A:            rol  CYC(2)  break;
			case 0x2B:            NOP  CYC(1)  break;	// invalid
			case 0x2C: ABS        BIT  CYC(4)  break;
			case 0x2D: ABS        AND  CYC(4)  break;
			case 0x2E: ABS        ROLc CYC(6)  break;
			case 0x2F:            NOP  CYC(1)  break;	// invalid
			case 0x30: REL        BMI  CYC(2)  break;
			case 0x31: INDY_OPT   AND  CYC(5)  break;
			case 0x32: izp        AND  CYC(5)  break;
			case 0x33:            NOP  CYC(1)  break;	// invalid
			case 0x34: zpx        BIT  CYC(4)  break;
			case 0x35: zpx        AND  CYC(4)  break;
			case 0x36: zpx        ROLc CYC(6)  break;
			case 0x37:            NOP  CYC(1)  break;	// invalid
			case 0x38:            SEC  CYC(2)  break;
			case 0x39: ABSY_OPT   AND  CYC(4)  break;
			case 0x3A:            DEA  CYC(2)  break;
			case 0x3B:            NOP  CYC(1)  break;	// invalid
			case 0x3C: ABSX_OPT   BIT  CYC(4)  break;
			case 0x3D: ABSX_OPT   AND  CYC(4)  break;
			case 0x3E: ABSX_OPT   ROLc CYC(6)  break;
			case 0x3F:            NOP  CYC(1)  break;	// invalid
			case 0x40:            RTI  CYC(6)  DoIrqProfiling(uExecutedCycles); break;
			case 0x41: idx        EOR  CYC(6)  break;
			case 0x42: IMM        NOP  CYC(2)  break;	// invalid
			case 0x43:            NOP  CYC(1)  break;	// invalid
			case 0x44: ZPG        NOP  CYC(3)  break;	// invalid
			case 0x45: ZPG        EOR  CYC(3)  break;
			case 0x46: ZPG        LSRc CYC(5)  break;
			case 0x47:            NOP  CYC(1)  break;	// invalid
			case 0x48:            PHA  CYC(3)  break;
			case 0x49: IMM        EOR  CYC(2)  break;
			case 0x4A:            lsr  CYC(2)  break;
			case 0x4B:            NOP  CYC(1)  break;	// invalid
			case 0x4C: ABS        JMP  CYC(3)  break;
			case 0x4D: ABS        EOR  CYC(4)  break;
			case 0x4E: ABS        LSRc CYC(6)  break;
			case 0x4F:            NOP  CYC(1)  break;	// invalid
			case 0x50: REL        BVC  CYC(2)  break;
			case 0x51: INDY_OPT   EOR  CYC(5)  break;
			case 0x52: izp        EOR  CYC(5)  break;
			case 0x53:            NOP  CYC(1)  break;	// invalid
			case 0x54: zpx        NOP  CYC(4)  break;	// invalid
			case 0x55: zpx        EOR  CYC(4)  break;
			case 0x56: zpx        LSRc CYC(6)  break;
			case 0x57:            NOP  CYC(1)  break;	// invalid
			case 0x58:            CLI  CYC(2)  break;
			case 0x59: ABSY_OPT   EOR  CYC(4)  break;
			case 0x5A:            PHY  CYC(3)  break;
			case 0x5B:            NOP  CYC(1)  break;	// invalid
			case 0x5C: ABS        NOP  CYC(8)  break;	// invalid
			case 0x5D: ABSX_OPT   EOR  CYC(4)  break;
			case 0x5E: ABSX_OPT   LSRc CYC(6)  break;
			case 0x5F:            NOP  CYC(1)  break;	// invalid
			case 0x60:            RTS  CYC(6)  break;
			case 0x61: idx        ADCc CYC(6)  break;
			case 0x62: IMM        NOP  CYC(2)  break;	// invalid
			case 0x63:            NOP  CYC(1)  break;	// invalid
			case 0x64: ZPG        STZ  CYC(3)  break;
			case 0x65: ZPG        ADCc CYC(3)  break;
			case 0x66: ZPG        RORc CYC(5)  break;
			case 0x67:            NOP  CYC(1)  break;	// invalid
			case 0x68:            PLA  CYC(4)  break;
			case 0x69: IMM        ADCc CYC(2)  break;
			case 0x6A:            ror  CYC(2)  break;
			case 0x6B:            NOP  CYC(1)  break;	// invalid
			case 0x6C: IABS_CMOS  JMP  CYC(6)  break;
			case 0x6D: ABS        ADCc CYC(4)  break;
			case 0x6E: ABS        RORc CYC(6)  break;
			case 0x6F:            NOP  CYC(1)  break;	// invalid
			case 0x70: REL        BVS  CYC(2)  break;
			case 0x71: INDY_OPT   ADCc CYC(5)  break;
			case 0x72: izp        ADCc CYC(5)  break;
			case 0x73:            NOP  CYC(1)  break;	// invalid
			case 0x74: zpx        STZ  CYC(4)  break;
			case 0x75: zpx        ADCc CYC(4)  break;
			case 0x76: zpx        RORc CYC(6)  break;
			case 0x77:            NOP  CYC(1)  break;	// invalid
			case 0x78:            SEI  CYC(2)  break;
			case 0x79: ABSY_OPT   ADCc CYC(4)  break;
			case 0x7A:            PLY  CYC(4)  break;
			case 0x7B:            NOP  CYC(1)  break;	// invalid
			case 0x7C: IABSX      JMP  CYC(6)  break;
			case 0x7D: ABSX_OPT   ADCc CYC(4)  break;
			case 0x7E: ABSX_OPT   RORc CYC(6)  break;
			case 0x7F:            NOP  CYC(1)  break;	// invalid
			case 0x80: REL        BRA  CYC(2)  break;
			case 0x81: idx        STA  CYC(6)  break;
			case 0x82: IMM        NOP  CYC(2)  break;	// invalid
			case 0x83:            NOP  CYC(1)  break;	// invalid
			case 0x84: ZPG        STY  CYC(3)  break;
			case 0x85: ZPG        STA  CYC(3)  break;
			case 0x86: ZPG        STX  CYC(3)  break;
			case 0x87:            NOP  CYC(1)  break;	// invalid
			case 0x88:            DEY  CYC(2)  break;
			case 0x89: IMM        BITI CYC(2)  break;
			case 0x8A:            TXA  CYC(2)  break;
			case 0x8B:            NOP  CYC(1)  break;	// invalid
			case 0x8C: ABS        STY  CYC(4)  break;
			case 0x8D: ABS        STA  CYC(4)  break;
			case 0x8E: ABS        STX  CYC(4)  break;
			case 0x8F:            NOP  CYC(1)  break;	// invalid
			case 0x90: REL        BCC  CYC(2)  break;
			case 0x91: INDY_CONST STA  CYC(6)  break;
			case 0x92: izp        STA  CYC(5)  break;
			case 0x93:            NOP  CYC(1)  break;	// invalid
			case 0x94: zpx        STY  CYC(4)  break;
			case 0x95: zpx        STA  CYC(4)  break;
			case 0x96: zpy        STX  CYC(4)  break;
			case 0x97:            NOP  CYC(1)  break;	// invalid
			case 0x98:            TYA  CYC(2)  break;
			case 0x99: ABSY_CONST STA  CYC(5)  break;
			case 0x9A:            TXS  CYC(2)  break;
			case 0x9B:            NOP  CYC(1)  break;	// invalid
			case 0x9C: ABS        STZ  CYC(4)  break;
			case 0x9D: ABSX_CONST STA  CYC(5)  break;
			case 0x9E: ABSX_CONST STZ  CYC(5)  break;
			case 0x9F:            NOP  CYC(1)  break;	// invalid
			case 0xA0: IMM        LDY  CYC(2)  break;
			case 0xA1: idx        LDA  CYC(6)  break;
			case 0xA2: IMM        LDX  CYC(2)  break;
			case 0xA3:            NOP  CYC(1)  break;	// invalid
			case 0xA4: ZPG        LDY  CYC(3)  break;
			case 0xA5: ZPG        LDA  CYC(3)  break;
			case 0xA6: ZPG        LDX  CYC(3)  break;
			case 0xA7:            NOP  CYC(1)  break;	// invalid
			case 0xA8:            TAY  CYC(2)  break;
			case 0xA9: IMM        LDA  CYC(2)  break;
			case 0xAA:            TAX  CYC(2)  break;
			case 0xAB:            NOP  CYC(1)  break;	// invalid
			case 0xAC: ABS        LDY  CYC(4)  break;
			case 0xAD: ABS        LDA  CYC(4)  break;
			case 0xAE: ABS        LDX  CYC(4)  break;
			case 0xAF:            NOP  CYC(1)  break;	// invalid
			case 0xB0: REL        BCS  CYC(2)  break;
			case 0xB1: INDY_OPT   LDA  CYC(5)  break;
			case 0xB2: izp        LDA  CYC(5)  break;
			case 0xB3:            NOP  CYC(1)  break;	// invalid
			case 0xB4: zpx        LDY  CYC(4)  break;
			case 0xB5: zpx        LDA  CYC(4)  break;
			case 0xB6: zpy        LDX  CYC(4)  break;
			case 0xB7:            NOP  CYC(1)  break;	// invalid
			case 0xB8:            CLV  CYC(2)  break;
			case 0xB9: ABSY_OPT   LDA  CYC(4)  break;
			case 0xBA:            TSX  CYC(2)  break;
			case 0xBB:            NOP  CYC(1)  break;	// invalid
			case 0xBC: ABSX_OPT   LDY  CYC(4)  break;
			case 0xBD: ABSX_OPT   LDA  CYC(4)  break;
			case 0xBE: ABSY_OPT   LDX  CYC(4)  break;
			case 0xBF:            NOP  CYC(1)  break;	// invalid
			case 0xC0: IMM        CPY  CYC(2)  break;
			case 0xC1: idx        CMP  CYC(6)  break;
			case 0xC2: IMM        NOP  CYC(2)  break;	// invalid
			case 0xC3:            NOP  CYC(1)  break;	// invalid
			case 0xC4: ZPG        CPY  CYC(3)  break;
			case 0xC5: ZPG        CMP  CYC(3)  break;
			case 0xC6: ZPG        DEC  CYC(5)  break;
			case 0xC7:            NOP  CYC(1)  break;	// invalid
			case 0xC8:            INY  CYC(2)  break;
			case 0xC9: IMM        CMP  CYC(2)  break;
			case 0xCA:            DEX  CYC(2)  break;
			case 0xCB:            NOP  CYC(1)  break;	// invalid
			case 0xCC: ABS        CPY  CYC(4)  break;
			case 0xCD: ABS        CMP  CYC(4)  break;
			case 0xCE: ABS        DEC  CYC(6)  break;
			case 0xCF:            NOP  CYC(1)  break;	// invalid
			case 0xD0: REL        BNE  CYC(2)  break;
			case 0xD1: INDY_OPT   CMP  CYC(5)  break;
			case 0xD2: izp        CMP  CYC(5)  break;
			case 0xD3:            NOP  CYC(1)  break;	// invalid
			case 0xD4: zpx        NOP  CYC(4)  break;	// invalid
			case 0xD5: zpx        CMP  CYC(4)  break;
			case 0xD6: zpx        DEC  CYC(6)  break;
			case 0xD7:            NOP  CYC(1)  break;	// invalid
			case 0xD8:            CLD  CYC(2)  break;
			case 0xD9: ABSY_OPT   CMP  CYC(4)  break;
			case 0xDA:            PHX  CYC(3)  break;
			case 0xDB:            NOP  CYC(1)  break;	// invalid
			case 0xDC: ABS        LDD  CYC(4)  break;	// invalid
			case 0xDD: ABSX_OPT   CMP  CYC(4)  break;
			case 0xDE: ABSX_CONST DEC  CYC(7)  break;
			case 0xDF:            NOP  CYC(1)  break;	// invalid
			case 0xE0: IMM        CPX  CYC(2)  break;
			case 0xE1: idx        SBCc CYC(6)  break;
			case 0xE2: IMM        NOP  CYC(2)  break;	// invalid
			case 0xE3:            NOP  CYC(1)  break;	// invalid
			case 0xE4: ZPG        CPX  CYC(3)  break;
			case 0xE5: ZPG        SBCc CYC(3)  break;
			case 0xE6: ZPG        INC  CYC(5)  break;
			case 0xE7:            NOP  CYC(1)  break;	// invalid
			case 0xE8:            INX  CYC(2)  break;
			case 0xE9: IMM        SBCc CYC(2)  break;
			case 0xEA:            NOP  CYC(2)  break;
			case 0xEB:            NOP  CYC(1)  break;	// invalid
			case 0xEC: ABS        CPX  CYC(4)  break;
			case 0xED: ABS        SBCc CYC(4)  break;
			case 0xEE: ABS        INC  CYC(6)  break;
			case 0xEF:            NOP  CYC(1)  break;	// invalid
			case 0xF0: REL        BEQ  CYC(2)  break;
			case 0xF1: INDY_OPT   SBCc CYC(5)  break;
			case 0xF2: izp        SBCc CYC(5)  break;
			case 0xF3:            NOP  CYC(1)  break;	// invalid
			case 0xF4: zpx        NOP  CYC(4)  break;	// invalid
			case 0xF5: zpx        SBCc CYC(4)  break;
			case 0xF6: zpx        INC  CYC(6)  break;
			case 0xF7:            NOP  CYC(1)  break;	// invalid
			case 0xF8:            SED  CYC(2)  break;
			case 0xF9: ABSY_OPT   SBCc CYC(4)  break;
			case 0xFA:            PLX  CYC(4)  break;
			case 0xFB:            NOP  CYC(1)  break;	// invalid
			case 0xFC: ABS        LDD  CYC(4)  break;	// invalid
			case 0xFD: ABSX_OPT   SBCc CYC(4)  break;
			case 0xFE: ABSX_CONST INC  CYC(7)  break;
			case 0xFF:            NOP  CYC(1)  break;	// invalid
			}
		}

		CheckSynchronousInterruptSources(uExecutedCycles - uPreviousCycles, uExecutedCycles);

// NTSC_BEGIN
		if ( bVideoUpdate )
		{
			ULONG uElapsedCycles = uExecutedCycles - uPreviousCycles;
			NTSC_VideoUpdateCycles( uElapsedCycles );
		}
// NTSC_END

	} while (uExecutedCycles < uTotalCycles);

	EF_TO_AF // Emulator Flags to Apple Flags

	return uExecutedCycles;
}

//===========================================================================
