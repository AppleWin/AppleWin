/*** Z80Em: Portable Z80 emulator *******************************************/
/***                                                                      ***/
/***                                 Z80.c                                ***/
/***                                                                      ***/
/*** This file contains the emulation code                                ***/
/***                                                                      ***/
/*** Copyright (C) Marcel de Kogel 1996,1997                              ***/
/***     You are not allowed to distribute this software commercially     ***/
/***     Please, notify me, if you make any changes to this file          ***/
/****************************************************************************/

#include "..\stdafx.h"
#include "Z80.h"

#define M_RDMEM(A)      Z80_RDMEM(A)
#define M_WRMEM(A,V)    Z80_WRMEM(A,V)
#define M_RDOP(A)       Z80_RDOP(A)
#define M_RDOP_ARG(A)   Z80_RDOP_ARG(A)
#define M_RDSTACK(A)    Z80_RDSTACK(A)
#define M_WRSTACK(A,V)  Z80_WRSTACK(A,V)

#define DoIn(lo,hi)     Z80_In((lo)+(((unsigned)(hi))<<8))
#define DoOut(lo,hi,v)  Z80_Out((lo)+(((unsigned)(hi))<<8),v)

static void Interrupt(int j);
static void ei(void);

#define S_FLAG          0x80
#define Z_FLAG          0x40
#define H_FLAG          0x10
#define V_FLAG          0x04
#define N_FLAG          0x02
#define C_FLAG          0x01

#define M_SKIP_CALL     R.PC.W.l+=2
#define M_SKIP_JP       R.PC.W.l+=2
#define M_SKIP_JR       R.PC.W.l+=1
#define M_SKIP_RET

static Z80_Regs R;
//int Z80_Running=1;		// [AppleWin] Not used
//int Z80_IPeriod=50000;	// [AppleWin] Not used
int Z80_ICount=0;			// [AppleWin] Modified from 50000 to 0
#ifdef DEBUG
int Z80_Trace=0;
int Z80_Trap=-1;
#endif
#ifdef TRACE
static unsigned pc_trace[256];
static unsigned pc_count=0;
#endif

static byte PTable[512];
static byte ZSTable[512];
static byte ZSPTable[512];
#include "Z80DAA.h"

typedef void (*opcode_fn) (void);

#define M_C     (R.AF.B.l&C_FLAG)
#define M_NC    (!M_C)
#define M_Z     (R.AF.B.l&Z_FLAG)
#define M_NZ    (!M_Z)
#define M_M     (R.AF.B.l&S_FLAG)
#define M_P     (!M_M)
#define M_PE    (R.AF.B.l&V_FLAG)
#define M_PO    (!M_PE)

/* Get next opcode argument and increment program counter */
INLINE unsigned M_RDMEM_OPCODE (void)
{
 unsigned retval;
 retval=M_RDOP_ARG(R.PC.D);
 R.PC.W.l++;
 return retval;
}

INLINE unsigned M_RDMEM_WORD (dword A)
{
 int i;
 i=M_RDMEM(A);
 i+=M_RDMEM(((A)+1)&0xFFFF)<<8;
 return i;
}

INLINE void M_WRMEM_WORD (dword A,word V)
{
 M_WRMEM (A,V&255);
 M_WRMEM (((A)+1)&0xFFFF,V>>8);
}

INLINE unsigned M_RDMEM_OPCODE_WORD (void)
{
 int i;
 i=M_RDMEM_OPCODE();
 i+=M_RDMEM_OPCODE()<<8;
 return i;
}

#define M_XIX       ((R.IX.D+(offset)M_RDMEM_OPCODE())&0xFFFF)
#define M_XIY       ((R.IY.D+(offset)M_RDMEM_OPCODE())&0xFFFF)
#define M_RD_XHL    M_RDMEM(R.HL.D)
INLINE unsigned M_RD_XIX(void)
{
 int i;
 i=M_XIX;
 return M_RDMEM(i);
}
INLINE unsigned M_RD_XIY(void)
{
 int i;
 i=M_XIY;
 return M_RDMEM(i);
}
INLINE void M_WR_XIX(byte a)
{
 int i;
 i=M_XIX;
 M_WRMEM(i,a);
}
INLINE void M_WR_XIY(byte a)
{
 int i;
 i=M_XIY;
 M_WRMEM(i,a);
}

#ifdef X86_ASM
#include "Z80CDx86.h"
#else
#include "Z80Codes.h"
#endif

static void adc_a_xhl(void) { byte i=M_RD_XHL; M_ADC(i); }
static void adc_a_xix(void) { byte i=M_RD_XIX(); M_ADC(i); }
static void adc_a_xiy(void) { byte i=M_RD_XIY(); M_ADC(i); }
static void adc_a_a(void) { M_ADC(R.AF.B.h); }
static void adc_a_b(void) { M_ADC(R.BC.B.h); }
static void adc_a_c(void) { M_ADC(R.BC.B.l); }
static void adc_a_d(void) { M_ADC(R.DE.B.h); }
static void adc_a_e(void) { M_ADC(R.DE.B.l); }
static void adc_a_h(void) { M_ADC(R.HL.B.h); }
static void adc_a_l(void) { M_ADC(R.HL.B.l); }
static void adc_a_ixl(void) { M_ADC(R.IX.B.l); }
static void adc_a_ixh(void) { M_ADC(R.IX.B.h); }
static void adc_a_iyl(void) { M_ADC(R.IY.B.l); }
static void adc_a_iyh(void) { M_ADC(R.IY.B.h); }
static void adc_a_byte(void) { byte i=M_RDMEM_OPCODE(); M_ADC(i); }

static void adc_hl_bc(void) { M_ADCW(BC); }
static void adc_hl_de(void) { M_ADCW(DE); }
static void adc_hl_hl(void) { M_ADCW(HL); }
static void adc_hl_sp(void) { M_ADCW(SP); }

static void add_a_xhl(void) { byte i=M_RD_XHL; M_ADD(i); }
static void add_a_xix(void) { byte i=M_RD_XIX(); M_ADD(i); }
static void add_a_xiy(void) { byte i=M_RD_XIY(); M_ADD(i); }
static void add_a_a(void) { M_ADD(R.AF.B.h); }
static void add_a_b(void) { M_ADD(R.BC.B.h); }
static void add_a_c(void) { M_ADD(R.BC.B.l); }
static void add_a_d(void) { M_ADD(R.DE.B.h); }
static void add_a_e(void) { M_ADD(R.DE.B.l); }
static void add_a_h(void) { M_ADD(R.HL.B.h); }
static void add_a_l(void) { M_ADD(R.HL.B.l); }
static void add_a_ixl(void) { M_ADD(R.IX.B.l); }
static void add_a_ixh(void) { M_ADD(R.IX.B.h); }
static void add_a_iyl(void) { M_ADD(R.IY.B.l); }
static void add_a_iyh(void) { M_ADD(R.IY.B.h); }
static void add_a_byte(void) { byte i=M_RDMEM_OPCODE(); M_ADD(i); }

static void add_hl_bc(void) { M_ADDW(HL,BC); }
static void add_hl_de(void) { M_ADDW(HL,DE); }
static void add_hl_hl(void) { M_ADDW(HL,HL); }
static void add_hl_sp(void) { M_ADDW(HL,SP); }
static void add_ix_bc(void) { M_ADDW(IX,BC); }
static void add_ix_de(void) { M_ADDW(IX,DE); }
static void add_ix_ix(void) { M_ADDW(IX,IX); }
static void add_ix_sp(void) { M_ADDW(IX,SP); }
static void add_iy_bc(void) { M_ADDW(IY,BC); }
static void add_iy_de(void) { M_ADDW(IY,DE); }
static void add_iy_iy(void) { M_ADDW(IY,IY); }
static void add_iy_sp(void) { M_ADDW(IY,SP); }

static void and_xhl(void) { byte i=M_RD_XHL; M_AND(i); }
static void and_xix(void) { byte i=M_RD_XIX(); M_AND(i); }
static void and_xiy(void) { byte i=M_RD_XIY(); M_AND(i); }
static void and_a(void) { R.AF.B.l=ZSPTable[R.AF.B.h]|H_FLAG; }
static void and_b(void) { M_AND(R.BC.B.h); }
static void and_c(void) { M_AND(R.BC.B.l); }
static void and_d(void) { M_AND(R.DE.B.h); }
static void and_e(void) { M_AND(R.DE.B.l); }
static void and_h(void) { M_AND(R.HL.B.h); }
static void and_l(void) { M_AND(R.HL.B.l); }
static void and_ixh(void) { M_AND(R.IX.B.h); }
static void and_ixl(void) { M_AND(R.IX.B.l); }
static void and_iyh(void) { M_AND(R.IY.B.h); }
static void and_iyl(void) { M_AND(R.IY.B.l); }
static void and_byte(void) { byte i=M_RDMEM_OPCODE(); M_AND(i); }

static void bit_0_xhl(void) { byte i=M_RD_XHL; M_BIT(0,i); }
static void bit_0_xix(void) { byte i=M_RD_XIX(); M_BIT(0,i); }
static void bit_0_xiy(void) { byte i=M_RD_XIY(); M_BIT(0,i); }
static void bit_0_a(void) { M_BIT(0,R.AF.B.h); }
static void bit_0_b(void) { M_BIT(0,R.BC.B.h); }
static void bit_0_c(void) { M_BIT(0,R.BC.B.l); }
static void bit_0_d(void) { M_BIT(0,R.DE.B.h); }
static void bit_0_e(void) { M_BIT(0,R.DE.B.l); }
static void bit_0_h(void) { M_BIT(0,R.HL.B.h); }
static void bit_0_l(void) { M_BIT(0,R.HL.B.l); }

static void bit_1_xhl(void) { byte i=M_RD_XHL; M_BIT(1,i); }
static void bit_1_xix(void) { byte i=M_RD_XIX(); M_BIT(1,i); }
static void bit_1_xiy(void) { byte i=M_RD_XIY(); M_BIT(1,i); }
static void bit_1_a(void) { M_BIT(1,R.AF.B.h); }
static void bit_1_b(void) { M_BIT(1,R.BC.B.h); }
static void bit_1_c(void) { M_BIT(1,R.BC.B.l); }
static void bit_1_d(void) { M_BIT(1,R.DE.B.h); }
static void bit_1_e(void) { M_BIT(1,R.DE.B.l); }
static void bit_1_h(void) { M_BIT(1,R.HL.B.h); }
static void bit_1_l(void) { M_BIT(1,R.HL.B.l); }

static void bit_2_xhl(void) { byte i=M_RD_XHL; M_BIT(2,i); }
static void bit_2_xix(void) { byte i=M_RD_XIX(); M_BIT(2,i); }
static void bit_2_xiy(void) { byte i=M_RD_XIY(); M_BIT(2,i); }
static void bit_2_a(void) { M_BIT(2,R.AF.B.h); }
static void bit_2_b(void) { M_BIT(2,R.BC.B.h); }
static void bit_2_c(void) { M_BIT(2,R.BC.B.l); }
static void bit_2_d(void) { M_BIT(2,R.DE.B.h); }
static void bit_2_e(void) { M_BIT(2,R.DE.B.l); }
static void bit_2_h(void) { M_BIT(2,R.HL.B.h); }
static void bit_2_l(void) { M_BIT(2,R.HL.B.l); }

static void bit_3_xhl(void) { byte i=M_RD_XHL; M_BIT(3,i); }
static void bit_3_xix(void) { byte i=M_RD_XIX(); M_BIT(3,i); }
static void bit_3_xiy(void) { byte i=M_RD_XIY(); M_BIT(3,i); }
static void bit_3_a(void) { M_BIT(3,R.AF.B.h); }
static void bit_3_b(void) { M_BIT(3,R.BC.B.h); }
static void bit_3_c(void) { M_BIT(3,R.BC.B.l); }
static void bit_3_d(void) { M_BIT(3,R.DE.B.h); }
static void bit_3_e(void) { M_BIT(3,R.DE.B.l); }
static void bit_3_h(void) { M_BIT(3,R.HL.B.h); }
static void bit_3_l(void) { M_BIT(3,R.HL.B.l); }

static void bit_4_xhl(void) { byte i=M_RD_XHL; M_BIT(4,i); }
static void bit_4_xix(void) { byte i=M_RD_XIX(); M_BIT(4,i); }
static void bit_4_xiy(void) { byte i=M_RD_XIY(); M_BIT(4,i); }
static void bit_4_a(void) { M_BIT(4,R.AF.B.h); }
static void bit_4_b(void) { M_BIT(4,R.BC.B.h); }
static void bit_4_c(void) { M_BIT(4,R.BC.B.l); }
static void bit_4_d(void) { M_BIT(4,R.DE.B.h); }
static void bit_4_e(void) { M_BIT(4,R.DE.B.l); }
static void bit_4_h(void) { M_BIT(4,R.HL.B.h); }
static void bit_4_l(void) { M_BIT(4,R.HL.B.l); }

static void bit_5_xhl(void) { byte i=M_RD_XHL; M_BIT(5,i); }
static void bit_5_xix(void) { byte i=M_RD_XIX(); M_BIT(5,i); }
static void bit_5_xiy(void) { byte i=M_RD_XIY(); M_BIT(5,i); }
static void bit_5_a(void) { M_BIT(5,R.AF.B.h); }
static void bit_5_b(void) { M_BIT(5,R.BC.B.h); }
static void bit_5_c(void) { M_BIT(5,R.BC.B.l); }
static void bit_5_d(void) { M_BIT(5,R.DE.B.h); }
static void bit_5_e(void) { M_BIT(5,R.DE.B.l); }
static void bit_5_h(void) { M_BIT(5,R.HL.B.h); }
static void bit_5_l(void) { M_BIT(5,R.HL.B.l); }

static void bit_6_xhl(void) { byte i=M_RD_XHL; M_BIT(6,i); }
static void bit_6_xix(void) { byte i=M_RD_XIX(); M_BIT(6,i); }
static void bit_6_xiy(void) { byte i=M_RD_XIY(); M_BIT(6,i); }
static void bit_6_a(void) { M_BIT(6,R.AF.B.h); }
static void bit_6_b(void) { M_BIT(6,R.BC.B.h); }
static void bit_6_c(void) { M_BIT(6,R.BC.B.l); }
static void bit_6_d(void) { M_BIT(6,R.DE.B.h); }
static void bit_6_e(void) { M_BIT(6,R.DE.B.l); }
static void bit_6_h(void) { M_BIT(6,R.HL.B.h); }
static void bit_6_l(void) { M_BIT(6,R.HL.B.l); }

static void bit_7_xhl(void) { byte i=M_RD_XHL; M_BIT(7,i); }
static void bit_7_xix(void) { byte i=M_RD_XIX(); M_BIT(7,i); }
static void bit_7_xiy(void) { byte i=M_RD_XIY(); M_BIT(7,i); }
static void bit_7_a(void) { M_BIT(7,R.AF.B.h); }
static void bit_7_b(void) { M_BIT(7,R.BC.B.h); }
static void bit_7_c(void) { M_BIT(7,R.BC.B.l); }
static void bit_7_d(void) { M_BIT(7,R.DE.B.h); }
static void bit_7_e(void) { M_BIT(7,R.DE.B.l); }
static void bit_7_h(void) { M_BIT(7,R.HL.B.h); }
static void bit_7_l(void) { M_BIT(7,R.HL.B.l); }

static void call_c(void) { if (M_C) { M_CALL; } else { M_SKIP_CALL; } }
static void call_m(void) { if (M_M) { M_CALL; } else { M_SKIP_CALL; } }
static void call_nc(void) { if (M_NC) { M_CALL; } else { M_SKIP_CALL; } }
static void call_nz(void) { if (M_NZ) { M_CALL; } else { M_SKIP_CALL; } }
static void call_p(void) { if (M_P) { M_CALL; } else { M_SKIP_CALL; } }
static void call_pe(void) { if (M_PE) { M_CALL; } else { M_SKIP_CALL; } }
static void call_po(void) { if (M_PO) { M_CALL; } else { M_SKIP_CALL; } }
static void call_z(void) { if (M_Z) { M_CALL; } else { M_SKIP_CALL; } }
static void call(void) { M_CALL; }

