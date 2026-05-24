#pragma once

#include "Card.h"

class Z80Card : public Card
{
public:
	Z80Card(UINT slot) :
		Card(CT_Z80, slot)
	{
	}
	virtual ~Z80Card() {}

	virtual void Destroy() {}
	virtual void Reset(const bool powerCycle) {}
	virtual void Update(const ULONG nExecutedCycles) {}

	virtual void InitializeIO(LPBYTE pCxRomPeripheral);

	static BYTE __stdcall IOWrite(WORD pc, WORD addr, BYTE bWrite, BYTE value, ULONG nExecutedCycles);

	static const std::string& GetSnapshotCardNameOld();
	static const std::string& GetSnapshotCardName();
	virtual void SaveSnapshot(YamlSaveHelper& yamlSaveHelper);
	virtual bool LoadSnapshot(YamlLoadHelper& yamlLoadHelper, UINT version);
};
