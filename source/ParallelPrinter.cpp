/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 1994-1996, Michael O'Brien
Copyright (C) 1999-2001, Oliver Schmidt
Copyright (C) 2002-2005, Tom Charlesworth
Copyright (C) 2006-2007, Tom Charlesworth, Michael Pohoreski, Nick Westgate

AppleWin is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

AppleWin is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with AppleWin; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/* Description: Parallel Printer Interface Card emulation
 *
 * Author: Nick Westgate, Stannev
 */

#include "StdAfx.h"

#include "AppleWin.h"
#include "Memory.h"
#include "ParallelPrinter.h"
#include "Registry.h"
#include "YamlHelper.h"

#include "..\resource\resource.h"

static DWORD inactivity = 0;
static int g_iPrinterIdleLimit = 10;
static FILE* file = NULL;
DWORD const PRINTDRVR_SIZE = APPLE_SLOT_SIZE;
#define DEFAULT_PRINT_FILENAME "Printer.txt"
static char g_szPrintFilename[MAX_PATH] = {0};
bool g_bDumpToPrinter = false;
bool g_bConvertEncoding = true;
bool g_bFilterUnprintable = true;
bool g_bPrinterAppend = false;
bool g_bEnableDumpToRealPrinter = false;

static UINT g_uSlot = 0;

//===========================================================================

static BYTE __stdcall PrintStatus(WORD, WORD, BYTE, BYTE, ULONG);
static BYTE __stdcall PrintTransmit(WORD, WORD, BYTE, BYTE value, ULONG);




VOID PrintLoadRom(LPBYTE pCxRomPeripheral, const UINT uSlot)
{
	HRSRC hResInfo = FindResource(NULL, MAKEINTRESOURCE(IDR_PRINTDRVR_FW), "FIRMWARE");
	if(hResInfo == NULL)
		return;

	DWORD dwResSize = SizeofResource(NULL, hResInfo);
	if(dwResSize != PRINTDRVR_SIZE)
		return;

	HGLOBAL hResData = LoadResource(NULL, hResInfo);
	if(hResData == NULL)
		return;

	BYTE* pData = (BYTE*) LockResource(hResData);	// NB. Don't need to unlock resource
	if(pData == NULL)
		return;

	memcpy(pCxRomPeripheral + uSlot*256, pData, PRINTDRVR_SIZE);

	//

	RegisterIoHandler(uSlot, PrintStatus, PrintTransmit, NULL, NULL, NULL, NULL);

	g_uSlot = uSlot;
}

//===========================================================================
static BOOL CheckPrint()
{
    inactivity = 0;
    if (file == NULL)
    {
		//TCHAR filepath[MAX_PATH * 2];
		//_tcsncpy(filepath, g_sProgramDir, MAX_PATH);
        //_tcsncat(filepath, _T("Printer.txt"), MAX_PATH);
		//file = fopen(filepath, "wb");
		if (g_bPrinterAppend )
			file = fopen(Printer_GetFilename(), "ab");
		else
			file = fopen(Printer_GetFilename(), "wb");
    }
    return (file != NULL);
}

//===========================================================================
static void ClosePrint()
{
    if (file != NULL)
    {
        fclose(file);
        file = NULL;
		std::string ExtendedFileName = "copy \"";
		ExtendedFileName.append (Printer_GetFilename());
		ExtendedFileName.append ("\" prn");
		//if (g_bDumpToPrinter) ShellExecute(NULL, "print", Printer_GetFilename(), NULL, NULL, 0); //Print through Notepad
		if (g_bDumpToPrinter) 
			system (ExtendedFileName.c_str ()); //Print through console. This is supposed to be the better way, because it shall print images (with older printers only).
			
    }
    inactivity = 0;
}

//===========================================================================
void PrintDestroy()
{
    ClosePrint();
}

//===========================================================================
void PrintUpdate(DWORD totalcycles)
{
    if (file == NULL)
    {
        return;
    }
//    if ((inactivity += totalcycles) > (Printer_GetIdleLimit () * 1000 * 1000))  //This line seems to give a very big deviation
	if ((inactivity += totalcycles) > (Printer_GetIdleLimit () * 710000)) 
    {
        // inactive, so close the file (next print will overwrite or append to it, according to the settings made)
        ClosePrint();
    }
}

