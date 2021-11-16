#pragma once

#include "Card.h"

class VidHDCard : public Card
{
public:
	VidHDCard(UINT slot) :
		Card(CT_VidHD, slot)
	{
	}
	virtual ~VidHDCard(void) {}

	virtual void Init(void) {}
	virtual void Reset(const bool powerCycle) {}
	virtual void Update(const ULONG nExecutedCycles) {}

	virtual void InitializeIO(LPBYTE pCxRomPeripheral);

	static BYTE __stdcall IORead(WORD pc, WORD addr, BYTE bWrite, BYTE value, ULONG nExecutedCycles);

	static std::string GetSnapshotCardName(void);
	void SaveSnapshot(class YamlSaveHelper& yamlSaveHelper);
	bool LoadSnapshot(class YamlLoadHelper& yamlLoadHelper, UINT slot, UINT version);

private:
	// to do
};
