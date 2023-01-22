#pragma once

#include "NetworkBackend.h"

#include <string>

struct pcap;
typedef struct pcap pcap_t;

class PCapBackend : public NetworkBackend
{
public:
	PCapBackend(const std::string & interfaceName);

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

	// get MAC for IPRAW (it is only supposed to handle addresses on the local network)
	virtual void getMACAddress(const uint32_t address, MACAddress & mac);

	// get interface name
	virtual const std::string & getInterfaceName();

	// global registry functions
	static void SetRegistryInterface(UINT slot, const std::string& name);
	static std::string GetRegistryInterface(UINT slot);

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
	static int tfe_enumadapter(std::string & name, std::string & description);
	static int tfe_enumadapter_close(void);
	static const char * tfe_lib_version(void);
	static int tfe_is_npcap_loaded();

private:
	const std::string m_interfaceName;
	pcap_t * m_tfePcapFP;
};
