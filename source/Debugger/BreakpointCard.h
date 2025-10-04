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
		m_status = kEmpty;
		m_BP_FIFO.clear();
	}

	virtual void Destroy(void) {}
	virtual void Reset(const bool powerCycle);
	virtual void Update(const ULONG nExecutedCycles) {}
	virtual void InitializeIO(LPBYTE pCxRomPeripheral);

	static BYTE __stdcall IORead(WORD pc, WORD addr, BYTE bWrite, BYTE value, ULONG nExecutedCycles);
	static BYTE __stdcall IOWrite(WORD pc, WORD addr, BYTE bWrite, BYTE value, ULONG nExecutedCycles);

	static void CbFunction(void);

	virtual void SaveSnapshot(YamlSaveHelper& yamlSaveHelper) {}
	virtual bool LoadSnapshot(YamlLoadHelper& yamlLoadHelper, UINT version) { return false; }

private:
	static const uint8_t kFull	= 1 << 1;
	static const uint8_t kEmpty	= 1 << 0;
	uint8_t m_status;

	bool m_interceptBPByCard;

	enum RW_t { _R = 0, _W = 1, _RW = 2 };
	struct BPSet
	{
		uint8_t slot;
		uint16_t bank;
		uint8_t langCard;
		uint16_t addr;
		uint8_t rw;
	};

	static const uint8_t kNumParams = 7;
	BYTE m_BPSet[kNumParams];
	BYTE m_BPSetIdx;

	static const uint8_t kFIFO_SIZE = 32;	// arbitrary size
	std::vector<BPSet> m_BP_FIFO;
};
