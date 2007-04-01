/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 1994-1996, Michael O'Brien
Copyright (C) 1999-2001, Oliver Schmidt
Copyright (C) 2002-2005, Tom Charlesworth
Copyright (C) 2006-2007, Tom Charlesworth, Michael Pohoreski

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
// . Thus they're not emulated in Enhanced //e g_nAppMode. Games relying on them
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
// . $EC CPX ABS (since there's no addressing g_nAppMode of CPY which has variable cycle number)
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
#pragma	 hdrstop

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

static signed long g_uInternalExecutedCycles;

//

// Assume all interrupt sources assert until the device is told to stop:
// - eg by r/w to device's register or a machine reset

static bool g_bCritSectionValid = false;	// Deleting CritialSection when not valid causes crash on Win98
static CRITICAL_SECTION g_CriticalSection;	// To guard /g_bmIRQ/ & /g_bmNMI/
static volatile UINT32 g_bmIRQ = 0;
static volatile UINT32 g_bmNMI = 0;
static volatile BOOL g_bNmiFlank = FALSE; // Positive going flank on NMI line

/****************************************************************************
*
*  GENERAL PURPOSE MACROS
*
***/

#define AF_TO_EF  flagc = (regs.ps & AF_CARRY);				    \
		  flagn = (regs.ps & AF_SIGN);				    \
		  flagv = (regs.ps & AF_OVERFLOW);			    \
		  flagz = (regs.ps & AF_ZERO);
#define EF_TO_AF  regs.ps = (regs.ps & ~(AF_CARRY | AF_SIGN |		    \
					 AF_OVERFLOW | AF_ZERO))	    \
			      | flagc 					    \
			      | flagn					    \
			      | (flagv ? AF_OVERFLOW : 0)		    \
			      | (flagz ? AF_ZERO     : 0)		    \
			      | AF_RESERVED | AF_BREAK;
// CYC(a): This can be optimised, as only certain opcodes will affect uExtraCycles
#define CYC(a)	 uExecutedCycles += (a)+uExtraCycles; MB_UpdateCycles((a)+uExtraCycles);
#define POP	 (*(mem+((regs.sp >= 0x1FF) ? (regs.sp = 0x100) : ++regs.sp)))
#define PUSH(a)	 *(mem+regs.sp--) = (a);				    \
		 if (regs.sp < 0x100)					    \
		   regs.sp = 0x1FF;
#define READ	 (							    \
		    ((addr & 0xFF00) == 0xC000)				    \
		    ? ioread[addr & 0xFF](regs.pc,(BYTE)addr,0,0,uExecutedCycles) \
		    : (							    \
			(((addr & 0xFF00) == 0xC400) || ((addr & 0xFF00) == 0xC500)) \
			? CxReadFunc(regs.pc, addr, 0, 0, uExecutedCycles) \
			: *(mem+addr)					    \
		      )							    \
		 )
#define SETNZ(a) {							    \
		   flagn = ((a) & 0x80);				    \
		   flagz = !((a) & 0xFF);					    \
		 }
#define SETZ(a)	 flagz = !((a) & 0xFF);
#define WRITE(a) {							    \
		   memdirty[addr >> 8] = 0xFF;				    \
		   LPBYTE page = memwrite[0][addr >> 8];		    \
		   if (page)						    \
		     *(page+(addr & 0xFF)) = (BYTE)(a);			    \
		   else if ((addr & 0xFF00) == 0xC000)			    \
		     iowrite[addr & 0xFF](regs.pc,(BYTE)addr,1,(BYTE)(a),uExecutedCycles); \
		   else if(((addr & 0xFF00) == 0xC400) || ((addr & 0xFF00) == 0xC500)) \
		     CxWriteFunc(regs.pc, addr, 1, (BYTE)(a), uExecutedCycles); \
		 }

//

// ExtraCycles:
// +1 if branch taken
// +1 if page boundary crossed
#define BRANCH_TAKEN {					\
			 base = regs.pc;		\
			 regs.pc += addr;		\
			 if ((base ^ regs.pc) & 0xFF00) \
			     uExtraCycles=2;		\
			 else				\
			     uExtraCycles=1;		\
		     }

//

#define CHECK_PAGE_CHANGE  if (bSlowerOnPagecross) {		      \
			       if ((base ^ addr) & 0xFF00)    \
				   uExtraCycles=1;	      \
			   }

/****************************************************************************
*
*  ADDRESSING MODE MACROS
*
***/

