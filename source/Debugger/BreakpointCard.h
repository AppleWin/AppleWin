#pragma once

#include "Card.h"
#include "Interface.h"
#include "SynchronousEventManager.h"

enum BreakType_t { BPTYPE_UNKNOWN, BPTYPE_PC, BPTYPE_MEM, BPTYPE_DMA };
enum BreakAccess_t { BPACCESS_R, BPACCESS_W, BPACCESS_RW };

struct INTERCEPTBREAKPOINT
{
	INTERCEPTBREAKPOINT()
	{
		type = BPTYPE_UNKNOWN;
		addrStart = 0x0000;
		addrEnd = 0x0000;
		access = BPACCESS_R;
	}

	void Set(uint8_t type_, uint16_t addrStart_, uint8_t access_)
	{
		type = type_;
		addrStart = addrStart_;
		access = access_;
	}

	void SetDMA(uint16_t addrStart_, uint16_t addrEnd_, uint8_t access_)
	{
		type = BPTYPE_DMA;
		addrStart = addrStart_;
		addrEnd = addrEnd_;
		access = access_;
	}

	uint8_t  type;
	uint16_t addrStart;
	uint16_t addrEnd;
	uint8_t  access;
};


class BreakpointCard : public Card
{
public:
	BreakpointCard(UINT slot);
	virtual ~BreakpointCard(void);

	void ResetState()
	{
		m_BPSetIdx = 0;
		m_status = 0;
		while (!m_BP_FIFO.empty())
			m_BP_FIFO.pop();
	}

	virtual void Destroy(void) {}
	virtual void Reset(const bool powerCycle);
	virtual void Update(const ULONG nExecutedCycles) {}
	virtual void InitializeIO(LPBYTE pCxRomPeripheral);

	static BYTE __stdcall IORead(WORD pc, WORD addr, BYTE bWrite, BYTE value, ULONG nExecutedCycles);
	static BYTE __stdcall IOWrite(WORD pc, WORD addr, BYTE bWrite, BYTE value, ULONG nExecutedCycles);

	static void CbFunction(uint8_t slot, INTERCEPTBREAKPOINT interceptBreakpoint);

	virtual void SaveSnapshot(YamlSaveHelper& yamlSaveHelper) {}
	virtual bool LoadSnapshot(YamlLoadHelper& yamlLoadHelper, UINT version) { return false; }

private:
	static int SyncEventCallback(int id, int cycles, ULONG uExecutedCycles);
	void Deferred(uint8_t type, uint16_t addrStart, uint16_t addrEnd, uint8_t access);

	static const uint8_t kMatch    = 1 << 7;
	static const uint8_t kMismatch = 1 << 6;
	static const uint8_t kFull	   = 1 << 1;
	static const uint8_t kEmpty	   = 1 << 0;
	uint8_t m_status;

	bool m_interceptBPByCard;

	static const uint8_t kNumParams = 6;	// sizeof(packed(INTERCEPTBREAKPOINT))
	BYTE m_BPSet[kNumParams];
	BYTE m_BPSetIdx;

	static const uint8_t kFIFO_SIZE = 32;	// needs to be big enough for each test's set of BPs
	std::queue<INTERCEPTBREAKPOINT> m_BP_FIFO;

	INTERCEPTBREAKPOINT m_deferred;
	SyncEvent m_syncEvent;
};
