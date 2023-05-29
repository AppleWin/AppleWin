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

#include <StdAfx.h>
#include "YamlHelper.h"
#include "Uthernet2.h"
#include "Interface.h"
#include "Tfe/NetworkBackend.h"
#include "Tfe/PCapBackend.h"
#include "Tfe/IPRaw.h"
#include "Tfe/DNS.h"
#include "W5100.h"
#include "Registry.h"


// Virtual DNS
// Virtual DNS is an extension to the W5100
// It enables DNS resolution by setting P3 = 1 for IP / TCP and UDP
// the length-prefixed hostname is in 0x2A-0xFF is each socket memory
// it can be identified with PTIMER = 0.
// this means, one can use TCP & UDP without a NetworkBackend.


// Linux uses EINPROGRESS while Windows returns WSAEWOULDBLOCK
// when the connect() calls is ongoing
//
// in the checks below we allow both in all cases
// (errno == SOCK_EINPROGRESS || errno == SOCK_EWOULDBLOCK)
// this works, bu we could instead define 2 functions and check only the correct one
#ifdef _MSC_VER

typedef int ssize_t;
typedef int socklen_t;

#define sock_error() WSAGetLastError()
// too complicated to call FormatMessage, just print number
#define ERROR_FMT "d"
#define STRERROR(x) x

#define SOCK_EAGAIN WSAEWOULDBLOCK
#define SOCK_EWOULDBLOCK WSAEWOULDBLOCK
#define SOCK_EINPROGRESS WSAEINPROGRESS

#else

#define sock_error() errno

#define ERROR_FMT "s"
#define STRERROR(x) strerror(x)

#define SOCK_EAGAIN EAGAIN
#define SOCK_EWOULDBLOCK EWOULDBLOCK
#define SOCK_EINPROGRESS EINPROGRESS
#include <sys/socket.h>
#include <arpa/inet.h>
#include <poll.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>

#endif

// fix SOCK_NONBLOCK for e.g. macOS
#ifndef SOCK_NONBLOCK
// DISCALIMER
// totally untested, use at your own risk
#include <fcntl.h>
#define SOCK_NONBLOCK O_NONBLOCK
#endif

// Dest MAC + Source MAC + Ether Type
#define ETH_MINIMUM_SIZE (6 + 6 + 2)

// #define U2_LOG_VERBOSE
// #define U2_LOG_TRAFFIC
// #define U2_LOG_STATE
// #define U2_LOG_UNKNOWN

#define MAC_FMT "%02X:%02X:%02X:%02X:%02X:%02X"
#define MAC_DEST(p) p[0], p[1], p[2], p[3], p[4], p[5]
#define MAC_SOURCE(p) p[6], p[7], p[8], p[9], p[10], p[11]

#include "Memory.h"
#include "Log.h"

namespace
{

    uint16_t readNetworkWord(const uint8_t *address)
    {
        const uint16_t network = *reinterpret_cast<const uint16_t *>(address);
        const uint16_t host = ntohs(network);
        return host;
    }

    uint32_t readAddress(const uint8_t *ptr)
    {
        const uint32_t address = *reinterpret_cast<const uint32_t *>(ptr);
        return address;
    }

    uint8_t getIByte(const uint16_t value, const size_t shift)
    {
        return (value >> shift) & 0xFF;
    }

    void write8(Socket &socket, std::vector<uint8_t> &memory, const uint8_t value)
    {
        const uint16_t base = socket.receiveBase;
        const uint16_t address = base + socket.sn_rx_wr;
        memory[address] = value;
        socket.sn_rx_wr = (socket.sn_rx_wr + 1) % socket.receiveSize;
        ++socket.sn_rx_rsr;
    }

    // reverses the byte order
    void write16(Socket &socket, std::vector<uint8_t> &memory, const uint16_t value)
    {
        write8(socket, memory, getIByte(value, 8)); // high
        write8(socket, memory, getIByte(value, 0)); // low
    }

    void writeData(Socket &socket, std::vector<uint8_t> &memory, const uint8_t *data, const size_t len)
    {
        for (size_t c = 0; c < len; ++c)
        {
            write8(socket, memory, data[c]);
        }
    }

    // no byte reversal
    template <typename T>
    void writeAny(Socket &socket, std::vector<uint8_t> &memory, const T &t)
    {
        const uint8_t *data = reinterpret_cast<const uint8_t *>(&t);
        const uint16_t len = sizeof(T);
        writeData(socket, memory, data, len);
    }

    void writeDataMacRaw(Socket &socket, std::vector<uint8_t> &memory, const uint8_t *data, const size_t len)
    {
        // size includes sizeof(size)
        const size_t size = len + sizeof(uint16_t);
        write16(socket, memory, static_cast<uint16_t>(size));
        writeData(socket, memory, data, len);
    }

    void writeDataIPRaw(Socket &socket, std::vector<uint8_t> &memory, const uint8_t *data, const size_t len, const uint32_t source)
    {
        writeAny(socket, memory, source);
        write16(socket, memory, static_cast<uint16_t>(len));
        writeData(socket, memory, data, len);
    }

    void writeDataForProtocol(Socket &socket, std::vector<uint8_t> &memory, const uint8_t *data, const size_t len, const sockaddr_in &source)
    {
        if (socket.getStatus() == W5100_SN_SR_SOCK_UDP)
        {
            // these are already in network order
            writeAny(socket, memory, source.sin_addr);
            writeAny(socket, memory, source.sin_port);

            // size does not include sizeof(size)
            write16(socket, memory, static_cast<uint16_t>(len));
        } // no header for TCP

        writeData(socket, memory, data, len);
    }

}

Socket::Socket()
    : transmitBase(0)
    , transmitSize(0)
    , receiveBase(0)
    , receiveSize(0)
    , registerAddress(0)
    , sn_rx_wr(0)
    , sn_rx_rsr(0)
    , mySocketStatus(W5100_SN_SR_CLOSED)
    , myFD(INVALID_SOCKET)
    , myHeaderSize(0)
{
}

void Socket::clearFD()
{
    if (myFD != INVALID_SOCKET)
    {
#ifdef _MSC_VER
        closesocket(myFD);
#else
        close(myFD);
#endif
    }
    myFD = INVALID_SOCKET;
    setStatus(W5100_SN_SR_CLOSED);
}

void Socket::setStatus(const uint8_t status)
{
    mySocketStatus = status;

    switch (mySocketStatus)
    {
    case W5100_SN_SR_ESTABLISHED:
        myHeaderSize = 0;  // no header for TCP
        break;
    case W5100_SN_SR_SOCK_UDP:
        myHeaderSize = 4 + 2 + 2;  // IP + Port + Size
        break;
    case W5100_SN_SR_SOCK_IPRAW:
        myHeaderSize = 4 + 2;  // IP + Size
        break;
    case W5100_SN_SR_SOCK_MACRAW:
        myHeaderSize = 2;  // Size
        break;
    default:
        myHeaderSize = 0;
        break;
    }
}

void Socket::setFD(const socket_t fd, const uint8_t status)
{
    clearFD();
    myFD = fd;
    setStatus(status);
}

Socket::socket_t Socket::getFD() const
{
    return myFD;
}

uint8_t Socket::getStatus() const
{
    return mySocketStatus;
}

uint8_t Socket::getHeaderSize() const
{
    return myHeaderSize;
}

