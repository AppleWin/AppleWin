#include "StdAfx.h"
#include "tfe2.h"

#include "Tfe/tfearch.h"
#include "Tfe/tfe.h"

#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "../Log.h"

namespace
{

    bool shouldAccept(const uint8_t * mac, const BYTE *buffer, const int len)
    {
        if (len < 6)
        {
            return false;
        }

        if (buffer[0] == mac[0] &&
            buffer[1] == mac[1] &&
            buffer[2] == mac[2] &&
            buffer[3] == mac[3] &&
            buffer[4] == mac[4] &&
            buffer[5] == mac[5])
        {
            return true;
        }

        if (buffer[0] == 0xFF &&
            buffer[1] == 0xFF &&
            buffer[2] == 0xFF &&
            buffer[3] == 0xFF &&
            buffer[4] == 0xFF &&
            buffer[5] == 0xFF)
        {
            return true;
        }

        return false;
    }

}

void tfeTransmitOnePacket(const BYTE * buffer, const int len)
{
    if (tfe_enabled)
    {
        tfe_arch_transmit(0, 0, 0, 0, len, buffer);
    }
}

bool tfeReceiveOnePacket(const uint8_t * mac, BYTE * buffer, int & len)
{
    if (!tfe_enabled)
    {
        return false;
    }

    bool done;

    do
    {
        done = true;
        int hashed;
        int hash_index;
        int rx_ok;
        int correct_mac;
        int broadcast;
        int multicast = 0;
        int crc_error;

        const int newframe = tfe_arch_receive(
            buffer,       /* where to store a frame */
            &len,         /* length of received frame */
            &hashed,      /* set if the dest. address is accepted by the hash filter */
            &hash_index,  /* hash table index if hashed == TRUE */
            &rx_ok,       /* set if good CRC and valid length */
            &correct_mac, /* set if dest. address is exactly our IA */
            &broadcast,   /* set if dest. address is a broadcast address */
            &crc_error    /* set if received frame had a CRC error */
        );

        if (newframe)
        {
            /* determine ourself the type of frame */
            if (shouldAccept(mac, buffer, len))
            {
                return true;
            }
            else
            {
                done = false; /* try another frame */
            }
        }
    } while (!done);
    return false;
}

void tfeTransmitOneUDPPacket(const uint8_t * dest, const BYTE * buffer, const int len)
{
    if (len < 8)
    {
        return;
    }
    const uint16_t length = readNetworkWord(buffer + 4);
    if (len != length)
    {
        return;
    }

    const int socketSD = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socketSD <= 0)
    {
        LogFileOutput("Error: Could not open socket.\n");
        return;
    }

    sockaddr_in source;
    memset(&source, 0, sizeof(source));
    source.sin_family = AF_INET;

    // already in network order
    source.sin_port = *reinterpret_cast<const uint16_t *>(buffer + 0);
    source.sin_addr.s_addr = INADDR_ANY;

    int ret = bind(socketSD, (sockaddr*)&source, sizeof(source));
    if (ret < 0)
    {
        perror(NULL);
        LogFileOutput("Error: Could not bind socket.");
        close(socketSD);
        return;
    }

    // Configure the port and ip we want to send to
    sockaddr_in destination;
    memset(&destination, 0, sizeof(destination));
    destination.sin_family = AF_INET;

    // already in network order
    destination.sin_port = *reinterpret_cast<const uint16_t *>(buffer + 2);
    destination.sin_addr.s_addr = *reinterpret_cast<const uint32_t *>(dest);

    if (destination.sin_addr.s_addr == INADDR_BROADCAST)
    {
        // set socket options enable broadcast
        int broadcastEnable = 1;
        const int ret = setsockopt(socketSD, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable));
        if (ret)
        {
            LogFileOutput("Error: Could not open set socket to broadcast mode.\n");
            close(socketSD);
            return;
        }
    }

    const void * data = buffer + 8;
    ret = sendto(socketSD, data, len - 8, 0, (sockaddr*)&destination, sizeof(destination));
    if (ret < 0)
    {
        LogFileOutput("Error: Could not open send broadcast.");
        close(socketSD);
        return;
    }

    close(socketSD);
}

uint16_t readNetworkWord(const uint8_t * address)
{
    const uint16_t network = *reinterpret_cast<const uint16_t *>(address);
    const uint16_t host = ntohs(network);
    return host;
}
