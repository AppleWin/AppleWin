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

static DWORD inactivity = 0;
static FILE* file = NULL;
DWORD const PRINTDRVR_SIZE = APPLE_SLOT_SIZE;

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
        TCHAR filepath[MAX_PATH * 2];
        _tcsncpy(filepath, g_sProgramDir, MAX_PATH);
        _tcsncat(filepath, _T("Printer.txt"), MAX_PATH);
        file = fopen(filepath, "wb");
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
    if ((inactivity += totalcycles) > (10 * 1000 * 1000)) // around 10 seconds
    {
        // inactive, so close the file (next print will overwrite it)
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
    if (!CheckPrint())
    {
        return 0;
    }
    char c = value & 0x7F;
	fwrite(&c, 1, 1, file);
    return 0;
}

//===========================================================================
