/*
 * tfearch.h - TFE ("The final ethernet") emulation.
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

#ifndef _TFEARCH_H
#define _TFEARCH_H

#include "types.h"

extern int  tfe_arch_init(void);
extern void tfe_arch_pre_reset(void);
extern void tfe_arch_post_reset(void);
extern int  tfe_arch_activate(const char *interface_name);
extern void tfe_arch_deactivate(void);
extern void tfe_arch_set_mac(const BYTE mac[6]);
extern void tfe_arch_set_hashfilter(const DWORD hash_mask[2]);

extern
void tfe_arch_recv_ctl( int bBroadcast,   /* broadcast */
                        int bIA,          /* individual address (IA) */
                        int bMulticast,   /* multicast if address passes the hash filter */
                        int bCorrect,     /* accept correct frames */
                        int bPromiscuous, /* promiscuous mode */
                        int bIAHash       /* accept if IA passes the hash filter */
                      );

extern
void tfe_arch_line_ctl(int bEnableTransmitter, int bEnableReceiver);

extern
void tfe_arch_transmit(int force,       /* FORCE: Delete waiting frames in transmit buffer */
                       int onecoll,     /* ONECOLL: Terminate after just one collision */
                       int inhibit_crc, /* INHIBITCRC: Do not append CRC to the transmission */
                       int tx_pad_dis,  /* TXPADDIS: Disable padding to 60 Bytes */
                       int txlength,    /* Frame length */
                       BYTE *txframe    /* Pointer to the frame to be transmitted */
                      );

extern
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
                     );

/*
 This is a helper for tfe_receive() to determine if the received frame should be accepted
 according to the settings.

 This function is even allowed to be called in tfearch.c from tfe_arch_receive() if 
 necessary, which is the reason why its prototype is included here in tfearch.h.
*/
extern 
int tfe_should_accept(unsigned char *buffer, int length, int *phashed, int *phash_index, 
                      int *pcorrect_mac, int *pbroadcast, int *pmulticast);

extern int tfe_arch_enumadapter_open(void);
extern int tfe_arch_enumadapter(char **ppname, char **ppdescription);
extern int tfe_arch_enumadapter_close(void);

#endif