Socket::~Socket()
{
    clearFD();
}

bool Socket::isOpen() const
{
    return (myFD != INVALID_SOCKET) &&
        ((mySocketStatus == W5100_SN_SR_ESTABLISHED) || (mySocketStatus == W5100_SN_SR_SOCK_UDP));
}

void Socket::process()
{
    if (myFD != INVALID_SOCKET && mySocketStatus == W5100_SN_SR_SOCK_SYNSENT)
    {
#ifdef _MSC_VER
        FD_SET writefds, exceptfds;
        FD_ZERO(&writefds);
        FD_ZERO(&exceptfds);
        FD_SET(myFD, &writefds);
        FD_SET(myFD, &exceptfds);
        const timeval timeout = {0, 0};
        if (select(0, NULL, &writefds, &exceptfds, &timeout) > 0)
#else
        pollfd pfd = {.fd = myFD, .events = POLLOUT};
        if (poll(&pfd, 1, 0) > 0)
#endif
        {
            int err = 0;
            socklen_t elen = sizeof(err);
            const int res = getsockopt(myFD, SOL_SOCKET, SO_ERROR, reinterpret_cast<char *>(&err), &elen);

            if (res == 0 && err == 0)
            {
                setStatus(W5100_SN_SR_ESTABLISHED);
#ifdef U2_LOG_STATE
                LogFileOutput("U2: TCP[]: Connected\n");
#endif
            }
            else
            {
                clearFD();
#ifdef U2_LOG_STATE
                LogFileOutput("U2: TCP[]: Connection error: %d - %" ERROR_FMT "\n", res, STRERROR(err));
#endif
            }
        }
    }
}

bool Socket::isThereRoomFor(const size_t len) const
{
    const uint16_t rsr = sn_rx_rsr;    // already present
    const uint16_t size = receiveSize; // total size

    return rsr + len + myHeaderSize < size; // "not =": we do not want to fill the buffer.
}

uint16_t Socket::getFreeRoom() const
{
    const uint16_t rsr = sn_rx_rsr;    // already present
    const uint16_t size = receiveSize; // total size

    const uint16_t total = size - rsr;
    return total > myHeaderSize ? total - myHeaderSize : 0;
}

#define SS_YAML_KEY_SOCKET_RX_WRITE_REGISTER "RX Write Register"
#define SS_YAML_KEY_SOCKET_RX_SIZE_REGISTER "RX Size Register"

#define SS_YAML_KEY_SOCKET_REGISTER "Socket Register"

void Socket::SaveSnapshot(YamlSaveHelper &yamlSaveHelper)
{
    yamlSaveHelper.SaveHexUint16(SS_YAML_KEY_SOCKET_RX_WRITE_REGISTER, sn_rx_wr);
    yamlSaveHelper.SaveHexUint16(SS_YAML_KEY_SOCKET_RX_SIZE_REGISTER, sn_rx_rsr);
    yamlSaveHelper.SaveHexUint8(SS_YAML_KEY_SOCKET_REGISTER, mySocketStatus);
}

bool Socket::LoadSnapshot(YamlLoadHelper &yamlLoadHelper)
{
    clearFD();

    sn_rx_wr = yamlLoadHelper.LoadUint(SS_YAML_KEY_SOCKET_RX_WRITE_REGISTER);
    sn_rx_rsr = yamlLoadHelper.LoadUint(SS_YAML_KEY_SOCKET_RX_SIZE_REGISTER);
    uint8_t socketStatus = yamlLoadHelper.LoadUint(SS_YAML_KEY_SOCKET_REGISTER);

    // transmit and receive sizes are restored from the card common registers
    switch (socketStatus)
    {
    case W5100_SN_SR_SOCK_MACRAW:
    case W5100_SN_SR_SOCK_IPRAW:
        // we can restore RAW sockets
        break;
    default:
        // no point in restoring a broken UDP or TCP connection
        // just reset the socket
        socketStatus = W5100_SN_SR_CLOSED;
        // for the same reason there is no point in saving myFD
        break;
    }

    setStatus(socketStatus);

    return true;
}

const std::string& Uthernet2::GetSnapshotCardName()
{
    static const std::string name("Uthernet II");
    return name;
}

Uthernet2::Uthernet2(UINT slot) : NetworkCard(CT_Uthernet2, slot)
{
#ifdef _MSC_VER
    WSADATA wsaData;
    myWSAStartup = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (myWSAStartup)
    {
        const int error = sock_error();
        LogFileOutput("U2: WSAStartup: error %" ERROR_FMT "\n", STRERROR(error));
    }
#endif

    myVirtualDNSEnabled = GetRegistryVirtualDNS(slot);
    Reset(true);
}

Uthernet2::~Uthernet2()
{
#ifdef _MSC_VER
    if (myWSAStartup == 0)
    {
        WSACleanup();
    }
#endif
}

void Uthernet2::setSocketModeRegister(const size_t i, const uint16_t address, const uint8_t value)
{
    myMemory[address] = value;
    const uint8_t protocol = value & W5100_SN_MR_PROTO_MASK;
    switch (protocol)
    {
    case W5100_SN_MR_CLOSED:
#ifdef U2_LOG_STATE
        LogFileOutput("U2: Mode[%" SIZE_T_FMT "]: closed\n", i);
#endif
        break;
    case W5100_SN_MR_TCP:
    case W5100_SN_MR_TCP_DNS:
#ifdef U2_LOG_STATE
        LogFileOutput("U2: Mode[%" SIZE_T_FMT "]: TCP\n", i);
#endif
        break;
    case W5100_SN_MR_UDP:
    case W5100_SN_MR_UDP_DNS:
#ifdef U2_LOG_STATE
        LogFileOutput("U2: Mode[%" SIZE_T_FMT "]: UDP\n", i);
#endif
        break;
    case W5100_SN_MR_IPRAW:
    case W5100_SN_MR_IPRAW_DNS:
#ifdef U2_LOG_STATE
        LogFileOutput("U2: Mode[%" SIZE_T_FMT "]: IPRAW\n", i);
#endif
        break;
    case W5100_SN_MR_MACRAW:
#ifdef U2_LOG_STATE
        LogFileOutput("U2: Mode[%" SIZE_T_FMT "]: MACRAW\n", i);
#endif
        break;
#ifdef U2_LOG_UNKNOWN
    default:
        LogFileOutput("U2: Unknown protocol: %02x\n", protocol);
#endif
    }
}

void Uthernet2::setTXSizes(const uint16_t address, uint8_t value)
{
    myMemory[address] = value;
    uint16_t base = W5100_TX_BASE;
    const uint16_t end = W5100_RX_BASE;
    for (Socket &socket : mySockets)
    {
        socket.transmitBase = base;

        const uint8_t bits = value & 0x03;
        value >>= 2;

        const uint16_t size = 1 << (10 + bits);
        base += size;

        if (base > end)
        {
            base = end;
        }
        socket.transmitSize = base - socket.transmitBase;
    }
}

void Uthernet2::setRXSizes(const uint16_t address, uint8_t value)
{
    myMemory[address] = value;
    uint16_t base = W5100_RX_BASE;
    const uint16_t end = W5100_MEM_SIZE;
    for (Socket &socket : mySockets)
    {
        socket.receiveBase = base;

        const uint8_t bits = value & 0x03;
        value >>= 2;

        const uint16_t size = 1 << (10 + bits);
        base += size;

        if (base > end)
        {
            base = end;
        }
        socket.receiveSize = base - socket.receiveBase;
    }
}

