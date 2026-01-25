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
	- Southwestern Data Systems' datakey for SpeedStar Applesoft Compiler (Matthew D'Asaro  Dec 2022)
	- Dynatech Microsoftware / Cortechs Corp's protection key for "CodeWriter"
	- Robocom Ltd's Interface Module for Robo Graphics 500/1000/1500 & RoboCAD 1/2 (BitStik joystick plugs in on top)
	- Hayden Book Company, Inc's protection key for "Applesoft Compiler"
*/
#include "StdAfx.h"
#include <sstream>

#include "CopyProtectionDongles.h"
#include "Memory.h"
#include "YamlHelper.h"

static DONGLETYPE copyProtectionDongleType = DT_DEFAULT;

static const BYTE codewriterInitialLFSR = 0x6B;	// %1101011 (7-bit LFSR)
static BYTE codewriterLFSR = codewriterInitialLFSR;

static void CodeWriterResetLFSR()
{
	codewriterLFSR = codewriterInitialLFSR;
}

static void CodeWriterClockLFSR()
{
	BYTE bit = ((codewriterLFSR >> 1) ^ (codewriterLFSR >> 0)) & 1;
	codewriterLFSR = (codewriterLFSR >> 1) | (bit << 6);
}

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
		if ((AN == 3 && state == true) || MemGetAnnunciator(3))	// reset or was already reset? (ie. takes precedent over AN2)
			CodeWriterResetLFSR();
		else if (AN == 2 && state == false && MemGetAnnunciator(2) == true)	// AN2 true->false edge?
			CodeWriterClockLFSR();
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
	if (copyProtectionDongleType == DT_HAYDENCOMPILER)
		return 0;	// connected to GND

	return -1;
}

// Returns the copy protection dongle state of PB2. A return value of -1 means not used by copy protection dongle
int CopyProtectionDonglePB2(void)
{
	switch (copyProtectionDongleType)
	{
	case DT_SDSSPEEDSTAR:
		return SdsSpeedStar();

	case DT_CODEWRITER:
		return codewriterLFSR & 1;

	default:
		return -1;
	}
}

// Returns the copy protection dongle state of PDL(n). A return value of -1 means not used by copy protection dongle
int CopyProtectionDonglePDL(UINT pdl)
{
	if (copyProtectionDongleType == DT_HAYDENCOMPILER && pdl == 3)
	{
		static BYTE haydenValue[4] = {0xFF, 0x96, 0x96, 0x50};	// Derived from reverse-engineered Hayden code - although other than 0xFF, actual values are unknown.
		UINT haydenDongleMode = ((UINT)MemGetAnnunciator(2) << 1) | (UINT)MemGetAnnunciator(0);
		return haydenValue[haydenDongleMode];
	}

	//

	if (copyProtectionDongleType != DT_ROBOCOM500 && copyProtectionDongleType != DT_ROBOCOM1000 && copyProtectionDongleType != DT_ROBOCOM1500)
		return -1;

	bool roboComInterfaceModulePower = !MemGetAnnunciator(3);
	if (!roboComInterfaceModulePower || pdl != 3)
		return -1;

	UINT roboComInterfaceModuleMode = ((UINT)MemGetAnnunciator(2) << 2) | ((UINT)MemGetAnnunciator(1) << 1) | (UINT)MemGetAnnunciator(0);

	switch (copyProtectionDongleType)
	{
		case DT_ROBOCOM500:
		{
			static BYTE robo500_lo[8] = { 0x3F,0x2E,0x54,0x54,0x2E,0x22,0x72,0x17 };	// PDL3 lower bound - see GH#1247
			static BYTE robo500_hi[8] = { 0x6F,0x54,0x94,0x94,0x54,0x40,0xC4,0x2E };	// PDL3 upper bound - see GH#1247
			// This mean value gives values that are very close to the actual 1000 & 1500 Module Interfaces - so assume it's similar for the 500 series.
			return (robo500_lo[roboComInterfaceModuleMode] + robo500_hi[roboComInterfaceModuleMode] - 1) / 2;
		}

		case DT_ROBOCOM1000:
		{
			static BYTE robo1000[8] = { 34,151,48,64,113,113,64,85 };	// Actual Module Interface values for PDL3
			return robo1000[roboComInterfaceModuleMode];
		}

		case DT_ROBOCOM1500:
		{
			static BYTE robo1500[8] = { 153,34,64,34,48,86,114,48 };	// Actual Module Interface values for PDL3
			return robo1500[roboComInterfaceModuleMode];
		}

		default:
			return -1;
	}
}

