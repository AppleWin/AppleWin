/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 1994-1996, Michael O'Brien
Copyright (C) 1999-2001, Oliver Schmidt
Copyright (C) 2002-2005, Tom Charlesworth
Copyright (C) 2006-2015, Tom Charlesworth, Michael Pohoreski

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

/* Description: Save-state (snapshot) module
 *
 * Author: Copyright (c) 2004-2015 Tom Charlesworth
 */

#include "StdAfx.h"

#include "YamlHelper.h"

#include "Applewin.h"
#include "CardManager.h"
#include "CPU.h"
#include "Debug.h"
#include "Disk.h"
#include "Frame.h"
#include "Joystick.h"
#include "Keyboard.h"
#include "LanguageCard.h"
#include "Memory.h"
#include "Mockingboard.h"
#include "MouseInterface.h"
#include "ParallelPrinter.h"
#include "Pravets.h"
#include "SerialComms.h"
#include "Speaker.h"
#include "Speech.h"
#include "Video.h"
#include "z80emu.h"

#include "Configuration/Config.h"
#include "Configuration/IPropertySheet.h"


#define DEFAULT_SNAPSHOT_NAME "SaveState.aws.yaml"

bool g_bSaveStateOnExit = false;

static std::string g_strSaveStateFilename;
static std::string g_strSaveStatePathname;
static std::string g_strSaveStatePath;

static YamlHelper yamlHelper;

#define SS_FILE_VER 2

// Unit version history:
// v2: Extended: keyboard (added 'Key Waiting'), memory (LC mem type for II/II+, inverted MF_INTCXROM bit)
// v3: Extended: memory (added 'AnnunciatorN')
// v4: Extended: video (added 'Video Refresh Rate')
// v5: Extended: cpu (added 'Defer IRQ By 1 Opcode')
#define UNIT_APPLE2_VER 5

#define UNIT_SLOTS_VER 1

//-----------------------------------------------------------------------------

void Snapshot_SetFilename(const std::string & strPathname)
{
	if (strPathname.empty())
	{
		g_strSaveStateFilename = DEFAULT_SNAPSHOT_NAME;

		g_strSaveStatePathname = g_sCurrentDir;
		if (g_strSaveStatePathname.length() && g_strSaveStatePathname[g_strSaveStatePathname.length()-1] != '\\')
			g_strSaveStatePathname += "\\";
		g_strSaveStatePathname.append(DEFAULT_SNAPSHOT_NAME);

		g_strSaveStatePath = g_sCurrentDir;

		return;
	}

	std::string strFilename = strPathname;	// Set default, as maybe there's no path
	g_strSaveStatePath.clear();

	int nIdx = strPathname.find_last_of('\\');
	if (nIdx >= 0 && nIdx+1 < (int)strPathname.length())
	{
		strFilename = &strPathname[nIdx+1];
		g_strSaveStatePath = strPathname.substr(0, nIdx+1); // Bugfix: 1.25.0.2 // Snapshot_LoadState() -> SetCurrentImageDir() -> g_sCurrentDir 
	}

	g_strSaveStateFilename = strFilename;
	g_strSaveStatePathname = strPathname;
}

const std::string & Snapshot_GetFilename()
{
	return g_strSaveStateFilename;
}

const std::string & Snapshot_GetPath()
{
	return g_strSaveStatePath;
}

//-----------------------------------------------------------------------------

static HANDLE m_hFile = INVALID_HANDLE_VALUE;
static CConfigNeedingRestart m_ConfigNew;

static std::string GetSnapshotUnitApple2Name(void)
{
	static const std::string name("Apple2");
	return name;
}

static std::string GetSnapshotUnitSlotsName(void)
{
	static const std::string name("Slots");
	return name;
}

#define SS_YAML_KEY_MODEL "Model"

