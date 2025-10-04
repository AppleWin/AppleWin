#pragma once

#include "Card.h"
#include "Interface.h"

class BreakpointCard : public Card
{
public:
	BreakpointCard(UINT slot) :
		Card(CT_BreakpointCard, slot)
	{
		Reset(true);
	}
	virtual ~BreakpointCard(void)
	{
	}

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

	static void CbFunction(uint8_t slot, uint8_t type, uint16_t addrStart, uint16_t addrEnd, uint8_t access);

	virtual void SaveSnapshot(YamlSaveHelper& yamlSaveHelper) {}
	virtual bool LoadSnapshot(YamlLoadHelper& yamlLoadHelper, UINT version) { return false; }

private:
	static const uint8_t kMatch    = 1 << 7;
	static const uint8_t kMismatch = 1 << 6;
	static const uint8_t kFull	   = 1 << 1;
	static const uint8_t kEmpty	   = 1 << 0;
	uint8_t m_status;

	bool m_interceptBPByCard;

	struct BPSet
	{
		uint8_t type;
		uint16_t addrStart;
		uint16_t addrEnd;
		uint8_t access;
	};

	static const uint8_t kNumParams = 7;
	BYTE m_BPSet[kNumParams];
	BYTE m_BPSetIdx;

	static const uint8_t kFIFO_SIZE = 32;	// arbitrary size
	std::queue<BPSet> m_BP_FIFO;
};
