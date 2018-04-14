/*
 * tfe.c - TFE ("The final ethernet") emulation.
 *
 * Written by
 *  Spiro Trikaliotis <Spiro.Trikaliotis@gmx.de>
 * 
 * This file is part of VICE, the Versatile Commodore Emulator.
 * See README for copyright notice.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 *  02111-1307  USA.
 *
 */

#include "../StdAfx.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef DOS_TFE
#include <pcap.h>
#endif

#include "tfe.h"
#include "tfearch.h"
#include "tfesupp.h"

#ifndef NULL
#define NULL 0
#endif
typedef unsigned int UINT;

#include "../Common.h"	// For: IS_APPLE2
#include "../Memory.h"

/**/
/** #define TFE_DEBUG_DUMP 1 **/

/* #define TFE_DEBUG_FRAMES - might be defined in TFE.H! */

#define TFE_DEBUG_WARN 1 /* this should not be deactivated */
#define TFE_DEBUG_INIT 1
/** #define TFE_DEBUG_LOAD 1 **/
/** #define TFE_DEBUG_STORE 1 **/
/**/

/* ------------------------------------------------------------------------- */
/*    variables needed                                                       */

/*
 This variable is used when we need to postpone the initialization
 because tfe_init() is not yet called
*/
static int should_activate = 0;


static int init_tfe_flag = 0;

/* status which received packages to accept 
   This is used in tfe_should_accept().
*/
static BYTE  tfe_ia_mac[6];

/* remember the value of the hash mask */
static DWORD tfe_hash_mask[2];

static int  tfe_recv_broadcast   = 0; /* broadcast */
static int  tfe_recv_mac         = 0; /* individual address (IA) */
static int  tfe_recv_multicast   = 0; /* multicast if address passes the hash filter */
static int  tfe_recv_correct     = 0; /* accept correct frames */
static int  tfe_recv_promiscuous = 0; /* promiscuous mode */
static int  tfe_recv_hashfilter  = 0; /* accept if IA passes the hash filter */


#ifdef TFE_DEBUG_WARN
/* remember if the TXCMD has been completed before a new one is issued */
static int tfe_started_tx       = 0;
#endif


/* Flag: Can we even use TFE, or is the hardware not available? */
static int tfe_cannot_use = 0;

/* Flag: Do we have the TFE enabled?  */
int tfe_enabled = 0;

/* Flag: Do we use the "original" memory map or the memory map of the RR-Net? */
//static int tfe_as_rr_net = 0;

char *tfe_interface = NULL;

/* TFE registers */
/* these are the 8 16-bit-ports for "I/O space configuration"
   (see 4.10 on page 75 of cs8900a-4.pdf, the cs8900a data sheet)

   REMARK: The TFE operatoes the cs8900a in IO space configuration, as
   it generates I/OW and I/OR signals.
*/
#define TFE_COUNT_IO_REGISTER 0x10 /* we have 16 I/O register */

static BYTE *tfe = NULL;
/*
	RW: RXTXDATA   = DE00/DE01
	RW: RXTXDATA2  = DE02/DE03 (for 32-bit-operation)
	-W: TXCMD      = DE04/DE05 (TxCMD, Transmit Command)   mapped to PP + 0144 (Reg. 9, Sec. 4.4, page 46)
	-W: TXLENGTH   = DE06/DE07 (TxLenght, Transmit Length) mapped to PP + 0146 
	R-: INTSTQUEUE = DE08/DE09 (Interrupt Status Queue)    mapped to PP + 0120 (ISQ, Sec. 5.1, page 78)
	RW: PP_PTR     = DE0A/DE0B (PacketPage Pointer)        (see. page 75p: Read -011.---- ----.----)
	RW: PP_DATA0   = DE0C/DE0D (PacketPage Data (Port 0))  
	RW: PP_DATA1   = DE0E/DE0F (PacketPage Data (Port 1))  (for 32 bit only)
*/

#define TFE_ADDR_RXTXDATA   0x00 /* RW */
#define TFE_ADDR_RXTXDATA2  0x02 /* RW 32 bit only! */
#define TFE_ADDR_TXCMD      0x04 /* -W Maps to PP+0144 */
#define TFE_ADDR_TXLENGTH   0x06 /* -W Maps to PP+0146 */
#define TFE_ADDR_INTSTQUEUE 0x08 /* R- Interrupt status queue, maps to PP + 0120 */
#define TFE_ADDR_PP_PTR     0x0a /* RW PacketPage Pointer */
#define TFE_ADDR_PP_DATA    0x0c /* RW PacketPage Data, Port 0 */
#define TFE_ADDR_PP_DATA2   0x0e /* RW PacketPage Data, Port 1 - 32 bit only */	

/* Makros for reading and writing the visible TFE register: */
#define GET_TFE_8(  _xxx_ ) \
	( assert(_xxx_<TFE_COUNT_IO_REGISTER), \
      tfe[_xxx_]                           \
    )

#define SET_TFE_8( _xxx_, _val_ ) \
	do { \
	    assert(_xxx_<TFE_COUNT_IO_REGISTER); \
        tfe[_xxx_  ] = (_val_     ) & 0xff; \
    } while (0) 

#define GET_TFE_16(  _xxx_ ) \
	( assert(_xxx_<TFE_COUNT_IO_REGISTER), \
      tfe[_xxx_] | (tfe[_xxx_+1] << 8)     \
    )

#define SET_TFE_16( _xxx_, _val_ ) \
	do { \
	    assert(_xxx_<TFE_COUNT_IO_REGISTER); \
        tfe[_xxx_  ] = (_val_     ) & 0xff; \
        tfe[_xxx_+1] = (_val_ >> 8) & 0xff; \
    } while (0) 

/* The PacketPage register */
/* note: The locations 0 to MAX_PACKETPAGE_ARRAY-1 are handled in this array. */

#define MAX_PACKETPAGE_ARRAY 0x1000 /* 4 KB */

static BYTE *tfe_packetpage = NULL;

static WORD tfe_packetpage_ptr = 0;

/* Makros for reading and writing the PacketPage register: */

#define GET_PP_8(  _xxx_ ) \
	(assert(_xxx_<MAX_PACKETPAGE_ARRAY), \
     tfe_packetpage[_xxx_] \
    )

#define GET_PP_16(  _xxx_ ) \
	( assert(_xxx_<MAX_PACKETPAGE_ARRAY),   \
      assert((_xxx_ & 1) == 0 ),            \
	  ((WORD)tfe_packetpage[_xxx_]        ) \
    | ((WORD)tfe_packetpage[_xxx_+1] <<  8) \
    )

#define GET_PP_32(  _xxx_ ) \
	( assert(_xxx_<MAX_PACKETPAGE_ARRAY),     \
      assert((_xxx_ & 3) == 0 ),              \
	  (((long)tfe_packetpage[_xxx_  ])      ) \
    | (((long)tfe_packetpage[_xxx_+1]) <<  8) \
    | (((long)tfe_packetpage[_xxx_+2]) << 16) \
    | (((long)tfe_packetpage[_xxx_+3]) << 24) \
    )

#define SET_PP_8( _xxx_, _val_ ) \
    do { \
	    assert(_xxx_<MAX_PACKETPAGE_ARRAY);           \
        tfe_packetpage[_xxx_  ] = (_val_    ) & 0xFF; \
    } while (0) 

