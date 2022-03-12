#pragma once

#include "Card.h"

//
// Language Card (base unit) for Apple //e and above
//

class LanguageCardUnit : public Card
{
public:
	// in modern C++ this could be a 2nd constructor
	static LanguageCardUnit * create(UINT slot);

	virtual ~LanguageCardUnit(void);

	virtual void Destroy(void) {}
	virtual void Reset(const bool powerCycle) {}
	virtual void Update(const ULONG nExecutedCycles) {}


	virtual void InitializeIO(LPBYTE pCxRomPeripheral);
	virtual UINT GetActiveBank(void) { return 0; }	// Always 0 as only 1x 16K bank
	virtual void SaveSnapshot(YamlSaveHelper& yamlSaveHelper) { } // A no-op for //e - called from CardManager::SaveSnapshot()
	virtual bool LoadSnapshot(YamlLoadHelper& yamlLoadHelper, UINT version) { _ASSERT(0); return false; } // Not used for //e

	BOOL GetLastRamWrite(void) { return m_uLastRamWrite; }
	void SetLastRamWrite(BOOL count) { m_uLastRamWrite = count; }
	SS_CARDTYPE GetMemoryType(void) { return QueryType(); }
	bool IsOpcodeRMWabs(WORD addr);

	static BYTE __stdcall IO(WORD PC, WORD uAddr, BYTE bWrite, BYTE uValue, ULONG nExecutedCycles);

	static const UINT kMemModeInitialState;
	static const UINT kSlot0 = SLOT0;

protected:
	LanguageCardUnit(SS_CARDTYPE type, UINT slot);

private:
	UINT m_uLastRamWrite;
};

//
// Language Card (slot-0) for Apple II or II+
//

class LanguageCardSlot0 : public LanguageCardUnit
{
public:
	// in modern C++ this could be a 2nd constructor
	static LanguageCardSlot0 * create(UINT slot);

	virtual ~LanguageCardSlot0(void);

	virtual void SaveSnapshot(YamlSaveHelper& yamlSaveHelper);
	virtual bool LoadSnapshot(YamlLoadHelper& yamlLoadHelper, UINT version);

	static const UINT kMemBankSize = 16*1024;
	static const std::string& GetSnapshotCardName(void);

protected:
	LanguageCardSlot0(SS_CARDTYPE type, UINT slot);
	void SaveLCState(class YamlSaveHelper& yamlSaveHelper);
	void LoadLCState(class YamlLoadHelper& yamlLoadHelper);

	LPBYTE m_pMemory;

private:
	const std::string& GetSnapshotMemStructName(void);
};

//
// Saturn 128K
//

class Saturn128K : public LanguageCardSlot0
{
public:
	Saturn128K(UINT slot, UINT banks);
	virtual ~Saturn128K(void);

	virtual void InitializeIO(LPBYTE pCxRomPeripheral);
	virtual UINT GetActiveBank(void);
	virtual void SaveSnapshot(YamlSaveHelper& yamlSaveHelper);
	virtual bool LoadSnapshot(YamlLoadHelper& yamlLoadHelper, UINT version);

	static UINT	GetSaturnMemorySize();
	static void	SetSaturnMemorySize(UINT banks);

	static BYTE __stdcall IO(WORD PC, WORD uAddr, BYTE bWrite, BYTE uValue, ULONG nExecutedCycles);

	// "The boards consist of 16K banks of memory (4 banks for the 64K board, 8 banks for the 128K), accessed one at a time" - Ref: "64K/128K RAM BOARD", Saturn Systems, Ch.1 Introduction(pg-5)
	static const UINT kMaxSaturnBanks = 8;		// 8 * 16K = 128K
	static const std::string& GetSnapshotCardName(void);

private:
	const std::string& GetSnapshotMemStructName(void);

	static UINT g_uSaturnBanksFromCmdLine;

	UINT m_uSaturnTotalBanks;	// Will be > 0 if Saturn card is installed
	UINT m_uSaturnActiveBank;	// Saturn 128K Language Card Bank 0 .. 7
	LPBYTE m_aSaturnBanks[kMaxSaturnBanks];
};
