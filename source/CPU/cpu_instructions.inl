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

/****************************************************************************
*
*  INSTRUCTION MACROS
*
***/

#undef ADC_NMOS
#undef ADC_CMOS
#undef ALR
#undef AND
#undef ANC
#undef ARR
#undef ASL_NMOS
#undef ASL_CMOS
#undef ASLA
#undef ASO
#undef AXA
#undef AXS
#undef BCC
#undef BCS
#undef BEQ
#undef BIT
#undef BITI
#undef BMI
#undef BNE
#undef BPL
#undef BRA
#undef BRK
#undef BVC
#undef BVS
#undef CLC
#undef CLD
#undef CLI
#undef CLV
#undef CMP
#undef CPX
#undef CPY
#undef DCM
#undef DEA
#undef DEC
#undef DEX
#undef DEY
#undef EOR
#undef HLT
#undef INA
#undef INC
#undef INS
#undef INX
#undef INY
#undef JMP
#undef JSR
#undef LAS
#undef LAX
#undef LDA
#undef LDX
#undef LDY
#undef LSE
#undef LSR_NMOS
#undef LSR_CMOS
#undef LSRA
#undef NOP
#undef OAL
#undef ORA
#undef PHA
#undef PHP
#undef PHX
#undef PHY
#undef PLA
#undef PLP
#undef PLX
#undef PLY
#undef RLA
#undef ROL_NMOS
#undef ROL_CMOS
#undef ROLA	
#undef ROR_NMOS
#undef ROR_CMOS
#undef RORA
#undef RRA
#undef RTI
#undef RTS
#undef SAX
#undef SAY
#undef SBC_NMOS
#undef SBC_CMOS
#undef SEC
#undef SED
#undef SEI
#undef STA
#undef STX
#undef STY
#undef STZ
#undef TAS
#undef TAX
#undef TAY
#undef TRB
#undef TSB
#undef TSX
#undef TXA
#undef TXS
#undef TYA
#undef XAA
#undef XAS

// ==========

#undef ADCn
#undef ASLn
#undef LSRn
#undef ROLn
#undef RORn
#undef SBCn

#define ADCn ADC_NMOS
#define ASLn ASL_NMOS
#define LSRn LSR_NMOS
#define ROLn ROL_NMOS
#define RORn ROR_NMOS
#define SBCn SBC_NMOS

// ==========

#undef ADCc
#undef ASLc
#undef LSRc
#undef ROLc
#undef RORc
#undef SBCc

#define ADCc ADC_CMOS
#define ASLc ASL_CMOS
#define LSRc LSR_CMOS
#define ROLc ROL_CMOS
#define RORc ROR_CMOS
#define SBCc SBC_CMOS

// ==========

#define ADC_NMOS /*bSlowerOnPagecross = 1;*/						    \
		 temp = READ;						    \
		 if (regs.ps & AF_DECIMAL) {				    \
		   val	  = (regs.a & 0x0F) + (temp & 0x0F) + flagc;	    \
		   if (val > 0x09)					    \
		     val += 0x06;					    \
		   if (val <= 0x0F)					    \
		     val = (val & 0x0F) + (regs.a & 0xF0) + (temp & 0xF0);  \
		   else							    \
		     val = (val & 0x0F) + (regs.a & 0xF0) + (temp & 0xF0) + 0x10;\
		   flagz = !((regs.a + temp + flagc) & 0xFF);		    \
		   flagn = (val & 0x80);				    \
		   flagv = ((regs.a ^ val) & 0x80) && !((regs.a ^ temp) & 0x80);\
		   if ((val & 0x1F0) > 0x90)				    \
		     val += 0x60;					    \
		   flagc = ((val & 0xFF0) > 0xF0);			    \
		   regs.a = val & 0xFF;                                     \
		  }							    \
		 else {							    \
		   val	  = regs.a + temp + flagc;			    \
		   flagc  = (val > 0xFF);				    \
		   flagv  = (((regs.a & 0x80) == (temp & 0x80)) &&	    \
			     ((regs.a & 0x80) != (val & 0x80)));	    \
		   regs.a = val & 0xFF;					    \
		   SETNZ(regs.a);					    \
		 }
