#pragma once

#include "Card.h"
#include "Interface.h"
#include "TMS5220.h"

class EchoII : public Card
{
public:
	EchoII(UINT slot) :
		Card(CT_EchoII, slot), m_tms5220(slot)
	{
		m_lastCumulativeCycle = 0;
	}
	virtual ~EchoII()
	{
	}

	virtual void Destroy() {}
	virtual void Reset(const bool powerCycle);
	virtual void Update(const ULONG nExecutedCycles);
	virtual void InitializeIO(LPBYTE pCxRomPeripheral);
	virtual void SaveSnapshot(YamlSaveHelper& yamlSaveHelper);
	virtual bool LoadSnapshot(YamlLoadHelper& yamlLoadHelper, UINT version);

	static BYTE __stdcall IORead(WORD pc, WORD addr, BYTE bWrite, BYTE value, ULONG nExecutedCycles);
	static BYTE __stdcall IOWrite(WORD pc, WORD addr, BYTE bWrite, BYTE value, ULONG nExecutedCycles);

	void MuteControl(bool mute);
	//void UpdateCycles(ULONG executedCycles);
	void SetCumulativeCycles();
#ifdef _DEBUG
	void CheckCumulativeCycles();
#endif

	UINT64 GetLastCumulativeCycles();

	static const std::string& GetSnapshotCardName();

private:
	TMS5220 m_tms5220;

	UINT64 m_lastCumulativeCycle;
};
