#include <StdAfx.h>
#include "linux/network/uthernet2.h"
#include "linux/network/slirp2.h"
#include "linux/network/registers.h"

#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <poll.h>

// fix SOCK_NONBLOCK for e.g. macOS
#ifndef SOCK_NONBLOCK
// DISCALIMER
// totally untested, use at your own risk
#include <fcntl.h>
#define SOCK_NONBLOCK O_NONBLOCK
#endif

#define MAX_RXLENGTH 1518

// #define U2_LOG_VERBOSE
// #define U2_LOG_TRAFFIC
// #define U2_LOG_STATE
// #define U2_LOG_UNKNOWN

#ifndef U2_USE_SLIRP
#include "linux/network/tfe2.h"
#include "Tfe/tfe.h"
#endif

#include "Memory.h"
#include "Log.h"

namespace
{

  uint16_t readNetworkWord(const uint8_t * address)
  {
    const uint16_t network = *reinterpret_cast<const uint16_t *>(address);
    const uint16_t host = ntohs(network);
    return host;
  }

  uint8_t getIByte(const uint16_t value, const size_t shift)
  {
    return (value >> shift) & 0xFF;
  }

  void write8(Socket & socket, std::vector<uint8_t> & memory, const uint8_t value)
  {
    const uint16_t base = socket.receiveBase;
    const uint16_t address = base + socket.sn_rx_wr;
    memory[address] = value;
    socket.sn_rx_wr = (socket.sn_rx_wr + 1) % socket.receiveSize;
    ++socket.sn_rx_rsr;
  }

  // reverses the byte order
  void write16(Socket & socket, std::vector<uint8_t> & memory, const uint16_t value)
  {
    write8(socket, memory, getIByte(value, 8));  // high
    write8(socket, memory, getIByte(value, 0));  // low
  }

  void writeData(Socket & socket, std::vector<uint8_t> & memory, const uint8_t * data, const size_t len)
  {
    for (size_t c = 0; c < len; ++c)
    {
      write8(socket, memory, data[c]);
    }
  }

  // no byte reversal
  template<typename T>
  void writeAny(Socket & socket, std::vector<uint8_t> & memory, const T & t)
  {
    const uint8_t * data = reinterpret_cast<const uint8_t *>(&t);
    const uint16_t len = sizeof(T);
    writeData(socket, memory, data, len);
  }

  void writeDataMacRaw(Socket & socket, std::vector<uint8_t> & memory, const uint8_t * data, const size_t len)
  {
    // size includes sizeof(size)
    const uint16_t size = len + sizeof(uint16_t);
    write16(socket, memory, size);
    writeData(socket, memory, data, len);
  }

  void writeDataForProtocol(Socket & socket, std::vector<uint8_t> & memory, const uint8_t * data, const size_t len, const sockaddr_in & source)
  {
    if (socket.sn_sr == SN_SR_SOCK_UDP)
    {
      // these are already in network order
      writeAny(socket, memory, source.sin_addr);
      writeAny(socket, memory, source.sin_port);

      // size does not include sizeof(size)
      write16(socket, memory, len);
    } // no header for TCP

    writeData(socket, memory, data, len);
  }

}

Socket::Socket()
  : sn_sr(SN_SR_CLOSED)
  , myFD(-1)
  , myErrno(0)
{

}

void Socket::clearFD()
{
  if (myFD != -1)
  {
    close(myFD);
  }
  myFD = -1;
  sn_sr = SN_SR_CLOSED;
}

void Socket::setFD(const int fd, const int status)
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
  if (myFD != -1 && sn_sr == SN_SR_SOCK_INIT && myErrno == EINPROGRESS)
  {
    pollfd pfd = {.fd = myFD, .events = POLLOUT};
    if (poll(&pfd, 1, 0) > 0)
    {
      int err = 0;
      socklen_t elen = sizeof err;
      getsockopt(myFD, SOL_SOCKET, SO_ERROR, &err, &elen);

      if (err == 0)
      {
        myErrno = 0;
        sn_sr = SN_SR_ESTABLISHED;
#ifdef U2_LOG_STATE
        LogFileOutput("U2: TCP[]: CONNECTED\n");
#endif
      }
    }
  }
}

bool Socket::isThereRoomFor(const size_t len, const size_t header) const
{
  const uint16_t rsr = sn_rx_rsr;     // already present
  const uint16_t size = receiveSize;  // total size

return rsr + len + header < size; // "not =": we do not want to fill the buffer.
}

uint16_t Socket::getFreeRoom() const
{
  const uint16_t rsr = sn_rx_rsr;     // already present
  const uint16_t size = receiveSize;  // total size

  return size - rsr;
}