#define ADC_CMOS /*bSlowerOnPagecross = 1*/;						    \
                 temp = READ;						    \
                 flagv = !((regs.a ^ temp) & 0x80);			    \
		 if (regs.ps & AF_DECIMAL) {				    \
		    uExtraCycles++;					    \
		    val = (regs.a & 0x0f) + (temp & 0x0f) + flagc;          \
		    if (val >= 0x0A)					    \
		       val = 0x10 | ((val + 6) & 0x0f);			    \
		    val += (regs.a & 0xf0) + (temp & 0xf0);		    \
		    if (val >= 0xA0) {					    \
		       flagc = 1;					    \
		       if (val >= 0x180)				    \
			  flagv = 0;					    \
		       val += 0x60;					    \
		    }							    \
		    else {						    \
		       flagc = 0;					    \
		       if (val < 0x80)					    \
		          flagv = 0;					    \
		    }							    \
		 }							    \
		 else {							    \
		    val = regs.a + temp + flagc;                            \
		    if (val >= 0x100) {					    \
		       flagc = 1;					    \
		       if (val >= 0x180) flagv = 0;			    \
		    }							    \
		    else {						    \
		       flagc = 0;					    \
		       if (val < 0x80) flagv = 0;			    \
		    }							    \
		 }							    \
		 regs.a = val & 0xFF;					    \
		 SETNZ(regs.a)
#define ALR	 regs.a &= READ;					    \
		 flagc = (regs.a & 1);					    \
		 flagn = 0;						    \
		 regs.a >>= 1;						    \
		 SETZ(regs.a)
#define AND	 /*bSlowerOnPagecross = 1;*/						    \
		 regs.a &= READ;					    \
		 SETNZ(regs.a)
#define ANC	 regs.a &= READ;					    \
		 SETNZ(regs.a)						    \
		 flagc = !!flagn;
#define ARR	 temp = regs.a & READ; /* Yes, this is sick */		    \
		 if (regs.ps & AF_DECIMAL) {				    \
		   val = temp;						    \
		   val |= (flagc ? 0x100 : 0);				    \
		   val >>= 1;						    \
		   flagn = (flagc ? 0x80 : 0);					    \
		   SETZ(val)						    \
		   flagv = ((val ^ temp) & 0x40);			    \
		   if (((val & 0x0F) + (val & 0x01)) > 0x05)                \
		     val = (val & 0xF0) | ((val + 0x06) & 0x0F);	    \
		   if (((val & 0xF0) + (val & 0x10)) > 0x50) {		    \
		     val = (val & 0x0F) | ((val + 0x60) & 0xF0);	    \
		     flagc = 1;						    \
		   }							    \
		   else							    \
		     flagc = 0;						    \
		   regs.a = (val & 0xFF);				    \
		 }							    \
		 else {							    \
		   val = temp | (flagc ? 0x100 : 0);			    \
		   val >>= 1;						    \
		   SETNZ(val)						    \
		   flagc = !!(val & 0x40);				    \
		   flagv = ((val & 0x40) ^ ((val & 0x20) << 1));	    \
		   regs.a = (val & 0xFF);				    \
		 }
#define ASL_NMOS /*bSlowerOnPagecross = 0;*/						    \
		 val   = READ << 1;					    \
		 flagc = (val > 0xFF);					    \
		 SETNZ(val)						    \
		 WRITE(val)
#define ASL_CMOS /*bSlowerOnPagecross = 1*/;						    \
		 val   = READ << 1;					    \
		 flagc = (val > 0xFF);					    \
		 SETNZ(val)						    \
		 WRITE(val)
#define ASLA	 val   = regs.a << 1;					    \
		 flagc = (val > 0xFF);					    \
		 SETNZ(val)						    \
		 regs.a = (BYTE)val;
#define ASO	 /*bSlowerOnPagecross = 0;*/						    \
		 val   = READ << 1;					    \
		 flagc = (val > 0xFF);					    \
		 WRITE(val)						    \
		 regs.a |= val;						    \
		 SETNZ(regs.a)
#define AXA	 /*bSlowerOnPagecross = 0;*/	    \
		 val = regs.a & regs.x & (((base >> 8) + 1) & 0xFF);	    \
		 ON_PAGECROSS_REPLACE_HI_ADDR								\
		 WRITE(val)
#define AXS	 /*bSlowerOnPagecross = 0;*/						    \
		 WRITE(regs.a & regs.x)
