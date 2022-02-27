#pragma once

#include "NetworkBackend.h"

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
		const int txlength,		/* Frame length */
		uint8_t *txframe		/* Pointer to the frame to be transmitted */
	);

	// receive a single packet, return size (>0) or missing (-1)
	virtual int receive(const int size, uint8_t * rxframe);

	// receive all pending packets (to the queue)
	virtual void update(const ULONG nExecutedCycles);

	// process pending packets
	virtual bool isValid();

	static void tfe_SetRegistryInterface(UINT slot, const std::string& name);
	static void get_disabled_state(int * param);

	/*
	 These functions let the UI enumerate the available interfaces.

	 First, tfe_enumadapter_open() is used to start enumeration.

	 tfe_enum_adapter is then used to gather information for each adapter present
	 on the system, where:

	   ppname points to a pointer which will hold the name of the interface
	   ppdescription points to a pointer which will hold the description of the interface

	   For each of these parameters, new memory is allocated, so it has to be
	   freed with lib_free().

	 tfe_enumadapter_close() must be used to stop processing.

	 Each function returns 1 on success, and 0 on failure.
	 tfe_enumadapter() only fails if there is no more adpater; in this case,
	   *ppname and *ppdescription are not altered.
	*/

	static int tfe_enumadapter_open(void);
	static int tfe_enumadapter(char **ppname, char **ppdescription);
	static int tfe_enumadapter_close(void);

	static std::string tfe_interface;

private:
	pcap_t * tfePcapFP;
};
