#include <StdAfx.h>

#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <poll.h>

#define MAX_RXLENGTH 1518

// #define U2_LOG_VERBOSE
// #define U2_LOG_TRAFFIC
// #define U2_LOG_STATE
// #define U2_LOG_UNKNOWN

// if this is defined, libslirp is used as opposed to libpcap
#define U2_USE_SLIRP

#include "linux/network/uthernet2.h"
#include "linux/network/registers.h"

#ifdef U2_USE_SLIRP
#include "linux/network/slirp2.h"
#else
#include "linux/network/tfe2.h"
#endif

#include "Memory.h"
#include "Log.h"

namespace
{

  struct Socket
  {
    uint16_t transmitBase;
    uint16_t transmitSize;
    uint16_t receiveBase;
    uint16_t receiveSize;
    uint16_t registers;

    uint16_t sn_rx_wr;
    uint16_t sn_rx_rsr;

    uint8_t sn_sr = SN_SR_CLOSED;

    int myFD = -1;
    int myErrno = 0;

    void clearFD();
    void setFD(int fd);
    void process();

    ~Socket();
  };

  void Socket::clearFD()
  {
    if (myFD != -1)
    {
      close(myFD);
    }
    myFD = -1;
    sn_sr = SN_SR_CLOSED;
  }

