#pragma once

#include "Card.h"

class FourPlayCard : public Card
{
public:
	FourPlayCard(UINT slot) :
		Card(CT_FourPlay, slot)
	{
	}
	virtual ~FourPlayCard(void) {}

	virtual void Destroy(void) {}
	virtual void Reset(const bool powerCycle) {}
	virtual void Update(const ULONG nExecutedCycles) {}

	virtual void InitializeIO(LPBYTE pCxRomPeripheral);

	static BYTE __stdcall IORead(WORD pc, WORD addr, BYTE bWrite, BYTE value, ULONG nExecutedCycles);

	static const std::string& GetSnapshotCardName(void);
	virtual void SaveSnapshot(YamlSaveHelper& yamlSaveHelper);
	virtual bool LoadSnapshot(YamlLoadHelper& yamlLoadHelper, UINT version);

	static const UINT JOYSTICKSTATIONARY = 0x20;

private:
	static BYTE MyGetAsyncKeyState(int vKey);
};
