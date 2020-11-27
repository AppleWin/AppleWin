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

 /****************************************************************************
*
*  GENERAL PURPOSE MACROS
*
***/

#undef AF_TO_EF
#undef EF_TO_AF



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
#define CYC(a)	 uExecutedCycles += (a)+uExtraCycles;
#define POP	 (*(mem+((regs.sp >= 0x1FF) ? (regs.sp = 0x100) : ++regs.sp)))
#define PUSH(a)	 *(mem+regs.sp--) = (a);				    \
		 if (regs.sp < 0x100)					    \
		   regs.sp = 0x1FF;
#define _READ	(																\
			((addr & 0xF000) == 0xC000)											\
				? IORead[(addr>>4) & 0xFF](regs.pc,addr,0,0,uExecutedCycles)	\
				: *(mem+addr)													\
		)
#define _READ_WITH_IO_F8xx (										/* GH#827 */\
			((addr & 0xF000) == 0xC000)											\
				? IORead[(addr>>4) & 0xFF](regs.pc,addr,0,0,uExecutedCycles)	\
				: (addr >= 0xF800)												\
					? IO_F8xx(regs.pc,addr,0,0,uExecutedCycles)					\
					: *(mem+addr)												\
		)
#define SETNZ(a) {							    \
		   flagn = ((a) & 0x80);				    \
		   flagz = !((a) & 0xFF);					    \
		 }
#define SETZ(a)	 flagz = !((a) & 0xFF);
#define _WRITE(a) {																		\
			{																			\
				memdirty[addr >> 8] = 0xFF;												\
				LPBYTE page = memwrite[addr >> 8];										\
				if (page)																\
					*(page+(addr & 0xFF)) = (BYTE)(a);									\
				else if ((addr & 0xF000) == 0xC000)										\
					IOWrite[(addr>>4) & 0xFF](regs.pc,addr,1,(BYTE)(a),uExecutedCycles);\
			}																			\
		}
#define _WRITE_WITH_IO_F8xx(a) {											/* GH#827 */\
			if (addr >= 0xF800)															\
				IO_F8xx(regs.pc,addr,1,(BYTE)(a),uExecutedCycles);						\
			else {																		\
				memdirty[addr >> 8] = 0xFF;												\
				LPBYTE page = memwrite[addr >> 8];										\
				if (page)																\
					*(page+(addr & 0xFF)) = (BYTE)(a);									\
				else if ((addr & 0xF000) == 0xC000)										\
					IOWrite[(addr>>4) & 0xFF](regs.pc,addr,1,(BYTE)(a),uExecutedCycles);\
			}																			\
		}

#define ON_PAGECROSS_REPLACE_HI_ADDR if ((base ^ addr) >> 8) {addr = (val<<8) | (addr&0xff);} /* GH#282 */

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

// TODO Optimization Note: uExtraCycles = ((base ^ addr) >> 8) & 1;
#define CHECK_PAGE_CHANGE	if ((base ^ addr) & 0xFF00)			\
									uExtraCycles=1;

/****************************************************************************
*
*  ADDRESSING MODE MACROS
*
***/

#define ABS	 addr = *(LPWORD)(mem+regs.pc);	 regs.pc += 2;
#define IABSX    addr = *(LPWORD)(mem+(*(LPWORD)(mem+regs.pc))+(WORD)regs.x); regs.pc += 2;

// Optimised for page-cross
#define ABSX_OPT base = *(LPWORD)(mem+regs.pc); addr = base+(WORD)regs.x; regs.pc += 2; CHECK_PAGE_CHANGE;
// Not optimised for page-cross
#define ABSX_CONST base = *(LPWORD)(mem+regs.pc); addr = base+(WORD)regs.x; regs.pc += 2;

// Optimised for page-cross
#define ABSY_OPT base = *(LPWORD)(mem+regs.pc); addr = base+(WORD)regs.y; regs.pc += 2; CHECK_PAGE_CHANGE;
// Not optimised for page-cross
#define ABSY_CONST base = *(LPWORD)(mem+regs.pc); addr = base+(WORD)regs.y; regs.pc += 2;

// TODO Optimization Note (just for IABSCMOS): uExtraCycles = ((base & 0xFF) + 1) >> 8;
#define IABS_CMOS base = *(LPWORD)(mem+regs.pc);	                          \
		 addr = *(LPWORD)(mem+base);		                  \
		 if ((base & 0xFF) == 0xFF) uExtraCycles=1;		  \
		 regs.pc += 2;
#define IABS_NMOS base = *(LPWORD)(mem+regs.pc);	                          \
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

// Optimised for page-cross
#define INDY_OPT	 if (*(mem+regs.pc) == 0xFF)             /*incurs an extra cycle for page-crossing*/ \
		     base = *(mem+0xFF)+(((WORD)*mem)<<8);           \
		 else                                                \
		     base = *(LPWORD)(mem+*(mem+regs.pc));           \
		 regs.pc++;                                          \
		 addr = base+(WORD)regs.y;                           \
		 CHECK_PAGE_CHANGE;
// Not optimised for page-cross
#define INDY_CONST	 if (*(mem+regs.pc) == 0xFF)             /*no extra cycle for page-crossing*/ \
		     base = *(mem+0xFF)+(((WORD)*mem)<<8);           \
		 else                                                \
		     base = *(LPWORD)(mem+*(mem+regs.pc));           \
		 regs.pc++;                                          \
		 addr = base+(WORD)regs.y;

#define IZPG	 base = *(mem+regs.pc++);                            \
		 if (base == 0xFF)                                   \
		     addr = *(mem+0xFF)+(((WORD)*mem)<<8);           \
		 else                                                \
		     addr = *(LPWORD)(mem+base);

#define REL	 addr = (signed char)*(mem+regs.pc++);

// TODO Optimization Note:
// . Opcodes that generate zero-page addresses can't be accessing $C000..$CFFF
//   so they could be paired with special READZP/WRITEZP macros (instead of READ/WRITE)
#define ZPG 	 addr =   *(mem+regs.pc++);
#define ZPGX	 addr = ((*(mem+regs.pc++))+regs.x) & 0xFF;
#define ZPGY	 addr = ((*(mem+regs.pc++))+regs.y) & 0xFF;

// Tidy 3 char addressing modes to keep the opcode table visually aligned, clean, and readable.
#undef asl
#undef idx
#undef imm
#undef izp
#undef lsr
#undef rel
#undef rol
#undef ror
#undef zpx
#undef zpy

#define asl ASLA // Arithmetic Shift Left
#define idx INDX
#define imm IMM
#define izp IZPG
#define lsr LSRA // Logical Shift Right
#define rel REL
#define rol ROLA // Rotate Left
#define ror RORA // Rotate Right
#define zpx ZPGX
#define zpy ZPGY