  void Socket::setFD(const int fd)
  {
    clearFD();
    myFD = fd;
    myErrno = 0;
    sn_sr = SN_SR_SOCK_INIT;
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

  std::vector<uint8_t> memory;
  std::vector<Socket> sockets;
  uint8_t modeRegister = 0;
  uint16_t dataAddress = 0;

#ifdef U2_USE_SLIRP
  std::shared_ptr<SlirpNet> slirp;
#endif

  void initialise();

  uint16_t readNetworkWord(const uint8_t * address)
  {
    const uint16_t network = *reinterpret_cast<const uint16_t *>(address);
    const uint16_t host = ntohs(network);
    return host;
  }

  bool isThereRoomFor(const size_t i, const size_t len, const size_t header)
  {
    const Socket & socket = sockets[i];
    const uint16_t rsr = socket.sn_rx_rsr;     // already present
    const uint16_t size = socket.receiveSize;  // total size

    return rsr + len + header < size; // "not =": we do not want to fill the buffer.
  }

  uint16_t getFreeRoom(const size_t i)
  {
    const Socket & socket = sockets[i];
    const uint16_t rsr = socket.sn_rx_rsr;     // already present
    const uint16_t size = socket.receiveSize;  // total size

    return size - rsr;
  }

  uint8_t getIByte(const uint16_t value, const size_t shift)
  {
    return (value >> shift) & 0xFF;
  }

  void write8(const size_t i, const uint8_t value)
  {
    Socket & socket = sockets[i];
    const uint16_t base = socket.receiveBase;
    const uint16_t address = base + socket.sn_rx_wr;
    memory[address] = value;
    socket.sn_rx_wr = (socket.sn_rx_wr + 1) % socket.receiveSize;
    ++socket.sn_rx_rsr;
  }

  void write16(const size_t i, const uint16_t value)
  {
    write8(i, getIByte(value, 8));  // high
    write8(i, getIByte(value, 0));  // low
  }

  void writeData(const size_t i, const BYTE * data, const size_t len)
  {
    for (size_t c = 0; c < len; ++c)
    {
      write8(i, data[c]);
    }
  }

  void writeDataMacRaw(const size_t i, const BYTE * data, const size_t len)
  {
    // size includes sizeof(size)
    const uint16_t size = len + sizeof(uint16_t);
    write16(i, size);
    for (size_t c = 0; c < len; ++c)
    {
      write8(i, data[c]);
    }
  }

  void setSocketModeRegister(const size_t i, const uint16_t address, const uint8_t value)
  {
    memory[address] = value;
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

  void setTXSizes(const uint16_t address, uint8_t value)
  {
    memory[address] = value;
    uint16_t base = TX_BASE;
    const uint16_t end = RX_BASE;
    for (Socket & socket : sockets)
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

  void setRXSizes(const uint16_t address, uint8_t value)
  {
    memory[address] = value;
    uint16_t base = RX_BASE;
    const uint16_t end = MEM_SIZE;
    for (Socket & socket : sockets)
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

  uint16_t getTXDataSize(const size_t i)
  {
    const Socket & socket = sockets[i];
    const uint16_t size = socket.transmitSize;
    const uint16_t mask = size - 1;

    const int sn_tx_rd = readNetworkWord(memory.data() + socket.registers + SN_TX_RD0) & mask;
    const int sn_tx_wr = readNetworkWord(memory.data() + socket.registers + SN_TX_WR0) & mask;

    int dataPresent = sn_tx_wr - sn_tx_rd;
    if (dataPresent < 0)
    {
      dataPresent += size;
    }
    return dataPresent;
  }

  uint8_t getTXFreeSizeRegister(const size_t i, const size_t shift)
  {
    const int size = sockets[i].transmitSize;
    const uint16_t present = getTXDataSize(i);
    const uint16_t free = size - present;
    const uint8_t reg = getIByte(free, shift);
    return reg;
  }

  uint8_t getRXDataSizeRegister(const size_t i, const size_t shift)
  {
    const uint16_t rsr = sockets[i].sn_rx_rsr;
    const uint8_t reg = getIByte(rsr, shift);
    return reg;
  }

  void updateRSR(const size_t i)
  {
    Socket & socket = sockets[i];

    const int size = socket.receiveSize;
    const uint16_t mask = size - 1;

    const int sn_rx_rd = readNetworkWord(memory.data() + socket.registers + SN_RX_RD0) & mask;
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
    socket.sn_rx_rsr = dataPresent;
#ifdef U2_LOG_TRAFFIC
    if (socket.sn_rx_rsr != dataPresent)
    {
      LogFileOutput("U2: RECV[%d]: %d -> %d bytes\n", i, socket.sn_rx_rsr, dataPresent);
    }
#endif
  }

  void receiveOnePacketMacRaw(const size_t i)
  {
    const Socket & socket = sockets[i];
    const uint16_t rsr = socket.sn_rx_rsr;

#ifdef U2_USE_SLIRP
    {
      std::queue<std::vector<uint8_t>> & queue = slirp->getQueue();
      if (!queue.empty())
      {
        const std::vector<uint8_t> & packet = queue.front();
        if (isThereRoomFor(i, packet.size(), sizeof(uint16_t)))
        {
          writeDataMacRaw(i, packet.data(), packet.size());
          queue.pop();
#ifdef U2_LOG_TRAFFIC
          LogFileOutput("U2: READ MACRAW[%d]: +%d -> %d bytes (%04x)\n", i, packet.size(), socket.sn_rx_rsr, socket.sn_rx_wr);
#endif
        }
      }
    }
#else
    {
      BYTE buffer[MAX_RXLENGTH];
      int len = sizeof(buffer);
      if (tfeReceiveOnePacket(memory.data() + SHAR0, buffer, len))
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
          // ??? we just skip it
#ifdef U2_LOG_TRAFFIC
          LogFileOutput("U2: SKIP MACRAW[%d]: %d bytes\n", i, len);
#endif
        }
      }
    }
#endif
  }

  void receiveOnePacketTCP(const size_t i)
  {
    Socket & socket = sockets[i];
    if (socket.myFD != -1)
    {
      const uint16_t freeRoom = getFreeRoom(i);
      if (freeRoom > 32) // avoid meaningless reads
      {
        std::vector<uint8_t> buffer(freeRoom - 1); // do not fill the buffer completely
        const ssize_t data = recv(socket.myFD, buffer.data(), buffer.size(), 0);
        if (data > 0)
        {
          // TCP: no header, just the data
          writeData(i, buffer.data(), data);
#ifdef U2_LOG_TRAFFIC
          LogFileOutput("U2: READ TCP[%d]: +%d -> %d bytes\n", i, data, socket.sn_rx_rsr);
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
            socket.clearFD();
          }
        }
      }
    }
  }

  void receiveOnePacket(const size_t i)
  {
    const Socket & socket = sockets[i];
    switch (socket.sn_sr)
    {
      case SN_SR_SOCK_MACRAW:
        receiveOnePacketMacRaw(i);
        break;
      case SN_SR_ESTABLISHED:
        receiveOnePacketTCP(i);
        break;
      case SN_SR_CLOSED:
        break;  // nothing to do
#ifdef U2_LOG_UNKNOWN
      default:
        LogFileOutput("U2: READ[%d]: unknown mode: %02x\n", i, socket.sn_sr);
#endif
    };
  }

  void sendDataMacRaw(const size_t i, const std::vector<uint8_t> & data)
  {
#ifdef U2_LOG_TRAFFIC
    LogFileOutput("U2: SEND MACRAW[%d]: %d bytes\n", i, data.size());
#endif
#ifdef U2_USE_SLIRP
    slirp->sendFromGuest(data.data(), data.size());
#else
    tfeTransmitOnePacket(data.data(), data.size());
#endif
  }

  void sendDataTCP(const size_t i, std::vector<uint8_t> & data)
  {
    Socket & socket = sockets[i];
    if (socket.myFD != -1)
    {
      const ssize_t res = send(socket.myFD, data.data(), data.size(), 0);
      if (res < 0)
      {
        const int error = errno;
        if (error != EAGAIN && error != EWOULDBLOCK)
        {
          socket.clearFD();
        }
      }
#ifdef U2_LOG_TRAFFIC
      LogFileOutput("U2: SEND TCP[%d]: %d of %d bytes\n", i, res, data.size());
#endif
    }
  }

  void sendData(const size_t i)
  {
    const Socket & socket = sockets[i];
    const uint16_t size = socket.transmitSize;
    const uint16_t mask = size - 1;

    const int sn_tx_rr = readNetworkWord(memory.data() + socket.registers + SN_TX_RD0) & mask;
    const int sn_tx_wr = readNetworkWord(memory.data() + socket.registers + SN_TX_WR0) & mask;

    const uint16_t base = socket.transmitBase;
    const uint16_t rr_address = base + sn_tx_rr;
    const uint16_t wr_address = base + sn_tx_wr;

    std::vector<uint8_t> data;
    if (rr_address < wr_address)
    {
      data.assign(memory.begin() + rr_address, memory.begin() + wr_address);
    }
    else
    {
      const uint16_t end = base + size;
      data.assign(memory.begin() + rr_address, memory.begin() + end);
      data.insert(data.end(), memory.begin() + base, memory.begin() + wr_address);
    }

    // move read pointer to writer
    memory[socket.registers + SN_TX_RD0] = getIByte(sn_tx_wr, 8);
    memory[socket.registers + SN_TX_RD1] = getIByte(sn_tx_wr, 0);

    switch (socket.sn_sr)
    {
      case SN_SR_SOCK_MACRAW:
        sendDataMacRaw(i, data);
        break;
      case SN_SR_ESTABLISHED:
        sendDataTCP(i, data);
        break;
#ifdef U2_LOG_UNKNOWN
      default:
        LogFileOutput("U2: SEND[%d]: unknown mode: %02x\n", i, socket.sn_sr);
#endif
    }
  }

  void resetRXTXBuffers(const size_t i)
  {
    Socket & socket = sockets[i];
    socket.sn_rx_wr = 0x00;
    socket.sn_rx_rsr = 0x00;
    memory[socket.registers + SN_TX_RD0] = 0x00;
    memory[socket.registers + SN_TX_RD1] = 0x00;
    memory[socket.registers + SN_TX_WR0] = 0x00;
    memory[socket.registers + SN_TX_WR1] = 0x00;
    memory[socket.registers + SN_RX_RD0] = 0x00;
    memory[socket.registers + SN_RX_RD1] = 0x00;
  }

  void openSocketTCP(const size_t i)
  {
    Socket & s = sockets[i];
    const int fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (fd < -1)
    {
#ifdef U2_LOG_STATE      
      LogFileOutput("U2: TCP[%d]: %s\n", i, strerror(errno));
#endif
      s.clearFD();
    }
    else
    {
      s.setFD(fd);
    }
  }

  void openSocket(const size_t i)
  {
    Socket & socket = sockets[i];
    const uint8_t mr = memory[socket.registers + SN_MR];
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
        openSocketTCP(i);
        break;
      case SN_MR_UDP:
        sr = SN_SR_SOCK_UDP;
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

  void closeSocket(const size_t i)
  {
    Socket & socket = sockets[i];
    socket.clearFD();
#ifdef U2_LOG_STATE      
    LogFileOutput("U2: CLOSE[%d]\n", i);
#endif
  }

  void connectSocket(const size_t i)
  {
    Socket & socket = sockets[i];
    const uint8_t * dest = memory.data() + socket.registers + SN_DIPR0;

    sockaddr_in destination = {};
    destination.sin_family = AF_INET;

    // already in network order
    destination.sin_port = *reinterpret_cast<const uint16_t *>(memory.data() + socket.registers + SN_DPORT0);
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
      LogFileOutput("U2: TCP[%d]: %s\n", i, strerror(error));
#endif
    }
  }

  void setCommandRegister(const size_t i, const uint8_t value)
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

  uint8_t readSocketRegister(const uint16_t address)
  {
    const uint16_t i = (address >> 8) - 0x04;
    const uint16_t loc = address & 0xFF;
    uint8_t value;
    switch (loc)
    {
      case SN_MR:
      case SN_CR:
        value = memory[address];
        break;
      case SN_SR:
        value = sockets[i].sn_sr;
        break;
      case SN_TX_FSR0:
        value = getTXFreeSizeRegister(i, 8);
        break;
      case SN_TX_FSR1:
        value = getTXFreeSizeRegister(i, 0);
        break;
      case SN_TX_RD0:
      case SN_TX_RD1:
        value = memory[address];
        break;
      case SN_TX_WR0:
      case SN_TX_WR1:
        value = memory[address];
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
        value = memory[address];
        break;
      default:
#ifdef U2_LOG_UNKNOWN
        LogFileOutput("U2: Get unknown socket register[%d]: %04x\n", i, address);
#endif
        value = memory[address];
        break;
    }
    return value;
  }

  uint8_t readValueAt(const uint16_t address)
  {
    uint8_t value;
    switch (address)
    {
      case MR:
        value = modeRegister;
        break;
      case GAR0 ... UPORT1:
        value = memory[address];
        break;
      case S0_BASE ... S3_MAX:
        value = readSocketRegister(address);
        break;
      case TX_BASE ... MEM_MAX:
        value = memory[address];
        break;
      default:
#ifdef U2_LOG_UNKNOWN
        LogFileOutput("U2: Read unknown location: %04x\n", address);
#endif
        // this might not be 100% correct if address >= 0x8000
        // see top of page 13 Uthernet II
        value = memory[address & MEM_MAX];
        break;
    }
    return value;
  }

  void autoIncrement()
  {
    if (modeRegister & MR_AI)
    {
      ++dataAddress;
      // Read bottom of Uthernet II page 12
      // Setting the address to values >= 0x8000 is not really supported
      switch (dataAddress)
      {
        case RX_BASE:
        case MEM_SIZE:
          dataAddress -= 0x2000;
          break;
      }
    }
  }

  uint8_t readValue()
  {
    const uint8_t value = readValueAt(dataAddress);
    autoIncrement();
    return value;
  }

  void setIPProtocol(const size_t i, const uint16_t address, const uint8_t value)
  {
    memory[address] = value;
#ifdef U2_LOG_STATE
    LogFileOutput("U2: IP PROTO[%d] = %d\n", i, value);
#endif
  }

  void setIPTypeOfService(const size_t i, const uint16_t address, const uint8_t value)
  {
    memory[address] = value;
#ifdef U2_LOG_STATE
    LogFileOutput("U2: IP TOS[%d] = %d\n", i, value);
#endif
  }

  void setIPTTL(const size_t i, const uint16_t address, const uint8_t value)
  {
    memory[address] = value;
#ifdef U2_LOG_STATE
    LogFileOutput("U2: IP TTL[%d] = %d\n", i, value);
#endif
  }

  void writeSocketRegister(const uint16_t address, const uint8_t value)
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
        memory[address] = value;
        break;
      case SN_DIPR0 ... SN_DIPR3:
        memory[address] = value;
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
        memory[address] = value;
        break;
      case SN_TX_WR1:
        memory[address] = value;
        break;
      case SN_RX_RD0:
        memory[address] = value;
        break;
      case SN_RX_RD1:
        memory[address] = value;
        break;
#ifdef U2_LOG_UNKNOWN
      default:
        LogFileOutput("U2: Set unknown socket register[%d]: %04x\n", i, address);
        break;
#endif
    };
  }

  void setModeRegister(const uint16_t address, const uint8_t value)
  {
    if (value & MR_RST)
    {
      initialise();
    }
    else
    {
      modeRegister = value;
    }
  }

  void writeCommonRegister(const uint16_t address, const uint8_t value)
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
        memory[address] = value;
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

  void writeValueAt(const uint16_t address, const uint8_t value)
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
        memory[address] = value;
        break;
