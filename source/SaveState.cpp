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

#include "SaveState_Structs_v1.h"
#include "SaveState_Structs_v2.h"
#include "YamlHelper.h"

#include "AppleWin.h"
#include "CPU.h"
#include "Disk.h"
#include "Frame.h"
#include "Joystick.h"
#include "Keyboard.h"
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

#include "Configuration\Config.h"
#include "Configuration\IPropertySheet.h"


#define DEFAULT_SNAPSHOT_NAME "SaveState.aws.yaml"

bool g_bSaveStateOnExit = false;

static std::string g_strSaveStateFilename;
static std::string g_strSaveStatePathname;
static std::string g_strSaveStatePath;

static YamlHelper yamlHelper;

//-----------------------------------------------------------------------------

void Snapshot_SetFilename(std::string strPathname)
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

const char* Snapshot_GetFilename()
{
	return g_strSaveStateFilename.c_str();
}

const char* Snapshot_GetPath()
{
	return g_strSaveStatePath.c_str();
}

//-----------------------------------------------------------------------------

static void Snapshot_LoadState_v1()	// .aws v1.0.0.1, up to (and including) AppleWin v1.25.0
{
	std::string strOldImageDir(g_sCurrentDir);

	APPLEWIN_SNAPSHOT_v1* pSS = (APPLEWIN_SNAPSHOT_v1*) new char[sizeof(APPLEWIN_SNAPSHOT_v1)];	// throw's bad_alloc

	try
	{
#if _MSC_VER >= 1600	// static_assert supported from VS2010 (cl.exe v16.00)
		static_assert(kSnapshotSize_v1 == sizeof(APPLEWIN_SNAPSHOT_v1), "Save-state v1 struct size mismatch");
#else
		// A compile error here means sizeof(APPLEWIN_SNAPSHOT_v1) is wrong, eg. one of the constituent structs has been modified
		typedef char VerifySizesAreEqual[kSnapshotSize_v1 == sizeof(APPLEWIN_SNAPSHOT_v1) ? 1 : -1];
#endif

		if (kSnapshotSize_v1 != sizeof(APPLEWIN_SNAPSHOT_v1))
			throw std::string("Save-state v1 struct size mismatch");

		SetCurrentImageDir(g_strSaveStatePath.c_str());	// Allow .dsk's load without prompting

		memset(pSS, 0, sizeof(APPLEWIN_SNAPSHOT_v1));

		//

		HANDLE hFile = CreateFile(	g_strSaveStatePathname.c_str(),
									GENERIC_READ,
									0,
									NULL,
									OPEN_EXISTING,
									FILE_ATTRIBUTE_NORMAL,
									NULL);

		if(hFile == INVALID_HANDLE_VALUE)
			throw std::string("File not found: ") + g_strSaveStatePathname;

		DWORD dwBytesRead;
		BOOL bRes = ReadFile(	hFile,
								pSS,
								sizeof(APPLEWIN_SNAPSHOT_v1),
								&dwBytesRead,
								NULL);

		CloseHandle(hFile);

		if(!bRes || (dwBytesRead != sizeof(APPLEWIN_SNAPSHOT_v1)))
			// File size wrong: probably because of version mismatch or corrupt file
			throw std::string("File size mismatch");

		if(pSS->Hdr.dwTag != AW_SS_TAG)
			throw std::string("File corrupt");

		if(pSS->Hdr.dwVersion != MAKE_VERSION(1,0,0,1))
			throw std::string("Version mismatch");

		// TO DO: Verify checksum

		//
		// Reset all sub-systems
		MemReset();
		DiskReset();
		KeybReset();
		VideoResetState();
		MB_Reset();

		//
		// Apple2 unit
		//

		SS_CPU6502& CPU = pSS->Apple2Unit.CPU6502;
		CpuSetSnapshot_v1(CPU.A, CPU.X, CPU.Y, CPU.P, CPU.S, CPU.PC, CPU.nCumulativeCycles);

		SS_IO_Comms& SSC = pSS->Apple2Unit.Comms;
		sg_SSC.SetSnapshot_v1(SSC.baudrate, SSC.bytesize, SSC.commandbyte, SSC.comminactivity, SSC.controlbyte, SSC.parity, SSC.stopbits);

		JoySetSnapshot_v1(pSS->Apple2Unit.Joystick.nJoyCntrResetCycle);
		KeybSetSnapshot_v1(pSS->Apple2Unit.Keyboard.nLastKey);
		SpkrSetSnapshot_v1(pSS->Apple2Unit.Speaker.nSpkrLastCycle);
		VideoSetSnapshot_v1(pSS->Apple2Unit.Video.bAltCharSet, pSS->Apple2Unit.Video.dwVidMode);
		MemSetSnapshot_v1(pSS->Apple2Unit.Memory.dwMemMode, pSS->Apple2Unit.Memory.bLastWriteRam, pSS->Apple2Unit.Memory.nMemMain, pSS->Apple2Unit.Memory.nMemAux);

		//

		//
		// Slot4: Mockingboard
		MB_SetSnapshot_v1(&pSS->Mockingboard1, 4);

		//
		// Slot5: Mockingboard
		MB_SetSnapshot_v1(&pSS->Mockingboard2, 5);

		//
		// Slot6: Disk][
		DiskSetSnapshot_v1(&pSS->Disk2);

		SetLoadedSaveStateFlag(true);

		MemUpdatePaging(TRUE);
	}
	catch(std::string szMessage)
	{
		MessageBox(	g_hFrameWindow,
					szMessage.c_str(),
					TEXT("Load State"),
					MB_ICONEXCLAMATION | MB_SETFOREGROUND);

		SetCurrentImageDir(strOldImageDir.c_str());

		PostMessage(g_hFrameWindow, WM_USER_RESTART, 0, 0);		// Power-cycle VM (undoing all the new state just loaded)
	}

	delete [] pSS;
}