static void ccf(void) { R.AF.B.l=((R.AF.B.l&0xED)|((R.AF.B.l&1)<<4))^1; }

static void cp_xhl(void) { byte i=M_RD_XHL; M_CP(i); }
static void cp_xix(void) { byte i=M_RD_XIX(); M_CP(i); }
static void cp_xiy(void) { byte i=M_RD_XIY(); M_CP(i); }
static void cp_a(void) { M_CP(R.AF.B.h); }
static void cp_b(void) { M_CP(R.BC.B.h); }
static void cp_c(void) { M_CP(R.BC.B.l); }
static void cp_d(void) { M_CP(R.DE.B.h); }
static void cp_e(void) { M_CP(R.DE.B.l); }
static void cp_h(void) { M_CP(R.HL.B.h); }
static void cp_l(void) { M_CP(R.HL.B.l); }
static void cp_ixh(void) { M_CP(R.IX.B.h); }
static void cp_ixl(void) { M_CP(R.IX.B.l); }
static void cp_iyh(void) { M_CP(R.IY.B.h); }
static void cp_iyl(void) { M_CP(R.IY.B.l); }
static void cp_byte(void) { byte i=M_RDMEM_OPCODE(); M_CP(i); }

static void cpd(void)
{
 byte i,j;
 i=M_RDMEM(R.HL.D);
 j=R.AF.B.h-i;
 --R.HL.W.l;
 --R.BC.W.l;
 R.AF.B.l=(R.AF.B.l&C_FLAG)|ZSTable[j]|
          ((R.AF.B.h^i^j)&H_FLAG)|(R.BC.D? V_FLAG:0)|N_FLAG;
}

static void cpdr(void)
{
 cpd ();
 if (R.BC.D && !(R.AF.B.l&Z_FLAG)) { Z80_ICount += 5; R.PC.W.l-=2; }
}

static void cpi(void)
{
 byte i,j;
 i=M_RDMEM(R.HL.D);
 j=R.AF.B.h-i;
 ++R.HL.W.l;
 --R.BC.W.l;
 R.AF.B.l=(R.AF.B.l&C_FLAG)|ZSTable[j]|
          ((R.AF.B.h^i^j)&H_FLAG)|(R.BC.D? V_FLAG:0)|N_FLAG;
}

static void cpir(void)
{
 cpi ();
 if (R.BC.D && !(R.AF.B.l&Z_FLAG)) { Z80_ICount += 5; R.PC.W.l-=2; }
}

static void cpl(void) { R.AF.B.h^=0xFF; R.AF.B.l|=(H_FLAG|N_FLAG); }

static void daa(void)
{
 int i;
 i=R.AF.B.h;
 if (R.AF.B.l&C_FLAG) i|=256;
 if (R.AF.B.l&H_FLAG) i|=512;
 if (R.AF.B.l&N_FLAG) i|=1024;
 R.AF.W.l=DAATable[i];
};

static void dec_xhl(void)
{
 byte i;
 i=M_RDMEM(R.HL.D);
 M_DEC(i);
 M_WRMEM(R.HL.D,i);
}
static void dec_xix(void)
{
 byte i;
 int j;
 j=M_XIX;
 i=M_RDMEM(j);
 M_DEC(i);
 M_WRMEM(j,i);
}
static void dec_xiy(void)
{
 byte i;
 int j;
 j=M_XIY;
 i=M_RDMEM(j);
 M_DEC(i);
 M_WRMEM(j,i);
}
static void dec_a(void) { M_DEC(R.AF.B.h); }
static void dec_b(void) { M_DEC(R.BC.B.h); }
static void dec_c(void) { M_DEC(R.BC.B.l); }
static void dec_d(void) { M_DEC(R.DE.B.h); }
static void dec_e(void) { M_DEC(R.DE.B.l); }
static void dec_h(void) { M_DEC(R.HL.B.h); }
static void dec_l(void) { M_DEC(R.HL.B.l); }
static void dec_ixh(void) { M_DEC(R.IX.B.h); }
static void dec_ixl(void) { M_DEC(R.IX.B.l); }
static void dec_iyh(void) { M_DEC(R.IY.B.h); }
static void dec_iyl(void) { M_DEC(R.IY.B.l); }

static void dec_bc(void) { --R.BC.W.l; }
static void dec_de(void) { --R.DE.W.l; }
static void dec_hl(void) { --R.HL.W.l; }
static void dec_ix(void) { --R.IX.W.l; }
static void dec_iy(void) { --R.IY.W.l; }
static void dec_sp(void) { --R.SP.W.l; }

static void di(void) { R.IFF1=R.IFF2=0; }

static void djnz(void) { if (--R.BC.B.h) { M_JR; } else { M_SKIP_JR; } }

static void ex_xsp_hl(void)
{
 int i;
 i=M_RDMEM_WORD(R.SP.D);
 M_WRMEM_WORD(R.SP.D,R.HL.D);
 R.HL.D=i;
}

static void ex_xsp_ix(void)
{
 int i;
 i=M_RDMEM_WORD(R.SP.D);
 M_WRMEM_WORD(R.SP.D,R.IX.D);
 R.IX.D=i;
}

static void ex_xsp_iy(void)
{
 int i;
 i=M_RDMEM_WORD(R.SP.D);
 M_WRMEM_WORD(R.SP.D,R.IY.D);
 R.IY.D=i;
}

static void ex_af_af(void)
{
 int i;
 i=R.AF.D;
 R.AF.D=R.AF2.D;
 R.AF2.D=i;
}

static void ex_de_hl(void)
{
 int i;
 i=R.DE.D;
 R.DE.D=R.HL.D;
 R.HL.D=i;
}

static void exx(void)
{
 int i;
 i=R.BC.D;
 R.BC.D=R.BC2.D;
 R.BC2.D=i;
 i=R.DE.D;
 R.DE.D=R.DE2.D;
 R.DE2.D=i;
 i=R.HL.D;
 R.HL.D=R.HL2.D;
 R.HL2.D=i;
}

static void halt(void)
{
 --R.PC.W.l;
 R.HALT=1;
 //if (Z80_ICount>0) Z80_ICount=0; // [AppleWin] Commented out
}

static void im_0(void) { R.IM=0; }
static void im_1(void) { R.IM=1; }
static void im_2(void) { R.IM=2; }

static void in_a_c(void) { M_IN(R.AF.B.h); }
static void in_b_c(void) { M_IN(R.BC.B.h); }
static void in_c_c(void) { M_IN(R.BC.B.l); }
static void in_d_c(void) { M_IN(R.DE.B.h); }
static void in_e_c(void) { M_IN(R.DE.B.l); }
static void in_h_c(void) { M_IN(R.HL.B.h); }
static void in_l_c(void) { M_IN(R.HL.B.l); }
static void in_0_c(void) { byte i; M_IN(i); }

static void in_a_byte(void)
{
 byte i=M_RDMEM_OPCODE();
 R.AF.B.h=DoIn(i,R.AF.B.h);
}

static void inc_xhl(void)
{
 byte i;
 i=M_RDMEM(R.HL.D);
 M_INC(i);
 M_WRMEM(R.HL.D,i);
}
static void inc_xix(void)
{
 byte i;
 int j;
 j=M_XIX;
 i=M_RDMEM(j);
 M_INC(i);
 M_WRMEM(j,i);
}
static void inc_xiy(void)
{
 byte i;
 int j;
 j=M_XIY;
 i=M_RDMEM(j);
 M_INC(i);
 M_WRMEM(j,i);
}
static void inc_a(void) { M_INC(R.AF.B.h); }
static void inc_b(void) { M_INC(R.BC.B.h); }
static void inc_c(void) { M_INC(R.BC.B.l); }
static void inc_d(void) { M_INC(R.DE.B.h); }
static void inc_e(void) { M_INC(R.DE.B.l); }
static void inc_h(void) { M_INC(R.HL.B.h); }
static void inc_l(void) { M_INC(R.HL.B.l); }
static void inc_ixh(void) { M_INC(R.IX.B.h); }
static void inc_ixl(void) { M_INC(R.IX.B.l); }
static void inc_iyh(void) { M_INC(R.IY.B.h); }
static void inc_iyl(void) { M_INC(R.IY.B.l); }

static void inc_bc(void) { ++R.BC.W.l; }
static void inc_de(void) { ++R.DE.W.l; }
static void inc_hl(void) { ++R.HL.W.l; }
static void inc_ix(void) { ++R.IX.W.l; }
static void inc_iy(void) { ++R.IY.W.l; }
static void inc_sp(void) { ++R.SP.W.l; }

static void ind(void)
{
 --R.BC.B.h;
 M_WRMEM(R.HL.D,DoIn(R.BC.B.l,R.BC.B.h));
 --R.HL.W.l;
 R.AF.B.l=(R.BC.B.h)? N_FLAG:(N_FLAG|Z_FLAG);
}

static void indr(void)
{
 ind ();
 if (R.BC.B.h) { Z80_ICount += 5; R.PC.W.l-=2; }
}

static void ini(void)
{
 --R.BC.B.h;
 M_WRMEM(R.HL.D,DoIn(R.BC.B.l,R.BC.B.h));
 ++R.HL.W.l;
 R.AF.B.l=(R.BC.B.h)? N_FLAG:(N_FLAG|Z_FLAG);
}

static void inir(void)
{
 ini ();
 if (R.BC.B.h) { Z80_ICount+=5; R.PC.W.l-=2; }
}

static void jp(void) { M_JP; }
static void jp_hl(void) { R.PC.D=R.HL.D; }
static void jp_ix(void) { R.PC.D=R.IX.D; }
static void jp_iy(void) { R.PC.D=R.IY.D; }
static void jp_c(void) { if (M_C) { M_JP; } else { M_SKIP_JP; } }
static void jp_m(void) { if (M_M) { M_JP; } else { M_SKIP_JP; } }
static void jp_nc(void) { if (M_NC) { M_JP; } else { M_SKIP_JP; } }
static void jp_nz(void) { if (M_NZ) { M_JP; } else { M_SKIP_JP; } }
static void jp_p(void) { if (M_P) { M_JP; } else { M_SKIP_JP; } }
static void jp_pe(void) { if (M_PE) { M_JP; } else { M_SKIP_JP; } }
static void jp_po(void) { if (M_PO) { M_JP; } else { M_SKIP_JP; } }
static void jp_z(void) { if (M_Z) { M_JP; } else { M_SKIP_JP; } }

static void jr(void) { M_JR; }
static void jr_c(void) { if (M_C) { M_JR; } else { M_SKIP_JR; } }
static void jr_nc(void) { if (M_NC) { M_JR; } else { M_SKIP_JR; } }
static void jr_nz(void) { if (M_NZ) { M_JR; } else { M_SKIP_JR; } }
static void jr_z(void) { if (M_Z) { M_JR; } else { M_SKIP_JR; } }

static void ld_xbc_a(void) { M_WRMEM(R.BC.D,R.AF.B.h); }
static void ld_xde_a(void) { M_WRMEM(R.DE.D,R.AF.B.h); }
static void ld_xhl_a(void) { M_WRMEM(R.HL.D,R.AF.B.h); }
static void ld_xhl_b(void) { M_WRMEM(R.HL.D,R.BC.B.h); }
static void ld_xhl_c(void) { M_WRMEM(R.HL.D,R.BC.B.l); }
static void ld_xhl_d(void) { M_WRMEM(R.HL.D,R.DE.B.h); }
static void ld_xhl_e(void) { M_WRMEM(R.HL.D,R.DE.B.l); }
static void ld_xhl_h(void) { M_WRMEM(R.HL.D,R.HL.B.h); }
static void ld_xhl_l(void) { M_WRMEM(R.HL.D,R.HL.B.l); }
static void ld_xhl_byte(void) { byte i=M_RDMEM_OPCODE(); M_WRMEM(R.HL.D,i); }
static void ld_xix_a(void) { M_WR_XIX(R.AF.B.h); }
static void ld_xix_b(void) { M_WR_XIX(R.BC.B.h); }
static void ld_xix_c(void) { M_WR_XIX(R.BC.B.l); }
static void ld_xix_d(void) { M_WR_XIX(R.DE.B.h); }
static void ld_xix_e(void) { M_WR_XIX(R.DE.B.l); }
static void ld_xix_h(void) { M_WR_XIX(R.HL.B.h); }
static void ld_xix_l(void) { M_WR_XIX(R.HL.B.l); }
static void ld_xix_byte(void)
{
 int i,j;
 i=M_XIX;
 j=M_RDMEM_OPCODE();
 M_WRMEM(i,j);
}
static void ld_xiy_a(void) { M_WR_XIY(R.AF.B.h); }
static void ld_xiy_b(void) { M_WR_XIY(R.BC.B.h); }
static void ld_xiy_c(void) { M_WR_XIY(R.BC.B.l); }
static void ld_xiy_d(void) { M_WR_XIY(R.DE.B.h); }
static void ld_xiy_e(void) { M_WR_XIY(R.DE.B.l); }
static void ld_xiy_h(void) { M_WR_XIY(R.HL.B.h); }
static void ld_xiy_l(void) { M_WR_XIY(R.HL.B.l); }
static void ld_xiy_byte(void)
{
 int i,j;
 i=M_XIY;
 j=M_RDMEM_OPCODE();
 M_WRMEM(i,j);
}
static void ld_xbyte_a(void)
{ int i=M_RDMEM_OPCODE_WORD(); M_WRMEM(i,R.AF.B.h); }
static void ld_xword_bc(void) { M_WRMEM_WORD(M_RDMEM_OPCODE_WORD(),R.BC.D); }
static void ld_xword_de(void) { M_WRMEM_WORD(M_RDMEM_OPCODE_WORD(),R.DE.D); }
static void ld_xword_hl(void) { M_WRMEM_WORD(M_RDMEM_OPCODE_WORD(),R.HL.D); }
static void ld_xword_ix(void) { M_WRMEM_WORD(M_RDMEM_OPCODE_WORD(),R.IX.D); }
static void ld_xword_iy(void) { M_WRMEM_WORD(M_RDMEM_OPCODE_WORD(),R.IY.D); }
static void ld_xword_sp(void) { M_WRMEM_WORD(M_RDMEM_OPCODE_WORD(),R.SP.D); }
static void ld_a_xbc(void) { R.AF.B.h=M_RDMEM(R.BC.D); }
static void ld_a_xde(void) { R.AF.B.h=M_RDMEM(R.DE.D); }
static void ld_a_xhl(void) { R.AF.B.h=M_RD_XHL; }
static void ld_a_xix(void) { R.AF.B.h=M_RD_XIX(); }
static void ld_a_xiy(void) { R.AF.B.h=M_RD_XIY(); }
static void ld_a_xbyte(void)
{ int i=M_RDMEM_OPCODE_WORD(); R.AF.B.h=M_RDMEM(i); }

static void ld_a_byte(void) { R.AF.B.h=M_RDMEM_OPCODE(); }
static void ld_b_byte(void) { R.BC.B.h=M_RDMEM_OPCODE(); }
static void ld_c_byte(void) { R.BC.B.l=M_RDMEM_OPCODE(); }
static void ld_d_byte(void) { R.DE.B.h=M_RDMEM_OPCODE(); }
static void ld_e_byte(void) { R.DE.B.l=M_RDMEM_OPCODE(); }
static void ld_h_byte(void) { R.HL.B.h=M_RDMEM_OPCODE(); }
static void ld_l_byte(void) { R.HL.B.l=M_RDMEM_OPCODE(); }
static void ld_ixh_byte(void) { R.IX.B.h=M_RDMEM_OPCODE(); }
static void ld_ixl_byte(void) { R.IX.B.l=M_RDMEM_OPCODE(); }
static void ld_iyh_byte(void) { R.IY.B.h=M_RDMEM_OPCODE(); }
static void ld_iyl_byte(void) { R.IY.B.l=M_RDMEM_OPCODE(); }