#define SET_PP_16( _xxx_, _val_ ) \
    do { \
	    assert(_xxx_<MAX_PACKETPAGE_ARRAY);           \
        assert((_xxx_ & 1) == 0 ),                    \
        tfe_packetpage[_xxx_  ] = (_val_    ) & 0xFF; \
        tfe_packetpage[_xxx_+1] = (_val_>> 8) & 0xFF; \
    } while (0) 

#define SET_PP_32( _xxx_, _val_ ) \
    do { \
	    assert(_xxx_<MAX_PACKETPAGE_ARRAY);           \
        assert((_xxx_ & 3) == 0 ),                    \
        tfe_packetpage[_xxx_  ] = (_val_    ) & 0xFF; \
        tfe_packetpage[_xxx_+1] = (_val_>> 8) & 0xFF; \
        tfe_packetpage[_xxx_+2] = (_val_>>16) & 0xFF; \
        tfe_packetpage[_xxx_+3] = (_val_>>24) & 0xFF; \
    } while (0) 


/* The packetpage register: see p. 39f */
#define TFE_PP_ADDR_PRODUCTID       0x0000 /*   R- - 4.3., p. 41 */
#define TFE_PP_ADDR_IOBASE          0x0020 /* i RW - 4.3., p. 41 - 4.7., p. 72 */
#define TFE_PP_ADDR_INTNO           0x0022 /* i RW - 3.2., p. 17 - 4.3., p. 41 */
#define TFE_PP_ADDR_DMA_CHAN        0x0024 /* i RW - 3.2., p. 17 - 4.3., p. 41 */
#define TFE_PP_ADDR_DMA_SOF         0x0026 /* ? R- - 4.3., p. 41 - 5.4., p. 89 */
#define TFE_PP_ADDR_DMA_FC          0x0028 /* ? R- - 4.3., p. 41, "Receive DMA" */
#define TFE_PP_ADDR_RXDMA_BC        0x002a /* ? R- - 4.3., p. 41 - 5.4., p. 89 */
#define TFE_PP_ADDR_MEMBASE         0x002c /* i RW - 4.3., p. 41 - 4.9., p. 73 */
#define TFE_PP_ADDR_BPROM_BASE      0x0030 /* i RW - 3.6., p. 24 - 4.3., p. 41 */
#define TFE_PP_ADDR_BPROM_MASK      0x0034 /* i RW - 3.6., p. 24 - 4.3., p. 41 */
/* 0x0038 - 0x003F: reserved */
#define TFE_PP_ADDR_EEPROM_CMD      0x0040 /* i RW - 3.5., p. 23 - 4.3., p. 41 */
#define TFE_PP_ADDR_EEPROM_DATA     0x0042 /* i RW - 3.5., p. 23 - 4.3., p. 41 */
/* 0x0044 - 0x004F: reserved */
#define TFE_PP_ADDR_REC_FRAME_BC    0x0050 /*   RW - 4.3., p. 41 - 5.2.9., p. 86 */
/* 0x0052 - 0x00FF: reserved */
#define TFE_PP_ADDR_CONF_CTRL       0x0100 /* - RW - 4.4., p. 46; see below */
#define TFE_PP_ADDR_STATUS_EVENT    0x0120 /* - R- - 4.4., p. 46; see below */
/* 0x0140 - 0x0143: reserved */
#define TFE_PP_ADDR_TXCMD           0x0144 /* # -W - 4.5., p. 70 - 5.7., p. 98 */
#define TFE_PP_ADDR_TXLENGTH        0x0146 /* # -W - 4.5., p. 70 - 5.7., p. 98 */
/* 0x0148 - 0x014F: reserved */
#define TFE_PP_ADDR_LOG_ADDR_FILTER 0x0150 /*   RW - 4.6., p. 71 - 5.3., p. 86 */
#define TFE_PP_ADDR_MAC_ADDR        0x0158 /* # RW - 4.6., p. 71 - 5.3., p. 86 */
/* 0x015E - 0x03FF: reserved */
#define TFE_PP_ADDR_RXSTATUS        0x0400 /*   R- - 4.7., p. 72 - 5.2., p. 78 */
#define TFE_PP_ADDR_RXLENGTH        0x0402 /*   R- - 4.7., p. 72 - 5.2., p. 78 */
#define TFE_PP_ADDR_RX_FRAMELOC     0x0404 /*   R- - 4.7., p. 72 - 5.2., p. 78 */
/* here, the received frame is stored */
#define TFE_PP_ADDR_TX_FRAMELOC     0x0A00 /*   -W - 4.7., p. 72 - 5.7., p. 98 */
/* here, the frame to transmit is stored */
#define TFE_PP_ADDR_END             0x1000 /* memory to TFE_PP_ADDR_END-1 is used */


/* TFE_PP_ADDR_CONF_CTRL is subdivided: */
#define TFE_PP_ADDR_CC_RXCFG        0x0102 /* # RW - 4.4.6.,  p. 52 - 0003 */
#define TFE_PP_ADDR_CC_RXCTL        0x0104 /* # RW - 4.4.8.,  p. 54 - 0005 */
#define TFE_PP_ADDR_CC_TXCFG        0x0106 /*   RW - 4.4.9.,  p. 55 - 0007 */
#define TFE_PP_ADDR_CC_TXCMD        0x0108 /*   R- - 4.4.11., p. 57 - 0009 */
#define TFE_PP_ADDR_CC_BUFCFG       0x010A /*   RW - 4.4.12., p. 58 - 000B */
#define TFE_PP_ADDR_CC_LINECTL      0x0112 /* # RW - 4.4.16., p. 62 - 0013 */
#define TFE_PP_ADDR_CC_SELFCTL      0x0114 /*   RW - 4.4.18., p. 64 - 0015 */
#define TFE_PP_ADDR_CC_BUSCTL       0x0116 /*   RW - 4.4.20., p. 66 - 0017 */
#define TFE_PP_ADDR_CC_TESTCTL      0x0118 /*   RW - 4.4.22., p. 68 - 0019 */

/* TFE_PP_ADDR_STATUS_EVENT is subdivided: */
#define TFE_PP_ADDR_SE_ISQ          0x0120 /*   R- - 4.4.5.,  p. 51 - 0000 */
#define TFE_PP_ADDR_SE_RXEVENT      0x0124 /* # R- - 4.4.7.,  p. 53 - 0004 */
#define TFE_PP_ADDR_SE_TXEVENT      0x0128 /*   R- - 4.4.10., p. 56 - 0008 */
#define TFE_PP_ADDR_SE_BUFEVENT     0x012C /*   R- - 4.4.13., p. 59 - 000C */
#define TFE_PP_ADDR_SE_RXMISS       0x0130 /*   R- - 4.4.14., p. 60 - 0010 */
#define TFE_PP_ADDR_SE_TXCOL        0x0132 /*   R- - 4.4.15., p. 61 - 0012 */
#define TFE_PP_ADDR_SE_LINEST       0x0134 /*   R- - 4.4.17., p. 63 - 0014 */
#define TFE_PP_ADDR_SE_SELFST       0x0136 /*   R- - 4.4.19., p. 65 - 0016 */
#define TFE_PP_ADDR_SE_BUSST        0x0138 /* # R- - 4.4.21., p. 67 - 0018 */
#define TFE_PP_ADDR_SE_TDR          0x013C /*   R- - 4.4.23., p. 69 - 001C */


/* ------------------------------------------------------------------------- */
/*    more variables needed                                                  */

static WORD txcollect_buffer = TFE_PP_ADDR_TX_FRAMELOC;
static WORD rx_buffer        = TFE_PP_ADDR_RXSTATUS;



