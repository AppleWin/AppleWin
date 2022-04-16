#include "StdAfx.h"

#include "DNS.h"

#ifndef _MSC_VER
#include <arpa/inet.h>
#include <netdb.h>
#endif

uint32_t getHostByName(const std::string & name)
{
    const hostent * host = gethostbyname(name.c_str());
    if (host && host->h_addrtype == AF_INET && host->h_length == sizeof(uint32_t))
    {
        const in_addr * addr = (const in_addr *)host->h_addr_list[0];
        if (addr)
        {
            return addr->s_addr;
        }
    }
    return 0;
}

const char * formatIP(const uint32_t address)
{
    in_addr in;
    in.s_addr = address;
    return inet_ntoa(in);
}