static void ld_b_xhl(void) { R.BC.B.h=M_RD_XHL; }
static void ld_c_xhl(void) { R.BC.B.l=M_RD_XHL; }
static void ld_d_xhl(void) { R.DE.B.h=M_RD_XHL; }
static void ld_e_xhl(void) { R.DE.B.l=M_RD_XHL; }
static void ld_h_xhl(void) { R.HL.B.h=M_RD_XHL; }
static void ld_l_xhl(void) { R.HL.B.l=M_RD_XHL; }
static void ld_b_xix(void) { R.BC.B.h=M_RD_XIX(); }
static void ld_c_xix(void) { R.BC.B.l=M_RD_XIX(); }
static void ld_d_xix(void) { R.DE.B.h=M_RD_XIX(); }
static void ld_e_xix(void) { R.DE.B.l=M_RD_XIX(); }
static void ld_h_xix(void) { R.HL.B.h=M_RD_XIX(); }
static void ld_l_xix(void) { R.HL.B.l=M_RD_XIX(); }
static void ld_b_xiy(void) { R.BC.B.h=M_RD_XIY(); }
static void ld_c_xiy(void) { R.BC.B.l=M_RD_XIY(); }
static void ld_d_xiy(void) { R.DE.B.h=M_RD_XIY(); }
static void ld_e_xiy(void) { R.DE.B.l=M_RD_XIY(); }
static void ld_h_xiy(void) { R.HL.B.h=M_RD_XIY(); }
static void ld_l_xiy(void) { R.HL.B.l=M_RD_XIY(); }
static void ld_a_a(void) { }
static void ld_a_b(void) { R.AF.B.h=R.BC.B.h; }
static void ld_a_c(void) { R.AF.B.h=R.BC.B.l; }
static void ld_a_d(void) { R.AF.B.h=R.DE.B.h; }
static void ld_a_e(void) { R.AF.B.h=R.DE.B.l; }
static void ld_a_h(void) { R.AF.B.h=R.HL.B.h; }
static void ld_a_l(void) { R.AF.B.h=R.HL.B.l; }
static void ld_a_ixh(void) { R.AF.B.h=R.IX.B.h; }
static void ld_a_ixl(void) { R.AF.B.h=R.IX.B.l; }
static void ld_a_iyh(void) { R.AF.B.h=R.IY.B.h; }
static void ld_a_iyl(void) { R.AF.B.h=R.IY.B.l; }
static void ld_b_b(void) { }
static void ld_b_a(void) { R.BC.B.h=R.AF.B.h; }
static void ld_b_c(void) { R.BC.B.h=R.BC.B.l; }
static void ld_b_d(void) { R.BC.B.h=R.DE.B.h; }
static void ld_b_e(void) { R.BC.B.h=R.DE.B.l; }
static void ld_b_h(void) { R.BC.B.h=R.HL.B.h; }
static void ld_b_l(void) { R.BC.B.h=R.HL.B.l; }
static void ld_b_ixh(void) { R.BC.B.h=R.IX.B.h; }
static void ld_b_ixl(void) { R.BC.B.h=R.IX.B.l; }
static void ld_b_iyh(void) { R.BC.B.h=R.IY.B.h; }
static void ld_b_iyl(void) { R.BC.B.h=R.IY.B.l; }
static void ld_c_c(void) { }
static void ld_c_a(void) { R.BC.B.l=R.AF.B.h; }
static void ld_c_b(void) { R.BC.B.l=R.BC.B.h; }
static void ld_c_d(void) { R.BC.B.l=R.DE.B.h; }
static void ld_c_e(void) { R.BC.B.l=R.DE.B.l; }
static void ld_c_h(void) { R.BC.B.l=R.HL.B.h; }
static void ld_c_l(void) { R.BC.B.l=R.HL.B.l; }
static void ld_c_ixh(void) { R.BC.B.l=R.IX.B.h; }
static void ld_c_ixl(void) { R.BC.B.l=R.IX.B.l; }
static void ld_c_iyh(void) { R.BC.B.l=R.IY.B.h; }
static void ld_c_iyl(void) { R.BC.B.l=R.IY.B.l; }
static void ld_d_d(void) { }
static void ld_d_a(void) { R.DE.B.h=R.AF.B.h; }
static void ld_d_c(void) { R.DE.B.h=R.BC.B.l; }
static void ld_d_b(void) { R.DE.B.h=R.BC.B.h; }
static void ld_d_e(void) { R.DE.B.h=R.DE.B.l; }
static void ld_d_h(void) { R.DE.B.h=R.HL.B.h; }
static void ld_d_l(void) { R.DE.B.h=R.HL.B.l; }
static void ld_d_ixh(void) { R.DE.B.h=R.IX.B.h; }
static void ld_d_ixl(void) { R.DE.B.h=R.IX.B.l; }
static void ld_d_iyh(void) { R.DE.B.h=R.IY.B.h; }
static void ld_d_iyl(void) { R.DE.B.h=R.IY.B.l; }
static void ld_e_e(void) { }
static void ld_e_a(void) { R.DE.B.l=R.AF.B.h; }
static void ld_e_c(void) { R.DE.B.l=R.BC.B.l; }
static void ld_e_b(void) { R.DE.B.l=R.BC.B.h; }
static void ld_e_d(void) { R.DE.B.l=R.DE.B.h; }
static void ld_e_h(void) { R.DE.B.l=R.HL.B.h; }
static void ld_e_l(void) { R.DE.B.l=R.HL.B.l; }
static void ld_e_ixh(void) { R.DE.B.l=R.IX.B.h; }
static void ld_e_ixl(void) { R.DE.B.l=R.IX.B.l; }
static void ld_e_iyh(void) { R.DE.B.l=R.IY.B.h; }
static void ld_e_iyl(void) { R.DE.B.l=R.IY.B.l; }
static void ld_h_h(void) { }
static void ld_h_a(void) { R.HL.B.h=R.AF.B.h; }
static void ld_h_c(void) { R.HL.B.h=R.BC.B.l; }
static void ld_h_b(void) { R.HL.B.h=R.BC.B.h; }
static void ld_h_e(void) { R.HL.B.h=R.DE.B.l; }
static void ld_h_d(void) { R.HL.B.h=R.DE.B.h; }
static void ld_h_l(void) { R.HL.B.h=R.HL.B.l; }
static void ld_l_l(void) { }
static void ld_l_a(void) { R.HL.B.l=R.AF.B.h; }
static void ld_l_c(void) { R.HL.B.l=R.BC.B.l; }
static void ld_l_b(void) { R.HL.B.l=R.BC.B.h; }
static void ld_l_e(void) { R.HL.B.l=R.DE.B.l; }
static void ld_l_d(void) { R.HL.B.l=R.DE.B.h; }
static void ld_l_h(void) { R.HL.B.l=R.HL.B.h; }
static void ld_ixh_a(void) { R.IX.B.h=R.AF.B.h; }
static void ld_ixh_b(void) { R.IX.B.h=R.BC.B.h; }
static void ld_ixh_c(void) { R.IX.B.h=R.BC.B.l; }
static void ld_ixh_d(void) { R.IX.B.h=R.DE.B.h; }
static void ld_ixh_e(void) { R.IX.B.h=R.DE.B.l; }
static void ld_ixh_ixh(void) { }
static void ld_ixh_ixl(void) { R.IX.B.h=R.IX.B.l; }
static void ld_ixl_a(void) { R.IX.B.l=R.AF.B.h; }
static void ld_ixl_b(void) { R.IX.B.l=R.BC.B.h; }
static void ld_ixl_c(void) { R.IX.B.l=R.BC.B.l; }
static void ld_ixl_d(void) { R.IX.B.l=R.DE.B.h; }
static void ld_ixl_e(void) { R.IX.B.l=R.DE.B.l; }
static void ld_ixl_ixh(void) { R.IX.B.l=R.IX.B.h; }
static void ld_ixl_ixl(void) { }
static void ld_iyh_a(void) { R.IY.B.h=R.AF.B.h; }
static void ld_iyh_b(void) { R.IY.B.h=R.BC.B.h; }
static void ld_iyh_c(void) { R.IY.B.h=R.BC.B.l; }
static void ld_iyh_d(void) { R.IY.B.h=R.DE.B.h; }
static void ld_iyh_e(void) { R.IY.B.h=R.DE.B.l; }
static void ld_iyh_iyh(void) { }
static void ld_iyh_iyl(void) { R.IY.B.h=R.IY.B.l; }
static void ld_iyl_a(void) { R.IY.B.l=R.AF.B.h; }
static void ld_iyl_b(void) { R.IY.B.l=R.BC.B.h; }
static void ld_iyl_c(void) { R.IY.B.l=R.BC.B.l; }
static void ld_iyl_d(void) { R.IY.B.l=R.DE.B.h; }
static void ld_iyl_e(void) { R.IY.B.l=R.DE.B.l; }
static void ld_iyl_iyh(void) { R.IY.B.l=R.IY.B.h; }
static void ld_iyl_iyl(void) { }
static void ld_bc_xword(void) { R.BC.D=M_RDMEM_WORD(M_RDMEM_OPCODE_WORD()); }
static void ld_bc_word(void) { R.BC.D=M_RDMEM_OPCODE_WORD(); }
static void ld_de_xword(void) { R.DE.D=M_RDMEM_WORD(M_RDMEM_OPCODE_WORD()); }
static void ld_de_word(void) { R.DE.D=M_RDMEM_OPCODE_WORD(); }
static void ld_hl_xword(void) { R.HL.D=M_RDMEM_WORD(M_RDMEM_OPCODE_WORD()); }
static void ld_hl_word(void) { R.HL.D=M_RDMEM_OPCODE_WORD(); }
static void ld_ix_xword(void) { R.IX.D=M_RDMEM_WORD(M_RDMEM_OPCODE_WORD()); }
static void ld_ix_word(void) { R.IX.D=M_RDMEM_OPCODE_WORD(); }
static void ld_iy_xword(void) { R.IY.D=M_RDMEM_WORD(M_RDMEM_OPCODE_WORD()); }
static void ld_iy_word(void) { R.IY.D=M_RDMEM_OPCODE_WORD(); }
static void ld_sp_xword(void) { R.SP.D=M_RDMEM_WORD(M_RDMEM_OPCODE_WORD()); }
static void ld_sp_word(void) { R.SP.D=M_RDMEM_OPCODE_WORD(); }
static void ld_sp_hl(void) { R.SP.D=R.HL.D; }
static void ld_sp_ix(void) { R.SP.D=R.IX.D; }
static void ld_sp_iy(void) { R.SP.D=R.IY.D; }
static void ld_a_i(void)
{
 R.AF.B.h=R.I;
 R.AF.B.l=(R.AF.B.l&C_FLAG)|ZSTable[R.I]|(R.IFF2<<2);
}
static void ld_i_a(void) { R.I=R.AF.B.h; }
static void ld_a_r(void)
{
 R.AF.B.h=(R.R&127)|(R.R2&128);
 R.AF.B.l=(R.AF.B.l&C_FLAG)|ZSTable[R.AF.B.h]|(R.IFF2<<2);
}
static void ld_r_a(void) { R.R=R.R2=R.AF.B.h; }

static void ldd(void)
{
 M_WRMEM(R.DE.D,M_RDMEM(R.HL.D));
 --R.DE.W.l;
 --R.HL.W.l;
 --R.BC.W.l;
 R.AF.B.l=(R.AF.B.l&0xE9)|(R.BC.D? V_FLAG:0);
}
static void lddr(void)
{
 ldd ();
 if (R.BC.D) { Z80_ICount+=5; R.PC.W.l-=2; }
}
static void ldi(void)
{
 M_WRMEM(R.DE.D,M_RDMEM(R.HL.D));
 ++R.DE.W.l;
 ++R.HL.W.l;
 --R.BC.W.l;
 R.AF.B.l=(R.AF.B.l&0xE9)|(R.BC.D? V_FLAG:0);
}
static void ldir(void)
{
 ldi ();
 if (R.BC.D) { Z80_ICount+=5; R.PC.W.l-=2; }
}

static void neg(void)
{
 byte i;
 i=R.AF.B.h;
 R.AF.B.h=0;
 M_SUB(i);
}

static void nop(void) { };

static void or_xhl(void) { byte i=M_RD_XHL; M_OR(i); }
static void or_xix(void) { byte i=M_RD_XIX(); M_OR(i); }
static void or_xiy(void) { byte i=M_RD_XIY(); M_OR(i); }
static void or_a(void) { R.AF.B.l=ZSPTable[R.AF.B.h]; }
static void or_b(void) { M_OR(R.BC.B.h); }
static void or_c(void) { M_OR(R.BC.B.l); }
static void or_d(void) { M_OR(R.DE.B.h); }
static void or_e(void) { M_OR(R.DE.B.l); }
static void or_h(void) { M_OR(R.HL.B.h); }
static void or_l(void) { M_OR(R.HL.B.l); }
static void or_ixh(void) { M_OR(R.IX.B.h); }
static void or_ixl(void) { M_OR(R.IX.B.l); }
static void or_iyh(void) { M_OR(R.IY.B.h); }
static void or_iyl(void) { M_OR(R.IY.B.l); }
static void or_byte(void) { byte i=M_RDMEM_OPCODE(); M_OR(i); }

static void outd(void)
{
 --R.BC.B.h;
 DoOut (R.BC.B.l,R.BC.B.h,M_RDMEM(R.HL.D));
 --R.HL.W.l;
 R.AF.B.l=(R.BC.B.h)? N_FLAG:(Z_FLAG|N_FLAG);
}
static void otdr(void)
{
 outd ();
 if (R.BC.B.h) { Z80_ICount+=5; R.PC.W.l-=2; }
}
static void outi(void)
{
 --R.BC.B.h;
 DoOut (R.BC.B.l,R.BC.B.h,M_RDMEM(R.HL.D));
 ++R.HL.W.l;
 R.AF.B.l=(R.BC.B.h)? N_FLAG:(Z_FLAG|N_FLAG);
}
static void otir(void)
{
 outi ();
 if (R.BC.B.h) { Z80_ICount+=5; R.PC.W.l-=2; }
}

static void out_c_a(void) { DoOut(R.BC.B.l,R.BC.B.h,R.AF.B.h); }
static void out_c_b(void) { DoOut(R.BC.B.l,R.BC.B.h,R.BC.B.h); }
static void out_c_c(void) { DoOut(R.BC.B.l,R.BC.B.h,R.BC.B.l); }
static void out_c_d(void) { DoOut(R.BC.B.l,R.BC.B.h,R.DE.B.h); }
static void out_c_e(void) { DoOut(R.BC.B.l,R.BC.B.h,R.DE.B.l); }
static void out_c_h(void) { DoOut(R.BC.B.l,R.BC.B.h,R.HL.B.h); }
static void out_c_l(void) { DoOut(R.BC.B.l,R.BC.B.h,R.HL.B.l); }
static void out_c_0(void) { DoOut(R.BC.B.l,R.BC.B.h,0); }
static void out_byte_a(void)
{
 byte i=M_RDMEM_OPCODE();
 DoOut(i,R.AF.B.h,R.AF.B.h);
}

static void pop_af(void) { M_POP(AF); }
static void pop_bc(void) { M_POP(BC); }
static void pop_de(void) { M_POP(DE); }
static void pop_hl(void) { M_POP(HL); }
static void pop_ix(void) { M_POP(IX); }
static void pop_iy(void) { M_POP(IY); }

