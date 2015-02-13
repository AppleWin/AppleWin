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

/* Description: Save-state (snapshot) module
 *
 * Author: Copyright (c) 2004-2015 Tom Charlesworth
 */

#include "StdAfx.h"

#include "SaveState_Structs_v1.h"
#include "SaveState_Structs_v2.h"

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
#include "SerialComms.h"
#include "Speaker.h"
#include "Video.h"

#include "Configuration\Config.h"
#include "Configuration\IPropertySheet.h"


#define DEFAULT_SNAPSHOT_NAME "SaveState.aws"

bool g_bSaveStateOnExit = false;

static std::string g_strSaveStateFilename;
static std::string g_strSaveStatePathname;
static std::string g_strSaveStatePath;

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

		if (!IS_APPLE2)
			MemResetPaging();

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
	}

	delete [] pSS;
}

//-----------------------------------------------------------------------------

HANDLE m_hFile = INVALID_HANDLE_VALUE;
CConfigNeedingRestart m_ConfigNew;

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

#define UNIT_APPLE2_VER 1
#define UNIT_CARD_VER 1
#define UNIT_CONFIG_VER 1

static void LoadUnitApple2(DWORD Length, DWORD Version)
{
	SS_APPLE2_Unit_v2 Apple2Unit;

	if (Version != UNIT_APPLE2_VER)
		throw std::string("Apple2: Version mismatch");

	if (Length != sizeof(Apple2Unit))
		throw std::string("Apple2: Length mismatch");

	if (SetFilePointer(m_hFile, -(LONG)sizeof(Apple2Unit.UnitHdr), NULL, FILE_CURRENT) == INVALID_SET_FILE_POINTER)
		throw std::string("Apple2: file corrupt");

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
	JoySetSnapshot(Apple2Unit.Joystick.JoyCntrResetCycle);
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
		throw std::string("Card: todo");
		break;
	case CT_Phasor:
		throw std::string("Card: todo");
		break;
	default:
		//throw std::string("Card: unsupported");
		bIsCardSupported = false;
		if (SetFilePointer(m_hFile, Card.UnitHdr.hdr.v2.Length, NULL, FILE_CURRENT) == INVALID_SET_FILE_POINTER)
			throw std::string("Card: failed to skip unsupported card");
	}

	if (bIsCardSupported)
		m_ConfigNew.m_Slot[Card.Slot] = (SS_CARDTYPE) Card.Type;
}

static void LoadUnitConfig(DWORD Length, DWORD Version)
{
	SS_APPLEWIN_CONFIG Cfg;

	if (Version != UNIT_CONFIG_VER)
		throw std::string("Config: Version mismatch");

	if (Length != sizeof(Cfg))
		throw std::string("Config: Length mismatch");

	if (SetFilePointer(m_hFile, -(LONG)sizeof(Cfg.UnitHdr), NULL, FILE_CURRENT) == INVALID_SET_FILE_POINTER)
		throw std::string("Config: file corrupt");

	DWORD dwBytesRead;
	BOOL bRes = ReadFile(	m_hFile,
							&Cfg,
							Length,
							&dwBytesRead,
							NULL);

	if (dwBytesRead != Length)
		throw std::string("Config: file corrupt");

	// todo:
	//m_ConfigNew.m_bEnhanceDisk;
	//m_ConfigNew.m_bEnableHDD;
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
		ConfigOld.m_Slot[6] = CT_Disk2;				// fixme
		ConfigOld.m_Slot[7] = ConfigOld.m_bEnableHDD ? CT_GenericHDD : CT_Empty;	// fixme

		for (UINT i=0; i<NUM_SLOTS; i++)
			m_ConfigNew.m_Slot[i] = CT_Empty;

		MemReset();

		if (!IS_APPLE2)
			MemResetPaging();

		DiskReset();
		KeybReset();
		VideoResetState();
		MB_Reset();
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

		//PostMessage(g_hFrameWindow, WM_USER_RESTART, 0, 0);	// No, as this power-cycles VM (undoing all the new state just loaded)
	}
	catch(std::string szMessage)
	{
		MessageBox(	g_hFrameWindow,
					szMessage.c_str(),
					TEXT("Load State"),
					MB_ICONEXCLAMATION | MB_SETFOREGROUND);
	}

	CloseHandle(m_hFile);
	m_hFile = INVALID_HANDLE_VALUE;
}

void Snapshot_LoadState()
{
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

void Snapshot_SaveState()
{
	try
	{
		// todo: append '.aws' if missing

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

		if(m_hFile == INVALID_HANDLE_VALUE)
		{
			//dwError = GetLastError();
			throw std::string("Save error");
		}

		//

		APPLEWIN_SNAPSHOT_v2 AppleSnapshotv2;

		AppleSnapshotv2.Hdr.dwTag = AW_SS_TAG;
		AppleSnapshotv2.Hdr.dwVersion = MAKE_VERSION(2,0,0,0);
		AppleSnapshotv2.Hdr.dwChecksum = 0;	// TO DO

		SS_APPLE2_Unit_v2& Apple2Unit = AppleSnapshotv2.Apple2Unit;

		//
		// Apple2 unit
		//

		Apple2Unit.UnitHdr.hdr.v2.Length = sizeof(SS_APPLE2_Unit_v2);
		Apple2Unit.UnitHdr.hdr.v2.Type = UT_Apple2;
		Apple2Unit.UnitHdr.hdr.v2.Version = UNIT_APPLE2_VER;

		Apple2Unit.Apple2Type = g_Apple2Type;

		CpuGetSnapshot(Apple2Unit.CPU6502);
		JoyGetSnapshot(Apple2Unit.Joystick.JoyCntrResetCycle);
		KeybGetSnapshot(Apple2Unit.Keyboard.LastKey);
		SpkrGetSnapshot(Apple2Unit.Speaker.SpkrLastCycle);
		VideoGetSnapshot(Apple2Unit.Video);
		MemGetSnapshot(Apple2Unit.Memory);

		DWORD dwBytesWritten;
		BOOL bRes = WriteFile(	m_hFile,
								&AppleSnapshotv2,
								sizeof(APPLEWIN_SNAPSHOT_v2),
								&dwBytesWritten,
								NULL);

		if(!bRes || (dwBytesWritten != sizeof(APPLEWIN_SNAPSHOT_v2)))
		{
			//dwError = GetLastError();
			throw std::string("Save error");
		}

		//

		Printer_GetSnapshot(m_hFile);

		sg_SSC.GetSnapshot(m_hFile);

		sg_Mouse.GetSnapshot(m_hFile);

		if (g_Slot4 == CT_MockingboardC)
			MB_GetSnapshot(m_hFile, 4);

		if (g_Slot5 == CT_MockingboardC)
			MB_GetSnapshot(m_hFile, 5);

		DiskGetSnapshot(m_hFile);

		HD_GetSnapshot(m_hFile);
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