//===========================================================================
void PrintReset()
{
    ClosePrint();
}

//===========================================================================
static BYTE __stdcall PrintStatus(WORD, WORD, BYTE, BYTE, ULONG)
{
    CheckPrint();
    return 0xFF; // status - TODO?
}

//===========================================================================
static BYTE __stdcall PrintTransmit(WORD, WORD, BYTE, BYTE value, ULONG)
{
	         char Lat8A[]= "abwgdevzijklmnoprstufhc~{}yx`q|]";
             char Lat82[]= "abwgdevzijklmnoprstufhc^[]yx@q{}~`"; 
			 char Kir82[]= "ÀÁÂÃÄÅÆÇÈÉÊËÌÍÎÏÐÑÒÓÔÕÖ×ØÙÚÜÞß[]^@";
	  char Kir8ACapital[]= "ÀÁÂÃÄÅÆÇÈÉÊËÌÍÎÏÐÑÒÓÔÕÖ×ØÙÚÜÞßÝ";
	char Kir8ALowerCase[]= "àáâãäåæçèéêëìíîïðñòóôõö÷øùúüþÿý";
	bool Pres = false;
    if (!CheckPrint())
    {
        return 0;
    }
	
	char c = NULL;
	if ((g_Apple2Type == A2TYPE_PRAVETS8A) &&  g_bConvertEncoding)  //This is print conversion for Pravets 8A/C. Print conversion for Pravets82/M is still to be done.
		{
			if ((value > 90) && (value < 128)) //This range shall be set more precisely
			{
			c = value;
			int loop = 0;
			while (loop < 31)
				{
				if (c== Lat8A[loop]) 
				c= 0 + Kir8ALowerCase  [loop] ;
				loop++;
				} //End loop
			}//End if (value < 128)
				else if ((value >64) && (value <91))
				{
					c = value + 32;
			    }
				else
				{
					c = value & 0x7F;
					int loop = 0;
					while (loop < 31)
					{
					if (c== Lat8A[loop]) c= 0 + Kir8ACapital  [loop];
					loop++;
					}
				}
	} //End if (g_Apple2Type == A2TYPE_PRAVETS8A)
		else if (((g_Apple2Type == A2TYPE_PRAVETS82) || (g_Apple2Type == A2TYPE_PRAVETS8M)) && g_bConvertEncoding)
		{
			c =  value & 0x7F;
			int loop = 0;
			while (loop < 34)
			{
				if (c == Lat82[loop])
					c= Kir82 [loop];
				loop++;
			} //end while
		}
		else //Apple II
		{			
			c =  value & 0x7F;
		}
	if ((g_bFilterUnprintable == false) || (c>31) || (c==13) || (c==10) || (c<0)) //c<0 is needed for cyrillic characters
		fwrite(&c, 1, 1, file); //break;
				

	/*else
	{
	char c = value & 0x7F;
	fwrite(&c, 1, 1, file);
	}*/
	return 0;
}

//===========================================================================

char* Printer_GetFilename()
{
	return g_szPrintFilename;
}

void Printer_SetFilename(char* prtFilename)
{
	if(*prtFilename)
		strcpy(g_szPrintFilename, (const char *) prtFilename);
	else  //No registry entry is available
	{
		_tcsncpy(g_szPrintFilename, g_sProgramDir, MAX_PATH);
        _tcsncat(g_szPrintFilename, _T(DEFAULT_PRINT_FILENAME), MAX_PATH);		
		RegSaveString(TEXT("Configuration"),REGVALUE_PRINTER_FILENAME,1,g_szPrintFilename);
	}
}

unsigned int Printer_GetIdleLimit()
{
	return g_iPrinterIdleLimit;
}

void Printer_SetIdleLimit(unsigned int Duration)
{	
	g_iPrinterIdleLimit = Duration;
}

//===========================================================================

#define SS_YAML_VALUE_CARD_PRINTER "Generic Printer"

