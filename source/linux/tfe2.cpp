#include "StdAfx.h"
#include "tfe2.h"

#include "Tfe/tfearch.h"
#include "Tfe/tfe.h"
#include <iostream>
#include <iomanip>

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

std::ostream & stream_mac(std::ostream & s, const uint8_t * mac)
{
    for (size_t i = 0; i < 6; ++i)
    {
        as_hex(s, mac[i], 2);
        if (i != 6 - 1)
        {
            s << ":";
        }
    }
    return s;
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
                // std::cerr << "DEST ";
                // stream_mac(std::cerr, buffer) << " ? ";
                // stream_mac(std::cerr, mac) << " ";
                // as_hex(std::cerr, len, 4) << " bytes" << std::endl;

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

std::ostream & as_hex(std::ostream & s, const size_t value, const size_t width)
{
    s << std::setfill('0') << std::setw(width) << std::hex << value << std::dec;
    return s;
}
