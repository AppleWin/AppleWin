#pragma once

#include "Card.h"

class FourPlayCard : public Card
{
public:
	FourPlayCard(UINT slot) :
		Card(CT_FourPlay, slot)
	{
	}
	virtual ~FourPlayCard() {}

	virtual void Destroy() {}
	virtual void Reset(const bool powerCycle) {}
	virtual void Update(const ULONG nExecutedCycles) {}

	virtual void InitializeIO(LPBYTE pCxRomPeripheral);

	static BYTE __stdcall IORead(WORD pc, WORD addr, BYTE bWrite, BYTE value, ULONG nExecutedCycles);

	static const std::string& GetSnapshotCardName();
	virtual void SaveSnapshot(YamlSaveHelper& yamlSaveHelper);
	virtual bool LoadSnapshot(YamlLoadHelper& yamlLoadHelper, UINT version);

private:
	static bool MyGetAsyncKeyState(int vKey);
    static BYTE MakeByte(bool up, bool down, bool left, bool right, bool trigger1, bool trigger2);
};