/* ------------------------------------------------------------------------- */
/*    some parameter definitions                                             */

#define MAX_TXLENGTH 1518
#define MIN_TXLENGTH 4

#define MAX_RXLENGTH 1518
#define MIN_RXLENGTH 64


/* ------------------------------------------------------------------------- */
/*    debugging functions                                                    */

#ifdef TFE_DEBUG_FRAMES

static int TfeDebugMaxFrameLengthToDump = 150;

char *debug_outbuffer(const int length, const unsigned char * const buffer)
{
#define MAXLEN_DEBUG 1600

    int i;
    static char outbuffer[MAXLEN_DEBUG*4+1];
    char *p = outbuffer;

    assert( TfeDebugMaxFrameLengthToDump <= MAXLEN_DEBUG );
    
    *p = 0;

    for (i=0; i<TfeDebugMaxFrameLengthToDump; i++) {
        if (i>=length)
            break;
    
        sprintf( p, "%02X%c", buffer[i], ((i+1)%16==0)?'*':(((i+1)%8==0)?'-':' '));
        p+=3;
    }

    return outbuffer;
}

#endif


#ifdef TFE_DEBUG_DUMP

#define NUMBER_PER_LINE 8

static
void tfe_debug_output_general( char *what, WORD (*getFunc)(int), int count )
{
    int i;
    char buffer[7+(6*NUMBER_PER_LINE)+2];

    if(g_fh) fprintf(g_fh, "%s contents:", what );
    for (i=0; i<count; i += 2*NUMBER_PER_LINE )
    {
        int j;
        char *p = buffer + 7;

        sprintf( buffer, "%04X:  ", i );

        for (j=0; j<NUMBER_PER_LINE; j++) 
        {
            sprintf( p, "%04X, ", (*getFunc)(i+j+j) );
            p += 6;
        }
        *p = 0;

        if(g_fh) fprintf(g_fh, "%s", buffer );
    }
}

static 
WORD tfe_debug_output_io_getFunc( int i )
{
    return GET_TFE_16(i);
}

static
void tfe_debug_output_io( void )
{
    tfe_debug_output_general( "TFE I/O", tfe_debug_output_io_getFunc, TFE_COUNT_IO_REGISTER );
}
#define TFE_DEBUG_OUTPUT_IO() tfe_debug_output_io()

static 
WORD tfe_debug_output_pp_getFunc( int i )
{
    return GET_PP_16(i);
}

static
void tfe_debug_output_pp( void )
{
    tfe_debug_output_general( "PacketPage", tfe_debug_output_pp_getFunc, 0x0160 /* MAX_PACKETPAGE_ARRAY */ );
}
#define TFE_DEBUG_OUTPUT_PP() tfe_debug_output_pp()

#define TFE_DEBUG_OUTPUT_REG() \
    do { TFE_DEBUG_OUTPUT_IO(); TFE_DEBUG_OUTPUT_PP(); } while (0)

#else

    #define TFE_DEBUG_OUTPUT_IO()
    #define TFE_DEBUG_OUTPUT_PP()
    #define TFE_DEBUG_OUTPUT_REG()

#endif

/* ------------------------------------------------------------------------- */
/*    initialization and deinitialization functions                          */

BYTE __stdcall TfeIoCxxx (WORD programcounter, WORD address, BYTE write, BYTE value, ULONG nCycles);
BYTE __stdcall TfeIo (WORD programcounter, WORD address, BYTE write, BYTE value, ULONG nCycles);

void tfe_reset(void)
{
    if (tfe_enabled && !should_activate)
    {
        assert( tfe );
        assert( tfe_packetpage );

        tfe_arch_pre_reset();

        /* initialize visible IO register and PacketPage registers */
        memset( tfe, 0, TFE_COUNT_IO_REGISTER );
        memset( tfe_packetpage, 0, MAX_PACKETPAGE_ARRAY );

        /* according to page 19 unless stated otherwise */
        SET_PP_32(TFE_PP_ADDR_PRODUCTID,      0x0700630E ); /* p.41: 0E630007 for Rev. B; reversed order! */
        SET_PP_16(TFE_PP_ADDR_IOBASE,         0x0300);
        SET_PP_16(TFE_PP_ADDR_INTNO,          0x0004); /* xxxx xxxx xxxx x100b */
        SET_PP_16(TFE_PP_ADDR_DMA_CHAN,       0x0003); /* xxxx xxxx xxxx xx11b */

#if 0 /* not needed since all memory is initialized with 0 */
        SET_PP_16(TFE_PP_ADDR_DMA_SOF,        0x0000);
        SET_PP_16(TFE_PP_ADDR_DMA_FC,         0x0000); /* x000h */
        SET_PP_16(TFE_PP_ADDR_RXDMA_BC,       0x0000);
        SET_PP_32(TFE_PP_ADDR_MEMBASE,        0x0000); /* xxx0 0000h */
        SET_PP_32(TFE_PP_ADDR_BPROM_BASE,     0x00000000); /* xxx0 0000h */
        SET_PP_32(TFE_PP_ADDR_BPROM_MASK,     0x00000000); /* xxx0 0000h */

        SET_PP_16(TFE_PP_ADDR_SE_ISQ,         0x0000); /* p. 51 */
#endif

        /* according to descriptions of the registers, see definitions of 
        TFE_PP_ADDR_CC_... and TFE_PP_ADDR_SE_... above! */

        SET_PP_16(TFE_PP_ADDR_CC_RXCFG,       0x0003);
        SET_PP_16(TFE_PP_ADDR_CC_RXCTL,       0x0005);
        SET_PP_16(TFE_PP_ADDR_CC_TXCFG,       0x0007);
        SET_PP_16(TFE_PP_ADDR_CC_TXCMD,       0x0009);
        SET_PP_16(TFE_PP_ADDR_CC_BUFCFG,      0x000B);
        SET_PP_16(TFE_PP_ADDR_CC_LINECTL,     0x0013);
        SET_PP_16(TFE_PP_ADDR_CC_SELFCTL,     0x0015);
        SET_PP_16(TFE_PP_ADDR_CC_BUSCTL,      0x0017);
        SET_PP_16(TFE_PP_ADDR_CC_TESTCTL,     0x0019);

        SET_PP_16(TFE_PP_ADDR_SE_ISQ,         0x0000);
        SET_PP_16(TFE_PP_ADDR_SE_RXEVENT,     0x0004);
        SET_PP_16(TFE_PP_ADDR_SE_TXEVENT,     0x0008);
        SET_PP_16(TFE_PP_ADDR_SE_BUFEVENT,    0x000C);
        SET_PP_16(TFE_PP_ADDR_SE_RXMISS,      0x0010);
        SET_PP_16(TFE_PP_ADDR_SE_TXCOL,       0x0012);
        SET_PP_16(TFE_PP_ADDR_SE_LINEST,      0x0014);
        SET_PP_16(TFE_PP_ADDR_SE_SELFST,      0x0016);
        SET_PP_16(TFE_PP_ADDR_SE_BUSST,       0x0018);
        SET_PP_16(TFE_PP_ADDR_SE_TDR,         0x001C);

        tfe_arch_post_reset();

        TFE_DEBUG_OUTPUT_REG();
    }

	const UINT uSlot = 3;
	RegisterIoHandler(uSlot, TfeIo, TfeIo, TfeIoCxxx, TfeIoCxxx, NULL, NULL);
}

