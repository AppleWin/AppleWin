#include <StdAfx.h>
#include "linux/uthernet2.h"
#include "linux/tfe2.h"

#include "Memory.h"

#define MAX_RXLENGTH 1518
//#define U2_VERBOSE
#define U2_UNKNOWN

#include <iostream>
#include <iomanip>

namespace
{
  #define MR_INDIRECT 0x01  // 0
  #define MR_AUTO_INC 0x02  // 1
  #define MR_PPOE_MOD 0x08  // 3
  #define MR_PING_BLK 0x10  // 4
  #define MR_SW_RESET 0x80  // 7

  struct Socket
  {
    uint16_t transmitBase;
    uint16_t transmitSize;
    uint16_t receiveBase;
    uint16_t receiveSize;
    uint16_t registers;

    uint16_t sn_rx_wr;
    uint16_t sn_rx_rsr;
  };

  std::vector<uint8_t> memory;
  std::vector<Socket> sockets;
  uint8_t modeRegister = 0;
  uint16_t dataAddress = 0;

  void initialise();

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
    const uint16_t size = len + sizeof(uint16_t);
    write16(i, size);
    for (size_t c = 0; c < len; ++c)
    {
      write8(i, data[c]);
    }
  }

  void writePacketString(const size_t i, const std::string & s)
  {
    const uint16_t size = s.size() + sizeof(uint16_t);  // no NULL
    write16(i, size);
    for (size_t c = 0; c < s.size(); ++c)
    {
      write8(i, s[c]);
    }
  }

  uint16_t readNetworkWord(const uint16_t address)
  {
    const uint8_t high = memory[address];
    const uint8_t low = memory[address + 1];
    const uint16_t value = low + (high << 8);
    return value;
  }

  void setSocketModeRegister(const size_t i, const uint16_t address, const uint8_t value)
  {
    memory[address] = value;
    const uint8_t protocol = value & 0x0F;
    switch (protocol)
    {
      case 0x00:
        std::cerr << "Mode[" << i << "]: Closed" << std::endl;
        break;
      case 0x03:
        std::cerr << "Mode[" << i << "]: IPRAW" << std::endl;
        break;
      case 0x04:
        std::cerr << "Mode[" << i << "]: MACRAW" << std::endl;
        break;
#ifdef U2_UNKNOWN
      default:
        std::cerr << "Unknown protocol: ";
        as_hex(std::cerr, value, 2) << std::endl;
#endif
    }
  }

  void setTXSizes(const uint16_t address, uint8_t value)
  {
    memory[address] = value;
    uint16_t base = 0x4000;
    const uint16_t ceiling = base + 0x2000;
    for (size_t i = 0; i < 4; ++i)
    {
      sockets[i].transmitBase = base;

      const uint8_t bits = value & 0x03;
      value >>= 2;

      const uint16_t size = 1 << (10 + bits);
      base += size;

      if (base > ceiling)
      {
        base = ceiling;
      }
      sockets[i].transmitSize = base - sockets[i].transmitBase;
      // std::cerr << "TX[" << i << "] = ";
      // as_hex(std::cerr, sockets[i].transmitSize, 4) << std::endl;
    }
  }

  void setRXSizes(const uint16_t address, uint8_t value)
  {
    memory[address] = value;
    uint16_t base = 0x6000;
    const uint16_t ceiling = base + 0x2000;
    for (size_t i = 0; i < 4; ++i)
    {
      sockets[i].receiveBase = base;

      const uint8_t bits = value & 0x03;
      value >>= 2;

      const uint16_t size = 1 << (10 + bits);
      base += size;

      if (base > ceiling)
      {
        base = ceiling;
      }
      sockets[i].receiveSize = base - sockets[i].receiveBase;
      // std::cerr << "RX[" << i << "] = ";
      // as_hex(std::cerr, sockets[i].receiveSize, 4) << std::endl;
    }
  }

  uint16_t getTXDataSize(const size_t i)
  {
    const uint16_t size = sockets[i].transmitSize;
    const uint16_t mask = size - 1;

    const int sn_tx_rr = readNetworkWord(sockets[i].registers + 0x22) & mask;
    const int sn_tx_wr = readNetworkWord(sockets[i].registers + 0x24) & mask;

    int dataPresent = sn_tx_wr - sn_tx_rr;
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

    const int size = sockets[i].receiveSize;
    const uint16_t mask = size - 1;

    const int sn_rx_rd = readNetworkWord(sockets[i].registers + 0x28) & mask;
    const int sn_rx_wr = sockets[i].sn_rx_wr & mask;
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
  }

  void receiveOnePacketMacRaw(const size_t i)
  {
    Socket & socket = sockets[i];
    const uint16_t rsr = socket.sn_rx_rsr;

    BYTE buffer[MAX_RXLENGTH];
    int len = sizeof(buffer);
    if (tfeReceiveOnePacket(memory.data() + 0x0009, buffer, len))
    {
      const int size = socket.receiveSize;
      if (rsr + len < size) // "not =": we do not want to fill the buffer.
      {
        writeData(i, buffer, len);
        std::cerr << "READ[" << i << "]: ";
        as_hex(std::cerr, len, 4) << " -> ";
        as_hex(std::cerr, socket.sn_rx_rsr, 4) << " bytes" << std::endl;
      }
      else
      {
        // ??? we just skip it
        std::cerr << "SKIP[" << i << "]: ";
        as_hex(std::cerr, len, 4) << " bytes" << std::endl;
      }
    }
  }

  void receiveOnePacket(const size_t i)
  {
    const Socket & socket = sockets[i];
    const uint8_t sr = memory[socket.registers + 0x03];
    switch (sr)
    {
      case 0x42:
        receiveOnePacketMacRaw(i);
        break;
    };
  }

  void sendData(const size_t i)
  {
    const Socket & socket = sockets[i];
    const uint16_t size = socket.transmitSize;
    const uint16_t mask = size - 1;

    const int sn_tx_rr = readNetworkWord(socket.registers + 0x22) & mask;
    const int sn_tx_wr = readNetworkWord(socket.registers + 0x24) & mask;

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
    memory[socket.registers + 0x22] = getIByte(sn_tx_wr, 8);
    memory[socket.registers + 0x23] = getIByte(sn_tx_wr, 0);

    std::cerr << "SEND[" << i << "]: ";
    as_hex(std::cerr, data.size(), 4) << " bytes " << std::endl;
    tfeTransmitOnePacket(data.data(), data.size());
  }

  void resetRXTXBuffers(const size_t i)
  {
    Socket & socket = sockets[i];
    socket.sn_rx_wr = 0x00;
    socket.sn_rx_rsr = 0x00;
    memory[socket.registers + 0x22] = 0x00;
    memory[socket.registers + 0x23] = 0x00;
    memory[socket.registers + 0x24] = 0x00;
    memory[socket.registers + 0x25] = 0x00;
    memory[socket.registers + 0x28] = 0x00;
    memory[socket.registers + 0x29] = 0x00;
  }

  void openSocket(const size_t i)
  {
    const Socket & socket = sockets[i];
    const uint8_t mr = memory[socket.registers + 0x00];
    const uint8_t protocol = mr & 0x0F;
    uint8_t & sr = memory[socket.registers + 0x03];
    switch (protocol)
    {
      case 0x03:    // IPRAW
        sr = 0x32;  // SOCK_IPRAW
        break;
      case 0x04:    // MACRAW
        sr = 0x42;  // SOCK_MACRAW
        break;
#ifdef U2_UNKNOWN
      default:
        std::cerr << "OPEN: unknown mode: ";
        as_hex(std::cerr, mr, 2) << std::endl;
#endif
    }
    resetRXTXBuffers(i);
    std::cerr << "OPEN[" << i << "] = ";
    as_hex(std::cerr, sr, 2) << std::endl;
  }

  void closeSocket(const size_t i)
  {
    const Socket & socket = sockets[i];
    memory[socket.registers + 0x03] = 0x00; // SOCK_CLOSED
    std::cerr << "CLOSE[" << i << "]" << std::endl;
  }

  void setCommandRegister(const size_t i, const uint8_t value)
  {
    switch (value)
    {
      case 0x01:  // OPEN
        openSocket(i);
        break;
      case 0x10:  // CLOSE
        closeSocket(i);
        break;
      case 0x20:  // SEND
        sendData(i);
        break;
      case 0x40:  // RECV
        updateRSR(i);
        break;
#ifdef U2_UNKNOWN
      default:
        std::cerr << "Unknown command: ";
        as_hex(std::cerr, value, 2) << std::endl;
#endif
    }
  }

  uint8_t readSocketRegister(const uint16_t address)
  {
    const uint16_t i = (address >> (2 + 8 + 8));
    const uint16_t loc = address & 0xFF;
    uint8_t value;
    switch (loc)
    {
      case 0x00:  // Sn_MR
      case 0x01:  // Sn_CR
        value = memory[address];
        break;
      case 0x20:  // Sn_TX_FSR  high
        value = getTXFreeSizeRegister(i, 8);
        break;
      case 0x21:  // Sn_TX_FSR  low
        value = getTXFreeSizeRegister(i, 0);
        break;
      case 0x22:  // Sn_TX_RR
      case 0x23:  // Sn_TX_RR
        value = memory[address];
        break;
      case 0x24:  // Sn_TX_WR
      case 0x25:  // Sn_TX_WR
        value = memory[address];
        break;
      case 0x26:  // Sn_RX_RSR
        value = getRXDataSizeRegister(i, 8);
        break;
      case 0x27:  // Sn_RX_RSR
        value = getRXDataSizeRegister(i, 0);
        break;
      case 0x28:  // Sn_RX_RD
      case 0x29:  // Sn_RX_RD
        value = memory[address];
        break;
      default:
#ifdef U2_UNKNOWN
        std::cerr << "Get unknown socket register: ";
        as_hex(std::cerr, address, 4) << std::endl;
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
      case 0x0000 ... 0x002F:
        value = memory[address];
        break;
      case 0x0400 ... 0x07FF:
        value = readSocketRegister(address);
        break;
      case 0x4000 ... 0x7FFF:
        value = memory[address];
        break;
      default:
#ifdef U2_UNKNOWN
        std::cerr << "Read unknown location: ";
        as_hex(std::cerr, address, 4) << std::endl;
#endif
        value = memory[address];
        break;
    }
    return value;
  }

  void autoIncrement()
  {
    if (modeRegister & MR_AUTO_INC)
    {
      ++dataAddress;
      switch (dataAddress)
      {
        case 0x8000:
        case 0x6000:
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

  void setIPProtocol(const size_t i, const uint8_t value)
  {
    std::cerr << "IP PROTO[" << i << "] = ";
    as_hex(std::cerr, value, 2) << std::endl;
  }

  void setIPTypeOfService(const size_t i, const uint8_t value)
  {
    std::cerr << "IP TOS[" << i << "] = ";
    as_hex(std::cerr, value, 2) << std::endl;
  }

  void setIPTTL(const size_t i, const uint8_t value)
  {
    std::cerr << "IP TTL[" << i << "] = ";
    as_hex(std::cerr, value, 2) << std::endl;
  }

  void writeSocketRegister(const uint16_t address, const uint8_t value)
  {
    const uint16_t i = (address >> (2 + 8 + 8));
    const uint16_t loc = address & 0xFF;
    switch (loc)
    {
      case 0x00:
        setSocketModeRegister(i, address, value);
        break;
      case 0x01:
        setCommandRegister(i, value);
        break;
      case 0x14:
        setIPProtocol(i, value);
        break;
      case 0x15:
        setIPTypeOfService(i, value);
        break;
      case 0x16:
        setIPTTL(i, value);
        break;
      case 0x24:    // Sn_TX_WR
        memory[address] = value;
        break;
      case 0x25:    // Sn_TX_WR
        memory[address] = value;
        break;
      case 0x28:    // Sn_RX_RD
        memory[address] = value;
        break;
      case 0x29:    // Sn_RX_RD
        memory[address] = value;
        break;
#ifdef U2_UNKNOWN
      default:
        std::cerr << "Set unknown socket register: ";
        as_hex(std::cerr, address, 4) << std::endl;
        break;
#endif
    };
  }

  void setModeRegister(const uint16_t address, const uint8_t value)
  {
    modeRegister = value;
    if (modeRegister & MR_SW_RESET)
    {
      initialise();
    }
    memory[address] = value; // redundant
  }

  void writeCommonRegister(const uint16_t address, const uint8_t value)
  {
    switch (address)
    {
      case 0x0000:
        setModeRegister(address, value);
        break;
      case 0x0009 ... 0x000e: // mac address
        memory[address] = value;
        break;
      case 0x001A:
        setRXSizes(address, value);
        break;
      case 0x001B:
        setTXSizes(address, value);
        break;
#ifdef U2_UNKNOWN
      default:
        std::cerr << "Set unknown common register: ";
        as_hex(std::cerr, address, 4) << std::endl;
        break;
#endif
    };
  }

  void writeValueAt(const uint16_t address, const uint8_t value)
  {
    switch (address)
    {
      case 0x0000 ... 0x002F:
        writeCommonRegister(address, value);
        break;
      case 0x0400 ... 0x07FF:
        writeSocketRegister(address, value);
        break;
      case 0x4000 ... 0x7FFF:
        memory[address] = value;
        break;
#ifdef U2_UNKNOWN
      default:
        std::cerr << "Write to an unknown location: ";
        as_hex(std::cerr, address, 4) << std::endl;
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
    std::cerr << "Init" << std::endl;
    modeRegister = 0;
    dataAddress = 0;
    sockets.resize(4);
    memory.clear();
    memory.resize(0x8000);

    for (size_t i = 0; i < 4; ++i)
    {
      sockets[i].registers = 0x0400 + (i << 8);
    }

    memory[0x0017] = 0x07;  // RTR
    memory[0x0018] = 0xD0;  // RTR
    setRXSizes(0x001A, 0x55);  // RMSR
    setTXSizes(0x001B, 0x55);  // TMSR
  }

  BYTE u2_C0(WORD programcounter, WORD address, BYTE write, BYTE value, ULONG nCycles)
  {
    for (size_t i = 0; i < 4; ++i)
    {
      receiveOnePacket(i);
    }

  	BYTE res = write ? 0 : MemReadFloatingBus(nCycles);

    const uint8_t loc = address & 0x0F;

    if (write)
    {
      switch (loc)
      {
        case 4:
          setModeRegister(0x0000, value);
          break;
        case 5: // set high. do not accept >= 0x8000
          dataAddress = ((value & 0x7F) << 8) | (dataAddress & 0x00FF);
          break;
        case 6: // set low
          dataAddress = ((value & 0XFF) << 0) | (dataAddress & 0xFF00);
          break;
        case 7:
          writeValue(value);
          break;
      }
    }
    else
    {
      switch (loc)
      {
        case 4:
          res = modeRegister;
          break;
        case 5:
          res = getIByte(dataAddress, 8);  // hi
          break;
        case 6:
          res = getIByte(dataAddress, 0);  // low
          break;
        case 7:
          res = readValue();
          break;
      }
    }

    const char * mode = write ? "WRITE " : "READ  ";

    char c;
    switch (res)
    {
      case 'A'...'Z':
      case 'a'...'z':
      case '0'...'9':
        c = res;
        break;
      default:
        c = '.';
        break;
    }

#ifdef U2_VERBOSE
    std::cerr << std::hex << programcounter << ": ";
    std::cerr << mode << std::hex << address << " ";
    std::cerr << std::setfill('0') << std::setw(2) << std::hex << size_t(value) << " = ";
    std::cerr << std::setfill('0') << std::setw(2) << std::hex << size_t(res) << " " << c << std::endl;
#endif

    return res;
  }

}

void registerUthernet2()
{
  initialise();
  RegisterIoHandler(3, u2_C0, u2_C0, nullptr, nullptr, nullptr, nullptr);
}