uint16_t Uthernet2::getTXDataSize(const size_t i) const
{
    const Socket &socket = mySockets[i];
    const uint16_t size = socket.transmitSize;
    const uint16_t mask = size - 1;

    const int sn_tx_rd = readNetworkWord(myMemory.data() + socket.registerAddress + W5100_SN_TX_RD0) & mask;
    const int sn_tx_wr = readNetworkWord(myMemory.data() + socket.registerAddress + W5100_SN_TX_WR0) & mask;

    int dataPresent = sn_tx_wr - sn_tx_rd;
    if (dataPresent < 0)
    {
        dataPresent += size;
    }
    return dataPresent;
}

uint8_t Uthernet2::getTXFreeSizeRegister(const size_t i, const size_t shift) const
{
    const int size = mySockets[i].transmitSize;
    const uint16_t present = getTXDataSize(i);
    const uint16_t free = size - present;
    const uint8_t reg = getIByte(free, shift);
    return reg;
}

uint8_t Uthernet2::getRXDataSizeRegister(const size_t i, const size_t shift) const
{
    const uint16_t rsr = mySockets[i].sn_rx_rsr;
    const uint8_t reg = getIByte(rsr, shift);
    return reg;
}

void Uthernet2::updateRSR(const size_t i)
{
    Socket &socket = mySockets[i];

    const int size = socket.receiveSize;
    const uint16_t mask = size - 1;

    const int sn_rx_rd = readNetworkWord(myMemory.data() + socket.registerAddress + W5100_SN_RX_RD0) & mask;
    const int sn_rx_wr = socket.sn_rx_wr & mask;
    int dataPresent = sn_rx_wr - sn_rx_rd;
    if (dataPresent < 0)
    {
        dataPresent += size;
    }
    // is this logic correct?
    // here we are re-synchronising the size with the pointers
    // elsewhere I have seen people updating this value
    // by the amount of how much 0x28 has moved forward
    // but then we need to keep track of where it was
    // the final result should be the same
#ifdef U2_LOG_TRAFFIC
    if (socket.sn_rx_rsr != dataPresent)
    {
        LogFileOutput("U2: Recv[%" SIZE_T_FMT "]: %d -> %d bytes\n", i, socket.sn_rx_rsr, dataPresent);
    }
#endif
    socket.sn_rx_rsr = dataPresent;
}

int Uthernet2::receiveForMacAddress(const bool acceptAll, const int size, uint8_t * data, PacketDestination & packetDestination)
{
    const uint8_t * mac = myMemory.data() + W5100_SHAR0;

    // loop until we receive a valid frame, or there is nothing to receive
    int len;
    while ((len = myNetworkBackend->receive(size, data)) > 0)
    {
        // smaller frames are not good anyway
        if (len >= ETH_MINIMUM_SIZE)
        {
            if (data[0] == mac[0] &&
                data[1] == mac[1] &&
                data[2] == mac[2] &&
                data[3] == mac[3] &&
                data[4] == mac[4] &&
                data[5] == mac[5])
            {
                packetDestination = HOST;
                return len;
            }

            if (data[0] == 0xFF &&
                data[1] == 0xFF &&
                data[2] == 0xFF &&
                data[3] == 0xFF &&
                data[4] == 0xFF &&
                data[5] == 0xFF)
            {
                packetDestination = BROADCAST;
                return len;
            }

            if (acceptAll)
            {
                packetDestination = OTHER;
                return len;
            }

        }
        // skip this frame and try with another one
    }
    // no frames available to process
    return len;
}

void Uthernet2::receiveOnePacketRaw()
{
    bool acceptAll = false;
    int macRawSocket = -1;  // to which IPRaw soccket to send to (can only be 0)

    Socket & socket0 = mySockets[0];
    if (socket0.getStatus() == W5100_SN_SR_SOCK_MACRAW)
    {
        macRawSocket = 0;  // the only MAC Raw socket is open, packet will go there as a fallback
        const uint8_t mr = myMemory[socket0.registerAddress + W5100_SN_MR];

        // see if MAC RAW filters or not
        const bool filterMAC = mr & W5100_SN_MR_MF;
        if (!filterMAC)
        {
            acceptAll = true;
        }
    }

    uint8_t buffer[MAX_RXLENGTH];
    PacketDestination packetDestination;
    const int len = receiveForMacAddress(acceptAll, sizeof(buffer), buffer, packetDestination);
    if (len > 0)
    {
        const uint8_t * payload;
        size_t lengthOfPayload;
        uint32_t source;
        uint8_t packetProtocol;
        getIPPayload(len, buffer, lengthOfPayload, payload, source, packetProtocol);

        // see if there is a IPRAW socket that should accept thi spacket
        int ipRawSocket = -1;
        if (packetDestination != OTHER)  // IPRaw always filters (HOST or BROADCAST, never OTHER)
        {
            for (size_t i = 0; i < mySockets.size(); ++i)
            {
                Socket & socket = mySockets[i];

                if (socket.getStatus() == W5100_SN_SR_SOCK_IPRAW)
                {
                    // IP only accepts by protocol & always filters MAC
                    const uint8_t socketProtocol = myMemory[socket.registerAddress + W5100_SN_PROTO];
                    if (payload && packetProtocol == socketProtocol)
                    {
                        ipRawSocket = i;
                        break; // a valid IPRAW socket has been found
                    }
                }
                // we should probably check for UDP & TCP sockets and filter these packets too
            }
        }

        // priority to IPRAW
        if (ipRawSocket >= 0)
        {
            receiveOnePacketIPRaw(ipRawSocket, lengthOfPayload, payload, source, packetProtocol, len);
        }
        // fallback to MACRAW (if open)
        else if (macRawSocket >= 0)
        {
            receiveOnePacketMacRaw(macRawSocket, len, buffer);
        }
        // else packet is dropped
    }
}

void Uthernet2::receiveOnePacketMacRaw(const size_t i, const int size, uint8_t * data)
{
    Socket &socket = mySockets[i];

    if (socket.isThereRoomFor(size))
    {
        writeDataMacRaw(socket, myMemory, data, size);
#ifdef U2_LOG_TRAFFIC
        LogFileOutput("U2: Read MACRAW[%" SIZE_T_FMT "]: " MAC_FMT " -> " MAC_FMT ": +%d+%d -> %d bytes\n", i, MAC_SOURCE(data), MAC_DEST(data),
            socket.getHeaderSize(), size, socket.sn_rx_rsr);
#endif
    }
    else
    {
        // drop it
#ifdef U2_LOG_TRAFFIC
        LogFileOutput("U2: Skip MACRAW[%" SIZE_T_FMT "]: %d bytes\n", i, size);
#endif
    }
}

void Uthernet2::receiveOnePacketIPRaw(const size_t i, const size_t lengthOfPayload, const uint8_t * payload, const uint32_t source, const uint8_t protocol, const int len)
{
    Socket &socket = mySockets[i];

    if (socket.isThereRoomFor(lengthOfPayload))
    {
        writeDataIPRaw(socket, myMemory, payload, lengthOfPayload, source);
#ifdef U2_LOG_TRAFFIC
        LogFileOutput("U2: Read IPRAW[%" SIZE_T_FMT "]: +%d+%" SIZE_T_FMT " (%d) -> %d bytes\n", i, socket.getHeaderSize(),
            lengthOfPayload, len, socket.sn_rx_rsr);
#endif
    }
    else
    {
        // drop it
#ifdef U2_LOG_TRAFFIC
        LogFileOutput("U2: Skip IPRAW[%" SIZE_T_FMT "]: %" SIZE_T_FMT " (%d) bytes \n", i, lengthOfPayload, len);
#endif
    }
}

