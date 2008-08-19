/*** Z80Em: Portable Z80 emulator *******************************************/
/***                                                                      ***/
/***                                Z80IO.h                               ***/
/***                                                                      ***/
/*** This file contains the prototypes for the functions accessing memory ***/
/*** and I/O                                                              ***/
/***                                                                      ***/
/*** Copyright (C) Marcel de Kogel 1996,1997                              ***/
/***     You are not allowed to distribute this software commercially     ***/
/***     Please, notify me, if you make any changes to this file          ***/
/****************************************************************************/

/****************************************************************************/
/* Input a byte from given I/O port                                         */
/****************************************************************************/
byte Z80_In (BYTE Port);

/****************************************************************************/
/* Output a byte to given I/O port                                          */
/****************************************************************************/
void Z80_Out (BYTE Port,BYTE Value);

/****************************************************************************/
/* Read a byte from given memory location                                   */
/****************************************************************************/
unsigned Z80_RDMEM(DWORD A);

/****************************************************************************/
/* Write a byte to given memory location                                    */
/****************************************************************************/
void Z80_WRMEM(DWORD A,BYTE V);

/****************************************************************************/
/* Just to show you can actually use macros as well                         */
/****************************************************************************/
/*
 extern byte *ReadPage[256];
 extern byte *WritePage[256];
 #define Z80_RDMEM(a) ReadPage[(a)>>8][(a)&0xFF]
 #define Z80_WRMEM(a,v) WritePage[(a)>>8][(a)&0xFF]=v
*/

/****************************************************************************/
/* Z80_RDOP() is identical to Z80_RDMEM() except it is used for reading     */
/* opcodes. In case of system with memory mapped I/O, this function can be  */
/* used to greatly speed up emulation                                       */
/****************************************************************************/
#define Z80_RDOP(A)		Z80_RDMEM(A)

/****************************************************************************/
/* Z80_RDOP_ARG() is identical to Z80_RDOP() except it is used for reading  */
/* opcode arguments. This difference can be used to support systems that    */
/* use different encoding mechanisms for opcodes and opcode arguments       */
/****************************************************************************/
#define Z80_RDOP_ARG(A)		Z80_RDOP(A)

/****************************************************************************/
/* Z80_RDSTACK() is identical to Z80_RDMEM() except it is used for reading  */
/* stack variables. In case of system with memory mapped I/O, this function */
/* can be used to slightly speed up emulation                               */
/****************************************************************************/
#define Z80_RDSTACK(A)		Z80_RDMEM(A)

/****************************************************************************/
/* Z80_WRSTACK() is identical to Z80_WRMEM() except it is used for writing  */
/* stack variables. In case of system with memory mapped I/O, this function */
/* can be used to slightly speed up emulation                               */
/****************************************************************************/
#define Z80_WRSTACK(A,V)	Z80_WRMEM(A,V)