#ifdef DOS_TFE
static void set_standard_tfe_interface(void)
{
    char *dev, errbuf[PCAP_ERRBUF_SIZE];
    dev = pcap_lookupdev(errbuf);
    util_string_set(&tfe_interface, dev);
}
#endif

static
int tfe_activate_i(void)
{
    assert( tfe == NULL );
    assert( tfe_packetpage == NULL );

#ifdef TFE_DEBUG
    if(g_fh) fprintf( g_fh, "tfe_activate_i()." );
#endif

    /* allocate memory for visible IO register */
	/* RGJ added BYTE * for AppleWin */
	tfe = (BYTE * )lib_malloc( TFE_COUNT_IO_REGISTER );
    if (tfe==NULL)
    {
#ifdef TFE_DEBUG_INIT
        if(g_fh) fprintf(g_fh, "tfe_activate_i: Allocating tfe failed.\n");
#endif
        tfe_enabled = 0;
        return 0;
    }

    /* allocate memory for PacketPage register */
	/* RGJ added BYTE * for AppleWin */
	tfe_packetpage = (BYTE * ) lib_malloc( MAX_PACKETPAGE_ARRAY );
    if (tfe_packetpage==NULL)
    {
#ifdef TFE_DEBUG_INIT
        if(g_fh) fprintf(g_fh, "tfe_activate: Allocating tfe_packetpage failed.\n");
#endif
        lib_free(tfe);
        tfe=NULL;
        tfe_enabled = 0;
        return 0;
    }

#ifdef TFE_DEBUG_INIT
    if(g_fh) fprintf(g_fh, "tfe_activate: Allocated memory successfully.\n");
    if(g_fh) fprintf(g_fh, "\ttfe at $%08X, tfe_packetpage at $%08X\n", uintptr_t(tfe), uintptr_t(tfe_packetpage) );
#endif

#ifdef DOS_TFE
    set_standard_tfe_interface();
#endif

    if (!tfe_arch_activate(tfe_interface)) {
        lib_free(tfe_packetpage);
        lib_free(tfe);
        tfe=NULL;
        tfe_packetpage=NULL;
        tfe_enabled = 0;
        tfe_cannot_use = 1;
        return 0;
    }

    /* virtually reset the LAN chip */
    tfe_reset();

    return 0;
}

static
int tfe_deactivate_i(void)
{
#ifdef TFE_DEBUG
    if(g_fh) fprintf( g_fh, "tfe_deactivate_i()." );
#endif

    assert(tfe && tfe_packetpage);

    tfe_arch_deactivate();

    lib_free(tfe);
    tfe = NULL;
    lib_free(tfe_packetpage);
    tfe_packetpage = NULL;
	return 0;
}

static
int tfe_activate(void) {
#ifdef TFE_DEBUG
    if(g_fh) fprintf( g_fh, "tfe_activate()." );
#endif

 if (init_tfe_flag) {
        return tfe_activate_i();
    }
    else {
        should_activate = 1;
    }
    return 0;
}

static
int tfe_deactivate(void) {
#ifdef TFE_DEBUG
    if(g_fh) fprintf( g_fh, "tfe_deactivate()." );
#endif

    if (should_activate)
        should_activate = 0;
    else {
        if (init_tfe_flag)
            return tfe_deactivate_i();
    }

    return 0;
}

void tfe_init(void)
{
    init_tfe_flag = 1;

    if (!tfe_arch_init()) {
        tfe_enabled = 0;
        tfe_cannot_use = 1;
    }

    if (should_activate) {
        should_activate = 0;
        if (tfe_activate() < 0) {
            tfe_enabled = 0;
            tfe_cannot_use = 1;
        }
    }
}

void tfe_shutdown(void)
{
    assert( (tfe && tfe_packetpage) || (!tfe && !tfe_packetpage));

    if (tfe)
        tfe_deactivate();

    if (tfe_interface != NULL)
        lib_free(tfe_interface);
}


/* ------------------------------------------------------------------------- */
/*    reading and writing TFE register functions                             */

/*
These registers are currently fully or partially supported:

TFE_PP_ADDR_CC_RXCFG        0x0102 * # RW - 4.4.6.,  p. 52 - 0003 *
TFE_PP_ADDR_CC_RXCTL        0x0104 * # RW - 4.4.8.,  p. 54 - 0005 *
TFE_PP_ADDR_CC_LINECTL      0x0112 * # RW - 4.4.16., p. 62 - 0013 *
TFE_PP_ADDR_SE_RXEVENT      0x0124 * # R- - 4.4.7.,  p. 53 - 0004 *
TFE_PP_ADDR_SE_BUSST        0x0138 * # R- - 4.4.21., p. 67 - 0018 *
TFE_PP_ADDR_TXCMD           0x0144 * # -W - 4.5., p. 70 - 5.7., p. 98 *
TFE_PP_ADDR_TXLENGTH        0x0146 * # -W - 4.5., p. 70 - 5.7., p. 98 *
TFE_PP_ADDR_MAC_ADDR        0x0158 * # RW - 4.6., p. 71 - 5.3., p. 86 *
                            0x015a
                            0x015c
*/


#ifdef TFE_DEBUG_FRAMES
    #define return( _x_ ) \
    { \
        int retval = _x_; \
        \
        if(g_fh) fprintf(g_fh, "%s correct_mac=%u, broadcast=%u, multicast=%u, hashed=%u, hash_index=%u", (retval? "+++ ACCEPTED":"--- rejected"), *pcorrect_mac, *pbroadcast, *pmulticast, *phashed, *phash_index); \
        \
        return retval; \
    }

#endif
/*
 This is a helper for tfe_receive() to determine if the received frame should be accepted
 according to the settings.

 This function is even allowed to be called in tfearch.c from tfe_arch_receive() if 
 necessary, which is the reason why its prototype is included here in tfearch.h.
*/
int tfe_should_accept(unsigned char *buffer, int length, int *phashed, int *phash_index, 
                      int *pcorrect_mac, int *pbroadcast, int *pmulticast) 
{
	int hashreg; /* Hash Register (for hash computation) */

    assert(length>=6); /* we need at least 6 octets since the DA has this length */

    /* first of all, delete any status */
    *phashed      = 0;
    *phash_index  = 0;
    *pcorrect_mac = 0;
    *pbroadcast   = 0;
    *pmulticast   = 0;

#ifdef TFE_DEBUG_FRAMES
    if(g_fh) fprintf(g_fh, "tfe_should_accept called with %02X:%02X:%02X:%02X:%02X:%02X, length=%4u and buffer %s", 
        tfe_ia_mac[0], tfe_ia_mac[1], tfe_ia_mac[2],
        tfe_ia_mac[3], tfe_ia_mac[4], tfe_ia_mac[5],
        length,
        debug_outbuffer(length, buffer)
        );
#endif


    if (   buffer[0]==tfe_ia_mac[0]
        && buffer[1]==tfe_ia_mac[1]
        && buffer[2]==tfe_ia_mac[2]
        && buffer[3]==tfe_ia_mac[3]
        && buffer[4]==tfe_ia_mac[4]
        && buffer[5]==tfe_ia_mac[5]
       ) {
        /* this is our individual address (IA) */

        *pcorrect_mac = 1;

        /* if we don't want "correct MAC", we might have the chance
         * that this address fits the hash index 
         */
        if (tfe_recv_mac || tfe_recv_promiscuous) 
            return(1);
    }

    if (   buffer[0]==0xFF
        && buffer[1]==0xFF
        && buffer[2]==0xFF
        && buffer[3]==0xFF
        && buffer[4]==0xFF
        && buffer[5]==0xFF
       ) {
        /* this is a broadcast address */
        *pbroadcast = 1;

        /* broadcasts cannot be accepted by the hash filter */
            return((tfe_recv_broadcast || tfe_recv_promiscuous) ? 1 : 0);
    }

	/* now check if DA passes the hash filter */
	/* RGJ added (const char *) for AppleWin */
	hashreg = (~crc32_buf((const char *)buffer,6) >> 26) & 0x3F;

    *phashed = (tfe_hash_mask[(hashreg>=32)?1:0] & (1 << (hashreg&0x1F))) ? 1 : 0;
    if (*phashed) {
        *phash_index = hashreg;

        if (buffer[0] & 0x80) {
            /* we have a multicast address */
            *pmulticast = 1;

            /* if the multicast address fits into the hash filter, 
             * the hashed bit has to be clear 
             */
            *phashed = 0;

            return((tfe_recv_multicast || tfe_recv_promiscuous) ? 1 : 0);
        }
        return((tfe_recv_hashfilter || tfe_recv_promiscuous) ? 1 : 0);
    }
       
    return(tfe_recv_promiscuous ? 1 : 0);
}