#define ABS	 addr = *(LPWORD)(mem+regs.pc);	 regs.pc += 2;
#define IABSX    addr = *(LPWORD)(mem+(*(LPWORD)(mem+regs.pc))+(WORD)regs.x); regs.pc += 2;
#define ABSX	 base = *(LPWORD)(mem+regs.pc); addr = base+(WORD)regs.x; regs.pc += 2; CHECK_PAGE_CHANGE;
#define ABSY	 base = *(LPWORD)(mem+regs.pc); addr = base+(WORD)regs.y; regs.pc += 2; CHECK_PAGE_CHANGE;
#define IABSCMOS base = *(LPWORD)(mem+regs.pc);	                          \
		 addr = *(LPWORD)(mem+base);		                  \
		 if ((base & 0xFF) == 0xFF) uExtraCycles=1;		  \
		 regs.pc += 2;
#define IABSNMOS base = *(LPWORD)(mem+regs.pc);	                          \
		 if ((base & 0xFF) == 0xFF)				  \
		       addr = *(mem+base)+((WORD)*(mem+(base&0xFF00))<<8);\
		   else                                                   \
		       addr = *(LPWORD)(mem+base);                        \
		 regs.pc += 2;
#define IMM	 addr = regs.pc++;
#define INDX	 base = ((*(mem+regs.pc++))+regs.x) & 0xFF;          \
		 if (base == 0xFF)                                   \
		     addr = *(mem+0xFF)+(((WORD)*mem)<<8);           \
		 else                                                \
		     addr = *(LPWORD)(mem+base);
#define INDY	 if (*(mem+regs.pc) == 0xFF)                         \
		     base = *(mem+0xFF)+(((WORD)*mem)<<8);           \
		 else                                                \
		     base = *(LPWORD)(mem+*(mem+regs.pc));           \
		 regs.pc++;                                          \
		 addr = base+(WORD)regs.y;                           \
		 CHECK_PAGE_CHANGE;
#define IZPG	 base = *(mem+regs.pc++);                            \
		 if (base == 0xFF)                                   \
		     addr = *(mem+0xFF)+(((WORD)*mem)<<8);           \
		 else                                                \
		     addr = *(LPWORD)(mem+base);
#define REL	 addr = (signed char)*(mem+regs.pc++);
#define ZPG	 addr = *(mem+regs.pc++);
#define ZPGX	 addr = ((*(mem+regs.pc++))+regs.x) & 0xFF;
#define ZPGY	 addr = ((*(mem+regs.pc++))+regs.y) & 0xFF;

/****************************************************************************
*
*  INSTRUCTION MACROS
*
***/

#define ADC_NMOS bSlowerOnPagecross = 1;						    \
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
#define ADC_CMOS bSlowerOnPagecross = 1;						    \
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
#define AND	 bSlowerOnPagecross = 1;						    \
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
#define ASL_NMOS bSlowerOnPagecross = 0;						    \
		 val   = READ << 1;					    \
		 flagc = (val > 0xFF);					    \
		 SETNZ(val)						    \
		 WRITE(val)
#define ASL_CMOS bSlowerOnPagecross = 1;						    \
		 val   = READ << 1;					    \
		 flagc = (val > 0xFF);					    \
		 SETNZ(val)						    \
		 WRITE(val)
#define ASLA	 val   = regs.a << 1;					    \
		 flagc = (val > 0xFF);					    \
		 SETNZ(val)						    \
		 regs.a = (BYTE)val;
#define ASO	 bSlowerOnPagecross = 0;						    \
		 val   = READ << 1;					    \
		 flagc = (val > 0xFF);					    \
		 WRITE(val)						    \
		 regs.a |= val;						    \
		 SETNZ(regs.a)
#define AXA	 bSlowerOnPagecross = 0;/*FIXME: $93 case is still unclear*/	    \
		 val = regs.a & regs.x & (((base >> 8) + 1) & 0xFF);	    \
		 WRITE(val)
#define AXS	 bSlowerOnPagecross = 0;						    \
		 WRITE(regs.a & regs.x)
#define BCC	 if (!flagc) BRANCH_TAKEN;
#define BCS	 if ( flagc) BRANCH_TAKEN;
#define BEQ	 if ( flagz) BRANCH_TAKEN;
#define BIT	 bSlowerOnPagecross = 1;						    \
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
#define CMP	 bSlowerOnPagecross = 1;						    \
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
#define DCM	 bSlowerOnPagecross = 0;						    \
		 val = READ-1;						    \
		 WRITE(val)						    \
		 flagc = (regs.a >= val);				    \
		 val   = regs.a-val;					    \
		 SETNZ(val)
#define DEA	 --regs.a;						    \
		 SETNZ(regs.a)
#define DEC_NMOS bSlowerOnPagecross = 0;						    \
		 val = READ-1;						    \
		 SETNZ(val)						    \
		 WRITE(val)
#define DEC_CMOS bSlowerOnPagecross = 1;						    \
		 val = READ-1;						    \
		 SETNZ(val)						    \
		 WRITE(val)
#define DEX	 --regs.x;						    \
		 SETNZ(regs.x)
