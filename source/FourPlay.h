#pragma once

#include "Card.h"

class FourPlayCard : public Card
{
public:
	FourPlayCard(UINT slot) :
		Card(CT_FourPlay),
		m_slot(slot)
	{
	}
	virtual ~FourPlayCard(void) {}

	virtual void Init(void) {};
	virtual void Reset(const bool powerCycle) {};

	void InitializeIO(LPBYTE pCxRomPeripheral, UINT slot);

	static BYTE __stdcall IORead(WORD pc, WORD addr, BYTE bWrite, BYTE value, ULONG nExecutedCycles);

	static std::string GetSnapshotCardName(void);
	void SaveSnapshot(class YamlSaveHelper& yamlSaveHelper);
	bool LoadSnapshot(class YamlLoadHelper& yamlLoadHelper, UINT slot, UINT version);

	static const UINT JOYSTICKSTATIONARY = 0x20;

private:
	static BYTE MyGetAsyncKeyState(int vKey);

	UINT m_slot;
};