#ifdef TFE_DEBUG_FRAMES
    #undef return
#endif

static 
WORD tfe_receive(void)
{
    WORD ret_val = 0x0004;

    BYTE buffer[MAX_RXLENGTH];

    int  len;
    int  hashed;
    int  hash_index;
    int  rx_ok;
    int  correct_mac;
    int  broadcast;
    int  multicast = 0;
    int  crc_error;

    int  newframe;

    int  ready;

#ifdef TFE_DEBUG_FRAMES
    if(g_fh) fprintf( g_fh, "");
#endif

    do {
        len = MAX_RXLENGTH;

        ready = 1 ; /* assume we will find a good frame */

        newframe = tfe_arch_receive(
            buffer,       /* where to store a frame */
            &len,         /* length of received frame */
            &hashed,      /* set if the dest. address is accepted by the hash filter */
            &hash_index,  /* hash table index if hashed == TRUE */   
            &rx_ok,       /* set if good CRC and valid length */
            &correct_mac, /* set if dest. address is exactly our IA */
            &broadcast,   /* set if dest. address is a broadcast address */
            &crc_error    /* set if received frame had a CRC error */
            );

        assert((len&1) == 0); /* length has to be even! */

        if (newframe) {
            if (hashed || correct_mac || broadcast) {
                /* we already know the type of frame: Trust it! */
#ifdef TFE_DEBUG_FRAMES
                if(g_fh) fprintf( g_fh, "+++ tfe_receive(): *** hashed=%u, correct_mac=%u, "
                    "broadcast=%u", hashed, correct_mac, broadcast);
#endif
            }
            else {
                /* determine ourself the type of frame */
                if (!tfe_should_accept(buffer, 
                    len, &hashed, &hash_index, &correct_mac, &broadcast, &multicast)) {

                    /* if we should not accept this frame, just do nothing
                     * now, look for another one */
                    ready = 0; /* try another frame */
                    continue;
                }
            }


            /* we did receive a frame, return that status */
            ret_val |= rx_ok     ? 0x0100 : 0;
            ret_val |= multicast ? 0x0200 : 0;

            if (!multicast) {
                ret_val |= hashed ? 0x0040 : 0;
            }

            if (hashed && rx_ok) {
                /* we have the 2nd, special format with hash index: */
                assert(hash_index < 64);
                ret_val |= hash_index << 9;
            }
            else {
                /* we have the regular format */
                ret_val |= correct_mac        ? 0x0400 : 0;
                ret_val |= broadcast          ? 0x0800 : 0;
                ret_val |= crc_error          ? 0x1000 : 0;
                ret_val |= (len<MIN_RXLENGTH) ? 0x2000 : 0;
                ret_val |= (len>MAX_RXLENGTH) ? 0x4000 : 0;
            }

            /* discard any octets that are beyond the MAX_RXLEN */
            if (len>MAX_RXLENGTH) {
                len = MAX_RXLENGTH;
            }

            if (rx_ok) {
                int i;

                /* set relevant parts of the PP area to correct values */
                SET_PP_16(TFE_PP_ADDR_RXLENGTH, len);

                for (i=0;i<len; i++) {
                    SET_PP_8(TFE_PP_ADDR_RX_FRAMELOC+i, buffer[i]);
                }

                /* set rx_buffer to where start reading *
                 * According to 4.10.9 (pp. 76-77), we start with RxStatus and RxLength!
                 */
                rx_buffer = TFE_PP_ADDR_RXSTATUS;
            }
        }
    } while (!ready);

#ifdef TFE_DEBUG_FRAMES
    if (ret_val != 0x0004)
        if(g_fh) fprintf( g_fh, "+++ tfe_receive(): ret_val=%04X", ret_val);
#endif

    return ret_val;
}



static
void tfe_sideeffects_write_pp_on_txframe(WORD ppaddress)
{
    if (ppaddress==TFE_PP_ADDR_TX_FRAMELOC+GET_PP_16(TFE_PP_ADDR_TXLENGTH)-1) {

        /* we have collected the whole frame, now start transmission */
        WORD txcmd = GET_PP_16(TFE_PP_ADDR_TXCMD);
        WORD txlen = GET_PP_16(TFE_PP_ADDR_TXLENGTH);
        WORD busst = GET_PP_16(TFE_PP_ADDR_SE_BUSST);

        if (    (txlen>MAX_TXLENGTH)
            || ((txlen>MAX_TXLENGTH-4) && (!(txcmd&0x1000)))
            ||  (txlen<MIN_TXLENGTH)
           ) {
#ifdef TFE_DEBUG_WARN
            if(g_fh) fprintf(g_fh, "WARNING! Should send %u octets: Not allowed, thus ignoring!\n", txlen);
#endif
        }
        else {
            /* clear BusST */
            SET_PP_16(TFE_PP_ADDR_SE_BUSST, busst & ~0x180);

#ifdef TFE_DEBUG_FRAMES
            if(g_fh) fprintf(g_fh, "tfe_arch_transmit() called with:                 "
                "length=%4u and buffer %s", txlen,
                debug_outbuffer(txlen, &tfe_packetpage[TFE_PP_ADDR_TX_FRAMELOC])
                );
#endif

            tfe_arch_transmit(
                txcmd & 0x0100 ? 1 : 0,   /* FORCE: Delete waiting frames in transmit buffer */
                txcmd & 0x0200 ? 1 : 0,   /* ONECOLL: Terminate after just one collision */
                txcmd & 0x1000 ? 1 : 0,   /* INHIBITCRC: Do not append CRC to the transmission */
                txcmd & 0x2000 ? 1 : 0,   /* TXPADDIS: Disable padding to 60/64 octets */
                txlen,
                &tfe_packetpage[TFE_PP_ADDR_TX_FRAMELOC]         
                );

        txcollect_buffer = TFE_PP_ADDR_TX_FRAMELOC;

#ifdef TFE_DEBUG_WARN
            /* remember that the TXCMD has been completed */
            tfe_started_tx = 0;
#endif
        }
    }
}

