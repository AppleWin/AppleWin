/*
  AppleWin : An Apple //e emulator for Windows

  Copyright (C) 1994-1996, Michael O'Brien
  Copyright (C) 1999-2001, Oliver Schmidt
  Copyright (C) 2002-2005, Tom Charlesworth
  Copyright (C) 2006-2007, Tom Charlesworth, Michael Pohoreski

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
/*
  CopyProtectionDongles.cpp

  Emulate hardware copy protection dongles for Apple II

  Currently supported:
	- Southwestern Data Systems SoftKey for Speed Star Applesoft Compiler

  Matthew D'Asaro  Dec 2022
*/
#include "StdAfx.h"

#include "CopyProtectionDongles.h"

static DWORD cpyPrtDongleType = 0;

static bool g_AN0 = 0;
static bool g_AN1 = 0;
static bool g_AN2 = 0;
static bool g_AN3 = 0;

// Must be in the same order as in PageAdvanced.cpp
enum CPYPRTDONGLETYPE { NONE, SDSSPEEDSTAR };

void SetCpyPrtDongleType(DWORD type)
{
	cpyPrtDongleType = type;
}

DWORD GetCpyPrtDongleType(void)
{
	return cpyPrtDongleType;
}

void CopyProtDongleControl(const UINT uControl)
{
	switch (uControl)
	{
	case 0xC058:	// AN0 clr
		g_AN0 = 0;
		break;
	case 0xC059:	// AN0 set
		g_AN0 = 1;
		break;
	case 0xC05A:	// AN1 clr
		g_AN1 = 0;
		break;
	case 0xC05B:	// AN1 set
		g_AN1 = 1;
		break;
	case 0xC05C:	// AN2 clr
		g_AN2 = 0;
		break;
	case 0xC05D:	// AN2 set
		g_AN2 = 1;
		break;
	case 0xC05E:	// AN3 clr
		g_AN3 = 0;
		break;
	case 0xC05F:	// AN3 set
		g_AN3 = 1;
		break;
	}
}

// This protection dongle consists of a NAND gate connected with AN1 and AN2 on the inputs
// PB2 on the output, and AN0 connected to power it.
bool SdsSpeedStar(void)
{
	return !g_AN0 || !(g_AN1 && g_AN2);
}

// Returns the copy protection dongle state of PB0. A return value of -1 means not used by copy protection dongle
DWORD CopyProtDonglePB0(void)
{
	return -1;
}

// Returns the copy protection dongle state of PB1. A return value of -1 means not used by copy protection dongle
DWORD CopyProtDonglePB1(void)
{
	return -1;
}

// Returns the copy protection dongle state of PB2. A return value of -1 means not used by copy protection dongle
DWORD CopyProtDonglePB2(void)
{
	switch (cpyPrtDongleType)
	{
	case SDSSPEEDSTAR:	// Southwestern Data Systems SoftKey for Speed Star Applesoft Compiler
		return SdsSpeedStar();
		break;

	default:
		return -1;
		break;
	}
}
