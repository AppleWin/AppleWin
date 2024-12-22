/*
 * tfearch.c - TFE ("The final ethernet") emulation, 
 *                 architecture-dependant stuff
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


#include <StdAfx.h> // this is necessary in linux, but in MSVC windows.h MUST come after winsock2.h (from pcap.h above)
#include "Tfe/tfearch.h"
#include "../Log.h"


/** #define TFE_DEBUG_ARCH 1 **/
/* #define TFE_DEBUG_FRAMES - might be defined in TFE.H! */

int tfe_arch_enumadapter_open(void)
{
    return 0;
}

int tfe_arch_enumadapter(std::string & name, std::string & description)
{
    return 0;
}

int tfe_arch_enumadapter_close(void)
{
    return 0;
}

pcap_t * TfePcapOpenAdapter(const std::string & interface_name)
{
    return NULL;
}

void TfePcapCloseAdapter(pcap_t * TfePcapFP)
{
}

/* ------------------------------------------------------------------------- */
/*    the architecture-dependend functions                                   */

void tfe_arch_set_mac( const BYTE mac[6] )
{
#if defined(TFE_DEBUG_ARCH) || defined(TFE_DEBUG_FRAMES)
    if(g_fh) fprintf( g_fh, "New MAC address set: %02X:%02X:%02X:%02X:%02X:%02X.\n",
        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5] );
#endif
}

void tfe_arch_set_hashfilter(const uint32_t hash_mask[2])
{
#if defined(TFE_DEBUG_ARCH) || defined(TFE_DEBUG_FRAMES)
    if(g_fh) fprintf( g_fh, "New hash filter set: %08X:%08X.\n",
        hash_mask[1], hash_mask[0]);
#endif
}

void tfe_arch_recv_ctl( int bBroadcast,   /* broadcast */
                        int bIA,          /* individual address (IA) */
                        int bMulticast,   /* multicast if address passes the hash filter */
                        int bCorrect,     /* accept correct frames */
                        int bPromiscuous, /* promiscuous mode */
                        int bIAHash       /* accept if IA passes the hash filter */
                      )
{
#if defined(TFE_DEBUG_ARCH) || defined(TFE_DEBUG_FRAMES)
    if(g_fh) {
        fprintf( g_fh, "tfe_arch_recv_ctl() called with the following parameters:" );
        fprintf( g_fh, "\tbBroadcast   = %s", bBroadcast   ? "TRUE" : "FALSE" );
        fprintf( g_fh, "\tbIA          = %s", bIA          ? "TRUE" : "FALSE" );
        fprintf( g_fh, "\tbMulticast   = %s", bMulticast   ? "TRUE" : "FALSE" );
        fprintf( g_fh, "\tbCorrect     = %s", bCorrect     ? "TRUE" : "FALSE" );
        fprintf( g_fh, "\tbPromiscuous = %s", bPromiscuous ? "TRUE" : "FALSE" );
        fprintf( g_fh, "\tbIAHash      = %s", bIAHash      ? "TRUE" : "FALSE" );
        fprintf( g_fh, "\n" );
    }
#endif
}

void tfe_arch_line_ctl(int bEnableTransmitter, int bEnableReceiver )
{
#if defined(TFE_DEBUG_ARCH) || defined(TFE_DEBUG_FRAMES)
    if(g_fh) {
        fprintf( g_fh, "tfe_arch_line_ctl() called with the following parameters:" );
        fprintf( g_fh, "\tbEnableTransmitter = %s", bEnableTransmitter ? "TRUE" : "FALSE" );
        fprintf( g_fh, "\tbEnableReceiver    = %s", bEnableReceiver    ? "TRUE" : "FALSE" );
        fprintf( g_fh, "\n" );
    }
#endif
}

void tfe_arch_transmit(pcap_t * TfePcapFP,
                       int txlength,    /* Frame length */
                       BYTE *txframe    /* Pointer to the frame to be transmitted */
                      )
{
}

int tfe_arch_receive(pcap_t * TfePcapFP,
                     const int size ,    /* Size of buffer */
                     BYTE *pbuffer       /* where to store a frame */
                    )
{
    return -1;
}

const char * tfe_arch_lib_version()
{
    return 0;
}

int tfe_arch_is_npcap_loaded()
{
    return 0;
}

//#endif /* #ifdef HAVE_TFE */