/*
 This is called *after* the relevant octets are written
*/
static
void tfe_sideeffects_write_pp(WORD ppaddress, int oddaddress)
{
    WORD content = GET_PP_16( ppaddress );

    assert((ppaddress & 1) == 0);

    oddaddress = oddaddress ? 1 : 0;

    switch (ppaddress)
    {
    case TFE_PP_ADDR_CC_RXCFG:
        if (content & 0x40)
        {
            /* remove 1 */
/*          tfe_arch_receive_remove_committed_frame(); */            
             
            /* this is an "act once" bit, thus restore it to zero. */
            content &= ~0x40; 
            SET_PP_16( ppaddress, content );
        }

        /* @SRT TODO: Other bits are not used by TFE */
        break;


    case TFE_PP_ADDR_CC_RXCTL:
        tfe_recv_broadcast   = content & 0x0800; /* broadcast */
        tfe_recv_mac         = content & 0x0400; /* individual address (IA) */
        tfe_recv_multicast   = content & 0x0200; /* multicast if address passes the hash filter */
        tfe_recv_correct     = content & 0x0100; /* accept correct frames */
        tfe_recv_promiscuous = content & 0x0080; /* promiscuous mode */
        tfe_recv_hashfilter  = content & 0x0040; /* accept if IA passes the hash filter */

        tfe_arch_recv_ctl( tfe_recv_broadcast,
                           tfe_recv_mac,
                           tfe_recv_multicast,
                           tfe_recv_correct,
                           tfe_recv_promiscuous,
						   tfe_recv_hashfilter
                         );
        break;

    case TFE_PP_ADDR_CC_LINECTL:
        tfe_arch_line_ctl( content & 0x0080, /* enable transmitter */
                           content & 0x0040  /* enable receiver    */
                         );
        break;

    case TFE_PP_ADDR_SE_RXEVENT:
#ifdef TFE_DEBUG_WARN
        if(g_fh) fprintf(g_fh, "WARNING! Written read-only register TFE_PP_ADDR_SE_RXEVENT: IGNORED\n");
#endif
        break;

    case TFE_PP_ADDR_SE_BUSST:
#ifdef TFE_DEBUG_WARN
        if(g_fh) fprintf(g_fh, "WARNING! Written read-only register TFE_PP_ADDR_SE_BUSST: IGNORED\n");
#endif
        break;

    case TFE_PP_ADDR_TXCMD:
        /* The transmit status command gets the last transmit command */
        SET_PP_16(TFE_PP_ADDR_CC_TXCMD, GET_PP_16(TFE_PP_ADDR_TXCMD));

#ifdef TFE_DEBUG_WARN
        /* check if we had a TXCMD, but not all octets were written */
        if (tfe_started_tx && !oddaddress) {
            if(g_fh) fprintf(g_fh, "WARNING! Early abort of transmitted frame\n");
        }
        tfe_started_tx = 1;
#endif

        /* make sure we put the octets to transmit at the right place */
        txcollect_buffer = TFE_PP_ADDR_TX_FRAMELOC;
        break;

    case TFE_PP_ADDR_TXLENGTH:
        {

            WORD txlength = GET_PP_16(TFE_PP_ADDR_TXLENGTH);
            WORD txcommand = GET_PP_16(TFE_PP_ADDR_TXCMD);

            if (    (txlength>MAX_TXLENGTH)
                || ((txlength>MAX_TXLENGTH-4) && (!(txcommand&0x1000)))
               ) {
                /* txlength too big, mark an error */
                SET_PP_16(TFE_PP_ADDR_SE_BUSST, (GET_PP_16(TFE_PP_ADDR_SE_BUSST) | 0x80) & ~0x100);
            }
            else {
                /* all right, signal that we're ready for the next frame */
                SET_PP_16(TFE_PP_ADDR_SE_BUSST, (GET_PP_16(TFE_PP_ADDR_SE_BUSST) & ~0x80) | 0x100);
            }
        }
        break;

    case TFE_PP_ADDR_LOG_ADDR_FILTER:
    case TFE_PP_ADDR_LOG_ADDR_FILTER+2:
    case TFE_PP_ADDR_LOG_ADDR_FILTER+4:
    case TFE_PP_ADDR_LOG_ADDR_FILTER+6:
		{
			unsigned int pos = 8 * (ppaddress - TFE_PP_ADDR_LOG_ADDR_FILTER + oddaddress);
			DWORD *p = (pos < 32) ? &tfe_hash_mask[0] : &tfe_hash_mask[1];

			*p &= ~(0xFF << pos); /* clear out relevant bits */
			*p |= GET_PP_8(ppaddress+oddaddress) << pos;

			tfe_arch_set_hashfilter(tfe_hash_mask);
		}
		break;

    case TFE_PP_ADDR_MAC_ADDR:
    case TFE_PP_ADDR_MAC_ADDR+2:
    case TFE_PP_ADDR_MAC_ADDR+4:
        /* the MAC address has been changed */
        tfe_ia_mac[ppaddress-TFE_PP_ADDR_MAC_ADDR+oddaddress] = 
            GET_PP_8(ppaddress+oddaddress);
        tfe_arch_set_mac(tfe_ia_mac);
		break;
    }
}

/*
 This is called *before* the relevant octets are read
*/
static
void tfe_sideeffects_read_pp(WORD ppaddress)
{
    assert((ppaddress & 1) == 0);

    switch (ppaddress)
    {
    case TFE_PP_ADDR_SE_RXEVENT:
        /* reading this before all octets of the frame are read
           performs an "implied skip" */
        {
            WORD ret_val = tfe_receive();

            /*
             RXSTATUS and RXEVENT are the same, except that RXSTATUS buffers
             the old value while RXEVENT sets a new value whenever it is called
            */
            SET_PP_16(TFE_PP_ADDR_RXSTATUS,   ret_val); 
            SET_PP_16(TFE_PP_ADDR_SE_RXEVENT, ret_val);
        }

        break;

    case TFE_PP_ADDR_SE_BUSST:
        break;

    case TFE_PP_ADDR_TXCMD:
#ifdef TFE_DEBUG_WARN
        if(g_fh) fprintf(g_fh, "WARNING! Read write-only register TFE_PP_ADDR_TXCMD: IGNORED\n");
#endif
        break;

    case TFE_PP_ADDR_TXLENGTH:
#ifdef TFE_DEBUG_WARN
        if(g_fh) fprintf(g_fh, "WARNING! Read write-only register TFE_PP_ADDR_TXLENGTH: IGNORED\n");
#endif
        break;
    }
}


void tfe_proceed_rx_buffer(int oddaddress) {
    /*
     According to the CS8900 spec, the handling is the following:
     first read H, then L, then H, then L.
     Now, we're inside the RX frame, now, we always get L then H, until the end is reached.

                                 even    odd
     TFE_PP_ADDR_RXSTATUS:         -     proceed 1)
     TFE_PP_ADDR_RXLENGTH:         -    proceed 
     TFE_PP_ADDR_RX_FRAMELOC:    2),3)       -
     TFE_PP_ADDR_RX_FRAMELOC+2: proceed     -
     TFE_PP_ADDR_RX_FRAMELOC+4: like TFE_PP_ADDR_RX_FRAMELOC+2

     1) set status "Inside FRAMELOC" FALSE
     2) set status "Inside FRAMELOC" TRUE if it is not already
     3) if "Inside FRAMELOC", proceed
     
     */

    static int inside_frameloc;
    int proceed = 0;

    if (rx_buffer==TFE_PP_ADDR_RX_FRAMELOC+GET_PP_16(TFE_PP_ADDR_RXLENGTH)) {
        /* we've read all that is available, go to start again */
        rx_buffer = TFE_PP_ADDR_RXSTATUS;
        inside_frameloc = 0;
    }
    else {
        switch (rx_buffer) {
        case TFE_PP_ADDR_RXSTATUS:
            if (oddaddress) {
                proceed = 1;
                inside_frameloc = 0;
            }
            break;

        case TFE_PP_ADDR_RXLENGTH:
            if (oddaddress) {
                proceed = 1;
            }
            break;

        case TFE_PP_ADDR_RX_FRAMELOC:
            if (oddaddress==0) {
                if (inside_frameloc) {
                    proceed = 1;
                } 
                else {
                    inside_frameloc = 1;
                }
            }
            break;

        default:
            proceed = (oddaddress==0) ? 1 : 0;
            break;
        }
    }
    
    if (proceed) {
        SET_TFE_16(TFE_ADDR_RXTXDATA, GET_PP_16(rx_buffer));
        rx_buffer += 2;
    }
}


