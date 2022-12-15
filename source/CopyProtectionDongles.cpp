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
#include "Memory.h"

static DWORD copyProtectionDongleType = 0;

// Must be in the same order as in PageAdvanced.cpp
enum COPYPROTECTIONDONGLETYPE { NONE, SDSSPEEDSTAR };

void SetCopyProtectionDongleType(DWORD type)
{
	copyProtectionDongleType = type;
}

DWORD GetCopyProtectionDongleType(void)
{
	return copyProtectionDongleType;
}

// This protection dongle consists of a NAND gate connected with AN1 and AN2 on the inputs
// PB2 on the output, and AN0 connected to power it.
bool SdsSpeedStar(void)
{
	return !MemGetAnnunciator(0) || !(MemGetAnnunciator(1) && MemGetAnnunciator(2));
}

// Returns the copy protection dongle state of PB0. A return value of -1 means not used by copy protection dongle
int CopyProtectionDonglePB0(void)
{
	return -1;
}

// Returns the copy protection dongle state of PB1. A return value of -1 means not used by copy protection dongle
int CopyProtectionDonglePB1(void)
{
	return -1;
}

// Returns the copy protection dongle state of PB2. A return value of -1 means not used by copy protection dongle
int CopyProtectionDonglePB2(void)
{
	switch (copyProtectionDongleType)
	{
	case SDSSPEEDSTAR:	// Southwestern Data Systems SoftKey for Speed Star Applesoft Compiler
		return SdsSpeedStar();
		break;

	default:
		return -1;
		break;
	}
}