static void push_af(void) { M_PUSH(AF); }
static void push_bc(void) { M_PUSH(BC); }
static void push_de(void) { M_PUSH(DE); }
static void push_hl(void) { M_PUSH(HL); }
static void push_ix(void) { M_PUSH(IX); }
static void push_iy(void) { M_PUSH(IY); }

static void res_0_xhl(void)
{
 byte i;
 i=M_RDMEM(R.HL.D);
 M_RES(0,i);
 M_WRMEM(R.HL.D,i);
};
static void res_0_xix(void)
{
 byte i;
 int j;
 j=M_XIX;
 i=M_RDMEM(j);
 M_RES(0,i);
 M_WRMEM(j,i);
};
static void res_0_xiy(void)
{
 byte i;
 int j;
 j=M_XIY;
 i=M_RDMEM(j);
 M_RES(0,i);
 M_WRMEM(j,i);
};
static void res_0_a(void) { M_RES(0,R.AF.B.h); };
static void res_0_b(void) { M_RES(0,R.BC.B.h); };
static void res_0_c(void) { M_RES(0,R.BC.B.l); };
static void res_0_d(void) { M_RES(0,R.DE.B.h); };
static void res_0_e(void) { M_RES(0,R.DE.B.l); };
static void res_0_h(void) { M_RES(0,R.HL.B.h); };
static void res_0_l(void) { M_RES(0,R.HL.B.l); };

static void res_1_xhl(void)
{
 byte i;
 i=M_RDMEM(R.HL.D);
 M_RES(1,i);
 M_WRMEM(R.HL.D,i);
};
static void res_1_xix(void)
{
 byte i;
 int j;
 j=M_XIX;
 i=M_RDMEM(j);
 M_RES(1,i);
 M_WRMEM(j,i);
};
static void res_1_xiy(void)
{
 byte i;
 int j;
 j=M_XIY;
 i=M_RDMEM(j);
 M_RES(1,i);
 M_WRMEM(j,i);
};
static void res_1_a(void) { M_RES(1,R.AF.B.h); };
static void res_1_b(void) { M_RES(1,R.BC.B.h); };
static void res_1_c(void) { M_RES(1,R.BC.B.l); };
static void res_1_d(void) { M_RES(1,R.DE.B.h); };
static void res_1_e(void) { M_RES(1,R.DE.B.l); };
static void res_1_h(void) { M_RES(1,R.HL.B.h); };
static void res_1_l(void) { M_RES(1,R.HL.B.l); };

static void res_2_xhl(void)
{
 byte i;
 i=M_RDMEM(R.HL.D);
 M_RES(2,i);
 M_WRMEM(R.HL.D,i);
};
static void res_2_xix(void)
{
 byte i;
 int j;
 j=M_XIX;
 i=M_RDMEM(j);
 M_RES(2,i);
 M_WRMEM(j,i);
};
static void res_2_xiy(void)
{
 byte i;
 int j;
 j=M_XIY;
 i=M_RDMEM(j);
 M_RES(2,i);
 M_WRMEM(j,i);
};
static void res_2_a(void) { M_RES(2,R.AF.B.h); };
static void res_2_b(void) { M_RES(2,R.BC.B.h); };
static void res_2_c(void) { M_RES(2,R.BC.B.l); };
static void res_2_d(void) { M_RES(2,R.DE.B.h); };
static void res_2_e(void) { M_RES(2,R.DE.B.l); };
static void res_2_h(void) { M_RES(2,R.HL.B.h); };
static void res_2_l(void) { M_RES(2,R.HL.B.l); };

static void res_3_xhl(void)
{
 byte i;
 i=M_RDMEM(R.HL.D);
 M_RES(3,i);
 M_WRMEM(R.HL.D,i);
};
static void res_3_xix(void)
{
 byte i;
 int j;
 j=M_XIX;
 i=M_RDMEM(j);
 M_RES(3,i);
 M_WRMEM(j,i);
};
static void res_3_xiy(void)
{
 byte i;
 int j;
 j=M_XIY;
 i=M_RDMEM(j);
 M_RES(3,i);
 M_WRMEM(j,i);
};
static void res_3_a(void) { M_RES(3,R.AF.B.h); };
static void res_3_b(void) { M_RES(3,R.BC.B.h); };
static void res_3_c(void) { M_RES(3,R.BC.B.l); };
static void res_3_d(void) { M_RES(3,R.DE.B.h); };
static void res_3_e(void) { M_RES(3,R.DE.B.l); };
static void res_3_h(void) { M_RES(3,R.HL.B.h); };
static void res_3_l(void) { M_RES(3,R.HL.B.l); };

static void res_4_xhl(void)
{
 byte i;
 i=M_RDMEM(R.HL.D);
 M_RES(4,i);
 M_WRMEM(R.HL.D,i);
};
static void res_4_xix(void)
{
 byte i;
 int j;
 j=M_XIX;
 i=M_RDMEM(j);
 M_RES(4,i);
 M_WRMEM(j,i);
};
static void res_4_xiy(void)
{
 byte i;
 int j;
 j=M_XIY;
 i=M_RDMEM(j);
 M_RES(4,i);
 M_WRMEM(j,i);
};
static void res_4_a(void) { M_RES(4,R.AF.B.h); };
static void res_4_b(void) { M_RES(4,R.BC.B.h); };
static void res_4_c(void) { M_RES(4,R.BC.B.l); };
static void res_4_d(void) { M_RES(4,R.DE.B.h); };
static void res_4_e(void) { M_RES(4,R.DE.B.l); };
static void res_4_h(void) { M_RES(4,R.HL.B.h); };
static void res_4_l(void) { M_RES(4,R.HL.B.l); };

static void res_5_xhl(void)
{
 byte i;
 i=M_RDMEM(R.HL.D);
 M_RES(5,i);
 M_WRMEM(R.HL.D,i);
};
static void res_5_xix(void)
{
 byte i;
 int j;
 j=M_XIX;
 i=M_RDMEM(j);
 M_RES(5,i);
 M_WRMEM(j,i);
};
static void res_5_xiy(void)
{
 byte i;
 int j;
 j=M_XIY;
 i=M_RDMEM(j);
 M_RES(5,i);
 M_WRMEM(j,i);
};
static void res_5_a(void) { M_RES(5,R.AF.B.h); };
static void res_5_b(void) { M_RES(5,R.BC.B.h); };
static void res_5_c(void) { M_RES(5,R.BC.B.l); };
static void res_5_d(void) { M_RES(5,R.DE.B.h); };
static void res_5_e(void) { M_RES(5,R.DE.B.l); };
static void res_5_h(void) { M_RES(5,R.HL.B.h); };
static void res_5_l(void) { M_RES(5,R.HL.B.l); };

static void res_6_xhl(void)
{
 byte i;
 i=M_RDMEM(R.HL.D);
 M_RES(6,i);
 M_WRMEM(R.HL.D,i);
};
static void res_6_xix(void)
{
 byte i;
 int j;
 j=M_XIX;
 i=M_RDMEM(j);
 M_RES(6,i);
 M_WRMEM(j,i);
};
static void res_6_xiy(void)
{
 byte i;
 int j;
 j=M_XIY;
 i=M_RDMEM(j);
 M_RES(6,i);
 M_WRMEM(j,i);
};
static void res_6_a(void) { M_RES(6,R.AF.B.h); };
static void res_6_b(void) { M_RES(6,R.BC.B.h); };
static void res_6_c(void) { M_RES(6,R.BC.B.l); };
static void res_6_d(void) { M_RES(6,R.DE.B.h); };
static void res_6_e(void) { M_RES(6,R.DE.B.l); };
static void res_6_h(void) { M_RES(6,R.HL.B.h); };
static void res_6_l(void) { M_RES(6,R.HL.B.l); };

static void res_7_xhl(void)
{
 byte i;
 i=M_RDMEM(R.HL.D);
 M_RES(7,i);
 M_WRMEM(R.HL.D,i);
};
static void res_7_xix(void)
{
 byte i;
 int j;
 j=M_XIX;
 i=M_RDMEM(j);
 M_RES(7,i);
 M_WRMEM(j,i);
};
static void res_7_xiy(void)
{
 byte i;
 int j;
 j=M_XIY;
 i=M_RDMEM(j);
 M_RES(7,i);
 M_WRMEM(j,i);
};
static void res_7_a(void) { M_RES(7,R.AF.B.h); };
static void res_7_b(void) { M_RES(7,R.BC.B.h); };
static void res_7_c(void) { M_RES(7,R.BC.B.l); };
static void res_7_d(void) { M_RES(7,R.DE.B.h); };
static void res_7_e(void) { M_RES(7,R.DE.B.l); };
static void res_7_h(void) { M_RES(7,R.HL.B.h); };
static void res_7_l(void) { M_RES(7,R.HL.B.l); };

static void ret(void) { M_RET; }
static void ret_c(void) { if (M_C) { M_RET; } else { M_SKIP_RET; } }
static void ret_m(void) { if (M_M) { M_RET; } else { M_SKIP_RET; } }
static void ret_nc(void) { if (M_NC) { M_RET; } else { M_SKIP_RET; } }
static void ret_nz(void) { if (M_NZ) { M_RET; } else { M_SKIP_RET; } }
static void ret_p(void) { if (M_P) { M_RET; } else { M_SKIP_RET; } }
static void ret_pe(void) { if (M_PE) { M_RET; } else { M_SKIP_RET; } }
static void ret_po(void) { if (M_PO) { M_RET; } else { M_SKIP_RET; } }
static void ret_z(void) { if (M_Z) { M_RET; } else { M_SKIP_RET; } }

static void reti(void) { Z80_Reti(); M_RET; }
static void retn(void) { R.IFF1=R.IFF2; Z80_Retn(); M_RET; }

static void rl_xhl(void)
{
 byte i;
 i=M_RDMEM(R.HL.D);
 M_RL(i);
 M_WRMEM(R.HL.D,i);
}
static void rl_xix(void)
{
 byte i;
 int j;
 j=M_XIX;
 i=M_RDMEM(j);
 M_RL(i);
 M_WRMEM(j,i);
}
static void rl_xiy(void)
{
 byte i;
 int j;
 j=M_XIY;
 i=M_RDMEM(j);
 M_RL(i);
 M_WRMEM(j,i);
}
static void rl_a(void) { M_RL(R.AF.B.h); }
static void rl_b(void) { M_RL(R.BC.B.h); }
static void rl_c(void) { M_RL(R.BC.B.l); }
static void rl_d(void) { M_RL(R.DE.B.h); }
static void rl_e(void) { M_RL(R.DE.B.l); }
static void rl_h(void) { M_RL(R.HL.B.h); }
static void rl_l(void) { M_RL(R.HL.B.l); }
static void rla(void)  { M_RLA; }

static void rlc_xhl(void)
{
 byte i;
 i=M_RDMEM(R.HL.D);
 M_RLC(i);
 M_WRMEM(R.HL.D,i);
}
static void rlc_xix(void)
{
 byte i;
 int j;
 j=M_XIX;
 i=M_RDMEM(j);
 M_RLC(i);
 M_WRMEM(j,i);
}
static void rlc_xiy(void)
{
 byte i;
 int j;
 j=M_XIY;
 i=M_RDMEM(j);
 M_RLC(i);
 M_WRMEM(j,i);
}
static void rlc_a(void) { M_RLC(R.AF.B.h); }
static void rlc_b(void) { M_RLC(R.BC.B.h); }
static void rlc_c(void) { M_RLC(R.BC.B.l); }
static void rlc_d(void) { M_RLC(R.DE.B.h); }
static void rlc_e(void) { M_RLC(R.DE.B.l); }
static void rlc_h(void) { M_RLC(R.HL.B.h); }
static void rlc_l(void) { M_RLC(R.HL.B.l); }
static void rlca(void)  { M_RLCA; }

static void rld(void)
{
 byte i;
 i=M_RDMEM(R.HL.D);
 M_WRMEM(R.HL.D,(i<<4)|(R.AF.B.h&0x0F));
 R.AF.B.h=(R.AF.B.h&0xF0)|(i>>4);
 R.AF.B.l=(R.AF.B.l&C_FLAG)|ZSPTable[R.AF.B.h];
}

static void rr_xhl(void)
{
 byte i;
 i=M_RDMEM(R.HL.D);
 M_RR(i);
 M_WRMEM(R.HL.D,i);
}
static void rr_xix(void)
{
 byte i;
 int j;
 j=M_XIX;
 i=M_RDMEM(j);
 M_RR(i);
 M_WRMEM(j,i);
}
static void rr_xiy(void)
{
 byte i;
 int j;
 j=M_XIY;
 i=M_RDMEM(j);
 M_RR(i);
 M_WRMEM(j,i);
}
static void rr_a(void) { M_RR(R.AF.B.h); }
static void rr_b(void) { M_RR(R.BC.B.h); }
static void rr_c(void) { M_RR(R.BC.B.l); }
static void rr_d(void) { M_RR(R.DE.B.h); }
static void rr_e(void) { M_RR(R.DE.B.l); }
static void rr_h(void) { M_RR(R.HL.B.h); }
static void rr_l(void) { M_RR(R.HL.B.l); }
static void rra(void)  { M_RRA; }

static void rrc_xhl(void)
{
 byte i;
 i=M_RDMEM(R.HL.D);
 M_RRC(i);
 M_WRMEM(R.HL.D,i);
}
static void rrc_xix(void)
{
 byte i;
 int j;
 j=M_XIX;
 i=M_RDMEM(j);
 M_RRC(i);
 M_WRMEM(j,i);
}
static void rrc_xiy(void)
{
 byte i;
 int j;
 j=M_XIY;
 i=M_RDMEM(j);
 M_RRC(i);
 M_WRMEM(j,i);
}
static void rrc_a(void) { M_RRC(R.AF.B.h); }
static void rrc_b(void) { M_RRC(R.BC.B.h); }
static void rrc_c(void) { M_RRC(R.BC.B.l); }
static void rrc_d(void) { M_RRC(R.DE.B.h); }
static void rrc_e(void) { M_RRC(R.DE.B.l); }
static void rrc_h(void) { M_RRC(R.HL.B.h); }
static void rrc_l(void) { M_RRC(R.HL.B.l); }
static void rrca(void)  { M_RRCA; }

static void rrd(void)
{
 byte i;
 i=M_RDMEM(R.HL.D);
 M_WRMEM(R.HL.D,(i>>4)|(R.AF.B.h<<4));
 R.AF.B.h=(R.AF.B.h&0xF0)|(i&0x0F);
 R.AF.B.l=(R.AF.B.l&C_FLAG)|ZSPTable[R.AF.B.h];
}

static void rst_00(void) { M_RST(0x00); }
static void rst_08(void) { M_RST(0x08); }
static void rst_10(void) { M_RST(0x10); }
static void rst_18(void) { M_RST(0x18); }
static void rst_20(void) { M_RST(0x20); }
static void rst_28(void) { M_RST(0x28); }
static void rst_30(void) { M_RST(0x30); }
static void rst_38(void) { M_RST(0x38); }

static void sbc_a_byte(void) { byte i=M_RDMEM_OPCODE(); M_SBC(i); }
static void sbc_a_xhl(void) { byte i=M_RD_XHL; M_SBC(i); }
static void sbc_a_xix(void) { byte i=M_RD_XIX(); M_SBC(i); }
static void sbc_a_xiy(void) { byte i=M_RD_XIY(); M_SBC(i); }
static void sbc_a_a(void) { M_SBC(R.AF.B.h); }
static void sbc_a_b(void) { M_SBC(R.BC.B.h); }
static void sbc_a_c(void) { M_SBC(R.BC.B.l); }
static void sbc_a_d(void) { M_SBC(R.DE.B.h); }
static void sbc_a_e(void) { M_SBC(R.DE.B.l); }
static void sbc_a_h(void) { M_SBC(R.HL.B.h); }
static void sbc_a_l(void) { M_SBC(R.HL.B.l); }
static void sbc_a_ixh(void) { M_SBC(R.IX.B.h); }
static void sbc_a_ixl(void) { M_SBC(R.IX.B.l); }
static void sbc_a_iyh(void) { M_SBC(R.IY.B.h); }
static void sbc_a_iyl(void) { M_SBC(R.IY.B.l); }