#ifdef U2_LOG_UNKNOWN
      default:
        LogFileOutput("U2: Write to unknown location: %02x to %04x\n", value, address);
        break;
#endif
    }
  }

  void writeValue(const uint8_t value)
  {
    writeValueAt(dataAddress, value);
    autoIncrement();
  }

  void initialise()
  {
    LogFileOutput("U2: Uthernet 2 initialisation\n");
    modeRegister = 0;
    // dataAddress is NOT reset, see page 10 of Uthernet II
    sockets.resize(4);
    memory.clear();
    memory.resize(MEM_SIZE, 0);

    for (size_t i = 0; i < sockets.size(); ++i)
    {
      sockets[i].clearFD();
      sockets[i].registers = S0_BASE + (i << 8);
    }

    // initial values
    memory[RTR0] = 0x07;
    memory[RTR1] = 0xD0;
    setRXSizes(RMSR, 0x55);
    setTXSizes(TMSR, 0x55);

#ifdef U2_USE_SLIRP
    slirp->clearQueue();
#endif
  }

  BYTE u2_C0(WORD programcounter, WORD address, BYTE write, BYTE value, ULONG nCycles)
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
          dataAddress = (value << 8) | (dataAddress & 0x00FF);
          break;
        case C0X_ADDRESS_LOW:
          dataAddress = (value << 0) | (dataAddress & 0xFF00);
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
          res = modeRegister;
          break;
        case C0X_ADDRESS_HIGH:
          res = getIByte(dataAddress, 8);
          break;
        case C0X_ADDRESS_LOW:
          res = getIByte(dataAddress, 0);
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

}

void registerUthernet2()
{
 #ifdef U2_USE_SLIRP
  slirp.reset();
  slirp = std::make_shared<SlirpNet>();
#endif
  initialise();
  RegisterIoHandler(SLOT3, u2_C0, u2_C0, nullptr, nullptr, nullptr, nullptr);
}

void processEventsUthernet2(uint32_t timeout)
{
#ifdef U2_USE_SLIRP
  if (slirp)
  {
    slirp->process(timeout);
  }
#endif
  for (Socket & socket : sockets)
  {
    socket.process();
  }
}