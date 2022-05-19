/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 2022, Andrea Odetti

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

#include "StdAfx.h"

#include "IPRaw.h"

#ifndef _MSC_VER
#include <arpa/inet.h>
#endif

#define IPV4 0x04

namespace
{

#pragma pack(push)
#pragma pack(1) // Ensure struct is packed
    struct IP4Header
    {
        uint8_t ihl : 4;
        uint8_t version : 4;
        uint8_t tos;
        uint16_t len;
        uint16_t id;
        uint16_t flags : 3;
        uint16_t fragmentOffset : 13;
        uint8_t ttl;
        uint8_t proto;
        uint16_t checksum;
        uint32_t sourceAddress;
        uint32_t destinationAddress;
    };

    struct ETH2Frame
    {
        uint8_t destinationMac[6];
        uint8_t sourceMac[6];
        uint16_t type;
    };
#pragma pack(pop)

    uint32_t sum_every_16bits(const void *addr, int count)
    {
        uint32_t sum = 0;
        const uint16_t *ptr = reinterpret_cast<const uint16_t *>(addr);

        while (count > 1)
        {
            /*  This is the inner loop */
            sum += *ptr++;
            count -= 2;
        }

        /*  Add left-over byte, if any */
        if (count > 0)
            sum += *reinterpret_cast<const uint8_t *>(ptr);

        return sum;
    }

    uint16_t checksum(const void *addr, int count)
    {
        /* Compute Internet Checksum for "count" bytes
         *         beginning at location "addr".
         * Taken from https://tools.ietf.org/html/rfc1071
         */
        uint32_t sum = sum_every_16bits(addr, count);

        /*  Fold 32-bit sum to 16 bits */
        while (sum >> 16)
            sum = (sum & 0xffff) + (sum >> 16);

        return ~sum;
    }

    // get the minimum size of a ETH Frame that contains a IP payload
    // 34 = 14 bytes for ETH2 + 20 bytes IPv4 (minimum)
    int getIPMinimumSize()
    {
        const int minimumSize = sizeof(ETH2Frame) + sizeof(IP4Header) + 0; // 0 len
        return minimumSize;
    }

}

std::vector<uint8_t> createETH2Frame(const std::vector<uint8_t> &data,
                                     const MACAddress *sourceMac, const MACAddress *destinationMac,
                                     const uint8_t ttl, const uint8_t tos, const uint8_t protocol,
                                     const uint32_t sourceAddress, const uint32_t destinationAddress)
{
    const size_t total = sizeof(ETH2Frame) + sizeof(IP4Header) + data.size();
    std::vector<uint8_t> frame(total);
    ETH2Frame *eth2frame = reinterpret_cast<ETH2Frame *>(frame.data() + 0);
    memcpy(eth2frame->destinationMac, destinationMac, sizeof(eth2frame->destinationMac));
    memcpy(eth2frame->sourceMac, sourceMac, sizeof(eth2frame->destinationMac));
    eth2frame->type = htons(0x0800);
    IP4Header *ip4header = reinterpret_cast<IP4Header *>(frame.data() + sizeof(ETH2Frame));

    ip4header->version = IPV4;
    ip4header->ihl = 0x05;  // minimum size = 20 bytes, without any extra option
    ip4header->tos = tos;
    ip4header->len = htons(static_cast<uint16_t>(sizeof(IP4Header) + data.size()));
    ip4header->id = 0;
    ip4header->fragmentOffset = 0;
    ip4header->ttl = ttl;
    ip4header->proto = protocol;
    ip4header->sourceAddress = sourceAddress;
    ip4header->destinationAddress = destinationAddress;
    ip4header->checksum = checksum(ip4header, sizeof(IP4Header));

    memcpy(frame.data() + sizeof(ETH2Frame) + sizeof(IP4Header), data.data(), data.size());

    return frame;
}

void getIPPayload(const int lengthOfFrame, const uint8_t *frame,
                  size_t &lengthOfPayload, const uint8_t *&payload, uint32_t &source, uint8_t &protocol)
{
    const int minimumSize = getIPMinimumSize();
    if (lengthOfFrame > minimumSize)
    {
        const ETH2Frame *eth2Frame = reinterpret_cast<const ETH2Frame *>(frame);
        const IP4Header *ip4header = reinterpret_cast<const IP4Header *>(frame + sizeof(ETH2Frame));
        if (eth2Frame->type == htons(0x0800) && ip4header->version == IPV4)
        {
            const uint16_t ipv4HeaderSize = ip4header->ihl * 4;
            const uint16_t ipPacketSize = ntohs(ip4header->len);
            const int expectedSize = sizeof(ETH2Frame) + ipPacketSize;
            if (ipPacketSize > ipv4HeaderSize && lengthOfFrame >= expectedSize)
            {
                protocol = ip4header->proto;
                payload = frame + sizeof(ETH2Frame) + ipv4HeaderSize;
                lengthOfPayload = ipPacketSize - ipv4HeaderSize;
                source = ip4header->sourceAddress;
                return;
            }
        }
    }
    // not a good packet
    protocol = 0xFF; // reserved protocol
    payload = nullptr;
    lengthOfPayload = 0;
    source = 0;
}