#define BCC	 if (!flagc) BRANCH_TAKEN;
#define BCS	 if ( flagc) BRANCH_TAKEN;
#define BEQ	 if ( flagz) BRANCH_TAKEN;
#define BIT	 /*bSlowerOnPagecross = 1;*/						    \
		 val   = READ;						    \
		 flagz = !(regs.a & val);				    \
		 flagn = val & 0x80;					    \
		 flagv = val & 0x40;
#define BITI	 flagz = !(regs.a & READ);
#define BMI	 if ( flagn) BRANCH_TAKEN;
#define BNE	 if (!flagz) BRANCH_TAKEN;
#define BPL	 if (!flagn) BRANCH_TAKEN;
#define BRA	 BRANCH_TAKEN;
#define BRK	 regs.pc++;						    \
		 PUSH(regs.pc >> 8)					    \
		 PUSH(regs.pc & 0xFF)					    \
		 EF_TO_AF						    \
		 PUSH(regs.ps);						    \
		 regs.ps |= AF_INTERRUPT;				    \
		 regs.pc = *(LPWORD)(mem+0xFFFE);
#define BVC	 if (!flagv) BRANCH_TAKEN;
#define BVS	 if ( flagv) BRANCH_TAKEN;
#define CLC	 flagc = 0;
#define CLD	 regs.ps &= ~AF_DECIMAL;
#define CLI	 regs.ps &= ~AF_INTERRUPT;
#define CLV	 flagv = 0;
#define CMP	 /*bSlowerOnPagecross = 1;*/						    \
		 val   = READ;						    \
		 flagc = (regs.a >= val);				    \
		 val   = regs.a-val;					    \
		 SETNZ(val)
#define CPX	 val   = READ;						    \
		 flagc = (regs.x >= val);				    \
		 val   = regs.x-val;					    \
		 SETNZ(val)
#define CPY	 val   = READ;						    \
		 flagc = (regs.y >= val);				    \
		 val   = regs.y-val;					    \
		 SETNZ(val)
#define DCM	 /*bSlowerOnPagecross = 0;*/						    \
		 val = READ-1;						    \
		 WRITE(val)						    \
		 flagc = (regs.a >= val);				    \
		 val   = regs.a-val;					    \
		 SETNZ(val)
#define DEA	 --regs.a;						    \
		 SETNZ(regs.a)
#define DEC /*bSlowerOnPagecross = 0;*/						    \
		 val = READ-1;						    \
		 SETNZ(val)						    \
		 WRITE(val)
#define DEX	 --regs.x;						    \
		 SETNZ(regs.x)
#define DEY	 --regs.y;						    \
		 SETNZ(regs.y)
#define EOR	 /*bSlowerOnPagecross = 1;*/						    \
		 regs.a ^= READ;					    \
		 SETNZ(regs.a)
#define HLT	 regs.bJammed = 1;					    \
		 --regs.pc;
#define INA	 ++regs.a;						    \
		 SETNZ(regs.a)
#define INC /*bSlowerOnPagecross = 0;*/						    \
		 val = READ+1;						    \
		 SETNZ(val)						    \
		 WRITE(val)
#define INS	 /*bSlowerOnPagecross = 0;*/						    \
		 val = READ+1;						    \
		 WRITE(val)						    \
		 temp = val;                                                \
		 temp2 = regs.a - temp - !flagc;			    \
		 if (regs.ps & AF_DECIMAL) {				    \
		   val  = (regs.a & 0x0F) - (temp & 0x0F) - !flagc;	    \
		   if (val & 0x10)					    \
		     val = ((val - 0x06) & 0x0F) | ((regs.a & 0xF0) - (temp & 0xF0) - 0x10);\
		   else							    \
		     val = (val & 0x0F) | ((regs.a & 0xF0) - (temp & 0xF0));\
		   if (val & 0x100)					    \
		     val -= 0x60;					    \
		   flagc  = (temp2 < 0x100);				    \
		   SETNZ(temp2 & 0xFF);					    \
		   flagv = ((regs.a ^ temp2) & 0x80) && ((regs.a ^ temp) & 0x80);\
		   regs.a = val & 0xFF;					    \
		 }							    \
		 else {							    \
		   val	  = temp2;					    \
		   flagc  = (val < 0x100);				    \
		   flagv  = (((regs.a & 0x80) != (temp & 0x80)) &&	    \
			     ((regs.a & 0x80) != (val & 0x80)));	    \
		   regs.a = val & 0xFF;					    \
		   SETNZ(regs.a);					    \
		 }		
