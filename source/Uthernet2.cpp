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
#include "W5100.h"

#include <cstdint>

// Linux uses EINPROGRESS while Windows returns WSAEWOULDBLOCK
// when the connect() calls is ongoing
//
// in the checks below we allow both in all cases
// (myErrno == SOCK_EINPROGRESS || myErrno == SOCK_EWOULDBLOCK)
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

    void writeDataForProtocol(Socket &socket, std::vector<uint8_t> &memory, const uint8_t *data, const size_t len, const sockaddr_in &source)
    {
        if (socket.sn_sr == SN_SR_SOCK_UDP)
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
    : sn_sr(SN_SR_CLOSED), myFD(INVALID_SOCKET), myErrno(0)
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
    sn_sr = SN_SR_CLOSED;
}

void Socket::setFD(const socket_t fd, const int status)
{
    clearFD();
    myFD = fd;
    myErrno = 0;
    sn_sr = status;
}

Socket::~Socket()
{
    clearFD();
}

void Socket::process()
{
    if (myFD != INVALID_SOCKET && sn_sr == SN_SR_SOCK_INIT && (myErrno == SOCK_EINPROGRESS || myErrno == SOCK_EWOULDBLOCK))
    {
#ifdef _MSC_VER
        FD_SET writefds;
        FD_ZERO(&writefds);
        FD_SET(myFD, &writefds);
        const timeval timeout = {0, 0};
        if (select(0, NULL, &writefds, NULL, &timeout) > 0)
#else
        pollfd pfd = {.fd = myFD, .events = POLLOUT};
        if (poll(&pfd, 1, 0) > 0)
#endif
        {
            int err = 0;
            socklen_t elen = sizeof err;
            getsockopt(myFD, SOL_SOCKET, SO_ERROR, reinterpret_cast<char *>(&err), &elen);

            if (err == 0)
            {
                myErrno = 0;
                sn_sr = SN_SR_ESTABLISHED;
#ifdef U2_LOG_STATE
                LogFileOutput("U2: TCP[]: Connected\n");
#endif
            }
        }
    }
}

bool Socket::isThereRoomFor(const size_t len, const size_t header) const
{
    const uint16_t rsr = sn_rx_rsr;    // already present
    const uint16_t size = receiveSize; // total size

    return rsr + len + header < size; // "not =": we do not want to fill the buffer.
}

uint16_t Socket::getFreeRoom() const
{
    const uint16_t rsr = sn_rx_rsr;    // already present
    const uint16_t size = receiveSize; // total size

    return size - rsr;
}

const std::string& Uthernet2::GetSnapshotCardName()
{
    static const std::string name("Uthernet2");
    return name;
}

Uthernet2::Uthernet2(UINT slot) : Card(CT_Uthernet2, slot)
{
    myNetworkBackend = GetFrame().CreateNetworkBackend();
    Reset(true);
}

void Uthernet2::Destroy()
{
    myNetworkBackend.reset();
}

