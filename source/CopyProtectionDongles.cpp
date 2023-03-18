/*
  AppleWin : An Apple //e emulator for Windows

  Copyright (C) 1994-1996, Michael O'Brien
  Copyright (C) 1999-2001, Oliver Schmidt
  Copyright (C) 2002-2005, Tom Charlesworth
  Copyright (C) 2006-2023, Tom Charlesworth, Michael Pohoreski

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
	- Southwestern Data Systems DataKey for SpeedStar Applesoft Compiler (Matthew D'Asaro  Dec 2022)
	- Dynatech Microsoftware / Cortechs Corp protection key for "CodeWriter"
*/
#include "StdAfx.h"
#include <sstream>

#include "CopyProtectionDongles.h"
#include "Memory.h"
#include "YamlHelper.h"

static DONGLETYPE copyProtectionDongleType = DT_EMPTY;

static UINT codeWriterIndex = 0;
static BYTE codeWriterBits[127] = {
	1,1,0,1,0,1,1,0,1,1,1,1,0,1,1,0,0,0,1,1,0,1,0,0,1,0,1,1,1,0,1,1,
	1,0,0,1,1,0,0,1,0,1,0,1,0,1,1,1,1,1,1,1,0,0,0,0,0,0,1,0,0,0,0,0,
	1,1,0,0,0,0,1,0,1,0,0,0,1,1,1,1,0,0,1,0,0,0,1,0,1,1,0,0,1,1,1,0,
	1,0,1,0,0,1,1,1,1,1,0,1,0,0,0,0,1,1,1,0,0,0,1,0,0,1,0,0,1,1,0 };

void SetCopyProtectionDongleType(DONGLETYPE type)
{
	copyProtectionDongleType = type;
}

DONGLETYPE GetCopyProtectionDongleType(void)
{
	return copyProtectionDongleType;
}

void DongleControl(WORD address)
{
	UINT AN = ((address - 8) >> 1) & 7;
	bool state = address & 1;	// ie. C058 = AN0_off; C059 = AN0_on

	if (copyProtectionDongleType == DT_EMPTY || copyProtectionDongleType == DT_SDSSPEEDSTAR)
		return;

	if (copyProtectionDongleType == DT_CODEWRITER)
	{
		if ((AN == 3 && state == false) || MemGetAnnunciator(3))	// reset?
			codeWriterIndex = 0;
		else if (AN == 2 && state == false && MemGetAnnunciator(2) == true)	// AN2 true->false edge?
			codeWriterIndex = (codeWriterIndex + 1) % std::size(codeWriterBits);
	}
}

// This protection dongle consists of a NAND gate connected with AN1 and AN2 on the inputs
// PB2 on the output, and AN0 connected to power it.
static bool SdsSpeedStar(void)
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
	case DT_SDSSPEEDSTAR:	// Southwestern Data Systems DataKey for SpeedStar Applesoft Compiler
		return SdsSpeedStar();
		break;

	case DT_CODEWRITER:		// Dynatech Microsoftware / Cortechs Corp protection key for "CodeWriter"
		return codeWriterBits[codeWriterIndex];

	default:
		return -1;
		break;
	}
}

//===========================================================================

#define SS_YAML_KEY_CODEWRITER_INDEX "Index"

static const UINT kUNIT_VERSION = 1;

static const std::string& GetSnapshotStructName_SDSSpeedStar(void)
{
	static const std::string name("SDS SpeedStar dongle");
	return name;
}

static const std::string& GetSnapshotStructName_CodeWriter(void)
{
	static const std::string name("Cortechs Corp CodeWriter protection key");
	return name;
}

void CopyProtectionDongleSaveSnapshot(YamlSaveHelper& yamlSaveHelper)
{
	if (copyProtectionDongleType == DT_SDSSPEEDSTAR)
	{
		yamlSaveHelper.SaveString(SS_YAML_KEY_DEVICE, GetSnapshotStructName_SDSSpeedStar());
		// NB. No state for this dongle
	}
	else if (copyProtectionDongleType == DT_CODEWRITER)
	{
		yamlSaveHelper.SaveString(SS_YAML_KEY_DEVICE, GetSnapshotStructName_CodeWriter());
		yamlSaveHelper.SaveUint(SS_YAML_KEY_CODEWRITER_INDEX, codeWriterIndex);
	}
	else
	{
		_ASSERT(0);
	}
}

void CopyProtectionDongleLoadSnapshot(YamlLoadHelper& yamlLoadHelper, UINT version)
{
	if (version < 1 || version > kUNIT_VERSION)
	{
		std::ostringstream msg;
		msg << "Version " << version;
		msg << " is not supported for game I/O device.";

		throw std::runtime_error(msg.str());
	}

	std::string device = yamlLoadHelper.LoadString(SS_YAML_KEY_DEVICE);

	if (device == GetSnapshotStructName_SDSSpeedStar())
	{
		copyProtectionDongleType = DT_SDSSPEEDSTAR;
	}
	else if (device == GetSnapshotStructName_CodeWriter())
	{
		copyProtectionDongleType = DT_CODEWRITER;
		codeWriterIndex = yamlLoadHelper.LoadUint(SS_YAML_KEY_CODEWRITER_INDEX);
	}
	else
	{
		_ASSERT(0);
	}
}