#define INX	 ++regs.x;						    \
		 SETNZ(regs.x)
#define INY	 ++regs.y;						    \
		 SETNZ(regs.y)
#define JMP	 regs.pc = addr;
#define JSR	 --regs.pc;						    \
		 PUSH(regs.pc >> 8)					    \
		 PUSH(regs.pc & 0xFF)					    \
		 regs.pc = addr;
#define LAS	 /*bSlowerOnPagecross = 1*/;						    \
		 val = (BYTE)(READ & regs.sp);				    \
		 regs.a = regs.x = (BYTE) val;				    \
		 regs.sp = val | 0x100;					    \
		 SETNZ(val)
#define LAX	 /*bSlowerOnPagecross = 1;*/						    \
		 regs.a = regs.x = READ;				    \
		 SETNZ(regs.a)
#define LDA	 /*bSlowerOnPagecross = 1;*/						    \
		 regs.a = READ;						    \
		 SETNZ(regs.a)
#define LDD	 /*Undocumented 65C02: LoaD and Discard*/		\
		 READ;
#define LDX	 /*bSlowerOnPagecross = 1;*/						    \
		 regs.x = READ;						    \
		 SETNZ(regs.x)
#define LDY	 /*bSlowerOnPagecross = 1;*/						    \
		 regs.y = READ;						    \
		 SETNZ(regs.y)
#define LSE	 /*bSlowerOnPagecross = 0;*/						    \
		 val   = READ;						    \
		 flagc = (val & 1);					    \
		 val >>= 1;						    \
		 WRITE(val)						    \
		 regs.a ^= val;						    \
		 SETNZ(regs.a)
#define LSR_NMOS /*bSlowerOnPagecross = 0;*/						    \
		 val   = READ;						    \
		 flagc = (val & 1);					    \
		 flagn = 0;						    \
		 val >>= 1;						    \
		 SETZ(val)						    \
		 WRITE(val)
#define LSR_CMOS /*bSlowerOnPagecross = 1;*/						    \
		 val   = READ;						    \
		 flagc = (val & 1);					    \
		 flagn = 0;						    \
		 val >>= 1;						    \
		 SETZ(val)						    \
		 WRITE(val)
#define LSRA	 flagc = (regs.a & 1);					    \
		 flagn = 0;						    \
		 regs.a >>= 1;						    \
		 SETZ(regs.a)
#define NOP	 /*bSlowerOnPagecross = 1;*/
#define OAL	 regs.a |= 0xEE;					    \
		 regs.a &= READ;					    \
		 regs.x = regs.a;					    \
		 SETNZ(regs.a)
#define ORA	 /*bSlowerOnPagecross = 1;*/						    \
		 regs.a |= READ;					    \
		 SETNZ(regs.a)
#define PHA	 PUSH(regs.a)
#define PHP	 EF_TO_AF						    \
		 PUSH(regs.ps)
#define PHX	 PUSH(regs.x)
#define PHY	 PUSH(regs.y)
#define PLA	 regs.a = POP;						    \
		 SETNZ(regs.a)
#define PLP	 regs.ps = POP | AF_RESERVED | AF_BREAK;		    \
		 AF_TO_EF
#define PLX	 regs.x = POP;						    \
		 SETNZ(regs.x)
#define PLY	 regs.y = POP;						    \
		 SETNZ(regs.y)
#define RLA	 /*bSlowerOnPagecross = 0;*/						    \
		 val   = (READ << 1) | flagc;				    \
		 flagc = (val > 0xFF);					    \
		 WRITE(val)						    \
		 regs.a &= val;						    \
		 SETNZ(regs.a)
#define ROL_NMOS /*bSlowerOnPagecross = 0;*/						    \
		 val   = (READ << 1) | flagc;				    \
		 flagc = (val > 0xFF);					    \
		 SETNZ(val)						    \
		 WRITE(val)
#define ROL_CMOS /*bSlowerOnPagecross = 1;*/						    \
		 val   = (READ << 1) | flagc;				    \
		 flagc = (val > 0xFF);					    \
		 SETNZ(val)						    \
		 WRITE(val)
#define ROLA	 val	= (((WORD)regs.a) << 1) | flagc;		    \
		 flagc	= (val > 0xFF);					    \
		 regs.a = val & 0xFF;					    \
		 SETNZ(regs.a);
