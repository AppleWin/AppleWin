#pragma once

#include "Card.h"
#include "CmdLine.h"

class SNESMAXCard : public Card
{
public:
	SNESMAXCard(UINT slot) :
		Card(CT_SNESMAX),
		m_slot(slot)
	{
		m_buttonIndex = 0;
		m_controller1Buttons = 0;
		m_controller2Buttons = 0;

		m_altControllerType[0] = g_cmdLine.snesMaxAltControllerType[0];
		m_altControllerType[1] = g_cmdLine.snesMaxAltControllerType[1];
	}
	virtual ~SNESMAXCard(void) {}

	virtual void Init(void) {};
	virtual void Reset(const bool powerCycle) {};

	void InitializeIO(LPBYTE pCxRomPeripheral, UINT slot);

	static BYTE __stdcall IORead(WORD pc, WORD addr, BYTE bWrite, BYTE value, ULONG nExecutedCycles);
	static BYTE __stdcall IOWrite(WORD pc, WORD addr, BYTE bWrite, BYTE value, ULONG nExecutedCycles);

	static std::string GetSnapshotCardName(void);
	void SaveSnapshot(class YamlSaveHelper& yamlSaveHelper);
	bool LoadSnapshot(class YamlLoadHelper& yamlLoadHelper, UINT slot, UINT version);

private:
	UINT m_slot;

	UINT m_buttonIndex;
	UINT m_controller1Buttons;
	UINT m_controller2Buttons;

	bool m_altControllerType[2];
};
