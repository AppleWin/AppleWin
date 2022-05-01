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

#include "../CommonVICE/types.h"
#include <string>

extern void tfe_arch_set_mac(const BYTE mac[6]);
extern void tfe_arch_set_hashfilter(const DWORD hash_mask[2]);

struct pcap;
typedef struct pcap pcap_t;

pcap_t * TfePcapOpenAdapter(const std::string & interface_name);
void TfePcapCloseAdapter(pcap_t * TfePcapFP);

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
void tfe_arch_transmit(pcap_t * TfePcapFP,
                       int txlength,    /* Frame length */
                       BYTE *txframe    /* Pointer to the frame to be transmitted */
                      );

extern
int tfe_arch_receive(pcap_t * TfePcapFP,
                     const int size ,    /* Size of buffer */
                     BYTE *pbuffer       /* where to store a frame */
                    );

extern int tfe_arch_is_npcap_loaded();
extern int tfe_arch_enumadapter_open(void);
extern int tfe_arch_enumadapter(std::string & name, std::string & description);
extern int tfe_arch_enumadapter_close(void);

extern const char * tfe_arch_lib_version();

#endif