Uthernet2::Uthernet2(UINT slot) : Card(CT_Uthernet2, slot)
{
 #ifdef U2_USE_SLIRP
  mySlirp = std::make_shared<SlirpNet>();
#else
  const int check = tfe_enabled;
  tfe_init(true);
  if (tfe_enabled != check)
  {
    // tfe initialisation failed
    return;
  }
#endif
  Reset(true);
}

void Uthernet2::setSocketModeRegister(const size_t i, const uint16_t address, const uint8_t value)
{
  myMemory[address] = value;
  const uint8_t protocol = value & SN_MR_PROTO_MASK;
  switch (protocol)
  {
    case SN_MR_CLOSED:
#ifdef U2_LOG_STATE
      LogFileOutput("U2: Mode[%d]: CLOSED\n", i);
#endif
      break;
    case SN_MR_TCP:
#ifdef U2_LOG_STATE
      LogFileOutput("U2: Mode[%d]: TCP\n", i);
#endif
      break;
    case SN_MR_UDP:
#ifdef U2_LOG_STATE
      LogFileOutput("U2: Mode[%d]: UDP\n", i);
#endif
      break;
    case SN_MR_IPRAW:
#ifdef U2_LOG_STATE
      LogFileOutput("U2: Mode[%d]: IPRAW\n", i);
#endif
      break;
    case SN_MR_MACRAW:
#ifdef U2_LOG_STATE
      LogFileOutput("U2: Mode[%d]: MACRAW\n", i);
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
  for (Socket & socket : mySockets)
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
  for (Socket & socket : mySockets)
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
  const Socket & socket = mySockets[i];
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
  Socket & socket = mySockets[i];

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
    LogFileOutput("U2: RECV[%d]: %d -> %d bytes\n", i, socket.sn_rx_rsr, dataPresent);
  }
#endif
  socket.sn_rx_rsr = dataPresent;
}

void Uthernet2::receiveOnePacketMacRaw(const size_t i)
{
  Socket & socket = mySockets[i];
  const uint16_t rsr = socket.sn_rx_rsr;

#ifdef U2_USE_SLIRP
  {
    std::queue<std::vector<uint8_t>> & queue = mySlirp->getQueue();
    while (!queue.empty())
    {
      const std::vector<uint8_t> & packet = queue.front();
      if (socket.isThereRoomFor(packet.size(), sizeof(uint16_t)))
      {
        writeDataMacRaw(socket, myMemory, packet.data(), packet.size());
        queue.pop();
#ifdef U2_LOG_TRAFFIC
        LogFileOutput("U2: READ MACRAW[%d]: +%d -> %d bytes (%04x)\n", i, packet.size(), socket.sn_rx_rsr, socket.sn_rx_wr);
#endif
      }
      else
      {
        break;
      }
    }
  }
#else
  {
    BYTE buffer[MAX_RXLENGTH];
    int len;
    if (tfeReceiveOnePacket(memory.data() + SHAR0, sizeof(buffer), buffer, len))
    {
      if (isThereRoomFor(i, len, sizeof(uint16_t)))
      {
        writeDataMacRaw(i, buffer, len);
#ifdef U2_LOG_TRAFFIC
        LogFileOutput("U2: READ MACRAW[%d]: +%d -> %d bytes\n", i, len, socket.sn_rx_rsr);
#endif
      }
      else
      {
        // drop it
#ifdef U2_LOG_TRAFFIC
        LogFileOutput("U2: SKIP MACRAW[%d]: %d bytes\n", i, len);
#endif
      }
    }
  }
#endif
}

// UDP & TCP
void Uthernet2::receiveOnePacketFromSocket(const size_t i)
{
  Socket & socket = mySockets[i];
  if (socket.myFD != -1)
  {
    const uint16_t freeRoom = socket.getFreeRoom();
    if (freeRoom > 32) // avoid meaningless reads
    {
      std::vector<uint8_t> buffer(freeRoom - 1); // do not fill the buffer completely
      sockaddr_in source = {0};
      socklen_t len = sizeof(sockaddr_in);
      const ssize_t data = recvfrom(socket.myFD, buffer.data(), buffer.size(), 0, (struct sockaddr *) &source, &len);
#ifdef U2_LOG_TRAFFIC
      const char * proto = socket.sn_sr == SN_SR_SOCK_UDP ? "UDP" : "TCP";
#endif
      if (data > 0)
      {
        writeDataForProtocol(socket, myMemory, buffer.data(), data, source);
#ifdef U2_LOG_TRAFFIC
        LogFileOutput("U2: READ %s[%d]: +%d -> %d bytes\n", proto, i, data, socket.sn_rx_rsr);
#endif
      }
      else if (data == 0)
      {
        // gracefull termination
        socket.clearFD();
      }
      else // data < 0;
      {
        const int error = errno;
        if (error != EAGAIN && error != EWOULDBLOCK)
        {
#ifdef U2_LOG_TRAFFIC
          LogFileOutput("U2: ERROR %s[%d]: %d\n", proto, i, error);
#endif
          socket.clearFD();
        }
      }
    }
  }
}