static void sbc_hl_bc(void) { M_SBCW(BC); }
static void sbc_hl_de(void) { M_SBCW(DE); }
static void sbc_hl_hl(void) { M_SBCW(HL); }
static void sbc_hl_sp(void) { M_SBCW(SP); }

static void scf(void) { R.AF.B.l=(R.AF.B.l&0xEC)|C_FLAG; }

static void set_0_xhl(void)
{
 byte i;
 i=M_RDMEM(R.HL.D);
 M_SET(0,i);
 M_WRMEM(R.HL.D,i);
};
static void set_0_xix(void)
{
 byte i;
 int j;
 j=M_XIX;
 i=M_RDMEM(j);
 M_SET(0,i);
 M_WRMEM(j,i);
};
static void set_0_xiy(void)
{
 byte i;
 int j;
 j=M_XIY;
 i=M_RDMEM(j);
 M_SET(0,i);
 M_WRMEM(j,i);
};
static void set_0_a(void) { M_SET(0,R.AF.B.h); };
static void set_0_b(void) { M_SET(0,R.BC.B.h); };
static void set_0_c(void) { M_SET(0,R.BC.B.l); };
static void set_0_d(void) { M_SET(0,R.DE.B.h); };
static void set_0_e(void) { M_SET(0,R.DE.B.l); };
static void set_0_h(void) { M_SET(0,R.HL.B.h); };
static void set_0_l(void) { M_SET(0,R.HL.B.l); };

static void set_1_xhl(void)
{
 byte i;
 i=M_RDMEM(R.HL.D);
 M_SET(1,i);
 M_WRMEM(R.HL.D,i);
};
static void set_1_xix(void)
{
 byte i;
 int j;
 j=M_XIX;
 i=M_RDMEM(j);
 M_SET(1,i);
 M_WRMEM(j,i);
};
static void set_1_xiy(void)
{
 byte i;
 int j;
 j=M_XIY;
 i=M_RDMEM(j);
 M_SET(1,i);
 M_WRMEM(j,i);
};
static void set_1_a(void) { M_SET(1,R.AF.B.h); };
static void set_1_b(void) { M_SET(1,R.BC.B.h); };
static void set_1_c(void) { M_SET(1,R.BC.B.l); };
static void set_1_d(void) { M_SET(1,R.DE.B.h); };
static void set_1_e(void) { M_SET(1,R.DE.B.l); };
static void set_1_h(void) { M_SET(1,R.HL.B.h); };
static void set_1_l(void) { M_SET(1,R.HL.B.l); };

static void set_2_xhl(void)
{
 byte i;
 i=M_RDMEM(R.HL.D);
 M_SET(2,i);
 M_WRMEM(R.HL.D,i);
};
static void set_2_xix(void)
{
 byte i;
 int j;
 j=M_XIX;
 i=M_RDMEM(j);
 M_SET(2,i);
 M_WRMEM(j,i);
};
static void set_2_xiy(void)
{
 byte i;
 int j;
 j=M_XIY;
 i=M_RDMEM(j);
 M_SET(2,i);
 M_WRMEM(j,i);
};
static void set_2_a(void) { M_SET(2,R.AF.B.h); };
static void set_2_b(void) { M_SET(2,R.BC.B.h); };
static void set_2_c(void) { M_SET(2,R.BC.B.l); };
static void set_2_d(void) { M_SET(2,R.DE.B.h); };
static void set_2_e(void) { M_SET(2,R.DE.B.l); };
static void set_2_h(void) { M_SET(2,R.HL.B.h); };
static void set_2_l(void) { M_SET(2,R.HL.B.l); };

static void set_3_xhl(void)
{
 byte i;
 i=M_RDMEM(R.HL.D);
 M_SET(3,i);
 M_WRMEM(R.HL.D,i);
};
static void set_3_xix(void)
{
 byte i;
 int j;
 j=M_XIX;
 i=M_RDMEM(j);
 M_SET(3,i);
 M_WRMEM(j,i);
};
static void set_3_xiy(void)
{
 byte i;
 int j;
 j=M_XIY;
 i=M_RDMEM(j);
 M_SET(3,i);
 M_WRMEM(j,i);
};
static void set_3_a(void) { M_SET(3,R.AF.B.h); };
static void set_3_b(void) { M_SET(3,R.BC.B.h); };
static void set_3_c(void) { M_SET(3,R.BC.B.l); };
static void set_3_d(void) { M_SET(3,R.DE.B.h); };
static void set_3_e(void) { M_SET(3,R.DE.B.l); };
static void set_3_h(void) { M_SET(3,R.HL.B.h); };
static void set_3_l(void) { M_SET(3,R.HL.B.l); };

static void set_4_xhl(void)
{
 byte i;
 i=M_RDMEM(R.HL.D);
 M_SET(4,i);
 M_WRMEM(R.HL.D,i);
};
static void set_4_xix(void)
{
 byte i;
 int j;
 j=M_XIX;
 i=M_RDMEM(j);
 M_SET(4,i);
 M_WRMEM(j,i);
};
static void set_4_xiy(void)
{
 byte i;
 int j;
 j=M_XIY;
 i=M_RDMEM(j);
 M_SET(4,i);
 M_WRMEM(j,i);
};
static void set_4_a(void) { M_SET(4,R.AF.B.h); };
static void set_4_b(void) { M_SET(4,R.BC.B.h); };
static void set_4_c(void) { M_SET(4,R.BC.B.l); };
static void set_4_d(void) { M_SET(4,R.DE.B.h); };
static void set_4_e(void) { M_SET(4,R.DE.B.l); };
static void set_4_h(void) { M_SET(4,R.HL.B.h); };
static void set_4_l(void) { M_SET(4,R.HL.B.l); };

static void set_5_xhl(void)
{
 byte i;
 i=M_RDMEM(R.HL.D);
 M_SET(5,i);
 M_WRMEM(R.HL.D,i);
};
static void set_5_xix(void)
{
 byte i;
 int j;
 j=M_XIX;
 i=M_RDMEM(j);
 M_SET(5,i);
 M_WRMEM(j,i);
};
static void set_5_xiy(void)
{
 byte i;
 int j;
 j=M_XIY;
 i=M_RDMEM(j);
 M_SET(5,i);
 M_WRMEM(j,i);
};
static void set_5_a(void) { M_SET(5,R.AF.B.h); };
static void set_5_b(void) { M_SET(5,R.BC.B.h); };
static void set_5_c(void) { M_SET(5,R.BC.B.l); };
static void set_5_d(void) { M_SET(5,R.DE.B.h); };
static void set_5_e(void) { M_SET(5,R.DE.B.l); };
static void set_5_h(void) { M_SET(5,R.HL.B.h); };
static void set_5_l(void) { M_SET(5,R.HL.B.l); };

static void set_6_xhl(void)
{
 byte i;
 i=M_RDMEM(R.HL.D);
 M_SET(6,i);
 M_WRMEM(R.HL.D,i);
};
static void set_6_xix(void)
{
 byte i;
 int j;
 j=M_XIX;
 i=M_RDMEM(j);
 M_SET(6,i);
 M_WRMEM(j,i);
};
static void set_6_xiy(void)
{
 byte i;
 int j;
 j=M_XIY;
 i=M_RDMEM(j);
 M_SET(6,i);
 M_WRMEM(j,i);
};
static void set_6_a(void) { M_SET(6,R.AF.B.h); };
static void set_6_b(void) { M_SET(6,R.BC.B.h); };
static void set_6_c(void) { M_SET(6,R.BC.B.l); };
static void set_6_d(void) { M_SET(6,R.DE.B.h); };
static void set_6_e(void) { M_SET(6,R.DE.B.l); };
static void set_6_h(void) { M_SET(6,R.HL.B.h); };
static void set_6_l(void) { M_SET(6,R.HL.B.l); };

static void set_7_xhl(void)
{
 byte i;
 i=M_RDMEM(R.HL.D);
 M_SET(7,i);
 M_WRMEM(R.HL.D,i);
};
static void set_7_xix(void)
{
 byte i;
 int j;
 j=M_XIX;
 i=M_RDMEM(j);
 M_SET(7,i);
 M_WRMEM(j,i);
};
static void set_7_xiy(void)
{
 byte i;
 int j;
 j=M_XIY;
 i=M_RDMEM(j);
 M_SET(7,i);
 M_WRMEM(j,i);
};
static void set_7_a(void) { M_SET(7,R.AF.B.h); };
static void set_7_b(void) { M_SET(7,R.BC.B.h); };
static void set_7_c(void) { M_SET(7,R.BC.B.l); };
static void set_7_d(void) { M_SET(7,R.DE.B.h); };
static void set_7_e(void) { M_SET(7,R.DE.B.l); };
static void set_7_h(void) { M_SET(7,R.HL.B.h); };
static void set_7_l(void) { M_SET(7,R.HL.B.l); };

static void sla_xhl(void)
{
 byte i;
 i=M_RDMEM(R.HL.D);
 M_SLA(i);
 M_WRMEM(R.HL.D,i);
}
static void sla_xix(void)
{
 byte i;
 int j;
 j=M_XIX;
 i=M_RDMEM(j);
 M_SLA(i);
 M_WRMEM(j,i);
}
static void sla_xiy(void)
{
 byte i;
 int j;
 j=M_XIY;
 i=M_RDMEM(j);
 M_SLA(i);
 M_WRMEM(j,i);
}
static void sla_a(void) { M_SLA(R.AF.B.h); }
static void sla_b(void) { M_SLA(R.BC.B.h); }
static void sla_c(void) { M_SLA(R.BC.B.l); }
static void sla_d(void) { M_SLA(R.DE.B.h); }
static void sla_e(void) { M_SLA(R.DE.B.l); }
static void sla_h(void) { M_SLA(R.HL.B.h); }
static void sla_l(void) { M_SLA(R.HL.B.l); }

static void sll_xhl(void)
{
 byte i;
 i=M_RDMEM(R.HL.D);
 M_SLL(i);
 M_WRMEM(R.HL.D,i);
}
static void sll_xix(void)
{
 byte i;
 int j;
 j=M_XIX;
 i=M_RDMEM(j);
 M_SLL(i);
 M_WRMEM(j,i);
}
static void sll_xiy(void)
{
 byte i;
 int j;
 j=M_XIY;
 i=M_RDMEM(j);
 M_SLL(i);
 M_WRMEM(j,i);
}
static void sll_a(void) { M_SLL(R.AF.B.h); }
static void sll_b(void) { M_SLL(R.BC.B.h); }
static void sll_c(void) { M_SLL(R.BC.B.l); }
static void sll_d(void) { M_SLL(R.DE.B.h); }
static void sll_e(void) { M_SLL(R.DE.B.l); }
static void sll_h(void) { M_SLL(R.HL.B.h); }
static void sll_l(void) { M_SLL(R.HL.B.l); }

static void sra_xhl(void)
{
 byte i;
 i=M_RDMEM(R.HL.D);
 M_SRA(i);
 M_WRMEM(R.HL.D,i);
}
static void sra_xix(void)
{
 byte i;
 int j;
 j=M_XIX;
 i=M_RDMEM(j);
 M_SRA(i);
 M_WRMEM(j,i);
}
static void sra_xiy(void)
{
 byte i;
 int j;
 j=M_XIY;
 i=M_RDMEM(j);
 M_SRA(i);
 M_WRMEM(j,i);
}
static void sra_a(void) { M_SRA(R.AF.B.h); }
static void sra_b(void) { M_SRA(R.BC.B.h); }
static void sra_c(void) { M_SRA(R.BC.B.l); }
static void sra_d(void) { M_SRA(R.DE.B.h); }
static void sra_e(void) { M_SRA(R.DE.B.l); }
static void sra_h(void) { M_SRA(R.HL.B.h); }
static void sra_l(void) { M_SRA(R.HL.B.l); }

static void srl_xhl(void)
{
 byte i;
 i=M_RDMEM(R.HL.D);
 M_SRL(i);
 M_WRMEM(R.HL.D,i);
}
static void srl_xix(void)
{
 byte i;
 int j;
 j=M_XIX;
 i=M_RDMEM(j);
 M_SRL(i);
 M_WRMEM(j,i);
}
static void srl_xiy(void)
{
 byte i;
 int j;
 j=M_XIY;
 i=M_RDMEM(j);
 M_SRL(i);
 M_WRMEM(j,i);
}
static void srl_a(void) { M_SRL(R.AF.B.h); }
static void srl_b(void) { M_SRL(R.BC.B.h); }
static void srl_c(void) { M_SRL(R.BC.B.l); }
static void srl_d(void) { M_SRL(R.DE.B.h); }
static void srl_e(void) { M_SRL(R.DE.B.l); }
static void srl_h(void) { M_SRL(R.HL.B.h); }
static void srl_l(void) { M_SRL(R.HL.B.l); }

static void sub_xhl(void) { byte i=M_RD_XHL; M_SUB(i); }
static void sub_xix(void) { byte i=M_RD_XIX(); M_SUB(i); }
static void sub_xiy(void) { byte i=M_RD_XIY(); M_SUB(i); }
static void sub_a(void) { R.AF.D=Z_FLAG|N_FLAG; }
static void sub_b(void) { M_SUB(R.BC.B.h); }
static void sub_c(void) { M_SUB(R.BC.B.l); }
static void sub_d(void) { M_SUB(R.DE.B.h); }
static void sub_e(void) { M_SUB(R.DE.B.l); }
static void sub_h(void) { M_SUB(R.HL.B.h); }
static void sub_l(void) { M_SUB(R.HL.B.l); }
static void sub_ixh(void) { M_SUB(R.IX.B.h); }
static void sub_ixl(void) { M_SUB(R.IX.B.l); }
static void sub_iyh(void) { M_SUB(R.IY.B.h); }
static void sub_iyl(void) { M_SUB(R.IY.B.l); }
static void sub_byte(void) { byte i=M_RDMEM_OPCODE(); M_SUB(i); }

static void xor_xhl(void) { byte i=M_RD_XHL; M_XOR(i); }
static void xor_xix(void) { byte i=M_RD_XIX(); M_XOR(i); }
static void xor_xiy(void) { byte i=M_RD_XIY(); M_XOR(i); }
static void xor_a(void) { R.AF.D=Z_FLAG|V_FLAG; }
static void xor_b(void) { M_XOR(R.BC.B.h); }
static void xor_c(void) { M_XOR(R.BC.B.l); }
static void xor_d(void) { M_XOR(R.DE.B.h); }
static void xor_e(void) { M_XOR(R.DE.B.l); }
static void xor_h(void) { M_XOR(R.HL.B.h); }
static void xor_l(void) { M_XOR(R.HL.B.l); }
static void xor_ixh(void) { M_XOR(R.IX.B.h); }
static void xor_ixl(void) { M_XOR(R.IX.B.l); }
static void xor_iyh(void) { M_XOR(R.IY.B.h); }
static void xor_iyl(void) { M_XOR(R.IY.B.l); }
static void xor_byte(void) { byte i=M_RDMEM_OPCODE(); M_XOR(i); }

static void no_op(void)
{
 --R.PC.W.l;
}

static void patch(void) { Z80_Patch(&R); }

