#pragma once

#include "Card.h"

#include <vector>
#include <map>

class NetworkBackend;
struct MACAddress;

struct Socket
{
#ifdef _MSC_VER
    typedef SOCKET socket_t;
#else
    typedef int socket_t;
#endif

    uint16_t transmitBase;
    uint16_t transmitSize;
    uint16_t receiveBase;
    uint16_t receiveSize;
    uint16_t registerAddress;

    uint16_t sn_rx_wr;
    uint16_t sn_rx_rsr;

    bool isOpen() const;
    void clearFD();
    void setStatus(const uint8_t status);
    void setFD(const socket_t fd, const uint8_t status);
    void process();

    socket_t getFD() const;
    uint8_t getStatus() const;
    uint8_t getHeaderSize() const;

    // both functions work in "data" space, the header size is added internally
    bool isThereRoomFor(const size_t len) const;
    uint16_t getFreeRoom() const;

    void SaveSnapshot(YamlSaveHelper &yamlSaveHelper);
    bool LoadSnapshot(YamlLoadHelper &yamlLoadHelper);

    Socket();

    ~Socket();

private:
    socket_t myFD;
    uint8_t mySocketStatus;  // aka W5100_SN_SR
    uint8_t myHeaderSize;
};

/*
* Documentation from
*   http://dserver.macgui.com/Uthernet%20II%20manual%2017%20Nov%2018.pdf
*   https://www.wiznet.io/wp-content/uploads/wiznethome/Chip/W5100/Document/W5100_DS_V128E.pdf
*   https://www.wiznet.io/wp-content/uploads/wiznethome/Chip/W5100/Document/3150Aplus_5100_ES_V260E.pdf
*/

class Uthernet2 : public Card
{
public:
    static const std::string& GetSnapshotCardName();

    enum PacketDestination { HOST, BROADCAST, OTHER };

    Uthernet2(UINT slot);
    virtual ~Uthernet2();

	virtual void Destroy(void) {}
    virtual void InitializeIO(LPBYTE pCxRomPeripheral);
    virtual void Reset(const bool powerCycle);
    virtual void Update(const ULONG nExecutedCycles);
    virtual void SaveSnapshot(YamlSaveHelper &yamlSaveHelper);
    virtual bool LoadSnapshot(YamlLoadHelper &yamlLoadHelper, UINT version);

    BYTE IO_C0(WORD programcounter, WORD address, BYTE write, BYTE value, ULONG nCycles);

    // global registry functions
    static void SetRegistryVirtualDNS(UINT slot, const bool enabled);
    static bool GetRegistryVirtualDNS(UINT slot);

private:
    bool myVirtualDNSEnabled; // extended virtualisation of DNS (not present in the real U II card)

#ifdef _MSC_VER
    int myWSAStartup;
#endif

    std::vector<uint8_t> myMemory;
    std::vector<Socket> mySockets;
    uint8_t myModeRegister;
    uint16_t myDataAddress;
    std::shared_ptr<NetworkBackend> myNetworkBackend;

    // the real Uthernet II card does not have a ARP Cache
    // but in the interest of speeding up the emulator
    // we introduce one
    std::map<uint32_t, MACAddress> myARPCache;
    std::map<std::string, uint32_t> myDNSCache;

    void getMACAddress(const uint32_t address, const MACAddress * & mac);
    void resolveDNS(const size_t i);

    void setSocketModeRegister(const size_t i, const uint16_t address, const uint8_t value);
    void setTXSizes(const uint16_t address, uint8_t value);
    void setRXSizes(const uint16_t address, uint8_t value);
    uint16_t getTXDataSize(const size_t i) const;
    uint8_t getTXFreeSizeRegister(const size_t i, const size_t shift) const;
    uint8_t getRXDataSizeRegister(const size_t i, const size_t shift) const;

    void receiveOnePacketRaw();
    void receiveOnePacketIPRaw(const size_t i, const size_t lengthOfPayload, const uint8_t * payload, const uint32_t source, const uint8_t protocol, const int len);
    void receiveOnePacketMacRaw(const size_t i, const int size, uint8_t * data);
    void receiveOnePacketFromSocket(const size_t i);
    void receiveOnePacket(const size_t i);
    int receiveForMacAddress(const bool acceptAll, const int size, uint8_t * data, PacketDestination & packetDestination);

    void sendDataIPRaw(const size_t i, std::vector<uint8_t> &data);
    void sendDataMacRaw(const size_t i, std::vector<uint8_t> &data) const;
    void sendDataToSocket(const size_t i, std::vector<uint8_t> &data);
    void sendData(const size_t i);

    void resetRXTXBuffers(const size_t i);
    void updateRSR(const size_t i);

    void openSystemSocket(const size_t i, const int type, const int protocol, const int status);
    void openSocket(const size_t i);
    void closeSocket(const size_t i);
    void connectSocket(const size_t i);

    void setCommandRegister(const size_t i, const uint8_t value);

    uint8_t readSocketRegister(const uint16_t address);
    uint8_t readValueAt(const uint16_t address);

    void autoIncrement();
    uint8_t readValue();

    void setIPProtocol(const size_t i, const uint16_t address, const uint8_t value);
    void setIPTypeOfService(const size_t i, const uint16_t address, const uint8_t value);
    void setIPTTL(const size_t i, const uint16_t address, const uint8_t value);
    void writeSocketRegister(const uint16_t address, const uint8_t value);

    void setModeRegister(const uint16_t address, const uint8_t value);
    void writeCommonRegister(const uint16_t address, const uint8_t value);
    void writeValueAt(const uint16_t address, const uint8_t value);
    void writeValue(const uint8_t value);
};