BYTE REGPARM1 tfe_read(WORD ioaddress)
{
    BYTE retval;

    assert( tfe );
    assert( tfe_packetpage );

	assert( ioaddress < 0x10);

    switch (ioaddress) {

    case TFE_ADDR_TXCMD:
    case TFE_ADDR_TXCMD+1:
    case TFE_ADDR_TXLENGTH:
    case TFE_ADDR_TXLENGTH+1:
#ifdef TFE_DEBUG_WARN
        if(g_fh) fprintf(g_fh, "WARNING! Reading write-only TFE register $%02X!\n", ioaddress);
#endif
        /* @SRT TODO: Verify with reality */
        retval = GET_TFE_8(ioaddress); 
        break;

    case TFE_ADDR_RXTXDATA2:
    case TFE_ADDR_RXTXDATA2+1:
    case TFE_ADDR_PP_DATA2:
    case TFE_ADDR_PP_DATA2+1:
#ifdef TFE_DEBUG_WARN
        if(g_fh) fprintf(g_fh, "WARNING! Reading not supported TFE register $%02X!\n", ioaddress);
#endif
        /* @SRT TODO */
        retval = GET_TFE_8(ioaddress);
        break;

    case TFE_ADDR_PP_DATA:
    case TFE_ADDR_PP_DATA+1:
        /* make sure the TFE register have the correct content */
        {
            WORD ppaddress = tfe_packetpage_ptr & (MAX_PACKETPAGE_ARRAY-1);

            /* perform side-effects the read may perform */
            tfe_sideeffects_read_pp( ppaddress );

            /* [3] make sure the data matches the real value - [1] assumes this! */
            SET_TFE_16( TFE_ADDR_PP_DATA, GET_PP_16(ppaddress) );
        }


#ifdef TFE_DEBUG_LOAD
        if(g_fh) fprintf(g_fh, "reading PP Ptr: $%04X => $%04X.", 
            tfe_packetpage_ptr, GET_PP_16(tfe_packetpage_ptr) );
#endif

        retval = GET_TFE_8(ioaddress);
        break;

    case TFE_ADDR_INTSTQUEUE:
    case TFE_ADDR_INTSTQUEUE+1:
        SET_TFE_16( TFE_ADDR_INTSTQUEUE, GET_PP_16(0x0120)  );
        retval = GET_TFE_8(ioaddress);
        break;

    case TFE_ADDR_RXTXDATA:
    case TFE_ADDR_RXTXDATA+1:
        /* we're trying to read a new 16 bit word, get it from the
           receive buffer
         */
        tfe_proceed_rx_buffer(ioaddress & 0x01);
        retval = GET_TFE_8(ioaddress);
        break;

    default:
        retval = GET_TFE_8(ioaddress);
        break;
    };

#ifdef TFE_DEBUG_LOAD
    if(g_fh) fprintf(g_fh, "read [$%02X] => $%02X.", ioaddress, retval);
#endif
    return retval;
}

void REGPARM2 tfe_store(WORD ioaddress, BYTE byte)
{
    assert( tfe );
    assert( tfe_packetpage );

	assert( ioaddress < 0x10);

    switch (ioaddress)
    {
    case TFE_ADDR_RXTXDATA:
    case TFE_ADDR_RXTXDATA+1:
        SET_PP_8(txcollect_buffer, byte);
        tfe_sideeffects_write_pp_on_txframe(txcollect_buffer++);
        break;

    case TFE_ADDR_INTSTQUEUE:
    case TFE_ADDR_INTSTQUEUE+1:
#ifdef TFE_DEBUG_WARN
        if(g_fh) fprintf(g_fh, "WARNING! Writing read-only TFE register $%02X!\n", ioaddress);
#endif
        /* @SRT TODO: Verify with reality */
        /* do nothing */
        return;

    case TFE_ADDR_RXTXDATA2:
    case TFE_ADDR_RXTXDATA2+1:
    case TFE_ADDR_PP_DATA2:
    case TFE_ADDR_PP_DATA2+1:
#ifdef TFE_DEBUG_WARN
        if(g_fh) fprintf(g_fh, "WARNING! Writing not supported TFE register $%02X!\n", ioaddress);
#endif
        /* do nothing */
        return;

    case TFE_ADDR_TXCMD:
    case TFE_ADDR_TXCMD+1:
        SET_TFE_8(ioaddress, byte);
        SET_PP_8((ioaddress-TFE_ADDR_TXCMD)+TFE_PP_ADDR_TXCMD, byte); /* perform the mapping to PP+0144 */
        tfe_sideeffects_write_pp(TFE_PP_ADDR_TXCMD, ioaddress-TFE_ADDR_TXCMD);
        break;

    case TFE_ADDR_TXLENGTH:
    case TFE_ADDR_TXLENGTH+1:

        SET_TFE_8(ioaddress, byte);
        SET_PP_8((ioaddress-TFE_ADDR_TXLENGTH)+TFE_PP_ADDR_TXLENGTH, byte ); /* perform the mapping to PP+0144 */

        tfe_sideeffects_write_pp(TFE_PP_ADDR_TXLENGTH, ioaddress-TFE_ADDR_TXLENGTH);
        break;

/*
#define TFE_ADDR_TXCMD      0x04 * -W Maps to PP+0144 *
#define TFE_ADDR_TXLENGTH   0x06 * -W Maps to PP+0146 *
#define TFE_ADDR_INTSTQUEUE 0x08 * R- Interrupt status queue, maps to PP + 0120 *
*/
    case TFE_ADDR_PP_DATA:
    case TFE_ADDR_PP_DATA+1:

        /* [2] make sure the data matches the real value - [1] assumes this! */
        SET_TFE_16(TFE_ADDR_PP_DATA, GET_PP_16(tfe_packetpage_ptr));
        /* FALL THROUGH */

    default:
        SET_TFE_8(ioaddress, byte);
    }

#ifdef TFE_DEBUG_STORE
    if(g_fh) fprintf(g_fh, "store [$%02X] <= $%02X.", ioaddress, (int)byte);
#endif

    /* now check if we have to do any side-effects */
    switch (ioaddress)
    {
    case TFE_ADDR_PP_PTR:
    case TFE_ADDR_PP_PTR+1:
        tfe_packetpage_ptr = GET_TFE_16(TFE_ADDR_PP_PTR);

#ifdef TFE_DEBUG_STORE
        if(g_fh) fprintf(g_fh, "set PP Ptr to $%04X.", tfe_packetpage_ptr);
#endif

        if ((tfe_packetpage_ptr & 1) != 0) {

#ifdef TFE_DEBUG_WARN
            if(g_fh) fprintf(g_fh,
                "WARNING! PacketPage register set to odd address $%04X (not allowed!)\n",
                tfe_packetpage_ptr );
#endif /* #ifdef TFE_DEBUG_WARN */

            /* "correct" the address to the next lower address 
             REMARK: I don't know how a real cs8900a will behave in this case,
                     since it is not allowed. Nevertheless, this "correction"
                     prevents assert()s to fail.
            */
            tfe_packetpage_ptr -= 1;
        }

        /*
         [1] The TFE_ADDR_PP_DATA does not need to be modified here,
         since it will be modified just before a read or store operation
         is to be performed.
         See [2] and [3]
        */
        break;

    case TFE_ADDR_PP_DATA:
    case TFE_ADDR_PP_DATA+1:

        {
            WORD ppaddress = tfe_packetpage_ptr & (MAX_PACKETPAGE_ARRAY-1);

#ifdef TFE_DEBUG_STORE
            if(g_fh) fprintf(g_fh, "before writing to PP Ptr: $%04X <= $%04X.", 
                ppaddress, GET_PP_16(ppaddress) );
#endif
            {
                register WORD tmpIoAddr = ioaddress & ~1; /* word-align the address */
                SET_PP_16(ppaddress, GET_TFE_16(tmpIoAddr));
            }

            /* perform side-effects the write may perform */
            /* the addresses are always aligned on the whole 16-bit-word */
            tfe_sideeffects_write_pp(ppaddress, ioaddress-TFE_ADDR_PP_DATA);

#ifdef TFE_DEBUG_STORE
            if(g_fh) fprintf(g_fh, "after  writing to PP Ptr: $%04X <= $%04X.", 
                ppaddress, GET_PP_16(ppaddress) );
#endif
        }
        break;
    }

    TFE_DEBUG_OUTPUT_REG();
}



