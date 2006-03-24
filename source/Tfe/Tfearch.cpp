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

#include "pcap.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tfe.h"
#include "tfearch.h"
#include "tfesupp.h"


typedef pcap_t	*(*pcap_open_live_t)(const char *, int, int, int, char *);
typedef int (*pcap_dispatch_t)(pcap_t *, int, pcap_handler, u_char *);
typedef int (*pcap_setnonblock_t)(pcap_t *, int, char *);
typedef int (*pcap_datalink_t)(pcap_t *);
typedef int (*pcap_findalldevs_t)(pcap_if_t **, char *);
typedef void (*pcap_freealldevs_t)(pcap_if_t *);
typedef int (*pcap_sendpacket_t)(pcap_t *p, u_char *buf, int size);

/** #define TFE_DEBUG_ARCH 1 **/
/** #define TFE_DEBUG_PKTDUMP 1 **/

/* #define TFE_DEBUG_FRAMES - might be defined in TFE.H! */

#define TFE_DEBUG_WARN 1 /* this should not be deactivated */

static pcap_open_live_t   p_pcap_open_live;
static pcap_dispatch_t    p_pcap_dispatch;
static pcap_setnonblock_t p_pcap_setnonblock;
static pcap_findalldevs_t p_pcap_findalldevs;
static pcap_freealldevs_t p_pcap_freealldevs;
static pcap_sendpacket_t  p_pcap_sendpacket;
static pcap_datalink_t p_pcap_datalink;

static HINSTANCE pcap_library = NULL;


/* ------------------------------------------------------------------------- */
/*    variables needed                                                       */


//static log_t g_fh = g_fh;


static pcap_if_t *TfePcapNextDev = NULL;
static pcap_if_t *TfePcapAlldevs = NULL;
static pcap_t *TfePcapFP = NULL;

static char TfePcapErrbuf[PCAP_ERRBUF_SIZE];

#ifdef TFE_DEBUG_PKTDUMP

static
void debug_output( const char *text, BYTE *what, int count )
{
    char buffer[256];
    char *p = buffer;
    char *pbuffer1 = what;
    int len1 = count;
    int i;

    sprintf(buffer, "\n%s: length = %u\n", text, len1);
    OutputDebugString(buffer);
    do {
        p = buffer;
        for (i=0; (i<8) && len1>0; len1--, i++) {
            sprintf( p, "%02x ", (unsigned int)(unsigned char)*pbuffer1++);
            p += 3;
        }
        *(p-1) = '\n'; *p = 0;
        OutputDebugString(buffer);
    } while (len1>0);
}
#endif // #ifdef TFE_DEBUG_PKTDUMP


static
void TfePcapFreeLibrary(void)
{
    if (pcap_library) {
        if (!FreeLibrary(pcap_library)) {
            if(g_fh) fprintf(g_fh, "FreeLibrary WPCAP.DLL failed!");
        }
        pcap_library = NULL;

        p_pcap_open_live = NULL;
        p_pcap_dispatch = NULL;
        p_pcap_setnonblock = NULL;
        p_pcap_findalldevs = NULL;
        p_pcap_freealldevs = NULL;
        p_pcap_sendpacket = NULL;
        p_pcap_datalink = NULL;
    }
}

/* since I don't like typing too much... */
#define GET_PROC_ADDRESS_AND_TEST( _name_ ) \
    p_##_name_ = (_name_##_t) GetProcAddress(pcap_library, #_name_ ); \
    if (!p_##_name_ ) { \
        if(g_fh) fprintf(g_fh, "GetProcAddress " #_name_ " failed!"); \
        TfePcapFreeLibrary(); \
        return FALSE; \
    } 

static
BOOL TfePcapLoadLibrary(void)
{
    if (!pcap_library) {
        pcap_library = LoadLibrary("wpcap.dll");

        if (!pcap_library) {
            if(g_fh) fprintf(g_fh, "LoadLibrary WPCAP.DLL failed!" );
            return FALSE;
        }

        GET_PROC_ADDRESS_AND_TEST(pcap_open_live);
        GET_PROC_ADDRESS_AND_TEST(pcap_dispatch);
        GET_PROC_ADDRESS_AND_TEST(pcap_setnonblock);
        GET_PROC_ADDRESS_AND_TEST(pcap_findalldevs);
        GET_PROC_ADDRESS_AND_TEST(pcap_freealldevs);
        GET_PROC_ADDRESS_AND_TEST(pcap_sendpacket);
        GET_PROC_ADDRESS_AND_TEST(pcap_datalink);
    }

    return TRUE;
}

#undef GET_PROC_ADDRESS_AND_TEST



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
        if(g_fh) fprintf(g_fh, "ERROR in TfeEnumAdapterOpen: pcap_findalldevs: '%s'", TfePcapErrbuf);
        return 0;
    }

	if (!TfePcapAlldevs) {
        if(g_fh) fprintf(g_fh, "ERROR in TfeEnumAdapterOpen, finding all pcap devices - "
			"Do we have the necessary privilege rights?");
		return 0;
	}

    TfePcapNextDev = TfePcapAlldevs;

    return 1;
}