//===========================================================================

#define SS_YAML_KEY_CODEWRITER_INDEX "LFSR"

// Unit version history:
// 1: Add SDS SpeedStar dongle
// 2: Add Cortechs Corp CodeWriter protection key
//    Add Robocom Ltd - Robo 500/1000/1500 Interface Modules
// 3: Add Hayden Compiler protection key

static const std::string& GetSnapshotStructName_SDSSpeedStar(void)
{
	static const std::string name("SDS SpeedStar dongle");
	return name;
}

static const std::string& GetSnapshotStructName_CodeWriter(void)
{
	static const std::string name("Cortechs Corp - CodeWriter protection key");
	return name;
}

static const std::string& GetSnapshotStructName_Robocom500(void)
{
	static const std::string name("Robocom Ltd - Robo 500 Interface Module");
	return name;
}

static const std::string& GetSnapshotStructName_Robocom1000(void)
{
	static const std::string name("Robocom Ltd - Robo 1000 Interface Module");
	return name;
}

static const std::string& GetSnapshotStructName_Robocom1500(void)
{
	static const std::string name("Robocom Ltd - Robo 1500 Interface Module");
	return name;
}

static const std::string& GetSnapshotStructName_HaydenCompiler(void)
{
	static const std::string name("Hayden - Applesoft Compiler protection key");
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
		yamlSaveHelper.SaveHexUint8(SS_YAML_KEY_CODEWRITER_INDEX, codewriterLFSR);
	}
	else if (copyProtectionDongleType == DT_ROBOCOM500)
	{
		yamlSaveHelper.SaveString(SS_YAML_KEY_DEVICE, GetSnapshotStructName_Robocom500());
		// NB. No state for this dongle
	}
	else if (copyProtectionDongleType == DT_ROBOCOM1000)
	{
		yamlSaveHelper.SaveString(SS_YAML_KEY_DEVICE, GetSnapshotStructName_Robocom1000());
		// NB. No state for this dongle
	}
	else if (copyProtectionDongleType == DT_ROBOCOM1500)
	{
		yamlSaveHelper.SaveString(SS_YAML_KEY_DEVICE, GetSnapshotStructName_Robocom1500());
		// NB. No state for this dongle
	}
	else if (copyProtectionDongleType == DT_HAYDENCOMPILER)
	{
		yamlSaveHelper.SaveString(SS_YAML_KEY_DEVICE, GetSnapshotStructName_HaydenCompiler());
		// NB. No state for this dongle
	}
	else
	{
		_ASSERT(0);
	}
}

void CopyProtectionDongleLoadSnapshot(YamlLoadHelper& yamlLoadHelper, UINT version, UINT kUNIT_VERSION)
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
		codewriterLFSR = yamlLoadHelper.LoadUint(SS_YAML_KEY_CODEWRITER_INDEX);
	}
	else if (device == GetSnapshotStructName_Robocom500())
	{
		copyProtectionDongleType = DT_ROBOCOM500;
	}
	else if (device == GetSnapshotStructName_Robocom1000())
	{
		copyProtectionDongleType = DT_ROBOCOM1000;
	}
	else if (device == GetSnapshotStructName_Robocom1500())
	{
		copyProtectionDongleType = DT_ROBOCOM1500;
	}
	else if (device == GetSnapshotStructName_HaydenCompiler())
	{
		copyProtectionDongleType = DT_HAYDENCOMPILER;
	}
	else
	{
		_ASSERT(0);
	}
}