//-----------------------------------------------------------------------------

static HANDLE m_hFile = INVALID_HANDLE_VALUE;
static CConfigNeedingRestart m_ConfigNew;

static void Snapshot_LoadState_FileHdr(SS_FILE_HDR& Hdr)
{
	try
	{
		m_hFile = CreateFile(	g_strSaveStatePathname.c_str(),
							GENERIC_READ,
							0,
							NULL,
							OPEN_EXISTING,
							FILE_ATTRIBUTE_NORMAL,
							NULL);

		if(m_hFile == INVALID_HANDLE_VALUE)
			throw std::string("File not found: ") + g_strSaveStatePathname;

		DWORD dwBytesRead;
		BOOL bRes = ReadFile(	m_hFile,
								&Hdr,
								sizeof(Hdr),
								&dwBytesRead,
								NULL);

		if(!bRes || (dwBytesRead != sizeof(Hdr)))
			throw std::string("File size mismatch");

		if(Hdr.dwTag != AW_SS_TAG)
			throw std::string("File corrupt");
	}
	catch(std::string szMessage)
	{
		MessageBox(	g_hFrameWindow,
					szMessage.c_str(),
					TEXT("Load State"),
					MB_ICONEXCLAMATION | MB_SETFOREGROUND);

		if(m_hFile == INVALID_HANDLE_VALUE)
		{
			CloseHandle(m_hFile);
			m_hFile = INVALID_HANDLE_VALUE;
		}
	}
}

static void LoadUnitApple2(DWORD Length, DWORD Version)
{
	if (Version != UNIT_APPLE2_VER)
		throw std::string("Apple2: Version mismatch");

	if (Length != sizeof(SS_APPLE2_Unit_v2))
		throw std::string("Apple2: Length mismatch");

	if (SetFilePointer(m_hFile, -(LONG)sizeof(SS_UNIT_HDR), NULL, FILE_CURRENT) == INVALID_SET_FILE_POINTER)
		throw std::string("Apple2: file corrupt");

	SS_APPLE2_Unit_v2 Apple2Unit;
	DWORD dwBytesRead;
	BOOL bRes = ReadFile(	m_hFile,
							&Apple2Unit,
							Length,
							&dwBytesRead,
							NULL);

	if (dwBytesRead != Length)
		throw std::string("Apple2: file corrupt");

	g_Apple2Type = (eApple2Type) Apple2Unit.Apple2Type;
	m_ConfigNew.m_Apple2Type = g_Apple2Type;

	CpuSetSnapshot(Apple2Unit.CPU6502);
	JoySetSnapshot(Apple2Unit.Joystick.JoyCntrResetCycle, &Apple2Unit.Joystick.Joystick0Trim[0], &Apple2Unit.Joystick.Joystick1Trim[0]);
	KeybSetSnapshot(Apple2Unit.Keyboard.LastKey);
	SpkrSetSnapshot(Apple2Unit.Speaker.SpkrLastCycle);
	VideoSetSnapshot(Apple2Unit.Video);
	MemSetSnapshot(Apple2Unit.Memory);
}

//===

static void LoadCardDisk2(void)
{
	DiskSetSnapshot(m_hFile);
}

