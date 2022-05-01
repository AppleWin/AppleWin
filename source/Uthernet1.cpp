/*
 * tfe.h - TFE ("The final ethernet" emulation.
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

/* Emulate a Uthernet 1 card (adapted from VICE's TFE support) */

#include "StdAfx.h"

#include "Uthernet1.h"
#include "YamlHelper.h"
#include "Log.h"
#include "Memory.h"
#include "Interface.h"
#include "Tfe/tfearch.h"
#include "Tfe/tfesupp.h"
#include "Tfe/NetworkBackend.h"
#include "Tfe/PCapBackend.h"

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


/* ------------------------------------------------------------------------- */
/*    debugging functions                                                    */

#ifdef TFE_DEBUG_FRAMES
std::string debug_outbuffer(size_t length, const unsigned char* buffer)
{
	std::string outbuffer;
	outbuffer.reserve(length * 3);
	for (size_t i = 0; i < length; i++)
	{
		StrAppendByteAsHex(outbuffer, buffer[i]);
		outbuffer += ((i+1)%16==0)?'*':(((i+1)%8==0)?'-':' ');
	}
	return outbuffer;
}
#endif


#ifdef TFE_DEBUG_DUMP

#define NUMBER_PER_LINE 8

void Uthernet1::tfe_debug_output_general( const char *what, WORD (Uthernet1::*getFunc)(int), int count )
{
	if (!g_fh) return;
	fprintf(g_fh, "%s contents:\n", what);
	for (int i = 0; i < count; i += 2*NUMBER_PER_LINE)
	{
		fprintf(g_fh, "%04X:  ", i);
		for (int j = 0; j < NUMBER_PER_LINE; j++) 
		{
			fprintf(g_fh, "%04X, ", (this->*getFunc)(i+j+j));
		}
		fputc('\n', g_fh);
	}
}

WORD Uthernet1::tfe_debug_output_io_getFunc( int i )
{
    return GET_TFE_16(i);
}

void Uthernet1::tfe_debug_output_io( void )
{
    tfe_debug_output_general( "TFE I/O", &Uthernet1::tfe_debug_output_io_getFunc, TFE_COUNT_IO_REGISTER );
}
#define TFE_DEBUG_OUTPUT_IO() tfe_debug_output_io()

WORD Uthernet1::tfe_debug_output_pp_getFunc( int i )
{
    return GET_PP_16(i);
}

void Uthernet1::tfe_debug_output_pp( void )
{
    tfe_debug_output_general( "PacketPage", &Uthernet1::tfe_debug_output_pp_getFunc, 0x0160 /* MAX_PACKETPAGE_ARRAY */ );
}
#define TFE_DEBUG_OUTPUT_PP() tfe_debug_output_pp()

#define TFE_DEBUG_OUTPUT_REG() \
    do { TFE_DEBUG_OUTPUT_IO(); TFE_DEBUG_OUTPUT_PP(); } while (0)

#else

    #define TFE_DEBUG_OUTPUT_IO()
    #define TFE_DEBUG_OUTPUT_PP()
    #define TFE_DEBUG_OUTPUT_REG()

#endif

Uthernet1::Uthernet1(UINT slot) : Card(CT_Uthernet, slot)
{
    if (m_slot != SLOT3)	// fixme
        ThrowErrorInvalidSlot();
    Init();
}

void Uthernet1::Init(void)
{
    // Initialise all state member variables
    // in the same order as the header file to ease maintenance
    memset( tfe_ia_mac, 0, sizeof(tfe_ia_mac) );
    memset( tfe_hash_mask, 0, sizeof(tfe_hash_mask) );

    tfe_recv_broadcast   = 0;
    tfe_recv_mac         = 0;
    tfe_recv_multicast   = 0;
    tfe_recv_correct     = 0;
    tfe_recv_promiscuous = 0;
    tfe_recv_hashfilter  = 0;

#ifdef TFE_DEBUG_WARN
    tfe_started_tx       = 0;
#endif

    /* initialize visible IO register and PacketPage registers */
    memset( tfe, 0, sizeof(tfe) );

    txcollect_buffer     = TFE_PP_ADDR_TX_FRAMELOC;
    rx_buffer            = TFE_PP_ADDR_RXSTATUS;

    memset( tfe_packetpage, 0, sizeof(tfe_packetpage) );

    tfe_packetpage_ptr   = 0;

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

    TFE_DEBUG_OUTPUT_REG();
}


void Uthernet1::tfe_sideeffects_write_pp_on_txframe(WORD ppaddress)
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
                debug_outbuffer(txlen, &tfe_packetpage[TFE_PP_ADDR_TX_FRAMELOC]).c_str()
                );
