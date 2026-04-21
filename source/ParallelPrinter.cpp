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

#include "ParallelPrinter.h"
#include "Core.h"
#include "Memory.h"
#include "Pravets.h"
#include "Registry.h"
#include "YamlHelper.h"
#include "Interface.h"

#include "../resource/resource.h"

//===========================================================================

void ParallelPrinterCard::InitializeIO(LPBYTE pCxRomPeripheral)
{
	const DWORD PRINTDRVR_SIZE = APPLE_SLOT_SIZE;
	BYTE* pData = GetFrame().GetResource(IDR_PRINTDRVR_FW, "FIRMWARE", PRINTDRVR_SIZE);
	if(pData == NULL)
		return;

	memcpy(pCxRomPeripheral + m_slot*APPLE_SLOT_SIZE, pData, PRINTDRVR_SIZE);

	RegisterIoHandler(m_slot, IORead, IOWrite, NULL, NULL, this, NULL);
}

//===========================================================================
bool ParallelPrinterCard::CheckPrint(void)
{
	m_inactivity = 0;
	if (m_file == NULL)
	{
		//TCHAR filepath[MAX_PATH * 2];
		//_tcsncpy(filepath, g_sProgramDir, MAX_PATH);
		//_tcsncat(filepath, _T("Printer.txt"), MAX_PATH);
		//file = fopen(filepath, "wb");
		if (m_bPrinterAppend )
			m_file = fopen(ParallelPrinterCard::GetFilename().c_str(), "ab");
		else
			m_file = fopen(ParallelPrinterCard::GetFilename().c_str(), "wb");
	}
	return (m_file != NULL);
}

//===========================================================================
void ParallelPrinterCard::ClosePrint(void)
{
	if (m_file != NULL)
	{
		fclose(m_file);
		m_file = NULL;
		std::string ExtendedFileName = "copy \"";
		ExtendedFileName.append (ParallelPrinterCard::GetFilename());
		ExtendedFileName.append ("\" prn");
		//if (g_bDumpToPrinter) ShellExecute(NULL, "print", Printer_GetFilename(), NULL, NULL, 0); //Print through Notepad
		if (m_bDumpToPrinter)
			system (ExtendedFileName.c_str ()); //Print through console. This is supposed to be the better way, because it shall print images (with older printers only).
			
	}
	m_inactivity = 0;
}

//===========================================================================
void ParallelPrinterCard::Destroy(void)
{
	ClosePrint();
}

//===========================================================================
void ParallelPrinterCard::Update(const ULONG nExecutedCycles)
{
	if (m_file == NULL)
		return;

//	if ((inactivity += totalcycles) > (Printer_GetIdleLimit () * 1000 * 1000))  //This line seems to give a very big deviation
	if ((m_inactivity += nExecutedCycles) > (ParallelPrinterCard::GetIdleLimit () * 710000))
	{
		// inactive, so close the file (next print will overwrite or append to it, according to the settings made)
		ClosePrint();
	}
}

//===========================================================================
void ParallelPrinterCard::Reset(const bool powerCycle)
{
	ClosePrint();
}

//===========================================================================
BYTE __stdcall ParallelPrinterCard::IORead(WORD, WORD address, BYTE, BYTE, ULONG)
{
	UINT slot = ((address & 0xff) >> 4) - 8;
	ParallelPrinterCard* card = (ParallelPrinterCard*)MemGetSlotParameters(slot);

	card->CheckPrint();
	return 0xFF; // status - TODO?
}

//===========================================================================
BYTE __stdcall ParallelPrinterCard::IOWrite(WORD, WORD address, BYTE, BYTE value, ULONG)
{
	UINT slot = ((address & 0xff) >> 4) - 8;
	ParallelPrinterCard* card = (ParallelPrinterCard*)MemGetSlotParameters(slot);

	if (!card->CheckPrint())
		return 0;

	// only allow writes to the load output port (i.e., $C090)
	if ((address & 0xF) != 0)
		return 0;

	BYTE c = value & 0x7F;

	if (IsPravets(GetApple2Type()))
	{
		if (card->m_bConvertEncoding)
			c = GetPravets().ConvertToPrinterChar(value);
	}

	if ((card->m_bFilterUnprintable == false) || (c>31) || (c==13) || (c==10) || (c>0x7F)) //c>0x7F is needed for cyrillic characters
		fwrite(&c, 1, 1, card->m_file);

	return 0;
}

//===========================================================================

const std::string& ParallelPrinterCard::GetFilename(void)
{
	return m_szPrintFilename;
}

#define DEFAULT_PRINT_FILENAME "Printer.txt"

void ParallelPrinterCard::SetFilename(const std::string& prtFilename)
{
	if (!prtFilename.empty())
	{
		m_szPrintFilename = prtFilename;
	}
	else  //No registry entry is available
	{
		m_szPrintFilename = g_sProgramDir + DEFAULT_PRINT_FILENAME;
		RegSaveString(REG_CONFIG, REGVALUE_PRINTER_FILENAME, 1, m_szPrintFilename);
	}
}

UINT ParallelPrinterCard::GetIdleLimit(void)
{
	return m_printerIdleLimit;
}

void ParallelPrinterCard::SetIdleLimit(UINT Duration)
{	
	m_printerIdleLimit = Duration;
}

//===========================================================================