#define DEY	 --regs.y;						    \
		 SETNZ(regs.y)
#define EOR	 bSlowerOnPagecross = 1;						    \
		 regs.a ^= READ;					    \
		 SETNZ(regs.a)
#define HLT	 regs.bJammed = 1;					    \
		 --regs.pc;
#define INA	 ++regs.a;						    \
		 SETNZ(regs.a)
#define INC_NMOS bSlowerOnPagecross = 0;						    \
		 val = READ+1;						    \
		 SETNZ(val)						    \
		 WRITE(val)
#define INC_CMOS bSlowerOnPagecross = 1;						    \
		 val = READ+1;						    \
		 SETNZ(val)						    \
		 WRITE(val)
#define INS	 bSlowerOnPagecross = 0;						    \
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
#define LAS	 bSlowerOnPagecross = 1;						    \
		 val = (BYTE)(READ & regs.sp);				    \
		 regs.a = regs.x = (BYTE) val;				    \
		 regs.sp = val | 0x100;					    \
		 SETNZ(val)
#define LAX	 bSlowerOnPagecross = 1;						    \
		 regs.a = regs.x = READ;				    \
		 SETNZ(regs.a)
#define LDA	 bSlowerOnPagecross = 1;						    \
		 regs.a = READ;						    \
		 SETNZ(regs.a)
#define LDX	 bSlowerOnPagecross = 1;						    \
		 regs.x = READ;						    \
		 SETNZ(regs.x)
#define LDY	 bSlowerOnPagecross = 1;						    \
		 regs.y = READ;						    \
		 SETNZ(regs.y)
#define LSE	 bSlowerOnPagecross = 0;						    \
		 val   = READ;						    \
		 flagc = (val & 1);					    \
		 val >>= 1;						    \
		 WRITE(val)						    \
		 regs.a ^= val;						    \
		 SETNZ(regs.a)
#define LSR_NMOS bSlowerOnPagecross = 0;						    \
		 val   = READ;						    \
		 flagc = (val & 1);					    \
		 flagn = 0;						    \
		 val >>= 1;						    \
		 SETZ(val)						    \
		 WRITE(val)
#define LSR_CMOS bSlowerOnPagecross = 1;						    \
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
#define NOP	 bSlowerOnPagecross = 1;
#define OAL	 regs.a |= 0xEE;					    \
		 regs.a &= READ;					    \
		 regs.x = regs.a;					    \
		 SETNZ(regs.a)
#define ORA	 bSlowerOnPagecross = 1;						    \
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
#define RLA	 bSlowerOnPagecross = 0;						    \
		 val   = (READ << 1) | flagc;				    \
		 flagc = (val > 0xFF);					    \
		 WRITE(val)						    \
		 regs.a &= val;						    \
		 SETNZ(regs.a)
#define ROL_NMOS bSlowerOnPagecross = 0;						    \
		 val   = (READ << 1) | flagc;				    \
		 flagc = (val > 0xFF);					    \
		 SETNZ(val)						    \
		 WRITE(val)
#define ROL_CMOS bSlowerOnPagecross = 1;						    \
		 val   = (READ << 1) | flagc;				    \
		 flagc = (val > 0xFF);					    \
		 SETNZ(val)						    \
		 WRITE(val)
#define ROLA	 val	= (((WORD)regs.a) << 1) | flagc;		    \
		 flagc	= (val > 0xFF);					    \
		 regs.a = val & 0xFF;					    \
		 SETNZ(regs.a);
#define ROR_NMOS bSlowerOnPagecross = 0;						    \
		 temp  = READ;						    \
		 val   = (temp >> 1) | (flagc ? 0x80 : 0);		    \
		 flagc = (temp & 1);					    \
		 SETNZ(val)						    \
		 WRITE(val)
#define ROR_CMOS bSlowerOnPagecross = 1;						    \
		 temp  = READ;						    \
		 val   = (temp >> 1) | (flagc ? 0x80 : 0);		    \
		 flagc = (temp & 1);					    \
		 SETNZ(val)						    \
		 WRITE(val)
#define RORA	 val	= (((WORD)regs.a) >> 1) | (flagc ? 0x80 : 0);	    \
		 flagc	= (regs.a & 1);					    \
		 regs.a = val & 0xFF;					    \
		 SETNZ(regs.a)
#define RRA	 bSlowerOnPagecross = 0;						    \
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
#define SAY	 bSlowerOnPagecross = 0;						    \
		 val = regs.y & (((base >> 8) + 1) & 0xFF);		    \
		 WRITE(val)
#define SBC_NMOS bSlowerOnPagecross = 1;						    \
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
#define SBC_CMOS bSlowerOnPagecross = 1;						    \
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
#define STA	 bSlowerOnPagecross = 0;						    \
		 WRITE(regs.a)