#define SS_YAML_VALUE_APPLE2			"Apple]["
#define SS_YAML_VALUE_APPLE2PLUS		"Apple][+"
#define SS_YAML_VALUE_APPLE2E			"Apple//e"
#define SS_YAML_VALUE_APPLE2EENHANCED	"Enhanced Apple//e"
#define SS_YAML_VALUE_APPLE2C			"Apple2c"
#define SS_YAML_VALUE_PRAVETS82			"Pravets82"
#define SS_YAML_VALUE_PRAVETS8M			"Pravets8M"
#define SS_YAML_VALUE_PRAVETS8A			"Pravets8A"
#define SS_YAML_VALUE_TK30002E			"TK3000//e"

static eApple2Type ParseApple2Type(std::string type)
{
	if (type == SS_YAML_VALUE_APPLE2)				return A2TYPE_APPLE2;
	else if (type == SS_YAML_VALUE_APPLE2PLUS)		return A2TYPE_APPLE2PLUS;
	else if (type == SS_YAML_VALUE_APPLE2E)			return A2TYPE_APPLE2E;
	else if (type == SS_YAML_VALUE_APPLE2EENHANCED)	return A2TYPE_APPLE2EENHANCED;
	else if (type == SS_YAML_VALUE_APPLE2C)			return A2TYPE_APPLE2C;
	else if (type == SS_YAML_VALUE_PRAVETS82)		return A2TYPE_PRAVETS82;
	else if (type == SS_YAML_VALUE_PRAVETS8M)		return A2TYPE_PRAVETS8M;
	else if (type == SS_YAML_VALUE_PRAVETS8A)		return A2TYPE_PRAVETS8A;
	else if (type == SS_YAML_VALUE_TK30002E)		return A2TYPE_TK30002E;

	throw std::string("Load: Unknown Apple2 type");
}

static std::string GetApple2TypeAsString(void)
{
	switch ( GetApple2Type() )
	{
		case A2TYPE_APPLE2:			return SS_YAML_VALUE_APPLE2;
		case A2TYPE_APPLE2PLUS:		return SS_YAML_VALUE_APPLE2PLUS;
		case A2TYPE_APPLE2E:		return SS_YAML_VALUE_APPLE2E;
		case A2TYPE_APPLE2EENHANCED:return SS_YAML_VALUE_APPLE2EENHANCED;
		case A2TYPE_APPLE2C:		return SS_YAML_VALUE_APPLE2C;
		case A2TYPE_PRAVETS82:		return SS_YAML_VALUE_PRAVETS82;
		case A2TYPE_PRAVETS8M:		return SS_YAML_VALUE_PRAVETS8M;
		case A2TYPE_PRAVETS8A:		return SS_YAML_VALUE_PRAVETS8A;
		case A2TYPE_TK30002E:		return SS_YAML_VALUE_TK30002E;
		default:
			throw std::string("Save: Unknown Apple2 type");
	}
}

//---

static UINT ParseFileHdr(void)
{
	std::string scalar;
	if (!yamlHelper.GetScalar(scalar))
		throw std::string(SS_YAML_KEY_FILEHDR ": Failed to find scalar");

	if (scalar != SS_YAML_KEY_FILEHDR)
		throw std::string("Failed to find file header");

	yamlHelper.GetMapStartEvent();

	YamlLoadHelper yamlLoadHelper(yamlHelper);

	//

	std::string value = yamlLoadHelper.LoadString(SS_YAML_KEY_TAG);
	if (value != SS_YAML_VALUE_AWSS)
	{
		//printf("%s: Bad tag (%s) - expected %s\n", SS_YAML_KEY_FILEHDR, value.c_str(), SS_YAML_VALUE_AWSS);
		throw std::string(SS_YAML_KEY_FILEHDR ": Bad tag");
	}

	return yamlLoadHelper.LoadUint(SS_YAML_KEY_VERSION);
}

//---