// UDP & TCP
void Uthernet2::receiveOnePacketFromSocket(const size_t i)
{
    Socket &socket = mySockets[i];
    if (socket.isOpen())
    {
        const uint16_t freeRoom = socket.getFreeRoom();
        if (freeRoom > 32) // avoid meaningless reads
        {
            std::vector<uint8_t> buffer(freeRoom - 1); // do not fill the buffer completely
            sockaddr_in source = {0};
            socklen_t len = sizeof(sockaddr_in);
            const ssize_t data = recvfrom(socket.getFD(), reinterpret_cast<char *>(buffer.data()), buffer.size(), 0, (struct sockaddr *)&source, &len);
#ifdef U2_LOG_TRAFFIC
            const char *proto = socket.getStatus() == W5100_SN_SR_SOCK_UDP ? "UDP" : "TCP";
#endif
            if (data > 0)
            {
                writeDataForProtocol(socket, myMemory, buffer.data(), data, source);
#ifdef U2_LOG_TRAFFIC
                LogFileOutput("U2: Read %s[%" SIZE_T_FMT "]: +%d+%" SIZE_T_FMT " -> %d bytes\n", proto, i, socket.getHeaderSize(),
                    data, socket.sn_rx_rsr);
#endif
            }
            else if (data == 0)
            {
                // gracefull termination
                socket.clearFD();
            }
            else // data < 0;
            {
                const int error = sock_error();
                if (error != SOCK_EAGAIN && error != SOCK_EWOULDBLOCK)
                {
#ifdef U2_LOG_TRAFFIC
                    LogFileOutput("U2: %s[%" SIZE_T_FMT "]: recvfrom error %" ERROR_FMT "\n", proto, i, STRERROR(error));
#endif
                    socket.clearFD();
                }
            }
        }
    }
}

void Uthernet2::receiveOnePacket(const size_t i)
{
    const Socket &socket = mySockets[i];
    switch (socket.getStatus())
    {
    case W5100_SN_SR_SOCK_MACRAW:
    case W5100_SN_SR_SOCK_IPRAW:
        receiveOnePacketRaw();
        break;
    case W5100_SN_SR_ESTABLISHED:
    case W5100_SN_SR_SOCK_UDP:
        receiveOnePacketFromSocket(i);
        break;
    case W5100_SN_SR_CLOSED:
#ifdef U2_LOG_STATE
        LogFileOutput("U2: Read[%" SIZE_T_FMT "]: reading from a closed socket\n", i);
#endif
        break;
#ifdef U2_LOG_UNKNOWN
    default:
        LogFileOutput("U2: Read[%" SIZE_T_FMT "]: unknown mode: %02x\n", i, socket.getStatus());
#endif
    };
}

void Uthernet2::sendDataIPRaw(const size_t i, std::vector<uint8_t> &payload)
{
    const Socket &socket = mySockets[i];

    const uint8_t ttl = myMemory[socket.registerAddress + W5100_SN_TTL];
    const uint8_t tos = myMemory[socket.registerAddress + W5100_SN_TOS];
    const uint8_t protocol = myMemory[socket.registerAddress + W5100_SN_PROTO];
    const uint32_t source = readAddress(myMemory.data() + W5100_SIPR0);
    const uint32_t dest = readAddress(myMemory.data() + socket.registerAddress + W5100_SN_DIPR0);

    const MACAddress * sourceMac = reinterpret_cast<const MACAddress *>(myMemory.data() + W5100_SHAR0);
    const MACAddress * destinationMac;
    getMACAddress(dest, destinationMac);

    std::vector<uint8_t> packet = createETH2Frame(payload, sourceMac, destinationMac, ttl, tos, protocol, source, dest);

#ifdef U2_LOG_TRAFFIC
    LogFileOutput("U2: Send IPRAW[%" SIZE_T_FMT "]: %" SIZE_T_FMT " (%" SIZE_T_FMT ") bytes\n", i, payload.size(), packet.size());
#endif

    myNetworkBackend->transmit(packet.size(), packet.data());
}

void Uthernet2::sendDataMacRaw(const size_t i, std::vector<uint8_t> &packet) const
{
#ifdef U2_LOG_TRAFFIC
    if (packet.size() >= 12)
    {
        const uint8_t * data = packet.data();
        LogFileOutput("U2: Send MACRAW[%" SIZE_T_FMT "]: " MAC_FMT " -> " MAC_FMT ": %" SIZE_T_FMT " bytes\n", i, MAC_SOURCE(data), MAC_DEST(data), packet.size());
    }
    else
    {
        // this is not a valid Ethernet Frame
        LogFileOutput("U2: Send MACRAW[%" SIZE_T_FMT "]: XX:XX:XX:XX:XX:XX -> XX:XX:XX:XX:XX:XX: %" SIZE_T_FMT " bytes\n", i, packet.size());
    }
#endif
    myNetworkBackend->transmit(packet.size(), packet.data());
}

void Uthernet2::sendDataToSocket(const size_t i, std::vector<uint8_t> &data)
{
    Socket &socket = mySockets[i];
    if (socket.isOpen())
    {
        sockaddr_in destination = {};
        destination.sin_family = AF_INET;

        // this seems to be ignored for TCP, and so we reuse the same code
        const uint32_t dest = readAddress(myMemory.data() + socket.registerAddress + W5100_SN_DIPR0);
        destination.sin_addr.s_addr = dest;
        destination.sin_port = *reinterpret_cast<const uint16_t *>(myMemory.data() + socket.registerAddress + W5100_SN_DPORT0);

        const ssize_t res = sendto(socket.getFD(), reinterpret_cast<const char *>(data.data()), data.size(), 0, (const struct sockaddr *)&destination, sizeof(destination));
#ifdef U2_LOG_TRAFFIC
        const char *proto = socket.getStatus() == W5100_SN_SR_SOCK_UDP ? "UDP" : "TCP";
        LogFileOutput("U2: Send %s[%" SIZE_T_FMT "]: %" SIZE_T_FMT " of %" SIZE_T_FMT " bytes\n", proto, i, res, data.size());
#endif
        if (res < 0)
        {
            const int error = sock_error();
            if (error != SOCK_EAGAIN && error != SOCK_EWOULDBLOCK)
            {
#ifdef U2_LOG_TRAFFIC
                LogFileOutput("U2: %s[%" SIZE_T_FMT "]: sendto error %" ERROR_FMT "\n", proto, i, STRERROR(error));
#endif
                socket.clearFD();
            }
        }
    }
}

