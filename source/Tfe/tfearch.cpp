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

/* #define WPCAP */

#ifdef _MSC_VER

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "pcap.h"
#else
// on Linux and Mac OS X, we use system's pcap.h, which needs to be included as <>
#include <pcap.h>
#endif

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <StdAfx.h> // this is necessary in linux, but in MSVC windows.h MUST come after winsock2.h (from pcap.h above)
#include "tfearch.h"
#include "../Log.h"


/** #define TFE_DEBUG_ARCH 1 **/
/** #define TFE_DEBUG_PKTDUMP 1 **/

/* #define TFE_DEBUG_FRAMES - might be defined in TFE.H! */

#define TFE_DEBUG_WARN 1 /* this should not be deactivated */

// once this is set, no further attempts to load npcap will be made
static int tfe_cannot_use = 0;

#ifdef _MSC_VER

typedef pcap_t	*(*pcap_open_live_t)(const char *, int, int, int, char *);
typedef void (*pcap_close_t)(pcap_t *);
typedef int (*pcap_dispatch_t)(pcap_t *, int, pcap_handler, u_char *);
typedef int (*pcap_setnonblock_t)(pcap_t *, int, char *);
typedef int (*pcap_datalink_t)(pcap_t *);
typedef int (*pcap_findalldevs_t)(pcap_if_t **, char *);
typedef void (*pcap_freealldevs_t)(pcap_if_t *);
typedef int (*pcap_sendpacket_t)(pcap_t *p, u_char *buf, int size);
typedef const char *(*pcap_lib_version_t)(void);
typedef char* (*pcap_geterr_t)(pcap_t* p);

static pcap_open_live_t   p_pcap_open_live;
static pcap_close_t       p_pcap_close;
static pcap_dispatch_t    p_pcap_dispatch;
static pcap_setnonblock_t p_pcap_setnonblock;
static pcap_findalldevs_t p_pcap_findalldevs;
static pcap_freealldevs_t p_pcap_freealldevs;
static pcap_sendpacket_t  p_pcap_sendpacket;
static pcap_datalink_t p_pcap_datalink;
static pcap_lib_version_t p_pcap_lib_version;
static pcap_geterr_t p_pcap_geterr;

static HINSTANCE pcap_library = NULL;

static
void TfePcapFreeLibrary(void)
{
    if (pcap_library) {
        if (!FreeLibrary(pcap_library)) {
            if(g_fh) fprintf(g_fh, "FreeLibrary WPCAP.DLL failed!\n");
        }
        pcap_library = NULL;

        p_pcap_open_live = NULL;
        p_pcap_close = NULL;
        p_pcap_dispatch = NULL;
        p_pcap_setnonblock = NULL;
        p_pcap_findalldevs = NULL;
        p_pcap_freealldevs = NULL;
        p_pcap_sendpacket = NULL;
        p_pcap_datalink = NULL;
        p_pcap_lib_version = NULL;
        p_pcap_geterr = NULL;
    }
}

