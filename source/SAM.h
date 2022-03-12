#pragma once

#include "Card.h"

class SAMCard : public Card
{
public:
	SAMCard(UINT slot) :
		Card(CT_SAM, slot)
	{
	}
	virtual ~SAMCard(void) {}

	virtual void Destroy(void) {}
	virtual void Reset(const bool powerCycle) {}
	virtual void Update(const ULONG nExecutedCycles) {}

	virtual void InitializeIO(LPBYTE pCxRomPeripheral);

	static BYTE __stdcall IOWrite(WORD pc, WORD addr, BYTE bWrite, BYTE value, ULONG nExecutedCycles);

	static const std::string& GetSnapshotCardName(void);
	virtual void SaveSnapshot(YamlSaveHelper& yamlSaveHelper);
	virtual bool LoadSnapshot(YamlLoadHelper& yamlLoadHelper, UINT version);

private:
	// no state
};