static void LoadCardMockingboardC(void)
{
	MB_SetSnapshot(m_hFile);
}

static void LoadCardMouseInterface(void)
{
	sg_Mouse.SetSnapshot(m_hFile);
}

static void LoadCardSSC(void)
{
	sg_SSC.SetSnapshot(m_hFile);
}

static void LoadCardPrinter(void)
{
	Printer_SetSnapshot(m_hFile);
}

static void LoadCardHDD(void)
{
	HD_SetSnapshot(m_hFile, g_strSaveStatePath);
	m_ConfigNew.m_bEnableHDD = true;
}

static void LoadCardPhasor(void)
{
	Phasor_SetSnapshot(m_hFile);
}

static void LoadCardZ80(void)
{
	Z80_SetSnapshot(m_hFile);
}

static void LoadCard80ColAuxMem(void)
{
	MemSetSnapshotAux(m_hFile);
}

//===

static void LoadUnitCard(DWORD Length, DWORD Version)
{
	SS_CARD_HDR Card;

	if (Version != UNIT_APPLE2_VER)
		throw std::string("Card: Version mismatch");

	if (Length < sizeof(Card))
		throw std::string("Card: file corrupt");

	if (SetFilePointer(m_hFile, -(LONG)sizeof(Card.UnitHdr), NULL, FILE_CURRENT) == INVALID_SET_FILE_POINTER)
		throw std::string("Card: file corrupt");

	DWORD dwBytesRead;
	BOOL bRes = ReadFile(	m_hFile,
							&Card,
							sizeof(Card),
							&dwBytesRead,
							NULL);

	if (dwBytesRead != sizeof(Card))
		throw std::string("Card: file corrupt");

	//currently cards are changed by restarting machine (ie. all slot empty) then adding cards (most of which are hardcoded to specific slots)

	if (SetFilePointer(m_hFile, -(LONG)sizeof(SS_CARD_HDR), NULL, FILE_CURRENT) == INVALID_SET_FILE_POINTER)
		throw std::string("Card: file corrupt");

	bool bIsCardSupported = true;

	switch(Card.Type)
	{
	case CT_Empty:
		throw std::string("Card: todo");
		break;
	case CT_Disk2:
		LoadCardDisk2();
		break;
	case CT_SSC:
		LoadCardSSC();
		break;
	case CT_MockingboardC:
		LoadCardMockingboardC();
		break;
	case CT_GenericPrinter:
		LoadCardPrinter();
		break;
	case CT_GenericHDD:
		LoadCardHDD();
		break;
	case CT_MouseInterface:
		LoadCardMouseInterface();
		break;
	case CT_Z80:
		LoadCardZ80();
		break;
	case CT_Phasor:
		LoadCardPhasor();
		break;
	case CT_80Col:
	case CT_Extended80Col:
	case CT_RamWorksIII:
		LoadCard80ColAuxMem();
		break;
	default:
		//throw std::string("Card: unsupported");
		bIsCardSupported = false;
		if (SetFilePointer(m_hFile, Card.UnitHdr.hdr.v2.Length, NULL, FILE_CURRENT) == INVALID_SET_FILE_POINTER)
			throw std::string("Card: failed to skip unsupported card");
	}

	if (bIsCardSupported)
	{
		if (Card.Slot <= 7)
			m_ConfigNew.m_Slot[Card.Slot] = (SS_CARDTYPE) Card.Type;
		else
			m_ConfigNew.m_SlotAux         = (SS_CARDTYPE) Card.Type;
	}
}

// Todo:
// . Should this newly loaded config state be persisted to the Registry?
//   - NB. it will get saved if the user opens the Config dialog + makes a change. Is this confusing to the user?
// Notes:
// . WindowScale - don't think this needs restoring (eg. like FullScreen)

