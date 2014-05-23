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

/* Description: Generic No-Slot Clock
 *
 * Author: Michael Pohoreski
 */

#include "StdAfx.h"
#pragma  hdrstop
#include "..\resource\resource.h"
// #include "resource.h" // BUG -- wrong resource!!!
#include <time.h>

/*

Old:

File: PRODOS
BLOAD PRODOS,TSYS,A$2000
CALL-151
2F76:60 5F 5E 5D 5C 5C 60
File offset $0F76-$0F7C:


ProDOS has a built-in clock driver that queries a clock/calendar card for the date and time. 
After the routine stores that information in the ProDOS Global Page ($BF90-$BF93),
either ProDOS or your own application programs can use it. See Figure 6-1.

Figure 6-1. ProDOS Date and Time Locations

         49041 ($BF91)     49040 ($BF90)

        7 6 5 4 3 2 1 0   7 6 5 4 3 2 1 0
       +-+-+-+-+-+-+-+-+ +-+-+-+-+-+-+-+-+
DATE:  |    year     |  month  |   day   |
       +-+-+-+-+-+-+-+-+ +-+-+-+-+-+-+-+-+

        7 6 5 4 3 2 1 0   7 6 5 4 3 2 1 0
       +-+-+-+-+-+-+-+-+ +-+-+-+-+-+-+-+-+
TIME:  |    hour       | |    minute     |
       +-+-+-+-+-+-+-+-+ +-+-+-+-+-+-+-+-+

         49043 ($BF93)     49042 ($BF92)


ProDOS recognizes a clock card if the following bytes are present in
the Cn00 ROM:

$Cn00 = $08
$Cn02 = $28
$Cn04 = $58
$Cn06 = $70

The address is preceded by a $4C (JMP) if a clock card is recognized,
or by a $60 (RTS) if not.

The ProDOS clock driver uses the following addresses for its I/O to the
clock:

Cn08 - READ entry point
Cn0B - WRITE entry point

The accumulator is loaded with an #A3 before the JSR to the WRITE
*/

tm* Clock_Util_GetTime()
{
	time_t tRawTime;
	time( &tRawTime );

	tm* pTime = localtime ( &tRawTime );
	if( pTime ) {
		mktime( pTime ); // get day of week: tm_wday
	}
	return pTime;
}

void Clock_Util_ConvertTimeToProdos( tm *pTime, unsigned char *pProDosBytes4_ )
{
	if( pTime )
	{
		BYTE nMonth = pTime->tm_mon + 1;

		BYTE nDate1 = 0
			| ((pTime->tm_mday & 0x1F) << 0)
			| ((nMonth & 0x0F) << 5);
		BYTE nDate2 = 0
			| ((pTime->tm_year % 100) << 1)
			| (nMonth >> 3) & 1;
		BYTE nTime1 = (pTime->tm_min  & 0x3F); // 60 = max(64) 0x3F
		BYTE nTime2 = (pTime->tm_hour & 0x1F); // 24 = max(32) 0x1F
		pProDosBytes4_[0] = nDate1;
		pProDosBytes4_[1] = nDate2;
		pProDosBytes4_[2] = nTime1;
		pProDosBytes4_[3] = nTime2;
	}
}

void Clock_Generic_UpdateProDos()
{
	tm* pTime = Clock_Util_GetTime();
	Clock_Util_ConvertTimeToProdos( pTime, &mem[ 0xBF90 ] ); // ProDos date/time buffer
}