#define SS_YAML_KEY_INACTIVITY "Inactivity"
#define SS_YAML_KEY_IDLELIMIT "Printer Idle Limit"
#define SS_YAML_KEY_FILENAME "Print Filename"
#define SS_YAML_KEY_FILEOPEN "Is File Open"
#define SS_YAML_KEY_DUMPTOPRINTER "Dump To Printer"
#define SS_YAML_KEY_CONVERTENCODING "Convert Encoding"
#define SS_YAML_KEY_FILTERUNPRINTABLE "Filter Unprintable"
#define SS_YAML_KEY_APPEND "Printer Append"
#define SS_YAML_KEY_DUMPTOREALPRINTER "Enable Dump To Real Printer"

std::string Printer_GetSnapshotCardName(void)
{
	static const std::string name(SS_YAML_VALUE_CARD_PRINTER);
	return name;
}

void Printer_SaveSnapshot(class YamlSaveHelper& yamlSaveHelper)
{
	YamlSaveHelper::Slot slot(yamlSaveHelper, Printer_GetSnapshotCardName(), g_uSlot, 1);

	YamlSaveHelper::Label state(yamlSaveHelper, "%s:\n", SS_YAML_KEY_STATE);
	yamlSaveHelper.Save("%s: %d\n", SS_YAML_KEY_INACTIVITY, inactivity);
	yamlSaveHelper.Save("%s: %d\n", SS_YAML_KEY_IDLELIMIT, g_iPrinterIdleLimit);
	yamlSaveHelper.Save("%s: %s\n", SS_YAML_KEY_FILENAME, yamlSaveHelper.GetSaveString(g_szPrintFilename).c_str());
	yamlSaveHelper.Save("%s: %d\n", SS_YAML_KEY_FILEOPEN, (file != NULL) ? 1 : 0);
	yamlSaveHelper.Save("%s: %d\n", SS_YAML_KEY_DUMPTOPRINTER, g_bDumpToPrinter);
	yamlSaveHelper.Save("%s: %d\n", SS_YAML_KEY_CONVERTENCODING, g_bConvertEncoding);
	yamlSaveHelper.Save("%s: %d\n", SS_YAML_KEY_FILTERUNPRINTABLE, g_bFilterUnprintable);
	yamlSaveHelper.Save("%s: %d\n", SS_YAML_KEY_APPEND, g_bPrinterAppend);
	yamlSaveHelper.Save("%s: %d\n", SS_YAML_KEY_DUMPTOREALPRINTER, g_bEnableDumpToRealPrinter);
}

bool Printer_LoadSnapshot(class YamlLoadHelper& yamlLoadHelper, UINT slot, UINT version)
{
	if (slot != 1)	// fixme
		throw std::string("Card: wrong slot");

	if (version != 1)
		throw std::string("Card: wrong version");

	inactivity					= yamlLoadHelper.GetMapValueUINT(SS_YAML_KEY_INACTIVITY);
	g_iPrinterIdleLimit			= yamlLoadHelper.GetMapValueUINT(SS_YAML_KEY_IDLELIMIT);
	strncpy(g_szPrintFilename, yamlLoadHelper.GetMapValueSTRING(SS_YAML_KEY_FILENAME).c_str(), sizeof(g_szPrintFilename));

	if (yamlLoadHelper.GetMapValueBool(SS_YAML_KEY_FILEOPEN))
	{
		yamlLoadHelper.GetMapValueBool(SS_YAML_KEY_APPEND);	// Consume
		g_bPrinterAppend = true;	// Re-open print-file in append mode
		BOOL bRes = CheckPrint();
		if (!bRes)
			throw std::string("Printer Card: Unable to resume printing to file");
	}
	else
	{
		g_bPrinterAppend = yamlLoadHelper.GetMapValueBool(SS_YAML_KEY_APPEND);
	}

	g_bDumpToPrinter			= yamlLoadHelper.GetMapValueBool(SS_YAML_KEY_DUMPTOPRINTER);
	g_bConvertEncoding			= yamlLoadHelper.GetMapValueBool(SS_YAML_KEY_CONVERTENCODING);
	g_bFilterUnprintable		= yamlLoadHelper.GetMapValueBool(SS_YAML_KEY_FILTERUNPRINTABLE);
	g_bEnableDumpToRealPrinter	= yamlLoadHelper.GetMapValueBool(SS_YAML_KEY_DUMPTOREALPRINTER);

	return true;
}

//---

