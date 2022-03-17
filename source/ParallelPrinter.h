#pragma once

#include "Card.h"
#include "Memory.h"

class ParallelPrinterCard : public Card
{
public:
	ParallelPrinterCard(UINT slot) :
		Card(CT_GenericPrinter, slot)
	{
		inactivity = 0;
		g_PrinterIdleLimit = 10;
		file = NULL;

		g_bDumpToPrinter = false;
		g_bConvertEncoding = false;
		g_bFilterUnprintable = false;
		g_bPrinterAppend = false;
		g_bEnableDumpToRealPrinter = false;
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

	const std::string& GetFilename(void);
	void SetFilename(const std::string& prtFilename);
	UINT GetIdleLimit(void);
	void SetIdleLimit(UINT Duration);

	bool GetDumpToPrinter(void) { return g_bDumpToPrinter; }
	void SetDumpToPrinter(bool value) { g_bDumpToPrinter = value; }
	bool GetConvertEncoding(void) { return g_bConvertEncoding; }
	void SetConvertEncoding(bool value) { g_bConvertEncoding = value; }
	bool GetFilterUnprintable(void) { return g_bFilterUnprintable; }
	void SetFilterUnprintable(bool value) { g_bFilterUnprintable = value; }
	bool GetPrinterAppend(void) { return g_bPrinterAppend; }
	void SetPrinterAppend(bool value) { g_bPrinterAppend = value; }
	bool GetEnableDumpToRealPrinter(void) { return g_bEnableDumpToRealPrinter; }
	void SetEnableDumpToRealPrinter(bool value) { g_bEnableDumpToRealPrinter = value; }

	void SetRegistryConfig(void);

private:
	bool CheckPrint(void);
	void ClosePrint(void);

	DWORD inactivity;
	UINT g_PrinterIdleLimit;
	FILE* file;
	static const DWORD PRINTDRVR_SIZE = APPLE_SLOT_SIZE;
	std::string g_szPrintFilename;

	bool g_bDumpToPrinter;
	bool g_bConvertEncoding;
	bool g_bFilterUnprintable;
	bool g_bPrinterAppend;
	bool g_bEnableDumpToRealPrinter;	// Set by cmd-line: -printer-real
};