static unsigned cycles_main[256]=
{
  4,10,7,6,4,4,7,4,
  4,11,7,6,4,4,7,4,
  8,10,7,6,4,4,7,4,
  7,11,7,6,4,4,7,4,
  7,10,16,6,4,4,7,4,
  7,11,16,6,4,4,7,4,
  7,10,13,6,11,11,10,4,
  7,11,13,6,4,4,7,4,
  4,4,4,4,4,4,7,4,
  4,4,4,4,4,4,7,4,
  4,4,4,4,4,4,7,4,
  4,4,4,4,4,4,7,4,
  4,4,4,4,4,4,7,4,
  4,4,4,4,4,4,7,4,
  7,7,7,7,7,7,4,7,
  4,4,4,4,4,4,7,4,
  4,4,4,4,4,4,7,4,
  4,4,4,4,4,4,7,4,
  4,4,4,4,4,4,7,4,
  4,4,4,4,4,4,7,4,
  4,4,4,4,4,4,7,4,
  4,4,4,4,4,4,7,4,
  4,4,4,4,4,4,7,4,
  4,4,4,4,4,4,7,4,
  5,10,10,10,10,11,7,11,
  5,4,10,0,10,10,7,11,
  5,10,10,11,10,11,7,11,
  5,4,10,11,10,0,7,11,
  5,10,10,19,10,11,7,11,
  5,4,10,4,10,0,7,11,
  5,10,10,4,10,11,7,11,
  5,6,10,4,10,0,7,11
};

static unsigned cycles_cb[256]=
{
  8,8,8,8,8,8,15,8,
  8,8,8,8,8,8,15,8,
  8,8,8,8,8,8,15,8,
  8,8,8,8,8,8,15,8,
  8,8,8,8,8,8,15,8,
  8,8,8,8,8,8,15,8,
  8,8,8,8,8,8,15,8,
  8,8,8,8,8,8,15,8,
  8,8,8,8,8,8,12,8,
  8,8,8,8,8,8,12,8,
  8,8,8,8,8,8,12,8,
  8,8,8,8,8,8,12,8,
  8,8,8,8,8,8,12,8,
  8,8,8,8,8,8,12,8,
  8,8,8,8,8,8,12,8,
  8,8,8,8,8,8,12,8,
  8,8,8,8,8,8,15,8,
  8,8,8,8,8,8,15,8,
  8,8,8,8,8,8,15,8,
  8,8,8,8,8,8,15,8,
  8,8,8,8,8,8,15,8,
  8,8,8,8,8,8,15,8,
  8,8,8,8,8,8,15,8,
  8,8,8,8,8,8,15,8,
  8,8,8,8,8,8,15,8,
  8,8,8,8,8,8,15,8,
  8,8,8,8,8,8,15,8,
  8,8,8,8,8,8,15,8,
  8,8,8,8,8,8,15,8,
  8,8,8,8,8,8,15,8,
  8,8,8,8,8,8,15,8,
  8,8,8,8,8,8,15,8
};
static unsigned cycles_xx_cb[]=
{
  0,0,0,0,0,0,23,0,
  0,0,0,0,0,0,23,0,
  0,0,0,0,0,0,23,0,
  0,0,0,0,0,0,23,0,
  0,0,0,0,0,0,23,0,
  0,0,0,0,0,0,23,0,
  0,0,0,0,0,0,23,0,
  0,0,0,0,0,0,23,0,
  20,20,20,20,20,20,20,20,
  20,20,20,20,20,20,20,20,
  20,20,20,20,20,20,20,20,
  20,20,20,20,20,20,20,20,
  20,20,20,20,20,20,20,20,
  20,20,20,20,20,20,20,20,
  20,20,20,20,20,20,20,20,
  20,20,20,20,20,20,20,20,
  0,0,0,0,0,0,23,0,
  0,0,0,0,0,0,23,0,
  0,0,0,0,0,0,23,0,
  0,0,0,0,0,0,23,0,
  0,0,0,0,0,0,23,0,
  0,0,0,0,0,0,23,0,
  0,0,0,0,0,0,23,0,
  0,0,0,0,0,0,23,0,
  0,0,0,0,0,0,23,0,
  0,0,0,0,0,0,23,0,
  0,0,0,0,0,0,23,0,
  0,0,0,0,0,0,23,0,
  0,0,0,0,0,0,23,0,
  0,0,0,0,0,0,23,0,
  0,0,0,0,0,0,23,0,
  0,0,0,0,0,0,23,0
};
static unsigned cycles_xx[256]=
{
  0,0,0,0,0,0,0,0,
  0,15,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,
  0,15,0,0,0,0,0,0,
  0,14,20,10,9,9,9,0,
  0,15,20,10,9,9,9,0,
  0,0,0,0,23,23,19,0,
  0,15,0,0,0,0,0,0,
  0,0,0,0,9,9,19,0,
  0,0,0,0,9,9,19,0,
  0,0,0,0,9,9,19,0,
  0,0,0,0,9,9,19,0,
  9,9,9,9,9,9,9,9,
  9,9,9,9,9,9,9,9,
  19,19,19,19,19,19,19,19,
  0,0,0,0,9,9,19,0,
  0,0,0,0,9,9,19,0,
  0,0,0,0,9,9,19,0,
  0,0,0,0,9,9,19,0,
  0,0,0,0,9,9,19,0,
  0,0,0,0,9,9,19,0,
  0,0,0,0,9,9,19,0,
  0,0,0,0,9,9,19,0,
  0,0,0,0,9,9,19,0,
  0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,
  0,14,0,23,0,15,0,0,
  0,8,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,
  0,10,0,0,0,0,0,0
};
static unsigned cycles_ed[256]=
{
  0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,
  12,12,15,20,8,8,8,9,
  12,12,15,20,8,8,8,9,
  12,12,15,20,8,8,8,9,
  12,12,15,20,8,8,8,9,
  12,12,15,20,8,8,8,18,
  12,12,15,20,8,8,8,18,
  12,12,15,20,8,8,8,0,
  12,12,15,20,8,8,8,0,
  0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,
  16,16,16,16,0,0,0,0,
  16,16,16,16,0,0,0,0,
  0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0
};

static void no_op_xx(void) {
++R.PC.W.l; }

static opcode_fn opcode_dd_cb[256]=
{
 no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,rlc_xix  ,no_op_xx ,
 no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,rrc_xix  ,no_op_xx ,
 no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,rl_xix   ,no_op_xx ,
 no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,rr_xix   ,no_op_xx ,
 no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,sla_xix  ,no_op_xx ,
 no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,sra_xix  ,no_op_xx ,
 no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,sll_xix  ,no_op_xx ,
 no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,srl_xix  ,no_op_xx ,
 bit_0_xix,bit_0_xix,bit_0_xix,bit_0_xix,bit_0_xix,bit_0_xix,bit_0_xix,bit_0_xix,
 bit_1_xix,bit_1_xix,bit_1_xix,bit_1_xix,bit_1_xix,bit_1_xix,bit_1_xix,bit_1_xix,
 bit_2_xix,bit_2_xix,bit_2_xix,bit_2_xix,bit_2_xix,bit_2_xix,bit_2_xix,bit_2_xix,
 bit_3_xix,bit_3_xix,bit_3_xix,bit_3_xix,bit_3_xix,bit_3_xix,bit_3_xix,bit_3_xix,
 bit_4_xix,bit_4_xix,bit_4_xix,bit_4_xix,bit_4_xix,bit_4_xix,bit_4_xix,bit_4_xix,
 bit_5_xix,bit_5_xix,bit_5_xix,bit_5_xix,bit_5_xix,bit_5_xix,bit_5_xix,bit_5_xix,
 bit_6_xix,bit_6_xix,bit_6_xix,bit_6_xix,bit_6_xix,bit_6_xix,bit_6_xix,bit_6_xix,
 bit_7_xix,bit_7_xix,bit_7_xix,bit_7_xix,bit_7_xix,bit_7_xix,bit_7_xix,bit_7_xix,
 no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,res_0_xix,no_op_xx ,
 no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,res_1_xix,no_op_xx ,
 no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,res_2_xix,no_op_xx ,
 no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,res_3_xix,no_op_xx ,
 no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,res_4_xix,no_op_xx ,
 no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,res_5_xix,no_op_xx ,
 no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,res_6_xix,no_op_xx ,
 no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,res_7_xix,no_op_xx ,
 no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,set_0_xix,no_op_xx ,
 no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,set_1_xix,no_op_xx ,
 no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,set_2_xix,no_op_xx ,
 no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,set_3_xix,no_op_xx ,
 no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,set_4_xix,no_op_xx ,
 no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,set_5_xix,no_op_xx ,
 no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,set_6_xix,no_op_xx ,
 no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,set_7_xix,no_op_xx
};

static opcode_fn opcode_fd_cb[256]=
{
 no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,rlc_xiy  ,no_op_xx ,
 no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,rrc_xiy  ,no_op_xx ,
 no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,rl_xiy   ,no_op_xx ,
 no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,rr_xiy   ,no_op_xx ,
 no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,sla_xiy  ,no_op_xx ,
 no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,sra_xiy  ,no_op_xx ,
 no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,sll_xiy  ,no_op_xx ,
 no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,srl_xiy  ,no_op_xx ,
 bit_0_xiy,bit_0_xiy,bit_0_xiy,bit_0_xiy,bit_0_xiy,bit_0_xiy,bit_0_xiy,bit_0_xiy,
 bit_1_xiy,bit_1_xiy,bit_1_xiy,bit_1_xiy,bit_1_xiy,bit_1_xiy,bit_1_xiy,bit_1_xiy,
 bit_2_xiy,bit_2_xiy,bit_2_xiy,bit_2_xiy,bit_2_xiy,bit_2_xiy,bit_2_xiy,bit_2_xiy,
 bit_3_xiy,bit_3_xiy,bit_3_xiy,bit_3_xiy,bit_3_xiy,bit_3_xiy,bit_3_xiy,bit_3_xiy,
 bit_4_xiy,bit_4_xiy,bit_4_xiy,bit_4_xiy,bit_4_xiy,bit_4_xiy,bit_4_xiy,bit_4_xiy,
 bit_5_xiy,bit_5_xiy,bit_5_xiy,bit_5_xiy,bit_5_xiy,bit_5_xiy,bit_5_xiy,bit_5_xiy,
 bit_6_xiy,bit_6_xiy,bit_6_xiy,bit_6_xiy,bit_6_xiy,bit_6_xiy,bit_6_xiy,bit_6_xiy,
 bit_7_xiy,bit_7_xiy,bit_7_xiy,bit_7_xiy,bit_7_xiy,bit_7_xiy,bit_7_xiy,bit_7_xiy,
 no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,res_0_xiy,no_op_xx ,
 no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,res_1_xiy,no_op_xx ,
 no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,res_2_xiy,no_op_xx ,
 no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,res_3_xiy,no_op_xx ,
 no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,res_4_xiy,no_op_xx ,
 no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,res_5_xiy,no_op_xx ,
 no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,res_6_xiy,no_op_xx ,
 no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,res_7_xiy,no_op_xx ,
 no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,set_0_xiy,no_op_xx ,
 no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,set_1_xiy,no_op_xx ,
 no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,set_2_xiy,no_op_xx ,
 no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,set_3_xiy,no_op_xx ,
 no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,set_4_xiy,no_op_xx ,
 no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,set_5_xiy,no_op_xx ,
 no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,set_6_xiy,no_op_xx ,
 no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,no_op_xx ,set_7_xiy,no_op_xx
};

static void dd_cb(void)
{
 unsigned opcode;
 opcode=M_RDOP_ARG((R.PC.D+1)&0xFFFF);
 Z80_ICount += cycles_xx_cb[opcode];
 (*(opcode_dd_cb[opcode]))();
 ++R.PC.W.l;
};
static void fd_cb(void)
{
 unsigned opcode;
 opcode=M_RDOP_ARG((R.PC.D+1)&0xFFFF);
 Z80_ICount+=cycles_xx_cb[opcode];
 (*(opcode_fd_cb[opcode]))();
 ++R.PC.W.l;
};

static opcode_fn opcode_cb[256]=
{
 rlc_b  ,rlc_c  ,rlc_d  ,rlc_e  ,rlc_h  ,rlc_l  ,rlc_xhl  ,rlc_a  ,
 rrc_b  ,rrc_c  ,rrc_d  ,rrc_e  ,rrc_h  ,rrc_l  ,rrc_xhl  ,rrc_a  ,
 rl_b   ,rl_c   ,rl_d   ,rl_e   ,rl_h   ,rl_l   ,rl_xhl   ,rl_a   ,
 rr_b   ,rr_c   ,rr_d   ,rr_e   ,rr_h   ,rr_l   ,rr_xhl   ,rr_a   ,
 sla_b  ,sla_c  ,sla_d  ,sla_e  ,sla_h  ,sla_l  ,sla_xhl  ,sla_a  ,
 sra_b  ,sra_c  ,sra_d  ,sra_e  ,sra_h  ,sra_l  ,sra_xhl  ,sra_a  ,
 sll_b  ,sll_c  ,sll_d  ,sll_e  ,sll_h  ,sll_l  ,sll_xhl  ,sll_a  ,
 srl_b  ,srl_c  ,srl_d  ,srl_e  ,srl_h  ,srl_l  ,srl_xhl  ,srl_a  ,
 bit_0_b,bit_0_c,bit_0_d,bit_0_e,bit_0_h,bit_0_l,bit_0_xhl,bit_0_a,
 bit_1_b,bit_1_c,bit_1_d,bit_1_e,bit_1_h,bit_1_l,bit_1_xhl,bit_1_a,
 bit_2_b,bit_2_c,bit_2_d,bit_2_e,bit_2_h,bit_2_l,bit_2_xhl,bit_2_a,
 bit_3_b,bit_3_c,bit_3_d,bit_3_e,bit_3_h,bit_3_l,bit_3_xhl,bit_3_a,
 bit_4_b,bit_4_c,bit_4_d,bit_4_e,bit_4_h,bit_4_l,bit_4_xhl,bit_4_a,
 bit_5_b,bit_5_c,bit_5_d,bit_5_e,bit_5_h,bit_5_l,bit_5_xhl,bit_5_a,
 bit_6_b,bit_6_c,bit_6_d,bit_6_e,bit_6_h,bit_6_l,bit_6_xhl,bit_6_a,
 bit_7_b,bit_7_c,bit_7_d,bit_7_e,bit_7_h,bit_7_l,bit_7_xhl,bit_7_a,
 res_0_b,res_0_c,res_0_d,res_0_e,res_0_h,res_0_l,res_0_xhl,res_0_a,
 res_1_b,res_1_c,res_1_d,res_1_e,res_1_h,res_1_l,res_1_xhl,res_1_a,
 res_2_b,res_2_c,res_2_d,res_2_e,res_2_h,res_2_l,res_2_xhl,res_2_a,
 res_3_b,res_3_c,res_3_d,res_3_e,res_3_h,res_3_l,res_3_xhl,res_3_a,
 res_4_b,res_4_c,res_4_d,res_4_e,res_4_h,res_4_l,res_4_xhl,res_4_a,
 res_5_b,res_5_c,res_5_d,res_5_e,res_5_h,res_5_l,res_5_xhl,res_5_a,
 res_6_b,res_6_c,res_6_d,res_6_e,res_6_h,res_6_l,res_6_xhl,res_6_a,
 res_7_b,res_7_c,res_7_d,res_7_e,res_7_h,res_7_l,res_7_xhl,res_7_a,
 set_0_b,set_0_c,set_0_d,set_0_e,set_0_h,set_0_l,set_0_xhl,set_0_a,
 set_1_b,set_1_c,set_1_d,set_1_e,set_1_h,set_1_l,set_1_xhl,set_1_a,
 set_2_b,set_2_c,set_2_d,set_2_e,set_2_h,set_2_l,set_2_xhl,set_2_a,
 set_3_b,set_3_c,set_3_d,set_3_e,set_3_h,set_3_l,set_3_xhl,set_3_a,
 set_4_b,set_4_c,set_4_d,set_4_e,set_4_h,set_4_l,set_4_xhl,set_4_a,
 set_5_b,set_5_c,set_5_d,set_5_e,set_5_h,set_5_l,set_5_xhl,set_5_a,
 set_6_b,set_6_c,set_6_d,set_6_e,set_6_h,set_6_l,set_6_xhl,set_6_a,
 set_7_b,set_7_c,set_7_d,set_7_e,set_7_h,set_7_l,set_7_xhl,set_7_a
};

