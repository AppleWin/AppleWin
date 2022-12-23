#pragma once

#include "Card.h"
#include "CmdLine.h"

class SNESMAXCard : public Card
{
public:
	SNESMAXCard(UINT slot) :
		Card(CT_SNESMAX, slot)
	{
		m_buttonIndex = 0;
		m_controller1Buttons = 0;
		m_controller2Buttons = 0;

		m_altControllerType[0] = g_cmdLine.snesMaxAltControllerType[0];
		m_altControllerType[1] = g_cmdLine.snesMaxAltControllerType[1];
	}
	virtual ~SNESMAXCard(void) {}

	virtual void Destroy(void) {}
	virtual void Reset(const bool powerCycle) {}
	virtual void Update(const ULONG nExecutedCycles) {}

	virtual void InitializeIO(LPBYTE pCxRomPeripheral);

	static BYTE __stdcall IORead(WORD pc, WORD addr, BYTE bWrite, BYTE value, ULONG nExecutedCycles);
	static BYTE __stdcall IOWrite(WORD pc, WORD addr, BYTE bWrite, BYTE value, ULONG nExecutedCycles);

	static const std::string& GetSnapshotCardName(void);
	virtual void SaveSnapshot(YamlSaveHelper& yamlSaveHelper);
	virtual bool LoadSnapshot(YamlLoadHelper& yamlLoadHelper, UINT version);

	static bool ParseControllerMappingFile(UINT joyNum, const char* pathname, std::string& errorMsg);

private:
	UINT GetControllerButtons(UINT joyNum, JOYINFOEX& infoEx, bool altControllerType);

	enum Button { B, Y, SELECT, START, U, D, L, R, A, X, LB, RB, UNUSED1, UNUSED2, UNUSED3, UNUSED4, NUM_BUTTONS, UNUSED };

	UINT m_buttonIndex;
	UINT m_controller1Buttons;
	UINT m_controller2Buttons;

	bool m_altControllerType[2];
	static UINT m_altControllerButtons[2][NUM_BUTTONS];
};