static void LoadUnitConfig(DWORD Length, DWORD Version)
{
	SS_APPLEWIN_CONFIG Config;

	if (Version != UNIT_CONFIG_VER)
		throw std::string("Config: Version mismatch");

	if (Length != sizeof(Config))
		throw std::string("Config: Length mismatch");

	if (SetFilePointer(m_hFile, -(LONG)sizeof(Config.UnitHdr), NULL, FILE_CURRENT) == INVALID_SET_FILE_POINTER)
		throw std::string("Config: file corrupt");

	DWORD dwBytesRead;
	BOOL bRes = ReadFile(	m_hFile,
							&Config,
							Length,
							&dwBytesRead,
							NULL);

	if (dwBytesRead != Length)
		throw std::string("Config: file corrupt");

	// Restore all config state

	//Config.Cfg.AppleWinVersion	// Nothing to do
	g_eVideoType = Config.Cfg.VideoMode;
	g_uHalfScanLines = Config.Cfg.IsHalfScanLines;
	g_bConfirmReboot = Config.Cfg.IsConfirmReboot;
	monochrome = Config.Cfg.MonochromeColor;
	//Config.Cfg.WindowScale		// NB. Just calling SetViewportScale() is no good. Use PostMessage() instead.

	g_dwSpeed = Config.Cfg.CpuSpeed;
	SetCurrentCLK6502();

	JoySetJoyType(JN_JOYSTICK0, Config.Cfg.JoystickType[0]);
	JoySetJoyType(JN_JOYSTICK1, Config.Cfg.JoystickType[1]);
	sg_PropertySheet.SetJoystickCursorControl(Config.Cfg.IsAllowCursorsToBeRead);
	sg_PropertySheet.SetAutofire(Config.Cfg.IsAutofire);
	sg_PropertySheet.SetJoystickCenteringControl(Config.Cfg.IsKeyboardAutocentering);
	//Config.Cfg.IsSwapButton0and1;	// TBD: not implemented yet
	sg_PropertySheet.SetScrollLockToggle(Config.Cfg.IsScrollLockToggle);
	sg_PropertySheet.SetMouseShowCrosshair(Config.Cfg.IsMouseShowCrosshair);
	sg_PropertySheet.SetMouseRestrictToWindow(Config.Cfg.IsMouseRestrictToWindow);

	soundtype = Config.Cfg.SoundType;
	SpkrSetVolume(Config.Cfg.SpeakerVolume, sg_PropertySheet.GetVolumeMax());
	MB_SetVolume(Config.Cfg.MockingboardVolume, sg_PropertySheet.GetVolumeMax());

	enhancedisk = Config.Cfg.IsEnhancedDiskSpeed;
	g_bSaveStateOnExit = Config.Cfg.IsSaveStateOnExit ? true : false;

	g_bPrinterAppend = Config.Cfg.IsAppendToFile ? true : false;
	sg_PropertySheet.SetTheFreezesF8Rom(Config.Cfg.IsUsingFreezesF8Rom);
}

//---

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

	throw std::string("Load: Unknown Apple2 type");
}