struct PrinterCard_Unit
{
	DWORD Inactivity;
	UINT PrinterIdleLimit;
	char PrintFilename[MAX_PATH];
	BOOL IsFileOpen;
	BOOL DumpToPrinter;
	BOOL ConvertEncoding;
	BOOL FilterUnprintable;
	BOOL PrinterAppend;
	BOOL EnableDumpToRealPrinter;
};

struct SS_CARD_PRINTER
{
	SS_CARD_HDR			Hdr;
	PrinterCard_Unit	Unit;
};

void Printer_GetSnapshot(const HANDLE hFile)
{
	SS_CARD_PRINTER CardPrinter;

	SS_CARD_PRINTER* const pSS = &CardPrinter;

	pSS->Hdr.UnitHdr.hdr.v2.Length = sizeof(SS_CARD_PRINTER);
	pSS->Hdr.UnitHdr.hdr.v2.Type = UT_Card;
	pSS->Hdr.UnitHdr.hdr.v2.Version = 1;

	pSS->Hdr.Slot = g_uSlot;
	pSS->Hdr.Type = CT_GenericPrinter;

	CardPrinter.Unit.Inactivity = inactivity;
	CardPrinter.Unit.PrinterIdleLimit = g_iPrinterIdleLimit;
	memcpy(CardPrinter.Unit.PrintFilename, g_szPrintFilename, sizeof(CardPrinter.Unit.PrintFilename));
	CardPrinter.Unit.IsFileOpen = (file != NULL);
	CardPrinter.Unit.DumpToPrinter = g_bDumpToPrinter;
	CardPrinter.Unit.ConvertEncoding = g_bConvertEncoding;
	CardPrinter.Unit.FilterUnprintable = g_bFilterUnprintable;
	CardPrinter.Unit.PrinterAppend = g_bPrinterAppend;
	CardPrinter.Unit.EnableDumpToRealPrinter = g_bEnableDumpToRealPrinter;

	//

	DWORD dwBytesWritten;
	BOOL bRes = WriteFile(	hFile,
							&CardPrinter,
							CardPrinter.Hdr.UnitHdr.hdr.v2.Length,
							&dwBytesWritten,
							NULL);

	if(!bRes || (dwBytesWritten != CardPrinter.Hdr.UnitHdr.hdr.v2.Length))
	{
		//dwError = GetLastError();
		throw std::string("Save error: Printer Card");
	}
}

void Printer_SetSnapshot(const HANDLE hFile)
{
	SS_CARD_PRINTER CardPrinter;

	DWORD dwBytesRead;
	BOOL bRes = ReadFile(	hFile,
							&CardPrinter,
							sizeof(CardPrinter),
							&dwBytesRead,
							NULL);

	if (dwBytesRead != sizeof(CardPrinter))
		throw std::string("Card: file corrupt");

	if (CardPrinter.Hdr.Slot != 1)	// fixme
		throw std::string("Card: wrong slot");

	if (CardPrinter.Hdr.UnitHdr.hdr.v2.Version > 1)
		throw std::string("Card: wrong version");

	if (CardPrinter.Hdr.UnitHdr.hdr.v2.Length != sizeof(SS_CARD_PRINTER))
		throw std::string("Card: unit size mismatch");

	ClosePrint();	// Close current print session (and close file handle)

	inactivity = CardPrinter.Unit.Inactivity;
	g_iPrinterIdleLimit = CardPrinter.Unit.PrinterIdleLimit;
	memcpy(g_szPrintFilename, CardPrinter.Unit.PrintFilename, sizeof(g_szPrintFilename));
	if (CardPrinter.Unit.IsFileOpen)
	{
		g_bPrinterAppend = true;	// Re-open print-file in append mode
		BOOL bRes = CheckPrint();
		if (!bRes)
			throw std::string("Printer Card: Unable to resume printing to file");
	}

	g_bDumpToPrinter = CardPrinter.Unit.DumpToPrinter == TRUE;
	g_bConvertEncoding = CardPrinter.Unit.ConvertEncoding == TRUE;
	g_bFilterUnprintable = CardPrinter.Unit.FilterUnprintable == TRUE;
	g_bPrinterAppend = CardPrinter.Unit.PrinterAppend == TRUE;
	g_bEnableDumpToRealPrinter = CardPrinter.Unit.EnableDumpToRealPrinter == TRUE;
}
