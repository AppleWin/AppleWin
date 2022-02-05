#pragma once

#include <cstdint>

#include <memory>

class NetworkBackend
{
public:
	virtual ~NetworkBackend();

	// transmit a packet
	virtual void transmit(
		int txlength,			/* Frame length */
		uint8_t *txframe		/* Pointer to the frame to be transmitted */
	) = 0;

	// receive a single packet
	virtual int receive(uint8_t * data, int * size) = 0;

	// process pending packets
	virtual void update(const ULONG nExecutedCycles) = 0;

	// if the backend is usable
	virtual bool isValid() = 0;

	static std::shared_ptr<NetworkBackend> createBackend();
};