int tfe_arch_enumadapter(char **ppname, char **ppdescription)
{
    if (!TfePcapNextDev)
        return 0;

    *ppname = lib_stralloc(TfePcapNextDev->name);
    *ppdescription = lib_stralloc(TfePcapNextDev->description);

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

static
BOOL TfePcapOpenAdapter(const char *interface_name) 
{
    pcap_if_t *TfePcapDevice = NULL;

    if (!tfe_enumadapter_open()) {
        return FALSE;
    }
    else {
        /* look if we can find the specified adapter */
        char *pname;
        char *pdescription;
        BOOL  found = FALSE;

        if (interface_name) {
            /* we have an interface name, try it */
            TfePcapDevice = TfePcapAlldevs;

            while (tfe_enumadapter(&pname, &pdescription)) {
                if (strcmp(pname, interface_name)==0) {
                    found = TRUE;
                }
                lib_free(pname);
                lib_free(pdescription);
                if (found) break;
                TfePcapDevice = TfePcapNextDev;
            }
        }

        if (!found) {
            /* just take the first adapter */
            TfePcapDevice = TfePcapAlldevs;
        }
    }

    TfePcapFP = (*p_pcap_open_live)(TfePcapDevice->name, 1700, 1, 20, TfePcapErrbuf);
    if ( TfePcapFP == NULL)
    {
        if(g_fh) fprintf(g_fh, "ERROR opening adapter: '%s'", TfePcapErrbuf);
        tfe_enumadapter_close();
        return FALSE;
    }

    if ((*p_pcap_setnonblock)(TfePcapFP, 1, TfePcapErrbuf)<0)
    {
        if(g_fh) fprintf(g_fh, "WARNING: Setting PCAP to non-blocking failed: '%s'", TfePcapErrbuf);
    }

	/* Check the link layer. We support only Ethernet for simplicity. */
	if((*p_pcap_datalink)(TfePcapFP) != DLT_EN10MB)
	{
		if(g_fh) fprintf(g_fh, "ERROR: TFE works only on Ethernet networks.");
		tfe_enumadapter_close();
        return FALSE;
	}
	
    tfe_enumadapter_close();
    return TRUE;
}


/* ------------------------------------------------------------------------- */
/*    the architecture-dependend functions                                   */


int tfe_arch_init(void)
{
 //   g_fh = log_open("TFEARCH");

    if (!TfePcapLoadLibrary()) {
        return 0;
    }

    return 1;
}

void tfe_arch_pre_reset( void )
{
#ifdef TFE_DEBUG_ARCH
    if(g_fh) fprintf( g_fh, "tfe_arch_pre_reset()." );
#endif
}

void tfe_arch_post_reset( void )
{
#ifdef TFE_DEBUG_ARCH
    if(g_fh) fprintf( g_fh, "tfe_arch_post_reset()." );
#endif
}

int tfe_arch_activate(const char *interface_name)
{
#ifdef TFE_DEBUG_ARCH
    if(g_fh) fprintf( g_fh, "tfe_arch_activate()." );
#endif
    if (!TfePcapOpenAdapter(interface_name)) {
        return 0;
    }
    return 1;
}

void tfe_arch_deactivate( void )
{
#ifdef TFE_DEBUG_ARCH
    if(g_fh) fprintf( g_fh, "tfe_arch_deactivate()." );
#endif
}

void tfe_arch_set_mac( const BYTE mac[6] )
{
#if defined(TFE_DEBUG_ARCH) || defined(TFE_DEBUG_FRAMES)
    if(g_fh) fprintf( g_fh, "New MAC address set: %02X:%02X:%02X:%02X:%02X:%02X.",
        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5] );
#endif
}

void tfe_arch_set_hashfilter(const DWORD hash_mask[2])
{
#if defined(TFE_DEBUG_ARCH) || defined(TFE_DEBUG_FRAMES)
    if(g_fh) fprintf( g_fh, "New hash filter set: %08X:%08X.",
        hash_mask[1], hash_mask[0]);
#endif
}