#define ROR_NMOS /*bSlowerOnPagecross = 0;*/						    \
		 temp  = READ;						    \
		 val   = (temp >> 1) | (flagc ? 0x80 : 0);		    \
		 flagc = (temp & 1);					    \
		 SETNZ(val)						    \
		 WRITE(val)
#define ROR_CMOS /*bSlowerOnPagecross = 1;*/						    \
		 temp  = READ;						    \
		 val   = (temp >> 1) | (flagc ? 0x80 : 0);		    \
		 flagc = (temp & 1);					    \
		 SETNZ(val)						    \
		 WRITE(val)
#define RORA	 val	= (((WORD)regs.a) >> 1) | (flagc ? 0x80 : 0);	    \
		 flagc	= (regs.a & 1);					    \
		 regs.a = val & 0xFF;					    \
		 SETNZ(regs.a)
#define RRA	 /*bSlowerOnPagecross = 0;*/						    \
		 temp  = READ;						    \
		 val   = (temp >> 1) | (flagc ? 0x80 : 0);		    \
		 flagc = (temp & 1);					    \
		 WRITE(val)						    \
		 temp = val;						    \
		 if (regs.ps & AF_DECIMAL) {				    \
		   val	  = (regs.a & 0x0F) + (temp & 0x0F) + flagc;	    \
		   if (val > 0x09)					    \
		     val += 0x06;					    \
		   if (val <= 0x0F)					    \
		     val = (val & 0x0F) + (regs.a & 0xF0) + (temp & 0xF0);  \
		   else							    \
		     val = (val & 0x0F) + (regs.a & 0xF0) + (temp & 0xF0) + 0x10;\
		   flagz = !((regs.a + temp + flagc) & 0xFF);		    \
		   flagn = (val & 0x80);				    \
		   flagv = ((regs.a ^ val) & 0x80) && !((regs.a ^ temp) & 0x80);\
		   if ((val & 0x1F0) > 0x90)				    \
		     val += 0x60;					    \
		   flagc = ((val & 0xFF0) > 0xF0);			    \
		   regs.a = val & 0xFF;                                     \
		 }							    \
		 else {							    \
		   val	  = regs.a + temp + flagc;			    \
		   flagc  = (val > 0xFF);				    \
		   flagv  = (((regs.a & 0x80) == (temp & 0x80)) &&	    \
			     ((regs.a & 0x80) != (val & 0x80)));	    \
		   regs.a = val & 0xFF;					    \
		   SETNZ(regs.a);					    \
		 }
#define RTI	 regs.ps = POP | AF_RESERVED | AF_BREAK;		    \
		 AF_TO_EF						    \
		 regs.pc = POP;						    \
		 regs.pc |= (((WORD)POP) << 8);
#define RTS	 regs.pc = POP;						    \
		 regs.pc |= (((WORD)POP) << 8);				    \
		 ++regs.pc;
#define SAX	 temp	= regs.a & regs.x;				    \
		 val	= READ;						    \
		 flagc	= (temp >= val);				    \
		 regs.x = temp-val;					    \
		 SETNZ(regs.x)
#define SAY	 /*bSlowerOnPagecross = 0;*/						    \
		 val = regs.y & (((base >> 8) + 1) & 0xFF);		    \
		 ON_PAGECROSS_REPLACE_HI_ADDR								\
		 WRITE(val)
#define SBC_NMOS /*bSlowerOnPagecross = 1;*/						    \
		 temp = READ;						    \
		 temp2 = regs.a - temp - !flagc;			    \
		 if (regs.ps & AF_DECIMAL) {				    \
		   val  = (regs.a & 0x0F) - (temp & 0x0F) - !flagc;	    \
		   if (val & 0x10)					    \
		     val = ((val - 0x06) & 0x0F) | ((regs.a & 0xF0) - (temp & 0xF0) - 0x10);\
		   else							    \
		     val = (val & 0x0F) | ((regs.a & 0xF0) - (temp & 0xF0));\
		   if (val & 0x100)					    \
		     val -= 0x60;					    \
		   flagc  = (temp2 < 0x100);				    \
		   SETNZ(temp2 & 0xFF);					    \
		   flagv = ((regs.a ^ temp2) & 0x80) && ((regs.a ^ temp) & 0x80);\
		   regs.a = val & 0xFF;					    \
		 }							    \
		 else {							    \
		   val	  = temp2;					    \
		   flagc  = (val < 0x100);				    \
		   flagv  = (((regs.a & 0x80) != (temp & 0x80)) &&	    \
			     ((regs.a & 0x80) != (val & 0x80)));	    \
		   regs.a = val & 0xFF;					    \
		   SETNZ(regs.a);					    \
		 }
