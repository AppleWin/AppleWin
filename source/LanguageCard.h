#pragma once

#include "Card.h"

//
// Language Card (base unit) for Apple //e and above
//

class LanguageCardUnit : public Card
{
public:
	LanguageCardUnit(SS_CARDTYPE type = CT_LanguageCardIIe);
	virtual ~LanguageCardUnit(void);

	virtual void Init(void) {};
	virtual void Reset(const bool powerCycle) {};

	virtual void InitializeIO(void);
	virtual void SetMemorySize(UINT banks) {}		// No-op for //e and slot-0 16K LC
	virtual UINT GetActiveBank(void) { return 0; }	// Always 0 as only 1x 16K bank
	virtual void SaveSnapshot(class YamlSaveHelper& yamlSaveHelper) { _ASSERT(0); } // Not used for //e
	virtual bool LoadSnapshot(class YamlLoadHelper& yamlLoadHelper, UINT slot, UINT version) { _ASSERT(0); return false; } // Not used for //e

	BOOL GetLastRamWrite(void) { return m_uLastRamWrite; }
	void SetLastRamWrite(BOOL count) { m_uLastRamWrite = count; }
	SS_CARDTYPE GetMemoryType(void) { return QueryType(); }
	bool IsOpcodeRMWabs(WORD addr);

	static BYTE __stdcall IO(WORD PC, WORD uAddr, BYTE bWrite, BYTE uValue, ULONG nExecutedCycles);

	static const UINT kMemModeInitialState;
	static const UINT kSlot0 = 0;

private:
	UINT m_uLastRamWrite;
};

//
// Language Card (slot-0) for Apple II or II+
//

class LanguageCardSlot0 : public LanguageCardUnit
{
public:
	LanguageCardSlot0(SS_CARDTYPE = CT_LanguageCard);
	virtual ~LanguageCardSlot0(void);

	virtual void SaveSnapshot(class YamlSaveHelper& yamlSaveHelper);
	virtual bool LoadSnapshot(class YamlLoadHelper& yamlLoadHelper, UINT slot, UINT version);

	static const UINT kMemBankSize = 16*1024;
	static std::string GetSnapshotCardName(void);

protected:
	void SaveLCState(class YamlSaveHelper& yamlSaveHelper);
	void LoadLCState(class YamlLoadHelper& yamlLoadHelper);

	LPBYTE m_pMemory;

private:
	std::string GetSnapshotMemStructName(void);
};

//
// Saturn 128K
//

class Saturn128K : public LanguageCardSlot0
{
public:
	Saturn128K(UINT banks);
	virtual ~Saturn128K(void);

	virtual void InitializeIO(void);
	virtual void SetMemorySize(UINT banks);
	virtual UINT GetActiveBank(void);
	virtual void SaveSnapshot(class YamlSaveHelper& yamlSaveHelper);
	virtual bool LoadSnapshot(class YamlLoadHelper& yamlLoadHelper, UINT slot, UINT version);

	static BYTE __stdcall IO(WORD PC, WORD uAddr, BYTE bWrite, BYTE uValue, ULONG nExecutedCycles);

	// "The boards consist of 16K banks of memory (4 banks for the 64K board, 8 banks for the 128K), accessed one at a time" - Ref: "64K/128K RAM BOARD", Saturn Systems, Ch.1 Introduction(pg-5)
	static const UINT kMaxSaturnBanks = 8;		// 8 * 16K = 128K
	static std::string GetSnapshotCardName(void);

private:
	std::string GetSnapshotMemStructName(void);

	UINT m_uSaturnTotalBanks;	// Will be > 0 if Saturn card is installed
	UINT m_uSaturnActiveBank;	// Saturn 128K Language Card Bank 0 .. 7
	LPBYTE m_aSaturnBanks[kMaxSaturnBanks];
};