static void ParseUnitApple2(YamlLoadHelper& yamlLoadHelper, UINT version)
{
	if (version == 0 || version > UNIT_APPLE2_VER)
		throw std::string(SS_YAML_KEY_UNIT ": Apple2: Version mismatch");

	std::string model = yamlLoadHelper.LoadString(SS_YAML_KEY_MODEL);
	SetApple2Type( ParseApple2Type(model) );	// NB. Sets default main CPU type
	m_ConfigNew.m_Apple2Type = GetApple2Type();

	CpuLoadSnapshot(yamlLoadHelper, version);	// NB. Overrides default main CPU type
	m_ConfigNew.m_CpuType = GetMainCpu();

	JoyLoadSnapshot(yamlLoadHelper);
	KeybLoadSnapshot(yamlLoadHelper, version);
	SpkrLoadSnapshot(yamlLoadHelper);
	VideoLoadSnapshot(yamlLoadHelper, version);
	MemLoadSnapshot(yamlLoadHelper, version);

	// g_Apple2Type may've changed: so redraw frame (title, buttons, leds, etc)
	VideoReinitialize();	// g_CharsetType changed
	FrameUpdateApple2Type();	// Calls VideoRedrawScreen() before the aux mem has been loaded (so if DHGR is enabled, then aux mem will be zeros at this stage)
}

//---

static void ParseSlots(YamlLoadHelper& yamlLoadHelper, UINT unitVersion)
{
	if (unitVersion != UNIT_SLOTS_VER)
		throw std::string(SS_YAML_KEY_UNIT ": Slots: Version mismatch");

	while (1)
	{
		std::string scalar = yamlLoadHelper.GetMapNextSlotNumber();
		if (scalar.empty())
			break;	// done all slots

		const int slot = strtoul(scalar.c_str(), NULL, 10);	// NB. aux slot supported as a different "unit"
															// NB. slot-0 only supported for Apple II or II+ (or similar clones)
		if (slot < 0 || slot > 7)
			throw std::string("Slots: Invalid slot #: ") + scalar;

		yamlLoadHelper.GetSubMap(scalar);

		std::string card = yamlLoadHelper.LoadString(SS_YAML_KEY_CARD);
		UINT cardVersion = yamlLoadHelper.LoadUint(SS_YAML_KEY_VERSION);

		if (!yamlLoadHelper.GetSubMap(std::string(SS_YAML_KEY_STATE)))
			throw std::string(SS_YAML_KEY_UNIT ": Expected sub-map name: " SS_YAML_KEY_STATE);

		SS_CARDTYPE type = CT_Empty;
		bool bRes = false;

		if (card == Printer_GetSnapshotCardName())
		{
			bRes = Printer_LoadSnapshot(yamlLoadHelper, slot, cardVersion);
			type = CT_GenericPrinter;
		}
		else if (card == CSuperSerialCard::GetSnapshotCardName())
		{
			type = CT_SSC;
			g_CardMgr.Insert(slot, type);
			bRes = dynamic_cast<CSuperSerialCard&>(g_CardMgr.GetRef(slot)).LoadSnapshot(yamlLoadHelper, slot, cardVersion);
		}
		else if (card == CMouseInterface::GetSnapshotCardName())
		{
			type = CT_MouseInterface;
			g_CardMgr.Insert(slot, type);
			bRes = dynamic_cast<CMouseInterface&>(g_CardMgr.GetRef(slot)).LoadSnapshot(yamlLoadHelper, slot, cardVersion);
		}
		else if (card == Z80_GetSnapshotCardName())
		{
			bRes = Z80_LoadSnapshot(yamlLoadHelper, slot, cardVersion);
			type = CT_Z80;
		}
		else if (card == MB_GetSnapshotCardName())
		{
			bRes = MB_LoadSnapshot(yamlLoadHelper, slot, cardVersion);
			type = CT_MockingboardC;
		}
		else if (card == Phasor_GetSnapshotCardName())
		{
			bRes = Phasor_LoadSnapshot(yamlLoadHelper, slot, cardVersion);
			type = CT_Phasor;
		}
		else if (card == Disk2InterfaceCard::GetSnapshotCardName())
		{
			type = CT_Disk2;
			g_CardMgr.Insert(slot, type);
			bRes = dynamic_cast<Disk2InterfaceCard&>(g_CardMgr.GetRef(slot)).LoadSnapshot(yamlLoadHelper, slot, cardVersion);
		}
		else if (card == HD_GetSnapshotCardName())
		{
			bRes = HD_LoadSnapshot(yamlLoadHelper, slot, cardVersion, g_strSaveStatePath);
			m_ConfigNew.m_bEnableHDD = true;
			type = CT_GenericHDD;
		}
		else if (card == LanguageCardSlot0::GetSnapshotCardName())
		{
			type = CT_LanguageCard;
			SetExpansionMemType(type);
			CreateLanguageCard();
			bRes = GetLanguageCard()->LoadSnapshot(yamlLoadHelper, slot, cardVersion);
		}
		else if (card == Saturn128K::GetSnapshotCardName())
		{
			type = CT_Saturn128K;
			SetExpansionMemType(type);
			CreateLanguageCard();
			bRes = GetLanguageCard()->LoadSnapshot(yamlLoadHelper, slot, cardVersion);
		}
		else
		{
			throw std::string("Slots: Unknown card: " + card);	// todo: don't throw - just ignore & continue
		}

		if (bRes)
		{
			m_ConfigNew.m_Slot[slot] = type;
		}

		yamlLoadHelper.PopMap();
		yamlLoadHelper.PopMap();
	}
}