void Uthernet2::sendData(const size_t i)
{
    const Socket &socket = mySockets[i];
    const uint16_t size = socket.transmitSize;
    const uint16_t mask = size - 1;

    const int sn_tx_rr = readNetworkWord(myMemory.data() + socket.registerAddress + W5100_SN_TX_RD0) & mask;
    const int sn_tx_wr = readNetworkWord(myMemory.data() + socket.registerAddress + W5100_SN_TX_WR0) & mask;

    const uint16_t base = socket.transmitBase;
    const uint16_t rr_address = base + sn_tx_rr;
    const uint16_t wr_address = base + sn_tx_wr;

    std::vector<uint8_t> data;
    if (rr_address < wr_address)
    {
        data.assign(myMemory.begin() + rr_address, myMemory.begin() + wr_address);
    }
    else
    {
        const uint16_t end = base + size;
        data.assign(myMemory.begin() + rr_address, myMemory.begin() + end);
        data.insert(data.end(), myMemory.begin() + base, myMemory.begin() + wr_address);
    }

    // move read pointer to writer
    myMemory[socket.registerAddress + W5100_SN_TX_RD0] = getIByte(sn_tx_wr, 8);
    myMemory[socket.registerAddress + W5100_SN_TX_RD1] = getIByte(sn_tx_wr, 0);

    switch (socket.getStatus())
    {
    case W5100_SN_SR_SOCK_MACRAW:
        sendDataMacRaw(i, data);
        break;
    case W5100_SN_SR_SOCK_IPRAW:
        sendDataIPRaw(i, data);
        break;
    case W5100_SN_SR_ESTABLISHED:
    case W5100_SN_SR_SOCK_UDP:
        sendDataToSocket(i, data);
        break;
    case W5100_SN_SR_CLOSED:
#ifdef U2_LOG_STATE
        LogFileOutput("U2: Send[%" SIZE_T_FMT "]: sending to a closed socket\n", i);
#endif
        break;
#ifdef U2_LOG_UNKNOWN
    default:
        LogFileOutput("U2: Send[%" SIZE_T_FMT "]: unknown mode: %02x\n", i, socket.getStatus());
#endif
    }
}

void Uthernet2::resetRXTXBuffers(const size_t i)
{
    Socket &socket = mySockets[i];
    socket.sn_rx_wr = 0x00;
    socket.sn_rx_rsr = 0x00;
    myMemory[socket.registerAddress + W5100_SN_TX_RD0] = 0x00;
    myMemory[socket.registerAddress + W5100_SN_TX_RD1] = 0x00;
    myMemory[socket.registerAddress + W5100_SN_TX_WR0] = 0x00;
    myMemory[socket.registerAddress + W5100_SN_TX_WR1] = 0x00;
    myMemory[socket.registerAddress + W5100_SN_RX_RD0] = 0x00;
    myMemory[socket.registerAddress + W5100_SN_RX_RD1] = 0x00;
}

void Uthernet2::openSystemSocket(const size_t i, const int type, const int protocol, const int status)
{
    Socket &s = mySockets[i];
#ifdef _MSC_VER
    const Socket::socket_t fd = socket(AF_INET, type, protocol);
#else
    const Socket::socket_t fd = socket(AF_INET, type | SOCK_NONBLOCK, protocol);
#endif
    if (fd == INVALID_SOCKET)
    {
#ifdef U2_LOG_STATE
        const char *proto = (status == W5100_SN_SR_SOCK_UDP) ? "UDP" : "TCP";
        LogFileOutput("U2: %s[%" SIZE_T_FMT "]: socket error: %" ERROR_FMT "\n", proto, i, STRERROR(sock_error()));
#endif
        s.clearFD();
    }
    else
    {
#ifdef _MSC_VER
        u_long on = 1;
        ioctlsocket(fd, FIONBIO, &on);
#endif
        s.setFD(fd, status);
    }
}

void Uthernet2::openSocket(const size_t i)
{
    Socket &socket = mySockets[i];
    socket.clearFD();

    const uint8_t mr = myMemory[socket.registerAddress + W5100_SN_MR];
    const uint8_t protocol = mr & W5100_SN_MR_PROTO_MASK;
    const bool virtual_dns = protocol & W5100_SN_VIRTUAL_DNS;

    // if virtual_dns is requested, but not enabled, we cannot handle it here.
    if (virtual_dns && !myVirtualDNSEnabled)
    {
#ifdef U2_LOG_STATE
        LogFileOutput("U2: Open[%" SIZE_T_FMT "]: virtual DNS not supported: %02x\n", i, mr);
#endif
        return;
    }

    switch (protocol)
    {
    case W5100_SN_MR_IPRAW:
    case W5100_SN_MR_IPRAW_DNS:
        socket.setStatus(W5100_SN_SR_SOCK_IPRAW);
        break;
    case W5100_SN_MR_MACRAW:
        socket.setStatus(W5100_SN_SR_SOCK_MACRAW);
        break;
    case W5100_SN_MR_TCP:
    case W5100_SN_MR_TCP_DNS:
        openSystemSocket(i, SOCK_STREAM, IPPROTO_TCP, W5100_SN_SR_SOCK_INIT);
        break;
    case W5100_SN_MR_UDP:
    case W5100_SN_MR_UDP_DNS:
        openSystemSocket(i, SOCK_DGRAM, IPPROTO_UDP, W5100_SN_SR_SOCK_UDP);
        break;
#ifdef U2_LOG_UNKNOWN
    default:
        LogFileOutput("U2: Open[%" SIZE_T_FMT "]: unknown mode: %02x\n", i, mr);
#endif
    }

    switch (protocol)
    {
    case W5100_SN_MR_IPRAW_DNS:
    case W5100_SN_MR_TCP_DNS:
    case W5100_SN_MR_UDP_DNS:
        resolveDNS(i);
        break;
    }

    resetRXTXBuffers(i); // needed?
#ifdef U2_LOG_STATE
    LogFileOutput("U2: Open[%" SIZE_T_FMT "]: SR = %02x\n", i, socket.getStatus());
#endif
}

void Uthernet2::closeSocket(const size_t i)
{
    Socket &socket = mySockets[i];
    socket.clearFD();
#ifdef U2_LOG_STATE
    LogFileOutput("U2: Close[%" SIZE_T_FMT "]\n", i);
#endif
}

void Uthernet2::resolveDNS(const size_t i)
{
    Socket &socket = mySockets[i];
    uint32_t *dest = reinterpret_cast<uint32_t *>(myMemory.data() + socket.registerAddress + W5100_SN_DIPR0);
    *dest = 0; // 0.0.0.0 signals failure by any reason

    const uint8_t length = myMemory[socket.registerAddress + W5100_SN_DNS_NAME_LEN];
    if (length <= W5100_SN_DNS_NAME_CPTY)
    {
        const uint8_t * start = myMemory.data() + socket.registerAddress + W5100_SN_DNS_NAME_BEGIN;
        const std::string name(start, start + length);
        const std::map<std::string, uint32_t>::iterator it = myDNSCache.find(name);
        if (it != myDNSCache.end())
        {
            *dest = it->second;
        }
        else
        {
            *dest = getHostByName(name);
            myDNSCache[name] = *dest;
#ifdef U2_LOG_STATE
            LogFileOutput("U2: DNS[%" SIZE_T_FMT "]: %s = %s\n", i, name.c_str(), formatIP(*dest));
#endif
        }
    }
}