#define SBC_CMOS /*bSlowerOnPagecross = 1;*/						    \
	         temp = READ;						    \
		 flagv = ((regs.a ^ temp) & 0x80);			    \
		 if (regs.ps & AF_DECIMAL) {				    \
		    uExtraCycles++;					    \
                    temp2 = 0x0F + (regs.a & 0x0F) - (temp & 0x0F) + flagc; \
		    if (temp2 < 0x10) {					    \
		       val = 0;						    \
		       temp2 -= 0x06;					    \
		    }							    \
		    else {						    \
		       val = 0x10;					    \
		       temp2 -= 0x10;					    \
		    }							    \
		    val += 0xF0 + (regs.a & 0xF0) - (temp & 0xF0);	    \
		    if (val < 0x100) {					    \
		       flagc = 0;					    \
		       if (val < 0x80)					    \
			  flagv = 0;					    \
		       val -= 0x60;					    \
		    }							    \
		    else {						    \
		       flagc = 1;					    \
		       if (val >= 0x180)				    \
			  flagv = 0;					    \
		    }							    \
		    val += temp2;					    \
		 }							    \
		 else {							    \
		    val = 0xff + regs.a - temp + flagc;                     \
		    if (val < 0x100) {					    \
		       flagc = 0;					    \
		       if (val < 0x80)					    \
			  flagv = 0;					    \
		    }							    \
		    else {						    \
		       flagc = 1;					    \
		       if (val >= 0x180)				    \
		          flagv = 0;					    \
		    }							    \
		 }							    \
		 regs.a = val & 0xFF;					    \
                 SETNZ(regs.a)
#define SEC	 flagc = 1;
#define SED	 regs.ps |= AF_DECIMAL;
#define SEI	 regs.ps |= AF_INTERRUPT;
#define STA	 /*bSlowerOnPagecross = 0;*/						    \
		 WRITE(regs.a)
#define STX	 /*bSlowerOnPagecross = 0;*/						    \
		 WRITE(regs.x)
#define STY	 /*bSlowerOnPagecross = 0;*/						    \
		 WRITE(regs.y)
#define STZ	 /*bSlowerOnPagecross = 0;*/						    \
		 WRITE(0)
#define TAS	 /*bSlowerOnPagecross = 0;*/						    \
		 val = regs.a & regs.x;					    \
		 regs.sp = 0x100 | val;					    \
		 val &= (((base >> 8) + 1) & 0xFF);			    \
		 ON_PAGECROSS_REPLACE_HI_ADDR								\
		 WRITE(val)
#define TAX	 regs.x = regs.a;					    \
		 SETNZ(regs.x)
#define TAY	 regs.y = regs.a;					    \
		 SETNZ(regs.y)
#define TRB	 /*bSlowerOnPagecross = 0;*/						    \
		 val   = READ;						    \
		 flagz = !(regs.a & val);				    \
		 val  &= ~regs.a;					    \
		 WRITE(val)
#define TSB	 /*bSlowerOnPagecross = 0;*/						    \
		 val   = READ;						    \
		 flagz = !(regs.a & val);				    \
		 val   |= regs.a;					    \
		 WRITE(val)
#define TSX	 regs.x = regs.sp & 0xFF;				    \
		 SETNZ(regs.x)
#define TXA	 regs.a = regs.x;					    \
		 SETNZ(regs.a)
#define TXS	 regs.sp = 0x100 | regs.x;
#define TYA	 regs.a = regs.y;					    \
		 SETNZ(regs.a)
#define XAA	 regs.a = regs.x;					    \
		 regs.a &= READ;					    \
		 SETNZ(regs.a)
#define XAS	 /*bSlowerOnPagecross = 0;*/						    \
		 val = regs.x & (((base >> 8) + 1) & 0xFF);		    \
		 ON_PAGECROSS_REPLACE_HI_ADDR								\
		 WRITE(val)