void Uthernet2::receiveOnePacket(const size_t i)
{
  const Socket & socket = mySockets[i];
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
      break;  // nothing to do
#ifdef U2_LOG_UNKNOWN
    default:
      LogFileOutput("U2: READ[%d]: unknown mode: %02x\n", i, socket.sn_sr);
#endif
  };
}

void Uthernet2::sendDataMacRaw(const size_t i, const std::vector<uint8_t> & data) const
{
#ifdef U2_LOG_TRAFFIC
  LogFileOutput("U2: SEND MACRAW[%d]: %d bytes\n", i, data.size());
#endif
#ifdef U2_USE_SLIRP
  mySlirp->sendFromGuest(data.data(), data.size());
#else
  tfeTransmitOnePacket(data.data(), data.size());
#endif
}

void Uthernet2::sendDataToSocket(const size_t i, std::vector<uint8_t> & data)
{
  Socket & socket = mySockets[i];
  if (socket.myFD != -1)
  {
    sockaddr_in destination = {};
    destination.sin_family = AF_INET;

    // already in network order
    // this seems to be ignored for TCP, and so we reuse the same code
    const uint8_t * dest = myMemory.data() + socket.registers + SN_DIPR0;
    destination.sin_addr.s_addr = *reinterpret_cast<const uint32_t *>(dest);
    destination.sin_port = *reinterpret_cast<const uint16_t *>(myMemory.data() + socket.registers + SN_DPORT0);

    const ssize_t res = sendto(socket.myFD, data.data(), data.size(), 0, (const struct sockaddr *) &destination, sizeof(destination));
#ifdef U2_LOG_TRAFFIC
    const char * proto = socket.sn_sr == SN_SR_SOCK_UDP ? "UDP" : "TCP";
    LogFileOutput("U2: SEND %s[%d]: %d of %d bytes\n", proto, i, res, data.size());
#endif
    if (res < 0)
    {
      const int error = errno;
      if (error != EAGAIN && error != EWOULDBLOCK)
      {
#ifdef U2_LOG_TRAFFIC
        LogFileOutput("U2: ERROR %s[%d]: %d\n", proto, i, error);
#endif
        socket.clearFD();
      }
    }
  }
}

void Uthernet2::sendData(const size_t i)
{
  const Socket & socket = mySockets[i];
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
      LogFileOutput("U2: SEND[%d]: unknown mode: %02x\n", i, socket.sn_sr);
#endif
  }
}

void Uthernet2::resetRXTXBuffers(const size_t i)
{
  Socket & socket = mySockets[i];
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
  Socket & s = mySockets[i];
  const int fd = socket(AF_INET, type | SOCK_NONBLOCK, protocol);
  if (fd < -1)
  {
#ifdef U2_LOG_STATE
    const char * proto = state == SN_SR_SOCK_UDP ? "UDP" : "TCP";
    LogFileOutput("U2: %s[%d]: %s\n", proto, i, strerror(errno));
#endif
    s.clearFD();
  }
  else
  {
    s.setFD(fd, state);
  }
}

void Uthernet2::openSocket(const size_t i)
{
  Socket & socket = mySockets[i];
  const uint8_t mr = myMemory[socket.registers + SN_MR];
  const uint8_t protocol = mr & SN_MR_PROTO_MASK;
  uint8_t & sr = socket.sn_sr;
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
      LogFileOutput("U2: OPEN[%d]: unknown mode: %02x\n", i, mr);
#endif
  }
  resetRXTXBuffers(i); // needed?
#ifdef U2_LOG_STATE
  LogFileOutput("U2: OPEN[%d]: %02x\n", i, sr);
#endif
}

void Uthernet2::closeSocket(const size_t i)
{
  Socket & socket = mySockets[i];
  socket.clearFD();
#ifdef U2_LOG_STATE
  LogFileOutput("U2: CLOSE[%d]\n", i);
#endif
}