static opcode_fn opcode_dd[256]=
{
  no_op   ,no_op     ,no_op      ,no_op    ,no_op      ,no_op      ,no_op      ,no_op   ,
  no_op   ,add_ix_bc ,no_op      ,no_op    ,no_op      ,no_op      ,no_op      ,no_op   ,
  no_op   ,no_op     ,no_op      ,no_op    ,no_op      ,no_op      ,no_op      ,no_op   ,
  no_op   ,add_ix_de ,no_op      ,no_op    ,no_op      ,no_op      ,no_op      ,no_op   ,
  no_op   ,ld_ix_word,ld_xword_ix,inc_ix   ,inc_ixh    ,dec_ixh    ,ld_ixh_byte,no_op   ,
  no_op   ,add_ix_ix ,ld_ix_xword,dec_ix   ,inc_ixl    ,dec_ixl    ,ld_ixl_byte,no_op   ,
  no_op   ,no_op     ,no_op      ,no_op    ,inc_xix    ,dec_xix    ,ld_xix_byte,no_op   ,
  no_op   ,add_ix_sp ,no_op      ,no_op    ,no_op      ,no_op      ,no_op      ,no_op   ,
  no_op   ,no_op     ,no_op      ,no_op    ,ld_b_ixh   ,ld_b_ixl   ,ld_b_xix   ,no_op   ,
  no_op   ,no_op     ,no_op      ,no_op    ,ld_c_ixh   ,ld_c_ixl   ,ld_c_xix   ,no_op   ,
  no_op   ,no_op     ,no_op      ,no_op    ,ld_d_ixh   ,ld_d_ixl   ,ld_d_xix   ,no_op   ,
  no_op   ,no_op     ,no_op      ,no_op    ,ld_e_ixh   ,ld_e_ixl   ,ld_e_xix   ,no_op   ,
  ld_ixh_b,ld_ixh_c  ,ld_ixh_d   ,ld_ixh_e ,ld_ixh_ixh ,ld_ixh_ixl ,ld_h_xix   ,ld_ixh_a,
  ld_ixl_b,ld_ixl_c  ,ld_ixl_d   ,ld_ixl_e ,ld_ixl_ixh ,ld_ixl_ixl ,ld_l_xix   ,ld_ixl_a,
  ld_xix_b,ld_xix_c  ,ld_xix_d   ,ld_xix_e ,ld_xix_h   ,ld_xix_l   ,no_op      ,ld_xix_a,
  no_op   ,no_op     ,no_op      ,no_op    ,ld_a_ixh   ,ld_a_ixl   ,ld_a_xix   ,no_op   ,
  no_op   ,no_op     ,no_op      ,no_op    ,add_a_ixh  ,add_a_ixl  ,add_a_xix  ,no_op   ,
  no_op   ,no_op     ,no_op      ,no_op    ,adc_a_ixh  ,adc_a_ixl  ,adc_a_xix  ,no_op   ,
  no_op   ,no_op     ,no_op      ,no_op    ,sub_ixh    ,sub_ixl    ,sub_xix    ,no_op   ,
  no_op   ,no_op     ,no_op      ,no_op    ,sbc_a_ixh  ,sbc_a_ixl  ,sbc_a_xix  ,no_op   ,
  no_op   ,no_op     ,no_op      ,no_op    ,and_ixh    ,and_ixl    ,and_xix    ,no_op   ,
  no_op   ,no_op     ,no_op      ,no_op    ,xor_ixh    ,xor_ixl    ,xor_xix    ,no_op   ,
  no_op   ,no_op     ,no_op      ,no_op    ,or_ixh     ,or_ixl     ,or_xix     ,no_op   ,
  no_op   ,no_op     ,no_op      ,no_op    ,cp_ixh     ,cp_ixl     ,cp_xix     ,no_op   ,
  no_op   ,no_op     ,no_op      ,no_op    ,no_op      ,no_op      ,no_op      ,no_op   ,
  no_op   ,no_op     ,no_op      ,dd_cb    ,no_op      ,no_op      ,no_op      ,no_op   ,
  no_op   ,no_op     ,no_op      ,no_op    ,no_op      ,no_op      ,no_op      ,no_op   ,
  no_op   ,no_op     ,no_op      ,no_op    ,no_op      ,no_op      ,no_op      ,no_op   ,
  no_op   ,pop_ix    ,no_op      ,ex_xsp_ix,no_op      ,push_ix    ,no_op      ,no_op   ,
  no_op   ,jp_ix     ,no_op      ,no_op    ,no_op      ,no_op      ,no_op      ,no_op   ,
  no_op   ,no_op     ,no_op      ,no_op    ,no_op      ,no_op      ,no_op      ,no_op   ,
  no_op   ,ld_sp_ix  ,no_op      ,no_op    ,no_op      ,no_op      ,no_op      ,no_op
};

static opcode_fn opcode_ed[256]=
{
 nop   ,nop    ,nop      ,nop        ,nop,nop ,nop  ,nop   ,
 nop   ,nop    ,nop      ,nop        ,nop,nop ,nop  ,nop   ,
 nop   ,nop    ,nop      ,nop        ,nop,nop ,nop  ,nop   ,
 nop   ,nop    ,nop      ,nop        ,nop,nop ,nop  ,nop   ,
 nop   ,nop    ,nop      ,nop        ,nop,nop ,nop  ,nop   ,
 nop   ,nop    ,nop      ,nop        ,nop,nop ,nop  ,nop   ,
 nop   ,nop    ,nop      ,nop        ,nop,nop ,nop  ,nop   ,
 nop   ,nop    ,nop      ,nop        ,nop,nop ,nop  ,nop   ,
 in_b_c,out_c_b,sbc_hl_bc,ld_xword_bc,neg,retn,im_0 ,ld_i_a,
 in_c_c,out_c_c,adc_hl_bc,ld_bc_xword,neg,reti,im_0 ,ld_r_a,
 in_d_c,out_c_d,sbc_hl_de,ld_xword_de,neg,retn,im_1 ,ld_a_i,
 in_e_c,out_c_e,adc_hl_de,ld_de_xword,neg,reti,im_2 ,ld_a_r,
 in_h_c,out_c_h,sbc_hl_hl,ld_xword_hl,neg,retn,im_0 ,rrd   ,
 in_l_c,out_c_l,adc_hl_hl,ld_hl_xword,neg,reti,im_0 ,rld   ,
 in_0_c,out_c_0,sbc_hl_sp,ld_xword_sp,neg,retn,im_1 ,nop   ,
 in_a_c,out_c_a,adc_hl_sp,ld_sp_xword,neg,reti,im_2 ,nop   ,
 nop   ,nop    ,nop      ,nop        ,nop,nop ,nop  ,nop   ,
 nop   ,nop    ,nop      ,nop        ,nop,nop ,nop  ,nop   ,
 nop   ,nop    ,nop      ,nop        ,nop,nop ,nop  ,nop   ,
 nop   ,nop    ,nop      ,nop        ,nop,nop ,nop  ,nop   ,
 ldi   ,cpi    ,ini      ,outi       ,nop,nop ,nop  ,nop   ,
 ldd   ,cpd    ,ind      ,outd       ,nop,nop ,nop  ,nop   ,
 ldir  ,cpir   ,inir     ,otir       ,nop,nop ,nop  ,nop   ,
 lddr  ,cpdr   ,indr     ,otdr       ,nop,nop ,nop  ,nop   ,
 nop   ,nop    ,nop      ,nop        ,nop,nop ,nop  ,nop   ,
 nop   ,nop    ,nop      ,nop        ,nop,nop ,nop  ,nop   ,
 nop   ,nop    ,nop      ,nop        ,nop,nop ,nop  ,nop   ,
 nop   ,nop    ,nop      ,nop        ,nop,nop ,nop  ,nop   ,
 nop   ,nop    ,nop      ,nop        ,nop,nop ,nop  ,nop   ,
 nop   ,nop    ,nop      ,nop        ,nop,nop ,nop  ,nop   ,
 nop   ,nop    ,nop      ,nop        ,nop,nop ,nop  ,nop   ,
 nop   ,nop    ,nop      ,nop        ,nop,nop ,patch,nop
};

static opcode_fn opcode_fd[256]=
{
  no_op   ,no_op     ,no_op      ,no_op    ,no_op      ,no_op      ,no_op      ,no_op   ,
  no_op   ,add_iy_bc ,no_op      ,no_op    ,no_op      ,no_op      ,no_op      ,no_op   ,
  no_op   ,no_op     ,no_op      ,no_op    ,no_op      ,no_op      ,no_op      ,no_op   ,
  no_op   ,add_iy_de ,no_op      ,no_op    ,no_op      ,no_op      ,no_op      ,no_op   ,
  no_op   ,ld_iy_word,ld_xword_iy,inc_iy   ,inc_iyh    ,dec_iyh    ,ld_iyh_byte,no_op   ,
  no_op   ,add_iy_iy ,ld_iy_xword,dec_iy   ,inc_iyl    ,dec_iyl    ,ld_iyl_byte,no_op   ,
  no_op   ,no_op     ,no_op      ,no_op    ,inc_xiy    ,dec_xiy    ,ld_xiy_byte,no_op   ,
  no_op   ,add_iy_sp ,no_op      ,no_op    ,no_op      ,no_op      ,no_op      ,no_op   ,
  no_op   ,no_op     ,no_op      ,no_op    ,ld_b_iyh   ,ld_b_iyl   ,ld_b_xiy   ,no_op   ,
  no_op   ,no_op     ,no_op      ,no_op    ,ld_c_iyh   ,ld_c_iyl   ,ld_c_xiy   ,no_op   ,
  no_op   ,no_op     ,no_op      ,no_op    ,ld_d_iyh   ,ld_d_iyl   ,ld_d_xiy   ,no_op   ,
  no_op   ,no_op     ,no_op      ,no_op    ,ld_e_iyh   ,ld_e_iyl   ,ld_e_xiy   ,no_op   ,
  ld_iyh_b,ld_iyh_c  ,ld_iyh_d   ,ld_iyh_e ,ld_iyh_iyh ,ld_iyh_iyl ,ld_h_xiy   ,ld_iyh_a,
  ld_iyl_b,ld_iyl_c  ,ld_iyl_d   ,ld_iyl_e ,ld_iyl_iyh ,ld_iyl_iyl ,ld_l_xiy   ,ld_iyl_a,
  ld_xiy_b,ld_xiy_c  ,ld_xiy_d   ,ld_xiy_e ,ld_xiy_h   ,ld_xiy_l   ,no_op      ,ld_xiy_a,
  no_op   ,no_op     ,no_op      ,no_op    ,ld_a_iyh   ,ld_a_iyl   ,ld_a_xiy   ,no_op   ,
  no_op   ,no_op     ,no_op      ,no_op    ,add_a_iyh  ,add_a_iyl  ,add_a_xiy  ,no_op   ,
  no_op   ,no_op     ,no_op      ,no_op    ,adc_a_iyh  ,adc_a_iyl  ,adc_a_xiy  ,no_op   ,
  no_op   ,no_op     ,no_op      ,no_op    ,sub_iyh    ,sub_iyl    ,sub_xiy    ,no_op   ,
  no_op   ,no_op     ,no_op      ,no_op    ,sbc_a_iyh  ,sbc_a_iyl  ,sbc_a_xiy  ,no_op   ,
  no_op   ,no_op     ,no_op      ,no_op    ,and_iyh    ,and_iyl    ,and_xiy    ,no_op   ,
  no_op   ,no_op     ,no_op      ,no_op    ,xor_iyh    ,xor_iyl    ,xor_xiy    ,no_op   ,
  no_op   ,no_op     ,no_op      ,no_op    ,or_iyh     ,or_iyl     ,or_xiy     ,no_op   ,
  no_op   ,no_op     ,no_op      ,no_op    ,cp_iyh     ,cp_iyl     ,cp_xiy     ,no_op   ,
  no_op   ,no_op     ,no_op      ,no_op    ,no_op      ,no_op      ,no_op      ,no_op   ,
  no_op   ,no_op     ,no_op      ,fd_cb    ,no_op      ,no_op      ,no_op      ,no_op   ,
  no_op   ,no_op     ,no_op      ,no_op    ,no_op      ,no_op      ,no_op      ,no_op   ,
  no_op   ,no_op     ,no_op      ,no_op    ,no_op      ,no_op      ,no_op      ,no_op   ,
  no_op   ,pop_iy    ,no_op      ,ex_xsp_iy,no_op      ,push_iy    ,no_op      ,no_op   ,
  no_op   ,jp_iy     ,no_op      ,no_op    ,no_op      ,no_op      ,no_op      ,no_op   ,
  no_op   ,no_op     ,no_op      ,no_op    ,no_op      ,no_op      ,no_op      ,no_op   ,
  no_op   ,ld_sp_iy  ,no_op      ,no_op    ,no_op      ,no_op      ,no_op      ,no_op
};

static void cb(void)
{
 unsigned opcode;
 ++R.R;
 opcode=M_RDOP(R.PC.D);
 R.PC.W.l++;
 Z80_ICount+=cycles_cb[opcode];
 (*(opcode_cb[opcode]))();
}
static void dd(void)
{
 unsigned opcode;
 ++R.R;
 opcode=M_RDOP(R.PC.D);
 R.PC.W.l++;
 Z80_ICount+=cycles_xx[opcode];
 (*(opcode_dd[opcode]))();
}
static void ed(void)
{
 unsigned opcode;
 ++R.R;
 opcode=M_RDOP(R.PC.D);
 R.PC.W.l++;
 Z80_ICount+=cycles_ed[opcode];
 (*(opcode_ed[opcode]))();
}
static void fd (void)
{
 unsigned opcode;
 ++R.R;
 opcode=M_RDOP(R.PC.D);
 R.PC.W.l++;
 Z80_ICount+=cycles_xx[opcode];
 (*(opcode_fd[opcode]))();
}

