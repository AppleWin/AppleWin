#pragma once

//
// Language Card
//

enum MemoryType_e;

class LanguageCard
{
public:
	LanguageCard(void);
	virtual ~LanguageCard(void){}

	virtual void Initialize(void);
	virtual void Destroy(void);
	virtual DWORD SetPaging(WORD address, DWORD memmode)
	{
		return 0;	// TODO
	}

	virtual void SetMemorySize(UINT banks)
	{
	}
	virtual UINT GetActiveBank(void)
	{
		return 0;	// Always 0 as only 1x 16K bank
	}

	virtual void SaveSnapshot(class YamlSaveHelper& yamlSaveHelper);
	virtual bool LoadSnapshot(class YamlLoadHelper& yamlLoadHelper, UINT slot, UINT version);

	MemoryType_e type;
	static const UINT kMemBankSize = 16*1024;
	static std::string GetSnapshotCardName(void);

protected:
	void SaveLCState(class YamlSaveHelper& yamlSaveHelper);

private:
	std::string GetSnapshotMemStructName(void);

	LPBYTE m_pMemory;
};

//
// Saturn 128K
//

class Saturn128K : public LanguageCard
{
public:
	Saturn128K(void);
	virtual ~Saturn128K(void){}

	virtual void Initialize(void);
	virtual void Destroy(void);
	virtual DWORD SetPaging(WORD address, DWORD memmode);

	virtual void SetMemorySize(UINT banks);
	virtual UINT GetActiveBank(void);

	virtual void SaveSnapshot(class YamlSaveHelper& yamlSaveHelper);
	virtual bool LoadSnapshot(class YamlLoadHelper& yamlLoadHelper, UINT slot, UINT version);

	static const UINT kMaxSaturnBanks = 8;		// 8 * 16K = 128K
	static std::string GetSnapshotCardName(void);

private:
	std::string GetSnapshotMemStructName(void);

	UINT m_uSaturnTotalBanks;	// Will be > 0 if Saturn card is installed
	UINT m_uSaturnActiveBank;	// Saturn 128K Language Card Bank 0 .. 7
	LPBYTE m_aSaturnBanks[kMaxSaturnBanks];
};
