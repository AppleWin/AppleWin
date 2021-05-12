#include "StdAfx.h"
#include "linux/network/tfe2.h"

#include "Tfe/tfearch.h"
#include "Tfe/tfe.h"

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
        tfe_arch_transmit(0, 0, 0, 0, len, const_cast<BYTE *>(buffer));
    }
}

bool tfeReceiveOnePacket(const uint8_t * mac, const int size, BYTE * buffer, int & len)
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

        len = size;
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
