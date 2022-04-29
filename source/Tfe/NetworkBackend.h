#pragma once

#define MAX_TXLENGTH 1518
#define MIN_TXLENGTH 4

#define MAX_RXLENGTH 1518
#define MIN_RXLENGTH 64

#pragma pack(push)
#pragma pack(1) // Ensure struct is packed
struct MACAddress
{
	uint8_t address[6];
};
#pragma pack(pop)

class NetworkBackend
{
public:
	virtual ~NetworkBackend();

	// transmit a packet
	virtual void transmit(
		const int txlength,		/* Frame length */
		uint8_t *txframe		/* Pointer to the frame to be transmitted */
	) = 0;

	// receive a single packet, return size (>0) or missing (-1)
	virtual int receive(
		const int size,			/* Buffer size */
		uint8_t * rxframe		/* Pointer to the buffer */
	) = 0;

	// process pending packets
	virtual void update(const ULONG nExecutedCycles) = 0;

	// get MAC for IPRAW (it is only supposed to handle addresses on the local network)
	virtual void getMACAddress(const uint32_t address, MACAddress & mac) = 0;

	// if the backend is usable
	virtual bool isValid() = 0;

	// get interface name
	virtual const std::string & getInterfaceName() = 0;
};