#define STX	 bSlowerOnPagecross = 0;						    \
		 WRITE(regs.x)
#define STY	 bSlowerOnPagecross = 0;						    \
		 WRITE(regs.y)
#define STZ	 bSlowerOnPagecross = 0;						    \
		 WRITE(0)
#define TAS	 bSlowerOnPagecross = 0;						    \
		 val = regs.a & regs.x;					    \
		 regs.sp = 0x100 | val;					    \
		 val &= (((base >> 8) + 1) & 0xFF);			    \
		 WRITE(val)
#define TAX	 regs.x = regs.a;					    \
		 SETNZ(regs.x)
#define TAY	 regs.y = regs.a;					    \
		 SETNZ(regs.y)
#define TRB	 bSlowerOnPagecross = 0;						    \
		 val   = READ;						    \
		 flagz = !(regs.a & val);				    \
		 val  &= ~regs.a;					    \
		 WRITE(val)
#define TSB	 bSlowerOnPagecross = 0;						    \
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
#define XAS	 bSlowerOnPagecross = 0;						    \
		 val = regs.x & (((base >> 8) + 1) & 0xFF);		    \
		 WRITE(val)

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

static inline void DoIrqProfiling(DWORD uCycles)
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

static DWORD Cpu65C02 (DWORD uTotalCycles)
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
	DWORD uExecutedCycles = 0;
	BOOL bSlowerOnPagecross;		// Set if opcode writes to memory (eg. ASL, STA)
	WORD base;
	bool bBreakOnInvalid = false;  

	do
	{
		g_uInternalExecutedCycles = uExecutedCycles;
		USHORT uExtraCycles = 0;

		BYTE iOpcode = *(mem+regs.pc);
		if (CheckDebugBreak( iOpcode ))
			break;

		regs.pc++;

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
	DWORD uExecutedCycles = 0;
	BOOL bSlowerOnPagecross;		// Set if opcode writes to memory (eg. ASL, STA)
	WORD base;
	bool bBreakOnInvalid = false;  

	do
	{
		g_uInternalExecutedCycles = uExecutedCycles;
		USHORT uExtraCycles = 0;

		BYTE iOpcode = *(mem+regs.pc);
		if (CheckDebugBreak( iOpcode ))
			break;

		regs.pc++;

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

		if(g_bNmiFlank && !regs.bJammed)
		{
			// NMI signals are only serviced once
			g_bNmiFlank = FALSE;
			g_nCycleIrqStart = g_nCumulativeCycles + uExecutedCycles;
			PUSH(regs.pc >> 8)
			PUSH(regs.pc & 0xFF)
			EF_TO_AF
			PUSH(regs.ps & ~AF_BREAK)
			regs.ps = regs.ps | AF_INTERRUPT;
			regs.pc = * (WORD*) (mem+0xFFFA);
			CYC(7)
		}

		if(g_bmIRQ && !(regs.ps & AF_INTERRUPT) && !regs.bJammed)
		{
			// IRQ signals are deasserted when a specific r/w operation is done on device
			g_nCycleIrqStart = g_nCumulativeCycles + uExecutedCycles;
			PUSH(regs.pc >> 8)
			PUSH(regs.pc & 0xFF)
			EF_TO_AF
			PUSH(regs.ps & ~AF_BREAK)
			regs.ps = regs.ps | AF_INTERRUPT;
			regs.pc = * (WORD*) (mem+0xFFFE);
			CYC(7)
		}

		if (bBreakOnInvalid)
			break;

	} while (uExecutedCycles < uTotalCycles);

	EF_TO_AF
	return uExecutedCycles;
}

//===========================================================================

static DWORD InternalCpuExecute (DWORD uTotalCycles)
{
	if (g_bApple2e)
		return Cpu65C02(uTotalCycles);
	else // Apple ][
		return Cpu6502(uTotalCycles);
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
void CpuCalcCycles(ULONG nCyclesExecuted)
{
	// Calc # of cycles executed since this func was last called
	ULONG nCycles = nCyclesExecuted - g_nCyclesExecuted;

	g_nCyclesExecuted += nCycles;
	g_nCumulativeCycles += nCycles;
}

//===========================================================================

ULONG CpuGetCyclesThisFrame()
{
	CpuCalcCycles(g_uInternalExecutedCycles);
	return g_dwCyclesThisFrame + g_nCyclesExecuted;
}

//===========================================================================

DWORD CpuExecute (DWORD uCycles)
{
	DWORD uExecutedCycles =	0;

	g_nCyclesSubmitted = uCycles;
	g_nCyclesExecuted =	0;

	if (uCycles	== 0)	// Do single step
		uExecutedCycles	= InternalCpuExecute(0);
	else				// Do multi-opcode emulation
		uExecutedCycles	= InternalCpuExecute(uCycles);

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
