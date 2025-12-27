#pragma once

#include "Card.h"

class ParallelPrinterCard : public Card
{
public:
	ParallelPrinterCard(UINT slot) :
		Card(CT_GenericPrinter, slot)
	{
		if (m_slot == SLOT0)
			ThrowErrorInvalidSlot();

		m_inactivity = 0;
		m_printerIdleLimit = 10;
		m_file = NULL;

		m_bDumpToPrinter = false;
		m_bConvertEncoding = false;
		m_bFilterUnprintable = false;
		m_bPrinterAppend = false;
		m_bEnableDumpToRealPrinter = false;
	}
	virtual ~ParallelPrinterCard(void) {}

	virtual void Destroy(void);
	virtual void Reset(const bool powerCycle);
	virtual void Update(const ULONG nExecutedCycles);
	virtual void InitializeIO(LPBYTE pCxRomPeripheral);

	static BYTE __stdcall IORead(WORD pc, WORD addr, BYTE bWrite, BYTE value, ULONG nExecutedCycles);
	static BYTE __stdcall IOWrite(WORD pc, WORD addr, BYTE bWrite, BYTE value, ULONG nExecutedCycles);

	static const std::string& GetSnapshotCardName(void);
	virtual void SaveSnapshot(YamlSaveHelper& yamlSaveHelper);
	virtual bool LoadSnapshot(YamlLoadHelper& yamlLoadHelper, UINT version);

	bool operator== (const ParallelPrinterCard& other) const
	{
		return m_szPrintFilename == other.m_szPrintFilename &&
			m_printerIdleLimit == other.m_printerIdleLimit &&
			m_bDumpToPrinter == other.m_bDumpToPrinter &&
			m_bConvertEncoding == other.m_bConvertEncoding &&
			m_bFilterUnprintable == other.m_bFilterUnprintable &&
			m_bPrinterAppend == other.m_bPrinterAppend;
	}

	bool operator!= (const ParallelPrinterCard& other) const
	{
		return !operator==(other);
	}

	const std::string& GetFilename(void);
	void SetFilename(const std::string& prtFilename);
	UINT GetIdleLimit(void);
	void SetIdleLimit(UINT Duration);

	bool GetDumpToPrinter(void) { return m_bDumpToPrinter; }
	void SetDumpToPrinter(bool value) { m_bDumpToPrinter = value; }
	bool GetConvertEncoding(void) { return m_bConvertEncoding; }
	void SetConvertEncoding(bool value) { m_bConvertEncoding = value; }
	bool GetFilterUnprintable(void) { return m_bFilterUnprintable; }
	void SetFilterUnprintable(bool value) { m_bFilterUnprintable = value; }
	bool GetPrinterAppend(void) { return m_bPrinterAppend; }
	void SetPrinterAppend(bool value) { m_bPrinterAppend = value; }
	bool GetEnableDumpToRealPrinter(void) { return m_bEnableDumpToRealPrinter; }
	void SetEnableDumpToRealPrinter(bool value) { m_bEnableDumpToRealPrinter = value; }

	void GetRegistryConfig(void);
	void SetRegistryConfig(void);

private:
	bool CheckPrint(void);
	void ClosePrint(void);

	uint32_t m_inactivity;
	UINT m_printerIdleLimit;
	FILE* m_file;
	std::string m_szPrintFilename;

	bool m_bDumpToPrinter;
	bool m_bConvertEncoding;
	bool m_bFilterUnprintable;
	bool m_bPrinterAppend;
	bool m_bEnableDumpToRealPrinter;	// Set by cmd-line: -printer-real
};