#endif

            tfe_transmit(
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
void Uthernet1::tfe_sideeffects_write_pp(WORD ppaddress, int oddaddress)
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
void Uthernet1::tfe_sideeffects_read_pp(WORD ppaddress)
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


void Uthernet1::tfe_proceed_rx_buffer(int oddaddress) {
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


BYTE REGPARM1 Uthernet1::tfe_read(WORD ioaddress)
{
    BYTE retval;

	assert( ioaddress < TFE_COUNT_IO_REGISTER);

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

void REGPARM2 Uthernet1::tfe_store(WORD ioaddress, BYTE byte)
{
	assert( ioaddress < TFE_COUNT_IO_REGISTER);

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
                WORD tmpIoAddr = ioaddress & ~1; /* word-align the address */
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
int Uthernet1::tfe_should_accept(unsigned char *buffer, int length, int *phashed, int *phash_index,
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
        debug_outbuffer(length, buffer).c_str()
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


WORD Uthernet1::tfe_receive(void)
{
    WORD ret_val = 0x0004;

    BYTE buffer[MAX_RXLENGTH];

    int  multicast = 0;

    int  ready;

#ifdef TFE_DEBUG_FRAMES
    if(g_fh) fprintf( g_fh, "");
#endif

    do {
        ready = 1 ; /* assume we will find a good frame */

        int len = networkBackend->receive(
            sizeof(buffer), /* size of buffer */
            buffer          /* where to store a frame */
            );

        if (len > 0) {
            assert((len&1) == 0); /* length has to be even! */

            int  hashed = 0;
            int  hash_index = 0;
            int  broadcast = 0;
            int  correct_mac = 0;
            int  crc_error = 0;

            int  rx_ok = 1;

            /* determine ourself the type of frame */
            if (!tfe_should_accept(buffer,
                len, &hashed, &hash_index, &correct_mac, &broadcast, &multicast)) {

                /* if we should not accept this frame, just do nothing
                    * now, look for another one */
                ready = 0; /* try another frame */
                continue;
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

void Uthernet1::tfe_transmit(
	int /* force */,		/* FORCE: Delete waiting frames in transmit buffer */
	int /* onecoll */,		/* ONECOLL: Terminate after just one collision */
	int /* inhibit_crc */,	/* INHIBITCRC: Do not append CRC to the transmission */
	int /* tx_pad_dis */,	/* TXPADDIS: Disable padding to 60 Bytes */
	int txlength,			/* Frame length */
	uint8_t *txframe		/* Pointer to the frame to be transmitted */
)
{
    // non eof the existing backends do anything with these flags
	networkBackend->transmit(txlength, txframe);
}


// Go via TfeIoCxxx() instead of directly calling IO_Null() to include this specific (slot-3) _DEBUG check
static BYTE __stdcall TfeIoCxxx (WORD programcounter, WORD address, BYTE write, BYTE value, ULONG nCycles)
{
#ifdef _DEBUG
    const UINT slot = (address >> 8) & 0xf;

    if (!IS_APPLE2 && slot == SLOT3)
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
    UINT uSlot = ((address & 0xff) >> 4) - 8;
    Uthernet1* pCard = (Uthernet1*) MemGetSlotParameters(uSlot);
    BYTE ret = 0;

    if (write)
    {
        pCard->tfe_store((WORD)(address & 0x0f), value);
    }
    else
    {
        ret = pCard->tfe_read((WORD)(address & 0x0f));
    }

    return ret;
}

void Uthernet1::InitializeIO(LPBYTE pCxRomPeripheral)
{
    const std::string interfaceName = PCapBackend::GetRegistryInterface(m_slot);
    networkBackend = GetFrame().CreateNetworkBackend(interfaceName);
    if (networkBackend->isValid())
    {
        RegisterIoHandler(m_slot, TfeIo, TfeIo, TfeIoCxxx, TfeIoCxxx, this, NULL);
    }
}

void Uthernet1::Reset(const bool powerCycle)
{
    if (powerCycle)
    {
        Init();
    }
}

void Uthernet1::Update(const ULONG nExecutedCycles)
{
    networkBackend->update(nExecutedCycles);
}

/* ------------------------------------------------------------------------- */
/*    snapshot support functions                                             */

#define SS_YAML_KEY_ENABLED "Enabled"
#define SS_YAML_KEY_NETWORK_INTERFACE "Network Interface"
#define SS_YAML_KEY_STARTED_TX "Started Tx"
#define SS_YAML_KEY_CANNOT_USE "Cannot Use"
#define SS_YAML_KEY_TXCOLLECT_BUFFER "Tx Collect Buffer"
#define SS_YAML_KEY_RX_BUFFER "Rx Buffer"
#define SS_YAML_KEY_CS8900A_REGS "CS8900A Registers"
#define SS_YAML_KEY_PACKETPAGE_REGS "PacketPage Registers"

static const UINT kUNIT_VERSION = 1;

const std::string& Uthernet1::GetSnapshotCardName(void)
{
    static const std::string name("Uthernet");
    return name;
}

void Uthernet1::SaveSnapshot(class YamlSaveHelper& yamlSaveHelper)
{
    YamlSaveHelper::Slot slot(yamlSaveHelper, GetSnapshotCardName(), m_slot, kUNIT_VERSION);

    YamlSaveHelper::Label unit(yamlSaveHelper, "%s:\n", SS_YAML_KEY_STATE);

    yamlSaveHelper.SaveBool(SS_YAML_KEY_ENABLED, networkBackend->isValid() ? true : false);
    yamlSaveHelper.SaveString(SS_YAML_KEY_NETWORK_INTERFACE, networkBackend->getInterfaceName());

    yamlSaveHelper.SaveBool(SS_YAML_KEY_STARTED_TX, tfe_started_tx ? true : false);
    yamlSaveHelper.SaveBool(SS_YAML_KEY_CANNOT_USE, PCapBackend::tfe_is_npcap_loaded() ? false : false);

    yamlSaveHelper.SaveHexUint16(SS_YAML_KEY_TXCOLLECT_BUFFER, txcollect_buffer);
    yamlSaveHelper.SaveHexUint16(SS_YAML_KEY_RX_BUFFER, rx_buffer);

    {
        YamlSaveHelper::Label state(yamlSaveHelper, "%s:\n", SS_YAML_KEY_CS8900A_REGS);
        yamlSaveHelper.SaveMemory(tfe, TFE_COUNT_IO_REGISTER);
    }

    {
        YamlSaveHelper::Label state(yamlSaveHelper, "%s:\n", SS_YAML_KEY_PACKETPAGE_REGS);
        yamlSaveHelper.SaveMemory(tfe_packetpage, MAX_PACKETPAGE_ARRAY);
    }
}

bool Uthernet1::LoadSnapshot(class YamlLoadHelper& yamlLoadHelper, UINT version)
{
    if (version < 1 || version > kUNIT_VERSION)
		ThrowErrorInvalidVersion(version);

    yamlLoadHelper.LoadBool(SS_YAML_KEY_ENABLED);  // FIXME: what is the point of this?
    PCapBackend::SetRegistryInterface(m_slot, yamlLoadHelper.LoadString(SS_YAML_KEY_NETWORK_INTERFACE));

    tfe_started_tx = yamlLoadHelper.LoadBool(SS_YAML_KEY_STARTED_TX) ? true : false;

    // it is meaningless to restore this boolean flag
    // as it depends on the availability of npcap on *this* pc
    const bool tfe_cannot_use = yamlLoadHelper.LoadBool(SS_YAML_KEY_CANNOT_USE);

    txcollect_buffer = yamlLoadHelper.LoadUint(SS_YAML_KEY_TXCOLLECT_BUFFER);
    rx_buffer = yamlLoadHelper.LoadUint(SS_YAML_KEY_RX_BUFFER);

    if (!yamlLoadHelper.GetSubMap(SS_YAML_KEY_CS8900A_REGS))
        throw std::runtime_error("Card: Expected key: " SS_YAML_KEY_CS8900A_REGS);

    memset(tfe, 0, TFE_COUNT_IO_REGISTER);
    yamlLoadHelper.LoadMemory(tfe, TFE_COUNT_IO_REGISTER);
    yamlLoadHelper.PopMap();

    if (!yamlLoadHelper.GetSubMap(SS_YAML_KEY_PACKETPAGE_REGS))
        throw std::runtime_error("Card: Expected key: " SS_YAML_KEY_PACKETPAGE_REGS);

    memset(tfe_packetpage, 0, MAX_PACKETPAGE_ARRAY);
    yamlLoadHelper.LoadMemory(tfe_packetpage, MAX_PACKETPAGE_ARRAY);
    yamlLoadHelper.PopMap();

    // Side effects after PackagePage has been loaded

    tfe_packetpage_ptr = GET_TFE_16(TFE_ADDR_PP_PTR);

    tfe_sideeffects_write_pp(TFE_PP_ADDR_CC_RXCTL, 0);  // set the 6 tfe_recv_* vars

    for (UINT i = 0; i < 8; i++)
        tfe_sideeffects_write_pp((TFE_PP_ADDR_LOG_ADDR_FILTER + i) & ~1, i & 1);    // set tfe_hash_mask

    for (UINT i = 0; i < 6; i++)
        tfe_sideeffects_write_pp((TFE_PP_ADDR_MAC_ADDR + i) & ~1, i & 1);           // set tfe_ia_mac

    return true;
}