static opcode_fn opcode_main[256]=
{
 nop     ,ld_bc_word,ld_xbc_a   ,inc_bc    ,inc_b   ,dec_b   ,ld_b_byte  ,rlca    ,
 ex_af_af,add_hl_bc ,ld_a_xbc   ,dec_bc    ,inc_c   ,dec_c   ,ld_c_byte  ,rrca    ,
 djnz    ,ld_de_word,ld_xde_a   ,inc_de    ,inc_d   ,dec_d   ,ld_d_byte  ,rla     ,
 jr      ,add_hl_de ,ld_a_xde   ,dec_de    ,inc_e   ,dec_e   ,ld_e_byte  ,rra     ,
 jr_nz   ,ld_hl_word,ld_xword_hl,inc_hl    ,inc_h   ,dec_h   ,ld_h_byte  ,daa     ,
 jr_z    ,add_hl_hl ,ld_hl_xword,dec_hl    ,inc_l   ,dec_l   ,ld_l_byte  ,cpl     ,
 jr_nc   ,ld_sp_word,ld_xbyte_a ,inc_sp    ,inc_xhl ,dec_xhl ,ld_xhl_byte,scf     ,
 jr_c    ,add_hl_sp ,ld_a_xbyte ,dec_sp    ,inc_a   ,dec_a   ,ld_a_byte  ,ccf     ,
 ld_b_b  ,ld_b_c    ,ld_b_d     ,ld_b_e    ,ld_b_h  ,ld_b_l  ,ld_b_xhl   ,ld_b_a  ,
 ld_c_b  ,ld_c_c    ,ld_c_d     ,ld_c_e    ,ld_c_h  ,ld_c_l  ,ld_c_xhl   ,ld_c_a  ,
 ld_d_b  ,ld_d_c    ,ld_d_d     ,ld_d_e    ,ld_d_h  ,ld_d_l  ,ld_d_xhl   ,ld_d_a  ,
 ld_e_b  ,ld_e_c    ,ld_e_d     ,ld_e_e    ,ld_e_h  ,ld_e_l  ,ld_e_xhl   ,ld_e_a  ,
 ld_h_b  ,ld_h_c    ,ld_h_d     ,ld_h_e    ,ld_h_h  ,ld_h_l  ,ld_h_xhl   ,ld_h_a  ,
 ld_l_b  ,ld_l_c    ,ld_l_d     ,ld_l_e    ,ld_l_h  ,ld_l_l  ,ld_l_xhl   ,ld_l_a  ,
 ld_xhl_b,ld_xhl_c  ,ld_xhl_d   ,ld_xhl_e  ,ld_xhl_h,ld_xhl_l,halt       ,ld_xhl_a,
 ld_a_b  ,ld_a_c    ,ld_a_d     ,ld_a_e    ,ld_a_h  ,ld_a_l  ,ld_a_xhl   ,ld_a_a  ,
 add_a_b ,add_a_c   ,add_a_d    ,add_a_e   ,add_a_h ,add_a_l ,add_a_xhl  ,add_a_a ,
 adc_a_b ,adc_a_c   ,adc_a_d    ,adc_a_e   ,adc_a_h ,adc_a_l ,adc_a_xhl  ,adc_a_a ,
 sub_b   ,sub_c     ,sub_d      ,sub_e     ,sub_h   ,sub_l   ,sub_xhl    ,sub_a   ,
 sbc_a_b ,sbc_a_c   ,sbc_a_d    ,sbc_a_e   ,sbc_a_h ,sbc_a_l ,sbc_a_xhl  ,sbc_a_a ,
 and_b   ,and_c     ,and_d      ,and_e     ,and_h   ,and_l   ,and_xhl    ,and_a   ,
 xor_b   ,xor_c     ,xor_d      ,xor_e     ,xor_h   ,xor_l   ,xor_xhl    ,xor_a   ,
 or_b    ,or_c      ,or_d       ,or_e      ,or_h    ,or_l    ,or_xhl     ,or_a    ,
 cp_b    ,cp_c      ,cp_d       ,cp_e      ,cp_h    ,cp_l    ,cp_xhl     ,cp_a    ,
 ret_nz  ,pop_bc    ,jp_nz      ,jp        ,call_nz ,push_bc ,add_a_byte ,rst_00  ,
 ret_z   ,ret       ,jp_z       ,cb        ,call_z  ,call    ,adc_a_byte ,rst_08  ,
 ret_nc  ,pop_de    ,jp_nc      ,out_byte_a,call_nc ,push_de ,sub_byte   ,rst_10  ,
 ret_c   ,exx       ,jp_c       ,in_a_byte ,call_c  ,dd      ,sbc_a_byte ,rst_18  ,
 ret_po  ,pop_hl    ,jp_po      ,ex_xsp_hl ,call_po ,push_hl ,and_byte   ,rst_20  ,
 ret_pe  ,jp_hl     ,jp_pe      ,ex_de_hl  ,call_pe ,ed      ,xor_byte   ,rst_28  ,
 ret_p   ,pop_af    ,jp_p       ,di        ,call_p  ,push_af ,or_byte    ,rst_30  ,
 ret_m   ,ld_sp_hl  ,jp_m       ,ei        ,call_m  ,fd      ,cp_byte    ,rst_38
};

static void ei(void)
{
 unsigned opcode;
 /* If interrupts were disabled, execute one more instruction and check the */
 /* IRQ line. If not, simply set interrupt flip-flop 2                      */
 if (!R.IFF1)
 {
#ifdef DEBUG
  if (R.PC.D==Z80_Trap) Z80_Trace=1;
  if (Z80_Trace) Z80_Debug(&R);
#endif
  R.IFF1=R.IFF2=1;
  ++R.R;
  opcode=M_RDOP(R.PC.D);
  R.PC.W.l++;
  Z80_ICount+=cycles_main[opcode];
  (*(opcode_main[opcode]))();
  Interrupt(Z80_IRQ);
 }
 else
  R.IFF2=1;
}

/****************************************************************************/
/* Reset registers to their initial values                                  */
/****************************************************************************/
void Z80_Reset (void)
{
 memset (&R,0,sizeof(Z80_Regs));
 R.SP.D=0x0000;  // [AppleWin] Modified from 0xF000 to 0x0000
 R.R=rand();
 Z80_ICount = 0; // [AppleWin] Modified from Z80_IPeriod to 0
}

/****************************************************************************/
/* Initialise the various lookup tables used by the emulation code          */
/****************************************************************************/
void InitTables (void)
{
 static int InitTables_virgin=1;
 byte zs;
 int i,p;
 if (!InitTables_virgin) return;
 InitTables_virgin=0;
 for (i=0;i<256;++i)
 {
  zs=0;
  if (i==0)
   zs|=Z_FLAG;
  if (i&0x80)
   zs|=S_FLAG;
  p=0;
  if (i&1) ++p;
  if (i&2) ++p;
  if (i&4) ++p;
  if (i&8) ++p;
  if (i&16) ++p;
  if (i&32) ++p;
  if (i&64) ++p;
  if (i&128) ++p;
  PTable[i]=(p&1)? 0:V_FLAG;
  ZSTable[i]=zs;
  ZSPTable[i]=zs|PTable[i];
 }
 for (i=0;i<256;++i)
 {
  ZSTable[i+256]=ZSTable[i]|C_FLAG;
  ZSPTable[i+256]=ZSPTable[i]|C_FLAG;
  PTable[i+256]=PTable[i]|C_FLAG;
 }
}

/****************************************************************************/
/* Issue an interrupt if necessary                                          */
/****************************************************************************/
static void Interrupt (int j)
{
 if (j==Z80_IGNORE_INT) return;
 if (j==Z80_NMI_INT || R.IFF1)
 {
  /* Clear interrupt flip-flop 1 */
  R.IFF1=0;
  /* Check if processor was halted */
  if (R.HALT)
  {
   ++R.PC.W.l;
   R.HALT=0;
  }
  if (j==Z80_NMI_INT)
  {
   M_PUSH (PC);
   R.PC.D=0x0066;
  }
  else
  {
   /* Interrupt mode 2. Call [R.I:databyte] */
   if (R.IM==2)
   {
    M_PUSH (PC);
    R.PC.D=M_RDMEM_WORD((j&255)|(R.I<<8));
   }
   else
    /* Interrupt mode 1. RST 38h */
    if (R.IM==1)
    {
     Z80_ICount+=cycles_main[0xFF];
     (*(opcode_main[0xFF]))();
    }
    else
    /* Interrupt mode 0. We check for CALL and JP instructions, if neither  */
    /* of these were found we assume a 1 byte opcode was placed on the      */
    /* databus                                                              */
    {
     switch (j&0xFF0000)
     {
      case 0xCD:
       M_PUSH(PC);
      case 0xC3:
       R.PC.D=j&0xFFFF;
       break;
      default:
       j&=255;
       Z80_ICount+=cycles_main[j];
       (*(opcode_main[j]))();
       break;
     }
    }
  }
 }
}

/****************************************************************************/
/* Set all registers to given values                                        */
/****************************************************************************/
void Z80_SetRegs (Z80_Regs *Regs)
{
 R=*Regs;
}

/****************************************************************************/
/* Get all registers in given buffer                                        */
/****************************************************************************/
void Z80_GetRegs (Z80_Regs *Regs)
{
 *Regs=R;
}

/****************************************************************************/
/* Return program counter                                                   */
/****************************************************************************/
unsigned Z80_GetPC (void)
{
 return R.PC.D;
}

#if 0 // [AppleWin] Not used
/****************************************************************************/
/* Execute IPeriod T-States. Return 0 if emulation should be stopped        */
/****************************************************************************/
int Z80_Execute (void)
{
 unsigned opcode;
 Z80_Running=1;
 InitTables ();
 do
 {
#ifdef TRACE
  pc_trace[pc_count]=R.PC.D;
  pc_count=(pc_count+1)&255;
#endif
#ifdef DEBUG
  if (R.PC.D==Z80_Trap) Z80_Trace=1;
  if (Z80_Trace) Z80_Debug(&R);
  if (!Z80_Running) break;
#endif
  ++R.R;
  opcode=M_RDOP(R.PC.D);
  R.PC.W.l++;
  Z80_ICount-=cycles_main[opcode];
  (*(opcode_main[opcode]))();
 }
 while (Z80_ICount>0);
 Z80_ICount+=Z80_IPeriod;
 Interrupt (Z80_Interrupt());
 return Z80_Running;
}

/****************************************************************************/
/* Interpret Z80 code                                                       */
/****************************************************************************/
word Z80 (void)
{
 while (Z80_Execute());
 return(R.PC.W.l);
}
#endif

/****************************************************************************/
/* Dump register contents and (optionally) a PC trace to stdout             */
/****************************************************************************/
void Z80_RegisterDump (void)
{
 int i;
 printf
 (
   "AF:%04X HL:%04X DE:%04X BC:%04X PC:%04X SP:%04X IX:%04X IY:%04X\n",
   R.AF.W.l,R.HL.W.l,R.DE.W.l,R.BC.W.l,R.PC.W.l,R.SP.W.l,R.IX.W.l,R.IY.W.l
 ); 
 printf ("STACK: ");
 for (i=0;i<10;++i) printf ("%04X ",M_RDMEM_WORD((R.SP.D+i*2)&0xFFFF));
 puts ("");
#ifdef TRACE
 puts ("PC TRACE:");
 for (i=1;i<=256;++i) printf ("%04X\n",pc_trace[(pc_count-i)&255]);
#endif
}

/****************************************************************************/
/* Set number of memory refresh wait states (i.e. extra cycles inserted     */
/* when the refresh register is being incremented)                          */
/****************************************************************************/
void Z80_SetWaitStates (int n)
{
 int i;
 for (i=0;i<256;++i)
 {
  cycles_main[i]+=n;
  cycles_cb[i]+=n;
  cycles_ed[i]+=n;
  cycles_xx[i]+=n;
 }
}

// AppleWin additions:

//===========================================================================

// NB. Z80_ICount can legitimately go -ve!

static const double uZ80ClockMultiplier = CLK_Z80 / CLK_6502;
inline static ULONG ConvertZ80TStatesTo6502Cycles(UINT uTStates)
{
	return (uTStates < 0) ? 0 : (ULONG) ((double)uTStates / uZ80ClockMultiplier);
}

DWORD InternalZ80Execute (ULONG totalcycles, ULONG uExecutedCycles)
{
	// Nb. If uExecutedCycles == 0 (single-step) then just execute a single opcode

	totalcycles     = (ULONG) ((double)totalcycles     * uZ80ClockMultiplier);
	uExecutedCycles = (ULONG) ((double)uExecutedCycles * uZ80ClockMultiplier);
	int cycles = uExecutedCycles;	// Must be signed int, as cycles can go -ve

	do {
#ifdef CPUDEBUG              // ...xxx...xxx  
		if (g_nCumulativeCycles >          0) fprintf(arquivocpu, "%4X\n", regs.pc);
		fflush(arquivocpu);
#endif					// Z80
#ifdef TRACE
		pc_trace[pc_count]=R.PC.D;
		pc_count=(pc_count+1)&255;
#endif
#ifdef DEBUG
		if (R.PC.D==Z80_Trap) Z80_Trace=1;
		if (Z80_Trace) Z80_Debug(&R);
#endif
		++R.R;
		unsigned int opcode = M_RDOP(R.PC.D);
		R.PC.W.l++;
		Z80_ICount = cycles;
		Z80_ICount += cycles_main[opcode];
		(*(opcode_main[opcode]))();
		cycles = Z80_ICount;
		if (g_ActiveCPU != CPU_Z80)
			break;
	}
	while (cycles < (int)totalcycles);
  
	Interrupt (Z80_Interrupt());

	return ConvertZ80TStatesTo6502Cycles(cycles - uExecutedCycles);
}

//
// ----- ALL GLOBALLY ACCESSIBLE FUNCTIONS ARE BELOW THIS LINE -----
//

// Implementação da CPU Z80:

/****************************************************************************/
/* Input a byte from given I/O port                                         */
/****************************************************************************/
byte Z80_In (byte Port)
{
	return 0;
}

/****************************************************************************/
/* Output a byte to given I/O port                                          */
/****************************************************************************/
void Z80_Out (byte Port,byte Value)
{
}

/****************************************************************************/
/* Read a byte from given memory location                                   */
/****************************************************************************/
unsigned Z80_RDMEM(DWORD Addr)
{
	WORD addr;

	switch (Addr / 0x1000)
	{
		case 0x0:
		case 0x1:
		case 0x2:
		case 0x3:
		case 0x4:
		case 0x5:
		case 0x6:
		case 0x7:
		case 0x8:
		case 0x9:
		case 0xA:
			addr = (WORD)Addr + 0x1000;
			return CpuRead( addr, ConvertZ80TStatesTo6502Cycles(Z80_ICount) );
		break;

		case 0xB:
		case 0xC:
		case 0xD:
			addr = (WORD)Addr + 0x2000;
			return CpuRead( addr, ConvertZ80TStatesTo6502Cycles(Z80_ICount) );
		break;

		case 0xE:
			addr = (WORD)Addr - 0x2000;
		    if ((addr & 0xF000) == 0xC000)
			{
				return IORead[(addr>>4) & 0xFF]( regs.pc, addr, 0, 0, ConvertZ80TStatesTo6502Cycles(Z80_ICount) );
			}
			else
			{
				return *(mem+addr);
			}
		break;

		case 0xF:
			addr = (WORD)Addr - 0xF000;
			return CpuRead( addr, ConvertZ80TStatesTo6502Cycles(Z80_ICount) );
		break;
	}
	return 255;
}

/****************************************************************************/
/* Write a byte to given memory location                                    */
/****************************************************************************/
void Z80_WRMEM(DWORD Addr, BYTE Value)
{
	unsigned int laddr;
	WORD addr;

	laddr = Addr & 0x0FFF;
	switch (Addr & 0xF000)
	{
		case 0x0000: addr = laddr+0x1000; break;
		case 0x1000: addr = laddr+0x2000; break;
		case 0x2000: addr = laddr+0x3000; break;
		case 0x3000: addr = laddr+0x4000; break;
		case 0x4000: addr = laddr+0x5000; break;
		case 0x5000: addr = laddr+0x6000; break;
		case 0x6000: addr = laddr+0x7000; break;
		case 0x7000: addr = laddr+0x8000; break;
		case 0x8000: addr = laddr+0x9000; break;
		case 0x9000: addr = laddr+0xA000; break;
		case 0xA000: addr = laddr+0xB000; break;
		case 0xB000: addr = laddr+0xD000; break;
		case 0xC000: addr = laddr+0xE000; break;
		case 0xD000: addr = laddr+0xF000; break;
		case 0xE000: addr = laddr+0xC000; break;
		case 0xF000: addr = laddr+0x0000; break;
	}
	CpuWrite( addr, Value, ConvertZ80TStatesTo6502Cycles(Z80_ICount) );
}

/* Called when ED FE occurs. Can be used */
/* to emulate disk access etc.           */
void Z80_Patch (Z80_Regs *Regs)
{
}

/* This is called after IPeriod T-States */
/* have been executed. It should return  */
/* Z80_IGNORE_INT, Z80_NMI_INT or a byte */
/* identifying the device (most often    */
/* 0xFF)                                 */
int Z80_Interrupt(void)
{
	return Z80_IGNORE_INT;
}

/* Called when RETI occurs               */
void Z80_Reti (void)
{
}

/* Called when RETN occurs               */
void Z80_Retn (void)
{
}