void Uthernet2::connectSocket(const size_t i)
{
    Socket &socket = mySockets[i];
    const uint32_t dest = readAddress(myMemory.data() + socket.registerAddress + W5100_SN_DIPR0);

    sockaddr_in destination = {};
    destination.sin_family = AF_INET;

    // already in network order
    destination.sin_port = *reinterpret_cast<const uint16_t *>(myMemory.data() + socket.registerAddress + W5100_SN_DPORT0);
    destination.sin_addr.s_addr = dest;

    const int res = connect(socket.getFD(), (struct sockaddr *)&destination, sizeof(destination));

    if (res == 0)
    {
        socket.setStatus(W5100_SN_SR_ESTABLISHED);
#ifdef U2_LOG_STATE
        const uint16_t port = readNetworkWord(myMemory.data() + socket.registerAddress + W5100_SN_DPORT0);
        LogFileOutput("U2: TCP[%" SIZE_T_FMT "]: CONNECT to %s:%d\n", i, formatIP(dest), port);
#endif
    }
    else
    {
        const int error = sock_error();
        if (error == SOCK_EINPROGRESS || error == SOCK_EWOULDBLOCK)
        {
            socket.setStatus(W5100_SN_SR_SOCK_SYNSENT);
        }
        else
        {
#ifdef U2_LOG_STATE
            LogFileOutput("U2: TCP[%" SIZE_T_FMT "]: connect error: %" ERROR_FMT "\n", i, STRERROR(error));
#endif
        }
    }
}

void Uthernet2::setCommandRegister(const size_t i, const uint8_t value)
{
    switch (value)
    {
    case W5100_SN_CR_OPEN:
        openSocket(i);
        break;
    case W5100_SN_CR_CONNECT:
        connectSocket(i);
        break;
    case W5100_SN_CR_CLOSE:
    case W5100_SN_CR_DISCON:
        closeSocket(i);
        break;
    case W5100_SN_CR_SEND:
        sendData(i);
        break;
    case W5100_SN_CR_RECV:
        updateRSR(i);
        break;
#ifdef U2_LOG_UNKNOWN
    default:
        LogFileOutput("U2: Unknown command[%" SIZE_T_FMT "]: %02x\n", i, value);
#endif
    }
}

uint8_t Uthernet2::readSocketRegister(const uint16_t address)
{
    const uint16_t i = (address >> 8) - 0x04;
    const uint16_t loc = address & 0xFF;
    uint8_t value;
    switch (loc)
    {
    case W5100_SN_MR:
    case W5100_SN_CR:
        value = myMemory[address];
        break;
    case W5100_SN_SR:
        value = mySockets[i].getStatus();
        break;
    case W5100_SN_TX_FSR0:
        value = getTXFreeSizeRegister(i, 8);
        break;
    case W5100_SN_TX_FSR1:
        value = getTXFreeSizeRegister(i, 0);
        break;
    case W5100_SN_TX_RD0:
    case W5100_SN_TX_RD1:
        value = myMemory[address];
        break;
    case W5100_SN_TX_WR0:
    case W5100_SN_TX_WR1:
        value = myMemory[address];
        break;
    case W5100_SN_RX_RSR0:
        receiveOnePacket(i);
        value = getRXDataSizeRegister(i, 8);
        break;
    case W5100_SN_RX_RSR1:
        receiveOnePacket(i);
        value = getRXDataSizeRegister(i, 0);
        break;
    case W5100_SN_RX_RD0:
    case W5100_SN_RX_RD1:
        value = myMemory[address];
        break;
    default:
#ifdef U2_LOG_UNKNOWN
        LogFileOutput("U2: Get unknown socket register[%d]: %04x\n", i, address);
#endif
        value = myMemory[address];
        break;
    }
    return value;
}

uint8_t Uthernet2::readValueAt(const uint16_t address)
{
    uint8_t value;
    if (address == W5100_MR)
    {
        value = myModeRegister;
    }
    else if (address >= W5100_GAR0 && address <= W5100_UPORT1)
    {
        value = myMemory[address];
    }
    else if (address >= W5100_S0_BASE && address <= W5100_S3_MAX)
    {
        value = readSocketRegister(address);
    }
    else if (address >= W5100_TX_BASE && address <= W5100_MEM_MAX)
    {
        value = myMemory[address];
    }
    else
    {
#ifdef U2_LOG_UNKNOWN
        LogFileOutput("U2: Read unknown location: %04x\n", address);
#endif
        // this might not be 100% correct if address >= 0x8000
        // see top of page 13 Uthernet II
        value = myMemory[address & W5100_MEM_MAX];
    }
    return value;
}

void Uthernet2::autoIncrement()
{
    if (myModeRegister & W5100_MR_AI)
    {
        ++myDataAddress;
        // Read bottom of Uthernet II page 12
        // Setting the address to values >= 0x8000 is not really supported
        switch (myDataAddress)
        {
        case W5100_RX_BASE:
        case W5100_MEM_SIZE:
            myDataAddress -= 0x2000;
            break;
        }
    }
}

uint8_t Uthernet2::readValue()
{
    const uint8_t value = readValueAt(myDataAddress);
    autoIncrement();
    return value;
}

void Uthernet2::setIPProtocol(const size_t i, const uint16_t address, const uint8_t value)
{
    myMemory[address] = value;
#ifdef U2_LOG_STATE
    LogFileOutput("U2: IP PROTO[%" SIZE_T_FMT "] = %d\n", i, value);
#endif
}

void Uthernet2::setIPTypeOfService(const size_t i, const uint16_t address, const uint8_t value)
{
    myMemory[address] = value;
#ifdef U2_LOG_STATE
    LogFileOutput("U2: IP TOS[%" SIZE_T_FMT "] = %d\n", i, value);
#endif
}

void Uthernet2::setIPTTL(const size_t i, const uint16_t address, const uint8_t value)
{
    myMemory[address] = value;
#ifdef U2_LOG_STATE
    LogFileOutput("U2: IP TTL[%" SIZE_T_FMT "] = %d\n", i, value);
#endif
}

void Uthernet2::writeSocketRegister(const uint16_t address, const uint8_t value)
{
    const uint16_t i = (address >> 8) - 0x04;
    const uint16_t loc = address & 0xFF;
    switch (loc)
    {
    case W5100_SN_MR:
        setSocketModeRegister(i, address, value);
        break;
    case W5100_SN_CR:
        setCommandRegister(i, value);
        break;
    case W5100_SN_PORT0:
    case W5100_SN_PORT1:
    case W5100_SN_DPORT0:
    case W5100_SN_DPORT1:
        myMemory[address] = value;
        break;
    case W5100_SN_DIPR0:
    case W5100_SN_DIPR1:
    case W5100_SN_DIPR2:
    case W5100_SN_DIPR3:
        myMemory[address] = value;
        break;
    case W5100_SN_PROTO:
        setIPProtocol(i, address, value);
        break;
    case W5100_SN_TOS:
        setIPTypeOfService(i, address, value);
        break;
    case W5100_SN_TTL:
        setIPTTL(i, address, value);
        break;
    case W5100_SN_TX_WR0:
        myMemory[address] = value;
        break;
    case W5100_SN_TX_WR1:
        myMemory[address] = value;
        break;
    case W5100_SN_RX_RD0:
        myMemory[address] = value;
        break;
    case W5100_SN_RX_RD1:
        myMemory[address] = value;
        break;
    default:
        if (myVirtualDNSEnabled && loc >= W5100_SN_DNS_NAME_LEN)
        {
            myMemory[address] = value;
        }
        else
        {
#ifdef U2_LOG_UNKNOWN
            LogFileOutput("U2: Set unknown socket register[%d]: %04x\n", i, address);
#endif
        }
        break;
    };
}