/* since I don't like typing too much... */
#define GET_PROC_ADDRESS_AND_TEST( _name_ ) \
    p_##_name_ = (_name_##_t) GetProcAddress(pcap_library, #_name_ ); \
    if (!p_##_name_ ) { \
        if(g_fh) fprintf(g_fh, "GetProcAddress " #_name_ " failed!\n"); \
        TfePcapFreeLibrary(); \
        return FALSE; \
    } 

static
BOOL TfePcapLoadLibrary(void)
{
    if (pcap_library)
    {
        // already loaded
        return TRUE;
    }

    if (tfe_cannot_use)
    {
        // already failed
        return FALSE;
    }

    // try to load
    if (!SetDllDirectory("C:\\Windows\\System32\\Npcap\\"))	// Prefer Npcap over WinPcap (GH#822)
    {
        const char* error = "Warning: SetDllDirectory() failed for Npcap";
        LogOutput("%s\n", error);
        LogFileOutput("%s\n", error);
    }

    pcap_library = LoadLibrary("wpcap.dll");

    if (!pcap_library)
    {
        tfe_cannot_use = 1;
        if(g_fh) fprintf(g_fh, "LoadLibrary WPCAP.DLL failed!\n" );
        return FALSE;
    }

    GET_PROC_ADDRESS_AND_TEST(pcap_open_live);
    GET_PROC_ADDRESS_AND_TEST(pcap_close);
    GET_PROC_ADDRESS_AND_TEST(pcap_dispatch);
    GET_PROC_ADDRESS_AND_TEST(pcap_setnonblock);
    GET_PROC_ADDRESS_AND_TEST(pcap_findalldevs);
    GET_PROC_ADDRESS_AND_TEST(pcap_freealldevs);
    GET_PROC_ADDRESS_AND_TEST(pcap_sendpacket);
    GET_PROC_ADDRESS_AND_TEST(pcap_datalink);
    GET_PROC_ADDRESS_AND_TEST(pcap_lib_version);
    GET_PROC_ADDRESS_AND_TEST(pcap_geterr);
    LogOutput("%s\n", p_pcap_lib_version());
    LogFileOutput("%s\n", p_pcap_lib_version());

    return TRUE;
}

#undef GET_PROC_ADDRESS_AND_TEST

#else

// libpcap is a standard package, just link to it
#define p_pcap_open_live pcap_open_live
#define p_pcap_close pcap_close
#define p_pcap_dispatch pcap_dispatch
#define p_pcap_setnonblock pcap_setnonblock
#define p_pcap_findalldevs pcap_findalldevs
#define p_pcap_freealldevs pcap_freealldevs
#define p_pcap_sendpacket pcap_sendpacket
#define p_pcap_datalink pcap_datalink
#define p_pcap_lib_version pcap_lib_version
#define p_pcap_geterr pcap_geterr

static BOOL TfePcapLoadLibrary(void)
{
    static bool loaded = false;
    if (!loaded)
    {
        loaded = true;
        LogOutput("%s\n", p_pcap_lib_version());
        LogFileOutput("%s\n", p_pcap_lib_version());
    }
    return TRUE;
}

#endif

/* ------------------------------------------------------------------------- */
/*    variables needed                                                       */


//static log_t g_fh = g_fh;


static pcap_if_t *TfePcapNextDev = NULL;
static pcap_if_t *TfePcapAlldevs = NULL;

static char TfePcapErrbuf[PCAP_ERRBUF_SIZE];

#ifdef TFE_DEBUG_PKTDUMP
static
void debug_output( const char *text, const BYTE *what, int count )
{
	std::string buffer;
	buffer.reserve(8 * 3 + 1);
	int len1 = count;
	const BYTE *pb = what;
	LogOutput("\n%s: length = %u\n", text, len1);
	do {
		buffer.clear();
		for (int i = 0; i < 8 && len1 > 0; ++i, --len1, ++pb)
		{
			StrAppendByteAsHex(buffer, *pb);
			buffer += ' ';
		}
		buffer += '\n';
		OutputDebugString(buffer.c_str());
	} while (len1 > 0);
}
#endif // #ifdef TFE_DEBUG_PKTDUMP

static
void TfePcapCloseAdapter(void) 
{
    if (TfePcapAlldevs) {
        (*p_pcap_freealldevs)(TfePcapAlldevs);
        TfePcapAlldevs = NULL;
    }
}

/*
 These functions let the UI enumerate the available interfaces.

 First, TfeEnumAdapterOpen() is used to start enumeration.

 TfeEnumAdapter is then used to gather information for each adapter present
 on the system, where:

   ppname points to a pointer which will hold the name of the interface
   ppdescription points to a pointer which will hold the description of the interface

   For each of these parameters, new memory is allocated, so it has to be
   freed with lib_free().

 TfeEnumAdapterClose() must be used to stop processing.

 Each function returns 1 on success, and 0 on failure.
 TfeEnumAdapter() only fails if there is no more adpater; in this case, 
   *ppname and *ppdescription are not altered.
*/
int tfe_arch_enumadapter_open(void)
{
    if (!TfePcapLoadLibrary()) {
        return 0;
    }

    if ((*p_pcap_findalldevs)(&TfePcapAlldevs, TfePcapErrbuf) == -1)
    {
        if(g_fh) fprintf(g_fh, "ERROR in TfeEnumAdapterOpen: pcap_findalldevs: '%s'\n", TfePcapErrbuf);
        return 0;
    }

	if (!TfePcapAlldevs) {
        if(g_fh) fprintf(g_fh, "ERROR in TfeEnumAdapterOpen, finding all pcap devices - "
			"Do we have the necessary privilege rights?\n");
		return 0;
	}

    TfePcapNextDev = TfePcapAlldevs;

    return 1;
}

int tfe_arch_enumadapter(std::string & name, std::string & description)
{
    if (!TfePcapNextDev)
        return 0;

    name = TfePcapNextDev->name;
    if (TfePcapNextDev->description)
        description = TfePcapNextDev->description;
    else
        description = TfePcapNextDev->name;

    TfePcapNextDev = TfePcapNextDev->next;

    return 1;
}

int tfe_arch_enumadapter_close(void)
{
    if (TfePcapAlldevs) {
        (*p_pcap_freealldevs)(TfePcapAlldevs);
        TfePcapAlldevs = NULL;
    }
    return 1;
}


pcap_t * TfePcapOpenAdapter(const std::string & interface_name)
{
    pcap_if_t *TfePcapDevice = NULL;

    if (!tfe_arch_enumadapter_open()) {
        return NULL;
    }
    else {
        /* look if we can find the specified adapter */
        std::string name;
        std::string description;
        BOOL  found = FALSE;

        if (!interface_name.empty()) {
            /* we have an interface name, try it */
            TfePcapDevice = TfePcapAlldevs;

            while (tfe_arch_enumadapter(name, description)) {
                if (name == interface_name) {
                    found = TRUE;
                }
                if (found) break;
                TfePcapDevice = TfePcapNextDev;
            }
        }

        if (!found) {
            /* just take the first adapter */
            TfePcapDevice = TfePcapAlldevs;
        }
    }

    pcap_t * TfePcapFP = (*p_pcap_open_live)(TfePcapDevice->name, 1700, 1, 20, TfePcapErrbuf);
    if ( TfePcapFP == NULL)
    {
        if(g_fh) fprintf(g_fh, "ERROR opening adapter: '%s'\n", TfePcapErrbuf);
        tfe_arch_enumadapter_close();
        return NULL;
    }

    if ((*p_pcap_setnonblock)(TfePcapFP, 1, TfePcapErrbuf)<0)
    {
        if(g_fh) fprintf(g_fh, "WARNING: Setting PCAP to non-blocking failed: '%s'\n", TfePcapErrbuf);
    }

	/* Check the link layer. We support only Ethernet for simplicity. */
	if((*p_pcap_datalink)(TfePcapFP) != DLT_EN10MB)
	{
		if(g_fh) fprintf(g_fh, "ERROR: TFE works only on Ethernet networks.\n");
		tfe_arch_enumadapter_close();
        (*p_pcap_close)(TfePcapFP);
        TfePcapFP = NULL;
        return NULL;
	}

    if(g_fh) fprintf(g_fh, "PCAP: Successfully opened adapter: '%s'\n", TfePcapDevice->name);

    tfe_arch_enumadapter_close();
    return TfePcapFP;
}

void TfePcapCloseAdapter(pcap_t * TfePcapFP)
{
    if (TfePcapFP)
    {
        (*p_pcap_close)(TfePcapFP);
    }
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

void tfe_arch_set_hashfilter(const DWORD hash_mask[2])
{
#if defined(TFE_DEBUG_ARCH) || defined(TFE_DEBUG_FRAMES)
    if(g_fh) fprintf( g_fh, "New hash filter set: %08X:%08X.\n",
        hash_mask[1], hash_mask[0]);
#endif
}


/*
void tfe_arch_receive_remove_committed_frame(void)
{
#ifdef TFE_DEBUG_ARCH
    if(g_fh) fprintf( g_fh, "tfe_arch_receive_remove_committed_frame().\n" );
#endif
}
*/

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


typedef struct TFE_PCAP_INTERNAL_tag {
    const unsigned int size;
    BYTE *buffer;
    unsigned int rxlength;

} TFE_PCAP_INTERNAL;

/* Callback function invoked by libpcap for every incoming packet */
static
void TfePcapPacketHandler(u_char *param, const struct pcap_pkthdr *header, const u_char *pkt_data)
{
	/* RGJ changed from void to TFE_PCAP_INTERNAL for AppleWin */
	TFE_PCAP_INTERNAL *pinternal = (TFE_PCAP_INTERNAL *)param;

    /* determine the count of bytes which has been returned, 
     * but make sure not to overrun the buffer 
     */
    pinternal->rxlength = std::min(pinternal->size, header->caplen);

    memcpy(pinternal->buffer, pkt_data, pinternal->rxlength);
}

/* the following function receives a frame.

   If there's none, it returns a -1.
   If there is one, it returns the length of the frame in bytes.

   It copies the frame to *buffer and returns the number of copied 
   bytes as return value.

   At most 'len' bytes are copied.
*/
static 
int tfe_arch_receive_frame(pcap_t * TfePcapFP, TFE_PCAP_INTERNAL *pinternal)
{
    const char* error = "";

    /* check if there is something to receive */
    /* RGJ changed from void to u_char for AppleWin */
    int ret = (*p_pcap_dispatch)(TfePcapFP, 1, TfePcapPacketHandler, (u_char*)pinternal);
    if (ret > 0) {
        /* Something has been received */
        ret = pinternal->rxlength;
    }
    else {  // GH#1095: -ve values are errors
        if (ret == -1)
            error = (*p_pcap_geterr)(TfePcapFP); // "return the error text pertaining to the last pcap library error."
        ret = -1;
    }

#ifdef TFE_DEBUG_ARCH
    if(g_fh) fprintf( g_fh, "tfe_arch_receive_frame() called, returns %d (%s).\n", ret, error );
#endif

    return ret;
}

void tfe_arch_transmit(pcap_t * TfePcapFP,
                       int txlength,    /* Frame length */
                       BYTE *txframe    /* Pointer to the frame to be transmitted */
                      )
{
#ifdef TFE_DEBUG_ARCH
    if(g_fh) fprintf( g_fh, "tfe_arch_transmit() called, with: txlength=%u\n", txlength);
#endif

#ifdef TFE_DEBUG_PKTDUMP
    debug_output( "Transmit frame: ", txframe, txlength);
#endif // #ifdef TFE_DEBUG_PKTDUMP

    if ((*p_pcap_sendpacket)(TfePcapFP, txframe, txlength) == -1) {
        if(g_fh) fprintf(g_fh, "WARNING! Could not send packet!\n");
    }
}

/*
  tfe_arch_receive()

  This function checks if there was a frame received.
  If so, it returns its size, else -1.

  If there was no frame, none of the parameters is changed!

  If there was a frame, the following actions are done:

  - at maximum *plen byte are transferred into the buffer given by pbuffer
  - *plen gets the length of the received frame, EVEN if this is more
    than has been copied to pbuffer!
  - if the dest. address was accepted by the hash filter, *phashed is set, else
    cleared.
  - if the dest. address was accepted by the hash filter, *phash_index is
    set to the number of the rule leading to the acceptance
  - if the receive was ok (good CRC and valid length), *prx_ok is set, 
    else cleared.
  - if the dest. address was accepted because it's exactly our MAC address
    (set by tfe_arch_set_mac()), *pcorrect_mac is set, else cleared.
  - if the dest. address was accepted since it was a broadcast address,
    *pbroadcast is set, else cleared.
  - if the received frame had a crc error, *pcrc_error is set, else cleared
*/
int tfe_arch_receive(pcap_t * TfePcapFP,
                     const int size ,    /* Size of buffer */
                     BYTE *pbuffer       /* where to store a frame */
                    )
{
    TFE_PCAP_INTERNAL internal = { static_cast<unsigned int>(size), pbuffer, 0 };

#ifdef TFE_DEBUG_ARCH
    if(g_fh) fprintf( g_fh, "tfe_arch_receive() called, with size=%u.\n", size );
#endif

    assert((size & 1)==0);

    int len = tfe_arch_receive_frame(TfePcapFP, &internal);

    if (len!=-1) {

#ifdef TFE_DEBUG_PKTDUMP
        debug_output( "Received frame: ", internal.buffer, internal.len );
#endif // #ifdef TFE_DEBUG_PKTDUMP

        if (len&1)
            ++len;

        return len;
    }

    return -1;
}

const char * tfe_arch_lib_version()
{
    if (!TfePcapLoadLibrary())
    {
        return 0;
    }

    return p_pcap_lib_version();
}

int tfe_arch_is_npcap_loaded()
{
    return TfePcapLoadLibrary();
}

//#endif /* #ifdef HAVE_TFE */
