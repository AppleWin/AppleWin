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
 * Author: Nick Westgate
 */

#include "StdAfx.h"
#pragma  hdrstop
#include "..\resource\resource.h"
#include <stdlib.h>

static DWORD inactivity = 0;
static FILE* file = NULL;
DWORD const PRINTDRVR_SIZE = APPLE_SLOT_SIZE;
TCHAR filepath[MAX_PATH * 2];
#define DEFAULT_PRINT_FILENAME "Printer.txt"
static char g_szPrintFilename[MAX_PATH] = {0};
bool g_bDumpToPrinter = false;
bool g_bConvertEncoding = true;
bool g_bFilterUnprintable = true;
bool g_bPrinterAppend = false;
int  g_iPrinterIdleLimit = 10;

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
		string ExtendedFileName = "copy \"";
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

//unsigned int
void Printer_SetIdleLimit(unsigned int Duration)
{	
	g_iPrinterIdleLimit = Duration;
}