void Uthernet2::setModeRegister(const uint16_t address, const uint8_t value)
{
    if (value & W5100_MR_RST)
    {
        Reset(false);
    }
    else
    {
        myModeRegister = value;
    }
}

void Uthernet2::writeCommonRegister(const uint16_t address, const uint8_t value)
{
    if (address == W5100_MR)
    {
        setModeRegister(address, value);
    }
    else if (address >= W5100_GAR0 && address <= W5100_GAR3 ||
             address >= W5100_SUBR0 && address <= W5100_SUBR3 ||
             address >= W5100_SHAR0 && address <= W5100_SHAR5 ||
             address >= W5100_SIPR0 && address <= W5100_SIPR3)
    {
        myMemory[address] = value;
    }
    else if (address == W5100_RMSR)
    {
        setRXSizes(address, value);
    }
    else if (address == W5100_TMSR)
    {
        setTXSizes(address, value);
    }
#ifdef U2_LOG_UNKNOWN
    else
    {
        LogFileOutput("U2: Set unknown common register: %04x\n", address);
    }
#endif
}

void Uthernet2::writeValueAt(const uint16_t address, const uint8_t value)
{
    if (address >= W5100_MR && address <= W5100_UPORT1)
    {
        writeCommonRegister(address, value);
    }
    else if (address >= W5100_S0_BASE && address <= W5100_S3_MAX)
    {
        writeSocketRegister(address, value);
    }
    else if (address >= W5100_TX_BASE && address <= W5100_MEM_MAX)
    {
        myMemory[address] = value;
    }
#ifdef U2_LOG_UNKNOWN
    else
    {
        LogFileOutput("U2: Write to unknown location: %02x to %04x\n", value, address);
    }
#endif
}

void Uthernet2::writeValue(const uint8_t value)
{
    writeValueAt(myDataAddress, value);
    autoIncrement();
}

void Uthernet2::Reset(const bool powerCycle)
{
    LogFileOutput("U2: Uthernet II initialisation\n");
    myModeRegister = 0;

    if (powerCycle)
    {
        // dataAddress is NOT reset, see page 10 of Uthernet II
        myDataAddress = 0;
        const std::string interfaceName = PCapBackend::GetRegistryInterface(m_slot);
        // first clean the old one, as 2 backends might not be able to exist at the same time
        myNetworkBackend.reset();
        myNetworkBackend = GetFrame().CreateNetworkBackend(interfaceName);
        myARPCache.clear();
        myDNSCache.clear();
    }

    mySockets.clear();
    mySockets.resize(4);
    myMemory.clear();
    myMemory.resize(W5100_MEM_SIZE, 0);

    for (size_t i = 0; i < mySockets.size(); ++i)
    {
        resetRXTXBuffers(i);
        mySockets[i].clearFD();
        const uint16_t registerAddress = static_cast<uint16_t>(W5100_S0_BASE + (i << 8));
        mySockets[i].registerAddress = registerAddress;

        myMemory[registerAddress + W5100_SN_DHAR0] = 0xFF;
        myMemory[registerAddress + W5100_SN_DHAR1] = 0xFF;
        myMemory[registerAddress + W5100_SN_DHAR2] = 0xFF;
        myMemory[registerAddress + W5100_SN_DHAR3] = 0xFF;
        myMemory[registerAddress + W5100_SN_DHAR4] = 0xFF;
        myMemory[registerAddress + W5100_SN_DHAR5] = 0xFF;
        myMemory[registerAddress + W5100_SN_TTL]   = 0x80;
    }

    // initial values
    myMemory[W5100_RTR0]    = 0x07;
    myMemory[W5100_RTR1]    = 0xD0;
    myMemory[W5100_RCR]     = 0x08;
    setRXSizes(W5100_RMSR, 0x55);
    setTXSizes(W5100_TMSR, 0x55);
    if (!myVirtualDNSEnabled)
    {
        // this is 0 if we support Virtual DNS
        myMemory[W5100_PTIMER]  = 0x28;
    }
}

BYTE Uthernet2::IO_C0(WORD programcounter, WORD address, BYTE write, BYTE value, ULONG nCycles)
{
    BYTE res = write ? 0 : MemReadFloatingBus(nCycles);

#ifdef U2_LOG_VERBOSE
    const uint16_t oldAddress = myDataAddress;
#endif

    const uint8_t loc = address & U2_C0X_MASK;

    if (write)
    {
        switch (loc)
        {
        case U2_C0X_MODE_REGISTER:
            setModeRegister(W5100_MR, value);
            break;
        case U2_C0X_ADDRESS_HIGH:
            myDataAddress = (value << 8) | (myDataAddress & 0x00FF);
            break;
        case U2_C0X_ADDRESS_LOW:
            myDataAddress = (value << 0) | (myDataAddress & 0xFF00);
            break;
        case U2_C0X_DATA_PORT:
            writeValue(value);
            break;
        }
    }
    else
    {
        switch (loc)
        {
        case U2_C0X_MODE_REGISTER:
            res = myModeRegister;
            break;
        case U2_C0X_ADDRESS_HIGH:
            res = getIByte(myDataAddress, 8);
            break;
        case U2_C0X_ADDRESS_LOW:
            res = getIByte(myDataAddress, 0);
            break;
        case U2_C0X_DATA_PORT:
            res = readValue();
            break;
        }
    }

#ifdef U2_LOG_VERBOSE
    const char *mode = write ? "WRITE" : "READ ";
    const char c = std::isprint(res) ? res : '.';
    LogFileOutput("U2: %04x: %s %04x[%04x] %02x -> %02x, '%c' (%d -> %d)\n", programcounter, mode, address, oldAddress, value, res, c, value, res);
#endif

    return res;
}

BYTE __stdcall u2_C0(WORD programcounter, WORD address, BYTE write, BYTE value, ULONG nCycles)
{
    UINT uSlot = ((address & 0xff) >> 4) - 8;
    Uthernet2 *pCard = (Uthernet2 *)MemGetSlotParameters(uSlot);
    return pCard->IO_C0(programcounter, address, write, value, nCycles);
}

void Uthernet2::InitializeIO(LPBYTE pCxRomPeripheral)
{
    RegisterIoHandler(m_slot, u2_C0, u2_C0, nullptr, nullptr, this, nullptr);
}

void Uthernet2::getMACAddress(const uint32_t address, const MACAddress * & mac)
{
    const auto it = myARPCache.find(address);
    if (it != myARPCache.end())
    {
        mac = &it->second;
    }
    else
    {
        MACAddress & macAddr = myARPCache[address];
        const uint32_t source  = readAddress(myMemory.data() + W5100_SIPR0);

        if (address == source)
        {
            const uint8_t * sourceMac = myMemory.data() + W5100_SHAR0;
            memcpy(macAddr.address, sourceMac, sizeof(macAddr.address));
        }
        else
        {
            memset(macAddr.address, 0xFF, sizeof(macAddr.address));  // fallback to broadcast
            if (address != INADDR_BROADCAST)
            {
                const uint32_t subnet  = readAddress(myMemory.data() + W5100_SUBR0);
                if ((address & subnet) == (source & subnet))
                {
                    // same network: send ARP request
                    myNetworkBackend->getMACAddress(address, macAddr);
                }
                else
                {
                    const uint32_t gateway = readAddress(myMemory.data() + W5100_GAR0);
                    // different network: go via gateway
                    myNetworkBackend->getMACAddress(gateway, macAddr);
                }
            }
        }
        mac = &macAddr;
    }
}

