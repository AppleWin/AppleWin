#pragma once

#include "Backend.h"

#include <string>

struct pcap;
typedef struct pcap pcap_t;

class PCapBackend : public NetworkBackend
{
public:
	PCapBackend(const std::string & pcapInterface);

	virtual ~PCapBackend();

	// transmit a packet
	virtual void transmit(
		int txlength,			/* Frame length */
		uint8_t *txframe		/* Pointer to the frame to be transmitted */
	);

	// receive a single packet
	virtual int receive(uint8_t * data, int * size);

	// receive all pending packets (to the queue)
	virtual void update(const ULONG nExecutedCycles);

	// process pending packets
	virtual bool isValid();

	static std::string tfe_interface;

private:
	pcap_t * tfePcapFP;
};
