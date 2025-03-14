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

#define ZERO_PAGE 0x00
#define STACK_PAGE 0x01

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

#define _POP (*(mem+((regs.sp >= 0x1FF) ? (regs.sp = 0x100) : ++regs.sp)))
#define _POP_ALT ( /*TODO: Support reads from IO & Floating bus*/\
			*(memshadow[STACK_PAGE]-0x100+((regs.sp >= 0x1FF) ? (regs.sp = 0x100) : ++regs.sp)) \
		)

#define _PUSH(a) *(mem+regs.sp--) = (a);									    \
		 if (regs.sp < 0x100)												    \
		   regs.sp = 0x1FF;
#define _PUSH_ALT(a) {																\
			LPBYTE page = memwrite[STACK_PAGE];										\
			if (page) {																\
				*(page+(regs.sp & 0xFF)) = (BYTE)(a);								\
			}																		\
			regs.sp--;																\
			if (regs.sp < 0x100)													\
				regs.sp = 0x1FF;													\
		}

#define _READ	(																\
			((addr & 0xF000) == 0xC000)											\
				? IORead[(addr>>4) & 0xFF](regs.pc,addr,0,0,uExecutedCycles)	\
				: *(mem+addr)													\
		)
#define _READ_ALT (																\
			(memreadPageType[addr >> 8] == MEM_Normal)							\
				? *(memshadow[addr >> 8]+(addr&0xff))							\
				: (memreadPageType[addr >> 8] == MEM_IORead)					\
					? IORead[(addr >> 4) & 0xFF](regs.pc, addr, 0, 0, uExecutedCycles)	\
					: MemReadFloatingBus(uExecutedCycles)						\
		)
#define _READ_ALT2(addr) (														\
			(memreadPageType[addr >> 8] == MEM_Normal)							\
				? *(memshadow[addr >> 8]+(addr&0xff))							\
				: (memreadPageType[addr >> 8] == MEM_IORead)					\
					? IORead[(addr >> 4) & 0xFF](regs.pc, addr, 0, 0, uExecutedCycles)	\
					: MemReadFloatingBus(uExecutedCycles)						\
		)
