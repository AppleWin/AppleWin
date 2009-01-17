#pragma once

void			PrintDestroy();
void			PrintLoadRom(LPBYTE pCxRomPeripheral, UINT uSlot);
void			PrintReset();
void			PrintUpdate(DWORD);
void			Printer_SetFilename(char* pszFilename);
char*			Printer_GetFilename();
void			Printer_SetIdleLimit(unsigned int Duration);
unsigned int	Printer_GetIdleLimit();

extern bool		g_bDumpToPrinter;
extern bool		g_bConvertEncoding;
extern bool		g_bFilterUnprintable;
extern bool		g_bPrinterAppend;
extern int		g_iPrinterIdleLimit;
extern bool		g_bFilterUnprintable;
extern bool		g_bPrinterAppend;
extern bool		g_bEnableDumpToRealPrinter;	// Set by cmd-line: -printer-real
