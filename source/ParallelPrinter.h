#pragma once

void			PrintDestroy();
void			PrintLoadRom(LPBYTE pCxRomPeripheral, UINT uSlot);
void			PrintReset();
void			PrintUpdate(DWORD);
void			Printer_SetFilename(const std::string & pszFilename);
const std::string &	Printer_GetFilename();
void			Printer_SetIdleLimit(unsigned int Duration);
unsigned int	Printer_GetIdleLimit();

std::string Printer_GetSnapshotCardName(void);
void Printer_SaveSnapshot(class YamlSaveHelper& yamlSaveHelper);
bool Printer_LoadSnapshot(class YamlLoadHelper& yamlLoadHelper, UINT slot, UINT version);

extern bool		g_bDumpToPrinter;
extern bool		g_bConvertEncoding;
extern bool		g_bFilterUnprintable;
extern bool		g_bPrinterAppend;
extern bool		g_bEnableDumpToRealPrinter;	// Set by cmd-line: -printer-real