#define _READ_WITH_IO_F8xx (										/* GH#827 */\
			((addr & 0xF000) == 0xC000)											\
				? IORead[(addr>>4) & 0xFF](regs.pc,addr,0,0,uExecutedCycles)	\
				: (addr >= 0xF800)												\
					? IO_F8xx(regs.pc,addr,0,0,uExecutedCycles)					\
					: *(memshadow[addr >> 8]+(addr&0xff))						\
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
#define _WRITE_ALT(a) {																	\
			{																			\
				memdirty[addr >> 8] = 0xFF;												\
				LPBYTE page = memwrite[addr >> 8];										\
				if (page) {																\
					*(page+(addr & 0xFF)) = (BYTE)(a);									\
					if (memVidHD)											/* GH#997 */\
						*(memVidHD + addr) = (BYTE)(a);									\
				}																		\
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
				if (page) {																\
					*(page+(addr & 0xFF)) = (BYTE)(a);									\
					if (memVidHD)											/* GH#997 */\
						*(memVidHD + addr) = (BYTE)(a);									\
				}																		\
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

#define READ_BYTE_ALT(pc) _READ_ALT2(pc)
#define READ_WORD_ALT(pc) (_READ_ALT2(pc) | (_READ_ALT2((pc+1))<<8))

/****************************************************************************
*
*  ADDRESSING MODE MACROS
*
***/

#define _ABS	addr = *(LPWORD)(mem+regs.pc);	 regs.pc += 2;
#define _ABS_ALT												\
		addr = READ_WORD_ALT(regs.pc);							\
		regs.pc += 2;

#define _IABSX	addr = *(LPWORD)(mem+(*(LPWORD)(mem+regs.pc))+(WORD)regs.x); regs.pc += 2;
#define _IABSX_ALT												\
		base = READ_WORD_ALT(regs.pc) + (WORD)regs.x;			\
		addr = READ_WORD_ALT(base);								\
		regs.pc += 2;

// Not optimised for page-cross
#define _ABSX_CONST	base = *(LPWORD)(mem+regs.pc); addr = base+(WORD)regs.x; regs.pc += 2;
#define _ABSX_CONST_ALT											\
		base = READ_WORD_ALT(regs.pc);							\
		addr = base + (WORD)regs.x;								\
		regs.pc += 2;

// Optimised for page-cross
#define _ABSX_OPT _ABSX_CONST; CHECK_PAGE_CHANGE;
#define _ABSX_OPT_ALT _ABSX_CONST_ALT; CHECK_PAGE_CHANGE;

// Not optimised for page-cross
#define _ABSY_CONST	base = *(LPWORD)(mem+regs.pc); addr = base+(WORD)regs.y; regs.pc += 2;
#define _ABSY_CONST_ALT											\
		base = READ_WORD_ALT(regs.pc);							\
		addr = base + (WORD)regs.y;								\
		regs.pc += 2;

// Optimised for page-cross
#define _ABSY_OPT _ABSY_CONST; CHECK_PAGE_CHANGE;
#define _ABSY_OPT_ALT _ABSY_CONST_ALT; CHECK_PAGE_CHANGE;

// TODO Optimization Note (just for IABSCMOS): uExtraCycles = ((base & 0xFF) + 1) >> 8;
#define _IABS_CMOS	base = *(LPWORD)(mem+regs.pc);				\
		 addr = *(LPWORD)(mem+base);							\
		 if ((base & 0xFF) == 0xFF) uExtraCycles=1;				\
		 regs.pc += 2;
#define _IABS_CMOS_ALT 											\
		base = READ_WORD_ALT(regs.pc);							\
		addr = READ_WORD_ALT(base);								\
		if ((base & 0xFF) == 0xFF) uExtraCycles=1;				\
		regs.pc += 2;

#define _IABS_NMOS	base = *(LPWORD)(mem+regs.pc);				\
		 if ((base & 0xFF) == 0xFF)								\
		       addr = *(mem+base)+((WORD)*(mem+(base&0xFF00))<<8);	\
		 else                                                   \
		       addr = *(LPWORD)(mem+base);						\
		 regs.pc += 2;
#define _IABS_NMOS_ALT											\
		base = READ_WORD_ALT(regs.pc);							\
		if ((base & 0xFF) == 0xFF)								\
			addr = READ_BYTE_ALT(base) | (READ_BYTE_ALT(base&0xFF00)<<8);	\
		else													\
			addr = READ_WORD_ALT(base);							\
		regs.pc += 2;

#define IMM	 addr = regs.pc++;

#define _INDX	base = ((*(mem+regs.pc++))+regs.x) & 0xFF;		\
		 if (base == 0xFF)										\
		     addr = *(mem+0xFF)+(((WORD)*mem)<<8);				\
		 else													\
		     addr = *(LPWORD)(mem+base);
#define _INDX_ALT												\
		base = (READ_BYTE_ALT(regs.pc)+regs.x) & 0xFF; regs.pc++;	\
		if (base == 0xFF)										\
			addr = READ_BYTE_ALT(0xFF) | (READ_BYTE_ALT(0x00)<<8);	\
		else													\
			addr = READ_WORD_ALT(base);

// Not optimised for page-cross
#define _INDY_CONST	if (*(mem+regs.pc) == 0xFF)             /*no extra cycle for page-crossing*/ \
		     base = *(mem+0xFF)+(((WORD)*mem)<<8);				\
		 else													\
		     base = *(LPWORD)(mem+*(mem+regs.pc));				\
		 regs.pc++;												\
		 addr = base+(WORD)regs.y;
#define _INDY_CONST_ALT											\
		base = READ_BYTE_ALT(regs.pc);							\
		if (base == 0xFF)										\
			base = READ_BYTE_ALT(0xFF) | (READ_BYTE_ALT(0x00)<<8);	\
		else													\
			base = READ_WORD_ALT(base);							\
		regs.pc++;												\
		addr = base+(WORD)regs.y;

// Optimised for page-cross
#define _INDY_OPT _INDY_CONST; CHECK_PAGE_CHANGE;
#define _INDY_OPT_ALT _INDY_CONST_ALT; CHECK_PAGE_CHANGE;

#define _IZPG	base = *(mem+regs.pc++);						\
		 if (base == 0xFF)										\
		     addr = *(mem+0xFF)+(((WORD)*mem)<<8);				\
		 else													\
		     addr = *(LPWORD)(mem+base);
#define _IZPG_ALT												\
		base = READ_BYTE_ALT(regs.pc); regs.pc++;				\
		if (base == 0xFF)										\
			addr = READ_BYTE_ALT(0xFF) | (READ_BYTE_ALT(0x00)<<8);	\
		else													\
			addr = READ_WORD_ALT(base);

#define _REL	addr = (signed char)*(mem+regs.pc++);
#define _REL_ALT	addr = (signed char)READ_BYTE_ALT(regs.pc); regs.pc++;

// TODO Optimization Note:
// . Opcodes that generate zero-page addresses can't be accessing $C000..$CFFF
//   so they could be paired with special READZP/WRITEZP macros (instead of READ/WRITE)
#define _ZPG	addr =   *(mem+regs.pc++);
#define _ZPGX	addr = ((*(mem+regs.pc++))+regs.x) & 0xFF;
#define _ZPGY	addr = ((*(mem+regs.pc++))+regs.y) & 0xFF;

#define _ZPG_ALT	addr =  READ_BYTE_ALT(regs.pc); regs.pc++;
#define _ZPGX_ALT	addr = (READ_BYTE_ALT(regs.pc) + regs.x) & 0xFF; regs.pc++;
#define _ZPGY_ALT	addr = (READ_BYTE_ALT(regs.pc) + regs.y) & 0xFF; regs.pc++;

// Tidy 3 char opcodes & addressing modes to keep the opcode table visually aligned, clean, and readable.
#undef asl
#undef lsr
#undef rol
#undef ror

#define asl ASLA
#define lsr LSRA
#define rol ROLA
#define ror RORA

#undef idx
#undef imm
#undef izp
#undef rel
#undef zpx
#undef zpy

#define idx INDX
#define imm IMM
#define izp IZPG
#define rel REL
#define zpx ZPGX
#define zpy ZPGY