/*
void tfe_arch_receive_remove_committed_frame(void)
{
#ifdef TFE_DEBUG_ARCH
    if(g_fh) fprintf( g_fh, "tfe_arch_receive_remove_committed_frame()." );
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
	}
#endif
}


typedef struct TFE_PCAP_INTERNAL_tag {

    unsigned int len;
    BYTE *buffer;

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
    if (header->caplen < pinternal->len)
        pinternal->len = header->caplen;

    memcpy(pinternal->buffer, pkt_data, pinternal->len);
}

/* the following function receives a frame.

   If there's none, it returns a -1.
   If there is one, it returns the length of the frame in bytes.

   It copies the frame to *buffer and returns the number of copied 
   bytes as return value.

   At most 'len' bytes are copied.
*/
static 
int tfe_arch_receive_frame(TFE_PCAP_INTERNAL *pinternal)
{
    int ret = -1;

    /* check if there is something to receive */
	/* RGJ changed from void to u_char for AppleWin */
	if ((*p_pcap_dispatch)(TfePcapFP, 1, TfePcapPacketHandler, (u_char *)pinternal)!=0) {
        /* Something has been received */
        ret = pinternal->len;
    }

#ifdef TFE_DEBUG_ARCH
    if(g_fh) fprintf( g_fh, "tfe_arch_receive_frame() called, returns %d.", ret );
#endif

    return ret;
}

void tfe_arch_transmit(int force,       /* FORCE: Delete waiting frames in transmit buffer */
                       int onecoll,     /* ONECOLL: Terminate after just one collision */
                       int inhibit_crc, /* INHIBITCRC: Do not append CRC to the transmission */
                       int tx_pad_dis,  /* TXPADDIS: Disable padding to 60 Bytes */
                       int txlength,    /* Frame length */
                       BYTE *txframe    /* Pointer to the frame to be transmitted */
                      )
{
#ifdef TFE_DEBUG_ARCH
    if(g_fh) fprintf( g_fh, "tfe_arch_transmit() called, with: "
        "force = %s, onecoll = %s, inhibit_crc=%s, tx_pad_dis=%s, txlength=%u",
        force ?       "TRUE" : "FALSE", 
        onecoll ?     "TRUE" : "FALSE", 
        inhibit_crc ? "TRUE" : "FALSE", 
        tx_pad_dis ?  "TRUE" : "FALSE", 
        txlength
        );
#endif

#ifdef TFE_DEBUG_PKTDUMP
    debug_output( "Transmit frame: ", txframe, txlength);
#endif // #ifdef TFE_DEBUG_PKTDUMP

    if ((*p_pcap_sendpacket)(TfePcapFP, txframe, txlength) == -1) {
        if(g_fh) fprintf(g_fh, "WARNING! Could not send packet!");
    }
}

/*
  tfe_arch_receive()

  This function checks if there was a frame received.
  If so, it returns 1, else 0.

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
int tfe_arch_receive(BYTE *pbuffer  ,    /* where to store a frame */
                     int  *plen,         /* IN: maximum length of frame to copy; 
                                            OUT: length of received frame 
                                            OUT can be bigger than IN if received frame was
                                                longer than supplied buffer */
                     int  *phashed,      /* set if the dest. address is accepted by the hash filter */
                     int  *phash_index,  /* hash table index if hashed == TRUE */   
                     int  *prx_ok,       /* set if good CRC and valid length */
                     int  *pcorrect_mac, /* set if dest. address is exactly our IA */
                     int  *pbroadcast,   /* set if dest. address is a broadcast address */
                     int  *pcrc_error    /* set if received frame had a CRC error */
                    )
{
    int len;

    TFE_PCAP_INTERNAL internal = { *plen, pbuffer };


#ifdef TFE_DEBUG_ARCH
    if(g_fh) fprintf( g_fh, "tfe_arch_receive() called, with *plen=%u.", *plen );
#endif

    assert((*plen&1)==0);

    len = tfe_arch_receive_frame(&internal);

    if (len!=-1) {

#ifdef TFE_DEBUG_PKTDUMP
        debug_output( "Received frame: ", internal.buffer, internal.len );
#endif // #ifdef TFE_DEBUG_PKTDUMP

        if (len&1)
            ++len;

        *plen = len;

        /* we don't decide if this frame fits the needs;
         * by setting all zero, we let tfe.c do the work
         * for us
         */
        *phashed =
        *phash_index =
        *pbroadcast = 
        *pcorrect_mac =
        *pcrc_error = 0;

        /* this frame has been received correctly */
        *prx_ok = 1;

        return 1;
    }

    return 0;
}

//#endif /* #ifdef HAVE_TFE */