//---

static void ParseUnit(void)
{
	yamlHelper.GetMapStartEvent();

	YamlLoadHelper yamlLoadHelper(yamlHelper);

	std::string unit = yamlLoadHelper.LoadString(SS_YAML_KEY_TYPE);
	UINT unitVersion = yamlLoadHelper.LoadUint(SS_YAML_KEY_VERSION);

	if (!yamlLoadHelper.GetSubMap(std::string(SS_YAML_KEY_STATE)))
		throw std::string(SS_YAML_KEY_UNIT ": Expected sub-map name: " SS_YAML_KEY_STATE);

	if (unit == GetSnapshotUnitApple2Name())
	{
		ParseUnitApple2(yamlLoadHelper, unitVersion);
	}
	else if (unit == MemGetSnapshotUnitAuxSlotName())
	{
		MemLoadSnapshotAux(yamlLoadHelper, unitVersion);
	}
	else if (unit == GetSnapshotUnitSlotsName())
	{
		ParseSlots(yamlLoadHelper, unitVersion);
	}
	else
	{
		throw std::string(SS_YAML_KEY_UNIT ": Unknown type: " ) + unit;
	}
}

static void Snapshot_LoadState_v2(void)
{
	bool restart = false;	// Only need to restart if any VM state has change

	try
	{
		if (!yamlHelper.InitParser( g_strSaveStatePathname.c_str() ))
			throw std::string("Failed to initialize parser or open file");

		if (ParseFileHdr() != SS_FILE_VER)
			throw std::string("Version mismatch");

		//

		restart = true;

		CConfigNeedingRestart ConfigOld;
		//ConfigOld.m_Slot[0] = CT_LanguageCard;	// fixme: II/II+=LC, //e=empty
		ConfigOld.m_Slot[1] = CT_GenericPrinter;	// fixme
		ConfigOld.m_Slot[2] = CT_SSC;				// fixme
		//ConfigOld.m_Slot[3] = CT_Uthernet;		// todo
		ConfigOld.m_Slot[6] = CT_Disk2;				// fixme
		ConfigOld.m_Slot[7] = ConfigOld.m_bEnableHDD ? CT_GenericHDD : CT_Empty;	// fixme
		//ConfigOld.m_SlotAux = ?;					// fixme

		for (UINT i=0; i<NUM_SLOTS; i++)
			m_ConfigNew.m_Slot[i] = CT_Empty;
		m_ConfigNew.m_SlotAux = CT_Empty;
		m_ConfigNew.m_bEnableHDD = false;
		//m_ConfigNew.m_bEnableTheFreezesF8Rom = ?;	// todo: when support saving config

		MemReset();							// Also calls CpuInitialize()
		PravetsReset();

		if (g_CardMgr.IsSSCInstalled())
		{
			g_CardMgr.GetSSC()->CommReset();
		}
		else
		{
			_ASSERT(g_CardMgr.QuerySlot(SLOT2) == CT_Empty);
			ConfigOld.m_Slot[2] = CT_Empty;
		}

		if (g_CardMgr.QuerySlot(SLOT4) == CT_MouseInterface)
			g_CardMgr.Remove(SLOT4);		// Remove Mouse card from slot-4

		if (g_CardMgr.QuerySlot(SLOT5) == CT_Disk2)
			g_CardMgr.Remove(SLOT5);		// Remove Disk2 card from slot-5

		g_CardMgr.GetDisk2CardMgr().Reset(false);

		HD_Reset();
		HD_SetEnabled(false);

		KeybReset();
		VideoResetState();
		SetVideoRefreshRate(VR_60HZ);		// Default to 60Hz as older save-states won't contain refresh rate
		MB_InitializeForLoadingSnapshot();	// GH#609
#ifdef USE_SPEECH_API
		g_Speech.Reset();
#endif

		std::string scalar;
		while(yamlHelper.GetScalar(scalar))
		{
			if (scalar == SS_YAML_KEY_UNIT)
				ParseUnit();
			else
				throw std::string("Unknown top-level scalar: " + scalar);
		}

		MB_SetCumulativeCycles();
		SetLoadedSaveStateFlag(true);

		// NB. The following disparity should be resolved:
		// . A change in h/w via the Configuration property sheets results in a the VM completely restarting (via WM_USER_RESTART)
		// . A change in h/w via loading a save-state avoids this VM restart
		// The latter is the desired approach (as the former needs a "power-on" / F2 to start things again)

		sg_PropertySheet.ApplyNewConfig(m_ConfigNew, ConfigOld);	// Mainly just saves (some) new state to Registry

		MemInitializeROM();
		MemInitializeCustomF8ROM();
		MemInitializeIO();
		MemInitializeCardExpansionRomFromSnapshot();

		MemUpdatePaging(TRUE);

		SetMouseCardInstalled( g_CardMgr.IsMouseCardInstalled() );
		DebugReset();
		if (g_nAppMode == MODE_DEBUG)
			DebugDisplay(TRUE);
	}
	catch(std::string szMessage)
	{
		MessageBox(	g_hFrameWindow,
					szMessage.c_str(),
					TEXT("Load State"),
					MB_ICONEXCLAMATION | MB_SETFOREGROUND);

		if (restart)
			PostMessage(g_hFrameWindow, WM_USER_RESTART, 0, 0);		// Power-cycle VM (undoing all the new state just loaded)
	}

	yamlHelper.FinaliseParser();
}

