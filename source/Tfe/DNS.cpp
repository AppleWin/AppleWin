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