static std::string GetApple2Type(void)
{
	switch (g_Apple2Type)
	{
		case A2TYPE_APPLE2:			return SS_YAML_VALUE_APPLE2;
		case A2TYPE_APPLE2PLUS:		return SS_YAML_VALUE_APPLE2PLUS;
		case A2TYPE_APPLE2E:		return SS_YAML_VALUE_APPLE2E;
		case A2TYPE_APPLE2EENHANCED:return SS_YAML_VALUE_APPLE2EENHANCED;
		case A2TYPE_APPLE2C:		return SS_YAML_VALUE_APPLE2C;
		case A2TYPE_PRAVETS82:		return SS_YAML_VALUE_PRAVETS82;
		case A2TYPE_PRAVETS8M:		return SS_YAML_VALUE_PRAVETS8M;
		case A2TYPE_PRAVETS8A:		return SS_YAML_VALUE_PRAVETS8A;
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

	std::string value = yamlLoadHelper.GetMapValueSTRING(SS_YAML_KEY_TAG);
	if (value != SS_YAML_VALUE_AWSS)
	{
		//printf("%s: Bad tag (%s) - expected %s\n", SS_YAML_KEY_FILEHDR, value.c_str(), SS_YAML_VALUE_AWSS);
		throw std::string(SS_YAML_KEY_FILEHDR ": Bad tag");
	}

	return yamlLoadHelper.GetMapValueUINT(SS_YAML_KEY_VERSION);
}

//---

static void ParseUnitApple2(YamlLoadHelper& yamlLoadHelper, UINT version)
{
	if (version != UNIT_APPLE2_VER)
		throw std::string(SS_YAML_KEY_UNIT ": Apple2: Version mismatch");

	std::string model = yamlLoadHelper.GetMapValueSTRING(SS_YAML_KEY_MODEL);
	g_Apple2Type = ParseApple2Type(model);

	CpuLoadSnapshot(yamlLoadHelper);
	JoyLoadSnapshot(yamlLoadHelper);
	KeybLoadSnapshot(yamlLoadHelper);
	SpkrLoadSnapshot(yamlLoadHelper);
	VideoLoadSnapshot(yamlLoadHelper);
	MemLoadSnapshot(yamlLoadHelper);
}

//---

static void ParseSlots(YamlLoadHelper& yamlLoadHelper, UINT version)
{
	if (version != UNIT_SLOTS_VER)
		throw std::string(SS_YAML_KEY_UNIT ": Slots: Version mismatch");

	while (1)
	{
		std::string scalar = yamlLoadHelper.GetMapNextSlotNumber();
		if (scalar.empty())
			break;	// done all slots

		const int slot = strtoul(scalar.c_str(), NULL, 10);	// NB. aux slot supported as a different "unit"
		if (slot < 1 || slot > 7)
			throw std::string("Slots: Invalid slot #: ") + scalar;

		yamlLoadHelper.GetSubMap(scalar);

		std::string card = yamlLoadHelper.GetMapValueSTRING(SS_YAML_KEY_CARD);
		UINT version     = yamlLoadHelper.GetMapValueUINT(SS_YAML_KEY_VERSION);

		if (!yamlLoadHelper.GetSubMap(std::string(SS_YAML_KEY_STATE)))
			throw std::string(SS_YAML_KEY_UNIT ": Expected sub-map name: " SS_YAML_KEY_STATE);

		bool bIsCardSupported = true;
		SS_CARDTYPE type = CT_Empty;
		bool bRes = false;

		if (card == Printer_GetSnapshotCardName())
		{
			bRes = Printer_LoadSnapshot(yamlLoadHelper, slot, version);
			type = CT_GenericPrinter;
		}
		else if (card == sg_SSC.GetSnapshotCardName())
		{
			bRes = sg_SSC.LoadSnapshot(yamlLoadHelper, slot, version);
			type = CT_SSC;
		}
		else if (card == sg_Mouse.GetSnapshotCardName())
		{
			bRes = sg_Mouse.LoadSnapshot(yamlLoadHelper, slot, version);
			type = CT_MouseInterface;
		}
		else if (card == Z80_GetSnapshotCardName())
		{
			bRes = Z80_LoadSnapshot(yamlLoadHelper, slot, version);
			type = CT_Z80;
		}
		else if (card == MB_GetSnapshotCardName())
		{
			bRes = MB_LoadSnapshot(yamlLoadHelper, slot, version);
			type = CT_MockingboardC;
		}
		else if (card == Phasor_GetSnapshotCardName())
		{
			bRes = Phasor_LoadSnapshot(yamlLoadHelper, slot, version);
			type = CT_Phasor;
		}
		else if (card == DiskGetSnapshotCardName())
		{
			bRes = DiskLoadSnapshot(yamlLoadHelper, slot, version);
			type = CT_Disk2;
		}
		else if (card == HD_GetSnapshotCardName())
		{
			bRes = HD_LoadSnapshot(yamlLoadHelper, slot, version, g_strSaveStatePath);
			m_ConfigNew.m_bEnableHDD = true;
			type = CT_GenericHDD;
		}
		else
		{
			bIsCardSupported = false;
			throw std::string("Slots: Unknown card: " + card);	// todo: don't throw - just ignore & continue
		}

		if (bRes && bIsCardSupported)
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

	std::string unit = yamlLoadHelper.GetMapValueSTRING(SS_YAML_KEY_TYPE);
	UINT version = yamlLoadHelper.GetMapValueUINT(SS_YAML_KEY_VERSION);

	if (!yamlLoadHelper.GetSubMap(std::string(SS_YAML_KEY_STATE)))
		throw std::string(SS_YAML_KEY_UNIT ": Expected sub-map name: " SS_YAML_KEY_STATE);

	if (unit == GetSnapshotUnitApple2Name())
	{
		ParseUnitApple2(yamlLoadHelper, version);
	}
	else if (unit == MemGetSnapshotUnitAuxSlotName())
	{
		MemLoadSnapshotAux(yamlLoadHelper, version);
	}
	else if (unit == GetSnapshotUnitSlotsName())
	{
		ParseSlots(yamlLoadHelper, version);
	}
	else if (unit == SS_YAML_VALUE_UNIT_CONFIG)
	{
		//...
	}
	else
	{
		throw std::string(SS_YAML_KEY_UNIT ": Unknown type: " ) + unit;
	}
}

static void Snapshot_LoadState_v2(void)
{
	try
	{
		int res = yamlHelper.InitParser( g_strSaveStatePathname.c_str() );
		if (!res)
			throw std::string("Failed to initialize parser or open file");	// TODO: disambiguate

		UINT version = ParseFileHdr();
		if (version != SS_FILE_VER)
			throw std::string("Version mismatch");

		//

		CConfigNeedingRestart ConfigOld;
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
		//m_ConfigNew.m_bEnhanceDisk = ?;			// todo: when support saving config

		MemReset();
		PravetsReset();
		DiskReset();
		KeybReset();
		VideoResetState();
		MB_Reset();
#ifdef USE_SPEECH_API
		g_Speech.Reset();
#endif
		sg_Mouse.Uninitialize();
		sg_Mouse.Reset();
		HD_SetEnabled(false);

		while(1)
		{
			std::string scalar;
			if (!yamlHelper.GetScalar(scalar))
				break;

			if (scalar == SS_YAML_KEY_UNIT)
				ParseUnit();
			else
				throw std::string("Unknown top-level scalar: " + scalar);
		}

		SetLoadedSaveStateFlag(true);

		// NB. The following disparity should be resolved:
		// . A change in h/w via the Configuration property sheets results in a the VM completely restarting (via WM_USER_RESTART)
		// . A change in h/w via loading a save-state avoids this VM restart
		// The latter is the desired approach (as the former needs a "power-on" / F2 to start things again)

		sg_PropertySheet.ApplyNewConfig(m_ConfigNew, ConfigOld);

		MemInitializeROM();
		MemInitializeCustomF8ROM();
		MemInitializeIO();

		MemUpdatePaging(TRUE);
	}
	catch(std::string szMessage)
	{
		MessageBox(	g_hFrameWindow,
					szMessage.c_str(),
					TEXT("Load State"),
					MB_ICONEXCLAMATION | MB_SETFOREGROUND);

		PostMessage(g_hFrameWindow, WM_USER_RESTART, 0, 0);		// Power-cycle VM (undoing all the new state just loaded)
	}

	yamlHelper.FinaliseParser();
}

static void Snapshot_LoadState_v2(DWORD dwVersion)
{
	try
	{
		if (dwVersion != MAKE_VERSION(2,0,0,0))
			throw std::string("Version mismatch");

		CConfigNeedingRestart ConfigOld;
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
		//m_ConfigNew.m_bEnhanceDisk = ?;			// todo: when support saving config

		MemReset();
		PravetsReset();
		DiskReset();
		KeybReset();
		VideoResetState();
		MB_Reset();
#ifdef USE_SPEECH_API
		g_Speech.Reset();
#endif
		sg_Mouse.Uninitialize();
		sg_Mouse.Reset();
		HD_SetEnabled(false);

		while(1)
		{
			SS_UNIT_HDR UnitHdr;
			DWORD dwBytesRead;
			BOOL bRes = ReadFile(	m_hFile,
									&UnitHdr,
									sizeof(UnitHdr),
									&dwBytesRead,
									NULL);

			if (dwBytesRead == 0)
				break;	// EOF (OK)

			if(!bRes || (dwBytesRead != sizeof(UnitHdr)))
				throw std::string("File size mismatch");

			switch (UnitHdr.hdr.v2.Type)
			{
			case UT_Apple2:
				LoadUnitApple2(UnitHdr.hdr.v2.Length, UnitHdr.hdr.v2.Version);
				break;
			case UT_Card:
				LoadUnitCard(UnitHdr.hdr.v2.Length, UnitHdr.hdr.v2.Version);
				break;
			case UT_Config:
				LoadUnitConfig(UnitHdr.hdr.v2.Length, UnitHdr.hdr.v2.Version);
				break;
			default:
				// Log then skip unsupported unit type
				break;
			}
		}

		SetLoadedSaveStateFlag(true);

		// NB. The following disparity should be resolved:
		// . A change in h/w via the Configuration property sheets results in a the VM completely restarting (via WM_USER_RESTART)
		// . A change in h/w via loading a save-state avoids this VM restart
		// The latter is the desired approach (as the former needs a "power-on" / F2 to start things again)

		sg_PropertySheet.ApplyNewConfig(m_ConfigNew, ConfigOld);

		MemInitializeROM();
		MemInitializeCustomF8ROM();
		MemInitializeIO();

		MemUpdatePaging(TRUE);
	}
	catch(std::string szMessage)
	{
		MessageBox(	g_hFrameWindow,
					szMessage.c_str(),
					TEXT("Load State"),
					MB_ICONEXCLAMATION | MB_SETFOREGROUND);

		PostMessage(g_hFrameWindow, WM_USER_RESTART, 0, 0);		// Power-cycle VM (undoing all the new state just loaded)
	}

	CloseHandle(m_hFile);
	m_hFile = INVALID_HANDLE_VALUE;
}

void Snapshot_LoadState()
{
	const std::string ext_yaml = (".yaml");
	const size_t pos = g_strSaveStatePathname.size() - ext_yaml.size();
	if (g_strSaveStatePathname.find(ext_yaml, pos) != std::string::npos)	// find ".yaml" at end of pathname
	{
		Snapshot_LoadState_v2();
		return;
	}

	//

	SS_FILE_HDR Hdr;
	Snapshot_LoadState_FileHdr(Hdr);

	if (m_hFile == INVALID_HANDLE_VALUE)
		return;

	if(Hdr.dwVersion <= MAKE_VERSION(1,0,0,1))
	{
		CloseHandle(m_hFile);
		m_hFile = INVALID_HANDLE_VALUE;

		Snapshot_LoadState_v1();
		return;
	}

	Snapshot_LoadState_v2(Hdr.dwVersion);
}

//-----------------------------------------------------------------------------

// Todo:
// . "Uthernet Active" - save this in slot3 card's state?
// Notes:
// . Full Screen - don't think this needs save/restoring

static void SaveUnitConfig()
{
	SS_APPLEWIN_CONFIG Config;
	memset(&Config, 0, sizeof(Config));

	Config.UnitHdr.hdr.v2.Length = sizeof(Config);
	Config.UnitHdr.hdr.v2.Type = UT_Config;
	Config.UnitHdr.hdr.v2.Version = UNIT_CONFIG_VER;

	//

	memcpy(Config.Cfg.AppleWinVersion, GetAppleWinVersion(), sizeof(Config.Cfg.AppleWinVersion));
	Config.Cfg.VideoMode = g_eVideoType;
	Config.Cfg.IsHalfScanLines = g_uHalfScanLines;
	Config.Cfg.IsConfirmReboot = g_bConfirmReboot;
	Config.Cfg.MonochromeColor = monochrome;
	Config.Cfg.WindowScale = GetViewportScale();
	Config.Cfg.CpuSpeed = g_dwSpeed;

	Config.Cfg.JoystickType[0] = JoyGetJoyType(JN_JOYSTICK0);
	Config.Cfg.JoystickType[1] = JoyGetJoyType(JN_JOYSTICK1);
	Config.Cfg.IsAllowCursorsToBeRead = sg_PropertySheet.GetJoystickCursorControl();
	Config.Cfg.IsAutofire = (sg_PropertySheet.GetAutofire(1)<<1) | sg_PropertySheet.GetAutofire(0);
	Config.Cfg.IsKeyboardAutocentering = sg_PropertySheet.GetJoystickCenteringControl();
	Config.Cfg.IsSwapButton0and1 = 0;	// TBD: not implemented yet
	Config.Cfg.IsScrollLockToggle = sg_PropertySheet.GetScrollLockToggle();
	Config.Cfg.IsMouseShowCrosshair = sg_PropertySheet.GetMouseShowCrosshair();
	Config.Cfg.IsMouseRestrictToWindow = sg_PropertySheet.GetMouseRestrictToWindow();

	Config.Cfg.SoundType = soundtype;
	Config.Cfg.SpeakerVolume = SpkrGetVolume();
	Config.Cfg.MockingboardVolume = MB_GetVolume();

	Config.Cfg.IsEnhancedDiskSpeed = enhancedisk;
	Config.Cfg.IsSaveStateOnExit = g_bSaveStateOnExit;

	Config.Cfg.IsAppendToFile = g_bPrinterAppend;
	Config.Cfg.IsUsingFreezesF8Rom = sg_PropertySheet.GetTheFreezesF8Rom();

	//

	DWORD dwBytesWritten;
	BOOL bRes = WriteFile(	m_hFile,
							&Config,
							Config.UnitHdr.hdr.v2.Length,
							&dwBytesWritten,
							NULL);

	if(!bRes || (dwBytesWritten != Config.UnitHdr.hdr.v2.Length))
	{
		//dwError = GetLastError();
		throw std::string("Save error: Config");
	}
}

// todo:
// . Uthernet card

#if 1
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

			yamlSaveHelper.Save("%s: %s\n", SS_YAML_KEY_MODEL, GetApple2Type().c_str());
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

			Printer_SaveSnapshot(yamlSaveHelper);

			sg_SSC.SaveSnapshot(yamlSaveHelper);

			sg_Mouse.SaveSnapshot(yamlSaveHelper);

			if (g_Slot4 == CT_Z80)
				Z80_SaveSnapshot(yamlSaveHelper, 4);

			if (g_Slot5 == CT_Z80)
				Z80_SaveSnapshot(yamlSaveHelper, 5);

			if (g_Slot4 == CT_MockingboardC)
				MB_SaveSnapshot(yamlSaveHelper, 4);

			if (g_Slot5 == CT_MockingboardC)
				MB_SaveSnapshot(yamlSaveHelper, 5);

			if (g_Slot4 == CT_Phasor)
				Phasor_SaveSnapshot(yamlSaveHelper, 4);

			DiskSaveSnapshot(yamlSaveHelper);

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
#else
void Snapshot_SaveState()
{
	try
	{
		m_hFile = CreateFile(	g_strSaveStatePathname.c_str(),
								GENERIC_WRITE,
								0,
								NULL,
								CREATE_ALWAYS,
								FILE_ATTRIBUTE_NORMAL,
								NULL);

		DWORD dwError = GetLastError();
		_ASSERT((dwError == 0) || (dwError == ERROR_ALREADY_EXISTS));

		// todo: handle ERROR_ALREADY_EXISTS - ask if user wants to replace existing file
		// - at this point any old file will have been truncated to zero

		if(m_hFile == INVALID_HANDLE_VALUE)
		{
			//dwError = GetLastError();
			throw std::string("Save error");
		}

		//

		APPLEWIN_SNAPSHOT_v2 AppleSnapshot;

		AppleSnapshot.Hdr.dwTag = AW_SS_TAG;
		AppleSnapshot.Hdr.dwVersion = MAKE_VERSION(2,0,0,0);
		AppleSnapshot.Hdr.dwChecksum = 0;	// TO DO

		SS_APPLE2_Unit_v2& Apple2Unit = AppleSnapshot.Apple2Unit;

		//
		// Apple2 unit
		//

		Apple2Unit.UnitHdr.hdr.v2.Length = sizeof(Apple2Unit);
		Apple2Unit.UnitHdr.hdr.v2.Type = UT_Apple2;
		Apple2Unit.UnitHdr.hdr.v2.Version = UNIT_APPLE2_VER;

		Apple2Unit.Apple2Type = g_Apple2Type;

		CpuGetSnapshot(Apple2Unit.CPU6502);
		JoyGetSnapshot(Apple2Unit.Joystick.JoyCntrResetCycle, &Apple2Unit.Joystick.Joystick0Trim[0], &Apple2Unit.Joystick.Joystick1Trim[0]);
		KeybGetSnapshot(Apple2Unit.Keyboard.LastKey);
		SpkrGetSnapshot(Apple2Unit.Speaker.SpkrLastCycle);
		VideoGetSnapshot(Apple2Unit.Video);
		MemGetSnapshot(Apple2Unit.Memory);

		DWORD dwBytesWritten;
		BOOL bRes = WriteFile(	m_hFile,
								&AppleSnapshot,
								sizeof(AppleSnapshot),
								&dwBytesWritten,
								NULL);

		if(!bRes || (dwBytesWritten != sizeof(AppleSnapshot)))
		{
			//dwError = GetLastError();
			throw std::string("Save error");
		}

		//

		MemGetSnapshotAux(m_hFile);

		Printer_GetSnapshot(m_hFile);

		sg_SSC.GetSnapshot(m_hFile);

		sg_Mouse.GetSnapshot(m_hFile);

		if (g_Slot4 == CT_Z80)
			Z80_GetSnapshot(m_hFile, 4);

		if (g_Slot5 == CT_Z80)
			Z80_GetSnapshot(m_hFile, 5);

		if (g_Slot4 == CT_MockingboardC)
			MB_GetSnapshot(m_hFile, 4);

		if (g_Slot5 == CT_MockingboardC)
			MB_GetSnapshot(m_hFile, 5);

		if (g_Slot4 == CT_Phasor)
			Phasor_GetSnapshot(m_hFile);

		DiskGetSnapshot(m_hFile);

		HD_GetSnapshot(m_hFile);

		//

		SaveUnitConfig();
	}
	catch(std::string szMessage)
	{
		MessageBox(	g_hFrameWindow,
					szMessage.c_str(),
					TEXT("Save State"),
					MB_ICONEXCLAMATION | MB_SETFOREGROUND);
	}

	CloseHandle(m_hFile);
	m_hFile = INVALID_HANDLE_VALUE;
}
#endif

//-----------------------------------------------------------------------------

void Snapshot_Startup()
{
	static bool bDone = false;

	if(!g_bSaveStateOnExit || bDone)
		return;

	Snapshot_LoadState();

	bDone = true;
}

void Snapshot_Shutdown()
{
	static bool bDone = false;

	if(!g_bSaveStateOnExit || bDone)
		return;

	Snapshot_SaveState();

	bDone = true;
}
