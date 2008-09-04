/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 2008, Michael Pohoreski

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

/* Description: Thunderware ThunderClock+
 *
 * Author: Michael Pohoreski
 */

#include "StdAfx.h"
#pragma  hdrstop
#include "..\resource\resource.h"
// #include "resource.h" // BUG -- wrong resource!!!
#include <time.h>

void LoadRom_Clock_ThunderClockPlus(LPBYTE pCxRomPeripheral, UINT uSlot);

/*

static BYTE __stdcall Clock_Generic_Read_C0(WORD PC, WORD nAddr, BYTE bWrite, BYTE nValue, ULONG nCyclesLeft);
static BYTE __stdcall Clock_Generic_Write_C0(WORD PC, WORD nAddr, BYTE bWrite, BYTE nValue, ULONG nCyclesLeft);
static BYTE __stdcall Clock_Generic_Read_CX(WORD PC, WORD nAddr, BYTE bWrite, BYTE nValue, ULONG nCyclesLeft);
static BYTE __stdcall Clock_Generic_Write_CX(WORD PC, WORD nAddr, BYTE bWrite, BYTE nValue, ULONG nCyclesLeft);

UINT g_uClockInSlot4 = CT_GenericClock;
BYTE *g_pGenericClockFirmware = NULL;

enum ClockCommandEnum
{
	CC_READ = 0x18
};

enum ClockLatchEnum
{
	CL_SEC_0X,
	CL_SEC_X0,
	CL_MINS_0X,
	CL_MINS_X0,
	CL_HOURS_0X,
	CL_HOURS_X0,
	CL_DAY_0X,
	CL_DAY_X0,
	CL_DOW_0X,
	CL_DOW_X0,
	CL_MONTH 
};

int iClockCommand = 0;
int iClockLatch = 0;
int iClockStrobe = 0;
int iClockBit = 0;

BYTE iClockOutByte = 0;

static BYTE __stdcall Clock_Generic_Read_C0(WORD PC, WORD nAddr, BYTE bWrite, BYTE nValue, ULONG nCyclesLeft)
{
	BYTE nOutput = 0;
	{

// BCD
		static int y = 0x2013;
		static int d = 4; // min
		static int n = 5;
		static int w = 6;
		static int h = 7;
		static int m = 8;
		static int s = 9;
//		int nDOW = pLocalTime->tm_wday + 1;

		switch( iClockLatch )
		{
			case CL_MONTH:
//				nOutput = (pLocalTime->tm_mon << (7 + iClockBit)) & 0x80;
				nOutput = ((n  >> 0) & 0xF) << (7 + iClockBit);
				break;
			case CL_DOW_X0:
				nOutput = ((w >> 4) & 0xF) << (7 + iClockBit);
				break;
			case CL_DOW_0X:
				nOutput = ((w >> 0) & 0xF) << (7 + iClockBit);
				break;
			case CL_DAY_X0:
				nOutput = ((d >> 4) & 0xF) << (7 + iClockBit);
				break;
			case CL_DAY_0X:
				nOutput = ((d >> 0) & 0xF) << (7 + iClockBit);
				break;
			case CL_HOURS_X0:
				nOutput = ((h >> 4) & 0xF) << (7 + iClockBit);
				break;
			case CL_HOURS_0X:
				nOutput = ((h >> 0) & 0xF) << (7 + iClockBit);
				break;
			case CL_MINS_X0:
				nOutput = ((m >> 4) & 0xF) << (7 + iClockBit);
				break;
			case CL_MINS_0X:
				nOutput = ((m >> 0) & 0xF) << (7 + iClockBit);
				break;
			case CL_SEC_X0:
				nOutput = ((s >> 4) & 0xF) << (7 + iClockBit);
				break;
			case CL_SEC_0X:
				nOutput = ((s >> 0) & 0xF) << (7 + iClockBit);
				break;
			default:
				break;
		}
//		nOutput = 0xFF;

	}
	char sText[ 128 ];
	sprintf( sText, "  Read [%d] (%d) = %02X\n", iClockLatch, iClockBit, nOutput );
	OutputDebugString( sText );
  
	return nOutput;
}

static BYTE __stdcall Clock_Generic_Write_C0(WORD PC, WORD nAddr, BYTE bWrite, BYTE nValue, ULONG nCyclesLeft)
{
	int iSlot = (nAddr >> 12) - 0x8;

	if( iSlot > 0 )
	{
			if( nValue & 0x02)
			{
				iClockBit++;
				if( iClockBit >= 4 )
				{
					iClockBit = 0;
					iClockLatch++;
				}
			}

		switch( nAddr & 0x0F ) 
		{
			case 0x00: // set time format
				if( nValue == CC_READ )
				{
					iClockCommand = nValue;
					iClockLatch = 0;
					iClockBit = 0;
				}
				// 0x18 = Time Read
					// get 10 nibbles
				// 0x1C =
				// 0x18
				// 0x04
				// 0x0C
				break;	
			default:
				break;
		}

		char sText[ 128 ];
		sprintf( sText, "  Write: $%04X: %02X  Clock: %s  Strobe: %s\n"
			, nAddr, nValue
			, (nValue & 0x02)
				? "Raise  "
				: "Drop "
			, (nValue & 0x04)
				? "On "
				: "off"
			);
		OutputDebugString( sText );
	}
	
	return 0;
}

static BYTE __stdcall Clock_Generic_Read_CX(WORD PC, WORD nAddr, BYTE bWrite, BYTE nValue, ULONG nCyclesLeft)
{
	return 0;
	// Chain to default...
}

static BYTE __stdcall Clock_Generic_Write_CX(WORD PC, WORD nAddr, BYTE bWrite, BYTE nValue, ULONG nCyclesLeft)
{
	return 0;
	// Chain to default...
}


void LoadRom_Clock_ThunderClockPlus(LPBYTE pCxRomPeripheral, UINT uSlot)
{
	HRSRC hResInfo = FindResource(NULL, MAKEINTRESOURCE(IDR_THUNDERCLOCKPLUS_FW), "FIRMWARE");
	if(hResInfo == NULL)
		return;

	DWORD dwResSize = SizeofResource(NULL, hResInfo);
	if(dwResSize != FIRMWARE_EXPANSION_SIZE)
		return;

	HGLOBAL hResData = LoadResource(NULL, hResInfo);
	if(hResData == NULL)
		return;

	BYTE* pData = (BYTE*) LockResource(hResData);	// NB. Don't need to unlock resource
	if(pData == NULL)
		return;

	// Slot ROM is a mirror of the 1st page of the firmware
	memcpy(pCxRomPeripheral + uSlot*256, pData, APPLE_SLOT_SIZE);

#if _DEBUG
	if (pCxRomPeripheral[ uSlot*256 + 0 ] != 0x08)
		MessageBox( NULL, "Bad ThunderClock ROM: 1st byte", NULL, MB_OK );
	if (pCxRomPeripheral[ uSlot*256 + 2 ] != 0x28)
		MessageBox( NULL, "Bad ThunderClock ROM: 2nd byte", NULL, MB_OK );
#endif

	if( !g_pGenericClockFirmware )
	{
		g_pGenericClockFirmware = new BYTE [FIRMWARE_EXPANSION_SIZE];

		if (g_pGenericClockFirmware)
			memcpy(g_pGenericClockFirmware, pData, FIRMWARE_EXPANSION_SIZE);
	}

//	RegisterIoHandler( uSlot, &Clock_Generic_Read_C0, &Clock_Generic_Write_C0, Clock_Generic_Read_CX, Clock_Generic_Write_CX, NULL, g_pGenericClockFirmware );
	RegisterIoHandler( uSlot, &Clock_Generic_Read_C0, &Clock_Generic_Write_C0, NULL, NULL, NULL, g_pGenericClockFirmware );
}

*/