void ParallelPrinterCard::GetRegistryConfig(void)
{
	std::string regSection = RegGetConfigSlotSection(m_slot);

	DWORD dwTmp;
	char szFilename[MAX_PATH];

	if (RegLoadValue(regSection.c_str(), REGVALUE_DUMP_TO_PRINTER, TRUE, &dwTmp))
		SetDumpToPrinter(dwTmp ? true : false);

	if (RegLoadValue(regSection.c_str(), REGVALUE_CONVERT_ENCODING, TRUE, &dwTmp))
		SetConvertEncoding(dwTmp ? true : false);

	if (RegLoadValue(regSection.c_str(), REGVALUE_FILTER_UNPRINTABLE, TRUE, &dwTmp))
		SetFilterUnprintable(dwTmp ? true : false);

	if (RegLoadValue(regSection.c_str(), REGVALUE_PRINTER_APPEND, TRUE, &dwTmp))
		SetPrinterAppend(dwTmp ? true : false);

	if (RegLoadString(regSection.c_str(), REGVALUE_PRINTER_FILENAME, 1, szFilename, MAX_PATH, TEXT("")))
		SetFilename(szFilename);

	if (RegLoadValue(regSection.c_str(), REGVALUE_PRINTER_IDLE_LIMIT, TRUE, &dwTmp))
		SetIdleLimit(dwTmp);
}

void ParallelPrinterCard::SetRegistryConfig(void)
{
	std::string regSection = RegGetConfigSlotSection(m_slot);
	RegSaveValue(regSection.c_str(), REGVALUE_DUMP_TO_PRINTER, TRUE, GetDumpToPrinter() ? 1 : 0);
	RegSaveValue(regSection.c_str(), REGVALUE_CONVERT_ENCODING, TRUE, GetConvertEncoding() ? 1 : 0);
	RegSaveValue(regSection.c_str(), REGVALUE_FILTER_UNPRINTABLE, TRUE, GetFilterUnprintable() ? 1 : 0);
	RegSaveValue(regSection.c_str(), REGVALUE_PRINTER_APPEND, TRUE, GetPrinterAppend() ? 1 : 0);
	RegSaveString(regSection.c_str(), REGVALUE_PRINTER_FILENAME, TRUE, GetFilename());
	RegSaveValue(regSection.c_str(), REGVALUE_PRINTER_IDLE_LIMIT, TRUE, GetIdleLimit());
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

const std::string& ParallelPrinterCard::GetSnapshotCardName(void)
{
	static const std::string name(SS_YAML_VALUE_CARD_PRINTER);
	return name;
}

void ParallelPrinterCard::SaveSnapshot(class YamlSaveHelper& yamlSaveHelper)
{
	YamlSaveHelper::Slot slot(yamlSaveHelper, ParallelPrinterCard::GetSnapshotCardName(), m_slot, 1);

	YamlSaveHelper::Label state(yamlSaveHelper, "%s:\n", SS_YAML_KEY_STATE);
	yamlSaveHelper.SaveUint(SS_YAML_KEY_INACTIVITY, m_inactivity);
	yamlSaveHelper.SaveUint(SS_YAML_KEY_IDLELIMIT, m_printerIdleLimit);
	yamlSaveHelper.SaveString(SS_YAML_KEY_FILENAME, m_szPrintFilename);
	yamlSaveHelper.SaveBool(SS_YAML_KEY_FILEOPEN, (m_file != NULL) ? true : false);
	yamlSaveHelper.SaveBool(SS_YAML_KEY_DUMPTOPRINTER, m_bDumpToPrinter);
	yamlSaveHelper.SaveBool(SS_YAML_KEY_CONVERTENCODING, m_bConvertEncoding);
	yamlSaveHelper.SaveBool(SS_YAML_KEY_FILTERUNPRINTABLE, m_bFilterUnprintable);
	yamlSaveHelper.SaveBool(SS_YAML_KEY_APPEND, m_bPrinterAppend);
	yamlSaveHelper.SaveBool(SS_YAML_KEY_DUMPTOREALPRINTER, m_bEnableDumpToRealPrinter);
}

bool ParallelPrinterCard::LoadSnapshot(class YamlLoadHelper& yamlLoadHelper, UINT version)
{
	if (m_slot != SLOT1)	// fixme
		Card::ThrowErrorInvalidSlot(CT_GenericPrinter, m_slot);

	if (version != 1)
		Card::ThrowErrorInvalidVersion(CT_GenericPrinter, version);

	m_inactivity				= yamlLoadHelper.LoadUint(SS_YAML_KEY_INACTIVITY);
	m_printerIdleLimit			= yamlLoadHelper.LoadUint(SS_YAML_KEY_IDLELIMIT);
	m_szPrintFilename = yamlLoadHelper.LoadString(SS_YAML_KEY_FILENAME);

	if (yamlLoadHelper.LoadBool(SS_YAML_KEY_FILEOPEN))
	{
		yamlLoadHelper.LoadBool(SS_YAML_KEY_APPEND);	// Consume
		m_bPrinterAppend = true;	// Re-open print-file in append mode
		BOOL bRes = CheckPrint();
		if (!bRes)
			throw std::runtime_error("Printer Card: Unable to resume printing to file");
	}
	else
	{
		m_bPrinterAppend = yamlLoadHelper.LoadBool(SS_YAML_KEY_APPEND);
	}

	m_bDumpToPrinter			= yamlLoadHelper.LoadBool(SS_YAML_KEY_DUMPTOPRINTER);
	m_bConvertEncoding			= yamlLoadHelper.LoadBool(SS_YAML_KEY_CONVERTENCODING);
	m_bFilterUnprintable		= yamlLoadHelper.LoadBool(SS_YAML_KEY_FILTERUNPRINTABLE);
	m_bEnableDumpToRealPrinter	= yamlLoadHelper.LoadBool(SS_YAML_KEY_DUMPTOREALPRINTER);

	return true;
}