void Snapshot_LoadState()
{
	const std::string ext_aws = (".aws");
	const size_t pos = g_strSaveStatePathname.size() - ext_aws.size();
	if (g_strSaveStatePathname.find(ext_aws, pos) != std::string::npos)	// find ".aws" at end of pathname
	{
		MessageBox(	g_hFrameWindow,
					"Save-state v1 no longer supported.\n"
					"Please load using AppleWin 1.27, and re-save as a v2 state file.",
					TEXT("Load State"),
					MB_ICONEXCLAMATION | MB_SETFOREGROUND);

		return;
	}

	Snapshot_LoadState_v2();
}

//-----------------------------------------------------------------------------

// todo:
// . Uthernet card

void Snapshot_SaveState(void)
{
	try
	{
		YamlSaveHelper yamlSaveHelper(g_strSaveStatePathname);
		yamlSaveHelper.FileHdr(SS_FILE_VER);

		// Unit: Apple2
		{
			yamlSaveHelper.UnitHdr(GetSnapshotUnitApple2Name(), UNIT_APPLE2_VER);
			YamlSaveHelper::Label state(yamlSaveHelper, "%s:\n", SS_YAML_KEY_STATE);

			yamlSaveHelper.Save("%s: %s\n", SS_YAML_KEY_MODEL, GetApple2TypeAsString().c_str());
			CpuSaveSnapshot(yamlSaveHelper);
			JoySaveSnapshot(yamlSaveHelper);
			KeybSaveSnapshot(yamlSaveHelper);
			SpkrSaveSnapshot(yamlSaveHelper);
			VideoSaveSnapshot(yamlSaveHelper);
			MemSaveSnapshot(yamlSaveHelper);
		}

		// Unit: Aux slot
		MemSaveSnapshotAux(yamlSaveHelper);

		// Unit: Slots
		{
			yamlSaveHelper.UnitHdr(GetSnapshotUnitSlotsName(), UNIT_SLOTS_VER);
			YamlSaveHelper::Label state(yamlSaveHelper, "%s:\n", SS_YAML_KEY_STATE);

			if (g_CardMgr.QuerySlot(SLOT0) != CT_Empty && IsApple2PlusOrClone(GetApple2Type()))
				GetLanguageCard()->SaveSnapshot(yamlSaveHelper);	// Language Card or Saturn 128K

			if (g_CardMgr.QuerySlot(SLOT1) == CT_GenericPrinter)
				Printer_SaveSnapshot(yamlSaveHelper);

			if (g_CardMgr.QuerySlot(SLOT2) == CT_SSC)
				dynamic_cast<CSuperSerialCard&>(g_CardMgr.GetRef(SLOT2)).SaveSnapshot(yamlSaveHelper);

//			if (g_CardMgr.QuerySlot(SLOT3) == CT_Uthernet)
//				sg_Uthernet.SaveSnapshot(yamlSaveHelper);

			if (g_CardMgr.QuerySlot(SLOT4) == CT_MouseInterface)
				dynamic_cast<CMouseInterface&>(g_CardMgr.GetRef(SLOT4)).SaveSnapshot(yamlSaveHelper);

			if (g_CardMgr.QuerySlot(SLOT4) == CT_Z80)
				Z80_SaveSnapshot(yamlSaveHelper, SLOT4);

			if (g_CardMgr.QuerySlot(SLOT5) == CT_Z80)
				Z80_SaveSnapshot(yamlSaveHelper, SLOT5);

			if (g_CardMgr.QuerySlot(SLOT4) == CT_MockingboardC)
				MB_SaveSnapshot(yamlSaveHelper, SLOT4);

			if (g_CardMgr.QuerySlot(SLOT5) == CT_MockingboardC)
				MB_SaveSnapshot(yamlSaveHelper, SLOT5);

			if (g_CardMgr.QuerySlot(SLOT4) == CT_Phasor)
				Phasor_SaveSnapshot(yamlSaveHelper, SLOT4);

			if (g_CardMgr.QuerySlot(SLOT5) == CT_Disk2)
				dynamic_cast<Disk2InterfaceCard&>(g_CardMgr.GetRef(SLOT5)).SaveSnapshot(yamlSaveHelper);

			if (g_CardMgr.QuerySlot(SLOT6) == CT_Disk2)
				dynamic_cast<Disk2InterfaceCard&>(g_CardMgr.GetRef(SLOT6)).SaveSnapshot(yamlSaveHelper);

			if (g_CardMgr.QuerySlot(SLOT7) == CT_GenericHDD)
				HD_SaveSnapshot(yamlSaveHelper);
		}
	}
	catch(std::string szMessage)
	{
		MessageBox(	g_hFrameWindow,
					szMessage.c_str(),
					TEXT("Save State"),
					MB_ICONEXCLAMATION | MB_SETFOREGROUND);
	}
}

//-----------------------------------------------------------------------------

void Snapshot_Startup()
{
	static bool bDone = false;

	if(!g_bSaveStateOnExit || bDone)
		return;

	Snapshot_LoadState();

	bDone = true;	// Prevents a g_bRestart from loading an old save-state
}

void Snapshot_Shutdown()
{
	static bool bDone = false;

	_ASSERT(!bDone);
	_ASSERT(!g_bRestart);
	if(!g_bSaveStateOnExit || bDone)
		return;

	Snapshot_SaveState();

	bDone = true;	// Debug flag: this func should only be called once, and never on a g_bRestart
}