void Uthernet2::connectSocket(const size_t i)
{
  Socket & socket = mySockets[i];
  const uint8_t * dest = myMemory.data() + socket.registers + SN_DIPR0;

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
    const uint16_t port = readNetworkWord(memory.data() + socket.registers + SN_DPORT0);
    LogFileOutput("U2: TCP[%d]: CONNECT to %d.%d.%d.%d:%d\n", i, dest[0], dest[1], dest[2], dest[3], port);
#endif
  }
  else
  {
    const int error = errno;
    if (error == EINPROGRESS)
    {
      socket.myErrno = error;
    }
#ifdef U2_LOG_STATE
    LogFileOutput("U2: TCP[%d]: connect: %s\n", i, strerror(error));
#endif
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
      LogFileOutput("U2: Unknown command[%d]: %02x\n", i, value);
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
  switch (address)
  {
    case MR:
      value = myModeRegister;
      break;
    case GAR0 ... UPORT1:
      value = myMemory[address];
      break;
    case S0_BASE ... S3_MAX:
      value = readSocketRegister(address);
      break;
    case TX_BASE ... MEM_MAX:
      value = myMemory[address];
      break;
    default:
#ifdef U2_LOG_UNKNOWN
      LogFileOutput("U2: Read unknown location: %04x\n", address);
#endif
      // this might not be 100% correct if address >= 0x8000
      // see top of page 13 Uthernet II
      value = myMemory[address & MEM_MAX];
      break;
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
  LogFileOutput("U2: IP PROTO[%d] = %d\n", i, value);
#endif
}

void Uthernet2::setIPTypeOfService(const size_t i, const uint16_t address, const uint8_t value)
{
  myMemory[address] = value;
#ifdef U2_LOG_STATE
  LogFileOutput("U2: IP TOS[%d] = %d\n", i, value);
#endif
}

void Uthernet2::setIPTTL(const size_t i, const uint16_t address, const uint8_t value)
{
  myMemory[address] = value;
#ifdef U2_LOG_STATE
  LogFileOutput("U2: IP TTL[%d] = %d\n", i, value);
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
    case SN_DIPR0 ... SN_DIPR3:
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
      LogFileOutput("U2: Set unknown socket register[%d]: %04x\n", i, address);
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
  switch (address)
  {
    case MR:
      setModeRegister(address, value);
      break;
    case GAR0 ... GAR3:
    case SUBR0 ... SUBR3:
    case SHAR0 ... SHAR5:
    case SIPR0 ... SIPR3:
      myMemory[address] = value;
      break;
    case RMSR:
      setRXSizes(address, value);
      break;
    case TMSR:
      setTXSizes(address, value);
      break;
#ifdef U2_LOG_UNKNOWN
    default:
      LogFileOutput("U2: Set unknown common register: %04x\n", address);
      break;
#endif
  };
}

void Uthernet2::writeValueAt(const uint16_t address, const uint8_t value)
{
  switch (address)
  {
    case MR ... UPORT1:
      writeCommonRegister(address, value);
      break;
    case S0_BASE ... S3_MAX:
      writeSocketRegister(address, value);
      break;
    case TX_BASE ... MEM_MAX:
      myMemory[address] = value;
      break;
#ifdef U2_LOG_UNKNOWN
    default:
      LogFileOutput("U2: Write to unknown location: %02x to %04x\n", value, address);
      break;
#endif
  }
}

void Uthernet2::writeValue(const uint8_t value)
{
  writeValueAt(myDataAddress, value);
  autoIncrement();
}

void Uthernet2::Reset(const bool powerCycle)
{
  LogFileOutput("U2: Uthernet 2 initialisation\n");
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
    mySockets[i].registers = S0_BASE + (i << 8);
  }

  // initial values
  myMemory[RTR0] = 0x07;
  myMemory[RTR1] = 0xD0;
  setRXSizes(RMSR, 0x55);
  setTXSizes(TMSR, 0x55);

#ifdef U2_USE_SLIRP
  mySlirp->clearQueue();
#endif
}

BYTE Uthernet2::IO_C0(WORD programcounter, WORD address, BYTE write, BYTE value, ULONG nCycles)
{
  BYTE res = write ? 0 : MemReadFloatingBus(nCycles);

#ifdef U2_LOG_VERBOSE
  const uint16_t oldAddress = dataAddress;
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
  const char * mode = write ? "WRITE " : "READ  ";
  const char c = std::isprint(res) ? res : '.';
  LogFileOutput("U2: %04x: %s %04x[%04x] %02x = %02x, %c [%d = %d]\n", programcounter, mode, address, oldAddress, value, res, c, value, res);
#endif

  return res;
}

BYTE u2_C0(WORD programcounter, WORD address, BYTE write, BYTE value, ULONG nCycles)
{
  UINT uSlot = ((address & 0xff) >> 4) - 8;
  Uthernet2* pCard = (Uthernet2*) MemGetSlotParameters(uSlot);
  return pCard->IO_C0(programcounter, address, write, value, nCycles);
}

void Uthernet2::processEvents(uint32_t timeout)
{
#ifdef U2_USE_SLIRP
  if (mySlirp)
  {
    mySlirp->process(timeout);
  }
#endif
  for (Socket & socket : mySockets)
  {
    socket.process();
  }
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
  processEvents(0);
}