static
int set_tfe_disabled(void *v, void *param)
{
    /* dummy function since we don't want "disabled" to be stored on disk */
    return 0;
}


static
int set_tfe_enabled(void *v, void *param)
{
    if (!tfe_cannot_use) {

        if (!(int)v) {
            /* TFE should be deactived */
            if (tfe_enabled) {
                tfe_enabled = 0;
				/* RGJ Commented out forAppleWin */
                //c64export_remove(&export_res);
                if (tfe_deactivate() < 0) {
                    return -1;
                }
            }
            return 0;
        } else { 
            if (!tfe_enabled) {
				/* RGJ Commented out forAppleWin */
                //if (c64export_query(&export_res) < 0)
                //    return -1;

                tfe_enabled = 1;
                if (tfe_activate() < 0) {
                    return -1;
                }
				/* RGJ Commented out forAppleWin */
                //if (c64export_add(&export_res) < 0)
                //    return -1;

            }

            return 0;
        }

    }
    return 0;
}


static 
int set_tfe_interface(void *v, void *param)
{
    const char *name = (const char *)v;

    if (tfe_interface != NULL && name != NULL
        && strcmp(name, tfe_interface) == 0)
        return 0;

    util_string_set(&tfe_interface, name);

    if (tfe_enabled) {
        /* ethernet is enabled, make sure that the new name is
           taken account of 
         */
        if (tfe_deactivate() < 0) {
            return -1;
        }
        if (tfe_activate() < 0) {
            return -1;
        }

        /* virtually reset the LAN chip */
        if (tfe) {
            tfe_reset();
        }
    }
    return 0;
}



/* ------------------------------------------------------------------------- */
/*    commandline support functions                                          */

//#ifdef HAS_TRANSLATION
//static const cmdline_option_t cmdline_options[] =
//{
//    { "-tfe", SET_RESOURCE, 0, NULL, NULL, "ETHERNET_ACTIVE", (resource_value_t)1,
//      0, IDCLS_ENABLE_TFE },
//    { "+tfe", SET_RESOURCE, 0, NULL, NULL, "ETHERNET_ACTIVE", (resource_value_t)0,
//      0, IDCLS_DISABLE_TFE },
//    { NULL }
//};
//#else
//static const cmdline_option_t cmdline_options[] =
//{
//    { "-tfe", SET_RESOURCE, 0, NULL, NULL, "ETHERNET_ACTIVE", (resource_value_t)1,
//      NULL, N_("Enable the TFE (\"the final ethernet\") unit") },
//    { "+tfe", SET_RESOURCE, 0, NULL, NULL, "ETHERNET_ACTIVE", (resource_value_t)0,
//      NULL, N_("Disable the TFE (\"the final ethernet\") unit") },
//    { NULL }
//};
//#endif

//int tfe_cmdline_options_init(void)
//{
//    return cmdline_register_options(cmdline_options);
//}


/* ------------------------------------------------------------------------- */
/*    snapshot support functions                                             */

#if 0

static char snap_module_name[] = "TFE1764";
#define SNAP_MAJOR 0
#define SNAP_MINOR 0

int tfe_read_snapshot_module(struct snapshot_s *s)
{
	/* @SRT TODO: not yet implemented */
	return -1;
}

int tfe_write_snapshot_module(struct snapshot_s *s)
{
	/* @SRT TODO: not yet implemented */
	return -1;
}

#endif /* #if 0 */

/* ------------------------------------------------------------------------- */
/*    functions for selecting and querying available NICs                    */

int tfe_enumadapter_open(void)
{
    if (!tfe_arch_enumadapter_open()) {
        tfe_cannot_use = 1;
        return 0;
    }
    return 1;
}

int tfe_enumadapter(char **ppname, char **ppdescription)
{
    return tfe_arch_enumadapter(ppname, ppdescription);
}

int tfe_enumadapter_close(void)
{
    return tfe_arch_enumadapter_close();
}

// Go via TfeIoCxxx() instead of directly calling IO_Null() to include this specific (slot-3) _DEBUG check
static BYTE __stdcall TfeIoCxxx (WORD programcounter, WORD address, BYTE write, BYTE value, ULONG nCycles)
{
#ifdef _DEBUG
	if (!IS_APPLE2)
	{
		// Derived from UTAIIe:5-28
		//
		// INTCXROM SLOTC3ROM TFE floating bus?
		//   0         0         N (internal ROM)
		//   0         1         Y
		//   1         0         N (internal ROM)
		//   1         1         N (internal ROM)
		if (! (!MemCheckINTCXROM() && MemCheckSLOTC3ROM()) )
		{
			_ASSERT(0);	// Card ROM disabled, so IO_Cxxx() returns the internal ROM
		}
	}
#endif

	return IO_Null(programcounter, address, write, value,nCycles);
}

static BYTE __stdcall TfeIo (WORD programcounter, WORD address, BYTE write, BYTE value, ULONG nCycles)
{
	BYTE ret = 0;

	if (write) {
	    if (tfe_enabled)
		    tfe_store((WORD)(address & 0x0f), value);
		}
	else {
		if (tfe_enabled)
        ret = tfe_read((WORD)(address & 0x0f));
	}

return ret;

}

void get_disabled_state(int * param)
{

*param = tfe_cannot_use;

}

int update_tfe_interface(void *v, void *param)
{
	return set_tfe_interface(v,param);
}

void * get_tfe_interface(void)
{
	void *v;
	v = tfe_interface;
	return v;
}

void get_tfe_enabled(int * param)
{
	*param = tfe_enabled;
}

//#endif /* #ifdef HAVE_TFE */