void Uthernet2::setSocketModeRegister(const size_t i, const uint16_t address, const uint8_t value)
{
    myMemory[address] = value;
    const uint8_t protocol = value & SN_MR_PROTO_MASK;
    switch (protocol)
    {
    case SN_MR_CLOSED:
#ifdef U2_LOG_STATE
        LogFileOutput("U2: Mode[%" SIZE_T_FMT "]: closed\n", i);
#endif
        break;
    case SN_MR_TCP:
#ifdef U2_LOG_STATE
        LogFileOutput("U2: Mode[%" SIZE_T_FMT "]: TCP\n", i);
#endif
        break;
    case SN_MR_UDP:
#ifdef U2_LOG_STATE
        LogFileOutput("U2: Mode[%" SIZE_T_FMT "]: UDP\n", i);
#endif
        break;
    case SN_MR_IPRAW:
#ifdef U2_LOG_STATE
        LogFileOutput("U2: Mode[%" SIZE_T_FMT "]: IPRAW\n", i);
#endif
        break;
    case SN_MR_MACRAW:
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
    uint16_t base = TX_BASE;
    const uint16_t end = RX_BASE;
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
    uint16_t base = RX_BASE;
    const uint16_t end = MEM_SIZE;
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

    const int sn_tx_rd = readNetworkWord(myMemory.data() + socket.registers + SN_TX_RD0) & mask;
    const int sn_tx_wr = readNetworkWord(myMemory.data() + socket.registers + SN_TX_WR0) & mask;

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

    const int sn_rx_rd = readNetworkWord(myMemory.data() + socket.registers + SN_RX_RD0) & mask;
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

int Uthernet2::receiveForMacAddress(const bool acceptAll, const int size, uint8_t * data)
{
    const uint8_t * mac = myMemory.data() + SHAR0;

    // loop until we receive a valid frame, or there is nothing to receive
    int len;
    while ((len = myNetworkBackend->receive(size, data)) > 0)
    {
        // minimum valid Ethernet frame is actually 64 bytes
        // 12 is the minimum to ensure valid MAC Address logging later
        if (len >= 12)
        {
            if (acceptAll)
            {
                return len;
            }

            if (data[0] == mac[0] &&
                data[1] == mac[1] &&
                data[2] == mac[2] &&
                data[3] == mac[3] &&
                data[4] == mac[4] &&
                data[5] == mac[5])
            {
                return len;
            }

            if (data[0] == 0xFF &&
                data[1] == 0xFF &&
                data[2] == 0xFF &&
                data[3] == 0xFF &&
                data[4] == 0xFF &&
                data[5] == 0xFF)
            {
                return len;
            }
        }
        // skip this frame and try with another one
    }
    // no frames available to process
    return len;
}

void Uthernet2::receiveOnePacketMacRaw(const size_t i)
{
    Socket &socket = mySockets[i];

    uint8_t buffer[MAX_RXLENGTH];

    const uint8_t mr = myMemory[socket.registers + SN_MR];
    const bool filterMAC = mr & SN_MR_MF;

    const int len = receiveForMacAddress(!filterMAC, sizeof(buffer), buffer);
    if (len > 0)
    {
        // we know the packet is at least 12 bytes, and logging is ok
        if (socket.isThereRoomFor(len, sizeof(uint16_t)))
        {
            writeDataMacRaw(socket, myMemory, buffer, len);
#ifdef U2_LOG_TRAFFIC
            LogFileOutput("U2: Read MACRAW[%" SIZE_T_FMT "]: " MAC_FMT " -> " MAC_FMT ": +%d -> %d bytes\n", i, MAC_SOURCE(buffer), MAC_DEST(buffer),
                len, socket.sn_rx_rsr);
#endif
        }
        else
        {
            // drop it
#ifdef U2_LOG_TRAFFIC
            LogFileOutput("U2: Skip MACRAW[%" SIZE_T_FMT "]: %d bytes\n", i, len);
#endif
        }
    }
}

// UDP & TCP
void Uthernet2::receiveOnePacketFromSocket(const size_t i)
{
    Socket &socket = mySockets[i];
    if (socket.myFD != INVALID_SOCKET)
    {
        const uint16_t freeRoom = socket.getFreeRoom();
        if (freeRoom > 32) // avoid meaningless reads
        {
            std::vector<uint8_t> buffer(freeRoom - 1); // do not fill the buffer completely
            sockaddr_in source = {0};
            socklen_t len = sizeof(sockaddr_in);
            const ssize_t data = recvfrom(socket.myFD, reinterpret_cast<char *>(buffer.data()), buffer.size(), 0, (struct sockaddr *)&source, &len);
#ifdef U2_LOG_TRAFFIC
            const char *proto = socket.sn_sr == SN_SR_SOCK_UDP ? "UDP" : "TCP";
#endif
            if (data > 0)
            {
                writeDataForProtocol(socket, myMemory, buffer.data(), data, source);
#ifdef U2_LOG_TRAFFIC
                LogFileOutput("U2: Read %s[%" SIZE_T_FMT "]: +%" SIZE_T_FMT " -> %d bytes\n", proto, i, data, socket.sn_rx_rsr);
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
    switch (socket.sn_sr)
    {
    case SN_SR_SOCK_MACRAW:
        receiveOnePacketMacRaw(i);
        break;
    case SN_SR_ESTABLISHED:
    case SN_SR_SOCK_UDP:
        receiveOnePacketFromSocket(i);
        break;
    case SN_SR_CLOSED:
        break; // nothing to do
#ifdef U2_LOG_UNKNOWN
    default:
        LogFileOutput("U2: Read[%" SIZE_T_FMT "]: unknown mode: %02x\n", i, socket.sn_sr);
#endif
    };
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
    if (socket.myFD != INVALID_SOCKET)
    {
        sockaddr_in destination = {};
        destination.sin_family = AF_INET;

        // already in network order
        // this seems to be ignored for TCP, and so we reuse the same code
        const uint8_t *dest = myMemory.data() + socket.registers + SN_DIPR0;
        destination.sin_addr.s_addr = *reinterpret_cast<const uint32_t *>(dest);
        destination.sin_port = *reinterpret_cast<const uint16_t *>(myMemory.data() + socket.registers + SN_DPORT0);

        const ssize_t res = sendto(socket.myFD, reinterpret_cast<const char *>(data.data()), data.size(), 0, (const struct sockaddr *)&destination, sizeof(destination));
#ifdef U2_LOG_TRAFFIC
        const char *proto = socket.sn_sr == SN_SR_SOCK_UDP ? "UDP" : "TCP";
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

    const int sn_tx_rr = readNetworkWord(myMemory.data() + socket.registers + SN_TX_RD0) & mask;
    const int sn_tx_wr = readNetworkWord(myMemory.data() + socket.registers + SN_TX_WR0) & mask;

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
    myMemory[socket.registers + SN_TX_RD0] = getIByte(sn_tx_wr, 8);
    myMemory[socket.registers + SN_TX_RD1] = getIByte(sn_tx_wr, 0);

    switch (socket.sn_sr)
    {
    case SN_SR_SOCK_MACRAW:
        sendDataMacRaw(i, data);
        break;
    case SN_SR_ESTABLISHED:
    case SN_SR_SOCK_UDP:
        sendDataToSocket(i, data);
        break;
#ifdef U2_LOG_UNKNOWN
    default:
        LogFileOutput("U2: Send[%" SIZE_T_FMT "]: unknown mode: %02x\n", i, socket.sn_sr);
#endif
    }
}

void Uthernet2::resetRXTXBuffers(const size_t i)
{
    Socket &socket = mySockets[i];
    socket.sn_rx_wr = 0x00;
    socket.sn_rx_rsr = 0x00;
    myMemory[socket.registers + SN_TX_RD0] = 0x00;
    myMemory[socket.registers + SN_TX_RD1] = 0x00;
    myMemory[socket.registers + SN_TX_WR0] = 0x00;
    myMemory[socket.registers + SN_TX_WR1] = 0x00;
    myMemory[socket.registers + SN_RX_RD0] = 0x00;
    myMemory[socket.registers + SN_RX_RD1] = 0x00;
}

void Uthernet2::openSystemSocket(const size_t i, const int type, const int protocol, const int state)
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
        const char *proto = state == SN_SR_SOCK_UDP ? "UDP" : "TCP";
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
        s.setFD(fd, state);
    }
}

void Uthernet2::openSocket(const size_t i)
{
    Socket &socket = mySockets[i];
    const uint8_t mr = myMemory[socket.registers + SN_MR];
    const uint8_t protocol = mr & SN_MR_PROTO_MASK;
    uint8_t &sr = socket.sn_sr;
    switch (protocol)
    {
    case SN_MR_IPRAW:
        sr = SN_SR_SOCK_IPRAW;
        break;
    case SN_MR_MACRAW:
        sr = SN_SR_SOCK_MACRAW;
        break;
    case SN_MR_TCP:
        openSystemSocket(i, SOCK_STREAM, IPPROTO_TCP, SN_SR_SOCK_INIT);
        break;
    case SN_MR_UDP:
        openSystemSocket(i, SOCK_DGRAM, IPPROTO_UDP, SN_SR_SOCK_UDP);
        break;
#ifdef U2_LOG_UNKNOWN
    default:
        LogFileOutput("U2: Open[%" SIZE_T_FMT "]: unknown mode: %02x\n", i, mr);
#endif
    }
    resetRXTXBuffers(i); // needed?
#ifdef U2_LOG_STATE
    LogFileOutput("U2: Open[%" SIZE_T_FMT "]: SR = %02x\n", i, sr);
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

void Uthernet2::connectSocket(const size_t i)
{
    Socket &socket = mySockets[i];
    const uint8_t *dest = myMemory.data() + socket.registers + SN_DIPR0;

    sockaddr_in destination = {};
    destination.sin_family = AF_INET;

    // already in network order
    destination.sin_port = *reinterpret_cast<const uint16_t *>(myMemory.data() + socket.registers + SN_DPORT0);
    destination.sin_addr.s_addr = *reinterpret_cast<const uint32_t *>(dest);

    const int res = connect(socket.myFD, (struct sockaddr *)&destination, sizeof(destination));

    if (res == 0)
    {
        socket.sn_sr = SN_SR_ESTABLISHED;
        socket.myErrno = 0;
#ifdef U2_LOG_STATE
        const uint16_t port = readNetworkWord(myMemory.data() + socket.registers + SN_DPORT0);
        LogFileOutput("U2: TCP[%" SIZE_T_FMT "]: CONNECT to %d.%d.%d.%d:%d\n", i, dest[0], dest[1], dest[2], dest[3], port);
#endif
    }
    else
    {
        const int error = sock_error();
        if (error == SOCK_EINPROGRESS || error == SOCK_EWOULDBLOCK)
        {
            socket.myErrno = error;
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
    case SN_CR_OPEN:
        openSocket(i);
        break;
    case SN_CR_CONNECT:
        connectSocket(i);
        break;
    case SN_CR_CLOSE:
    case SN_CR_DISCON:
        closeSocket(i);
        break;
    case SN_CR_SEND:
        sendData(i);
        break;
    case SN_CR_RECV:
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
    case SN_MR:
    case SN_CR:
        value = myMemory[address];
        break;
    case SN_SR:
        value = mySockets[i].sn_sr;
        break;
    case SN_TX_FSR0:
        value = getTXFreeSizeRegister(i, 8);
        break;
    case SN_TX_FSR1:
        value = getTXFreeSizeRegister(i, 0);
        break;
    case SN_TX_RD0:
    case SN_TX_RD1:
        value = myMemory[address];
        break;
    case SN_TX_WR0:
    case SN_TX_WR1:
        value = myMemory[address];
        break;
    case SN_RX_RSR0:
        receiveOnePacket(i);
        value = getRXDataSizeRegister(i, 8);
        break;
    case SN_RX_RSR1:
        receiveOnePacket(i);
        value = getRXDataSizeRegister(i, 0);
        break;
    case SN_RX_RD0:
    case SN_RX_RD1:
        value = myMemory[address];
        break;
    default:
#ifdef U2_LOG_UNKNOWN
        LogFileOutput("U2: Get unknown socket register[%" SIZE_T_FMT "]: %04x\n", i, address);
#endif
        value = myMemory[address];
        break;
    }
    return value;
}

uint8_t Uthernet2::readValueAt(const uint16_t address)
{
    uint8_t value;
    if (address == MR)
    {
        value = myModeRegister;
    }
    else if (address >= GAR0 && address <= UPORT1)
    {
        value = myMemory[address];
    }
    else if (address >= S0_BASE && address <= S3_MAX)
    {
        value = readSocketRegister(address);
    }
    else if (address >= TX_BASE && address <= MEM_MAX)
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
        value = myMemory[address & MEM_MAX];
    }
    return value;
}

void Uthernet2::autoIncrement()
{
    if (myModeRegister & MR_AI)
    {
        ++myDataAddress;
        // Read bottom of Uthernet II page 12
        // Setting the address to values >= 0x8000 is not really supported
        switch (myDataAddress)
        {
        case RX_BASE:
        case MEM_SIZE:
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
    case SN_MR:
        setSocketModeRegister(i, address, value);
        break;
    case SN_CR:
        setCommandRegister(i, value);
        break;
    case SN_PORT0:
    case SN_PORT1:
    case SN_DPORT0:
    case SN_DPORT1:
        myMemory[address] = value;
        break;
    case SN_DIPR0:
    case SN_DIPR1:
    case SN_DIPR2:
    case SN_DIPR3:
        myMemory[address] = value;
        break;
    case SN_PROTO:
        setIPProtocol(i, address, value);
        break;
    case SN_TOS:
        setIPTypeOfService(i, address, value);
        break;
    case SN_TTL:
        setIPTTL(i, address, value);
        break;
    case SN_TX_WR0:
        myMemory[address] = value;
        break;
    case SN_TX_WR1:
        myMemory[address] = value;
        break;
    case SN_RX_RD0:
        myMemory[address] = value;
        break;
    case SN_RX_RD1:
        myMemory[address] = value;
        break;
#ifdef U2_LOG_UNKNOWN
    default:
        LogFileOutput("U2: Set unknown socket register[%" SIZE_T_FMT "]: %04x\n", i, address);
        break;
#endif
    };
}

void Uthernet2::setModeRegister(const uint16_t address, const uint8_t value)
{
    if (value & MR_RST)
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
    if (address == MR)
    {
        setModeRegister(address, value);
    }
    else if (address >= GAR0 && address <= GAR3 ||
             address >= SUBR0 && address <= SUBR3 ||
             address >= SHAR0 && address <= SHAR5 ||
             address >= SIPR0 && address <= SIPR3)
    {
        myMemory[address] = value;
    }
    else if (address == RMSR)
    {
        setRXSizes(address, value);
    }
    else if (address == TMSR)
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
    if (address >= MR && address <= UPORT1)
    {
        writeCommonRegister(address, value);
    }
    else if (address >= S0_BASE && address <= S3_MAX)
    {
        writeSocketRegister(address, value);
    }
    else if (address >= TX_BASE && address <= MEM_MAX)
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
    }

    mySockets.resize(4);
    myMemory.clear();
    myMemory.resize(MEM_SIZE, 0);

    for (size_t i = 0; i < mySockets.size(); ++i)
    {
        mySockets[i].clearFD();
        mySockets[i].registers = static_cast<uint16_t>(S0_BASE + (i << 8));
    }

    // initial values
    myMemory[RTR0] = 0x07;
    myMemory[RTR1] = 0xD0;
    setRXSizes(RMSR, 0x55);
    setTXSizes(TMSR, 0x55);
}

BYTE Uthernet2::IO_C0(WORD programcounter, WORD address, BYTE write, BYTE value, ULONG nCycles)
{
    BYTE res = write ? 0 : MemReadFloatingBus(nCycles);

#ifdef U2_LOG_VERBOSE
    const uint16_t oldAddress = myDataAddress;
#endif

    const uint8_t loc = address & 0x0F;

    if (write)
    {
        switch (loc)
        {
        case C0X_MODE_REGISTER:
            setModeRegister(MR, value);
            break;
        case C0X_ADDRESS_HIGH:
            myDataAddress = (value << 8) | (myDataAddress & 0x00FF);
            break;
        case C0X_ADDRESS_LOW:
            myDataAddress = (value << 0) | (myDataAddress & 0xFF00);
            break;
        case C0X_DATA_PORT:
            writeValue(value);
            break;
        }
    }
    else
    {
        switch (loc)
        {
        case C0X_MODE_REGISTER:
            res = myModeRegister;
            break;
        case C0X_ADDRESS_HIGH:
            res = getIByte(myDataAddress, 8);
            break;
        case C0X_ADDRESS_LOW:
            res = getIByte(myDataAddress, 0);
            break;
        case C0X_DATA_PORT:
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

void Uthernet2::Init()
{
}

void Uthernet2::Update(const ULONG nExecutedCycles)
{
    myNetworkBackend->update(nExecutedCycles);
    for (Socket &socket : mySockets)
    {
        socket.process();
    }
}

static const UINT kUNIT_VERSION = 1;
#define SS_YAML_KEY_ENABLED "Enabled"
#define SS_YAML_KEY_NETWORK_INTERFACE "Network Interface"

void Uthernet2::SaveSnapshot(YamlSaveHelper &yamlSaveHelper)
{
    YamlSaveHelper::Slot slot(yamlSaveHelper, GetSnapshotCardName(), m_slot, kUNIT_VERSION);

    YamlSaveHelper::Label unit(yamlSaveHelper, "%s:\n", SS_YAML_KEY_STATE);
    yamlSaveHelper.SaveBool(SS_YAML_KEY_ENABLED, myNetworkBackend->isValid() ? true : false);
    yamlSaveHelper.SaveString(SS_YAML_KEY_NETWORK_INTERFACE, PCapBackend::tfe_interface);
}

bool Uthernet2::LoadSnapshot(YamlLoadHelper &yamlLoadHelper, UINT version)
{
    if (version < 1 || version > kUNIT_VERSION)
        throw std::runtime_error("Card: wrong version");

    yamlLoadHelper.LoadBool(SS_YAML_KEY_ENABLED); // FIXME: what is the point of this?
    PCapBackend::tfe_interface = yamlLoadHelper.LoadString(SS_YAML_KEY_NETWORK_INTERFACE);

    PCapBackend::tfe_SetRegistryInterface(m_slot, PCapBackend::tfe_interface);

    return true;
}