void Uthernet2::Update(const ULONG nExecutedCycles)
{
    myNetworkBackend->update(nExecutedCycles);
    for (Socket &socket : mySockets)
    {
        socket.process();
    }
}

// Unit version history:
// 2: Added: Virtual DNS
static const UINT kUNIT_VERSION = 2;

#define SS_YAML_KEY_VIRTUAL_DNS "Virtual DNS"
#define SS_YAML_KEY_NETWORK_INTERFACE "Network Interface"

#define SS_YAML_KEY_COMMON_REGISTERS "Common Registers"
#define SS_YAML_KEY_MODE_REGISTER "Mode Register"
#define SS_YAML_KEY_DATA_ADDRESS "Data Address"

#define SS_YAML_KEY_SOCKETS_REGISTERS "Socket Registers"
#define SS_YAML_KEY_SOCKET "Socket"

#define SS_YAML_KEY_TX_MEMORY "TX Memory"
#define SS_YAML_KEY_RX_MEMORY "RX Memory"

void Uthernet2::SaveSnapshot(YamlSaveHelper &yamlSaveHelper)
{
    YamlSaveHelper::Slot slot(yamlSaveHelper, GetSnapshotCardName(), m_slot, kUNIT_VERSION);

    YamlSaveHelper::Label unit(yamlSaveHelper, "%s:\n", SS_YAML_KEY_STATE);
    yamlSaveHelper.SaveString(SS_YAML_KEY_NETWORK_INTERFACE, myNetworkBackend->getInterfaceName());
    yamlSaveHelper.SaveBool(SS_YAML_KEY_VIRTUAL_DNS, myVirtualDNSEnabled);
    yamlSaveHelper.SaveHexUint8(SS_YAML_KEY_MODE_REGISTER, myModeRegister);
    yamlSaveHelper.SaveHexUint16(SS_YAML_KEY_DATA_ADDRESS, myDataAddress);

    // we skip the reserved areas as seen @ P.14 2.MemoryMap of the W5100 Manual

    {
        YamlSaveHelper::Label state(yamlSaveHelper, "%s:\n", SS_YAML_KEY_COMMON_REGISTERS);
        yamlSaveHelper.SaveMemory(myMemory.data(), W5100_UPORT1 + 1);
    }

    // 0x0030 to 0x0400 RESERVED

    {
        YamlSaveHelper::Label state(yamlSaveHelper, "%s:\n", SS_YAML_KEY_SOCKETS_REGISTERS);
        yamlSaveHelper.SaveMemory(myMemory.data() + W5100_S0_BASE, (W5100_S3_MAX + 1) - W5100_S0_BASE);
    }

    // 0x0800 to 0x4000 RESERVED

    {
        YamlSaveHelper::Label state(yamlSaveHelper, "%s:\n", SS_YAML_KEY_TX_MEMORY);
        yamlSaveHelper.SaveMemory(myMemory.data() + W5100_TX_BASE, W5100_RX_BASE - W5100_TX_BASE);
    }

    {
        YamlSaveHelper::Label state(yamlSaveHelper, "%s:\n", SS_YAML_KEY_RX_MEMORY);
        yamlSaveHelper.SaveMemory(myMemory.data() + W5100_RX_BASE, W5100_MEM_SIZE - W5100_RX_BASE);
    }

    for (size_t i = 0; i < mySockets.size(); ++i)
    {
        YamlSaveHelper::Label state(yamlSaveHelper, "%s %" SIZE_T_FMT ":\n", SS_YAML_KEY_SOCKET, i);
        mySockets[i].SaveSnapshot(yamlSaveHelper);
    }

}

bool Uthernet2::LoadSnapshot(YamlLoadHelper &yamlLoadHelper, UINT version)
{
    if (version < 1 || version > kUNIT_VERSION)
        ThrowErrorInvalidVersion(version);

    PCapBackend::SetRegistryInterface(m_slot, yamlLoadHelper.LoadString(SS_YAML_KEY_NETWORK_INTERFACE));

    if (version >= 2)
    {
        myVirtualDNSEnabled = yamlLoadHelper.LoadBool(SS_YAML_KEY_VIRTUAL_DNS);
    }
    else
    {
        myVirtualDNSEnabled = false;
    }
    SetRegistryVirtualDNS(m_slot, myVirtualDNSEnabled);

    Reset(true); // AFTER the parameters have been restored

    myModeRegister = yamlLoadHelper.LoadUint(SS_YAML_KEY_MODE_REGISTER);
    myDataAddress = yamlLoadHelper.LoadUint(SS_YAML_KEY_DATA_ADDRESS);

    if (yamlLoadHelper.GetSubMap(SS_YAML_KEY_COMMON_REGISTERS))
    {
        yamlLoadHelper.LoadMemory(myMemory.data(), W5100_UPORT1 + 1);
        yamlLoadHelper.PopMap();
    }

    if (yamlLoadHelper.GetSubMap(SS_YAML_KEY_SOCKETS_REGISTERS))
    {
        yamlLoadHelper.LoadMemory(myMemory.data() + W5100_S0_BASE, (W5100_S3_MAX + 1) - W5100_S0_BASE);
        yamlLoadHelper.PopMap();
    }

    if (yamlLoadHelper.GetSubMap(SS_YAML_KEY_TX_MEMORY))
    {
        yamlLoadHelper.LoadMemory(myMemory.data() + W5100_TX_BASE, W5100_RX_BASE - W5100_TX_BASE);
        yamlLoadHelper.PopMap();
    }

    if (yamlLoadHelper.GetSubMap(SS_YAML_KEY_RX_MEMORY))
    {
        yamlLoadHelper.LoadMemory(myMemory.data() + W5100_RX_BASE, W5100_MEM_SIZE - W5100_RX_BASE);
        yamlLoadHelper.PopMap();
    }

    setRXSizes(W5100_RMSR, myMemory[W5100_RMSR]);
    setTXSizes(W5100_TMSR, myMemory[W5100_TMSR]);

    for (size_t i = 0; i < mySockets.size(); ++i)
    {
        const std::string key = StrFormat("%s %" SIZE_T_FMT, SS_YAML_KEY_SOCKET, i);
        if (yamlLoadHelper.GetSubMap(key))
        {
            mySockets[i].LoadSnapshot(yamlLoadHelper);
            yamlLoadHelper.PopMap();
        }
    }

    return true;
}

void Uthernet2::SetRegistryVirtualDNS(UINT slot, const bool enabled)
{
    const std::string regSection = RegGetConfigSlotSection(slot);
    RegSaveValue(regSection.c_str(), REGVALUE_UTHERNET_VIRTUAL_DNS, TRUE, enabled);
}

bool Uthernet2::GetRegistryVirtualDNS(UINT slot)
{
    const std::string regSection = RegGetConfigSlotSection(slot);

    // By default, VirtualDNS is enabled
    // as it is backward compatible
    // (except for the initial value of PTIMER which is anyway never used)

    DWORD enabled = 1;
    RegLoadValue(regSection.c_str(), REGVALUE_UTHERNET_VIRTUAL_DNS, TRUE, &enabled);
    return enabled != 0;
}

const std::shared_ptr<NetworkBackend> & Uthernet2::GetNetworkBackend() const
{
    return myNetworkBackend;
}
