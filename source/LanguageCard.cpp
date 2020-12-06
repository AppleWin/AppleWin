/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 1994-1996, Michael O'Brien
Copyright (C) 1999-2001, Oliver Schmidt
Copyright (C) 2002-2005, Tom Charlesworth
Copyright (C) 2006-2018, Tom Charlesworth, Michael Pohoreski, Nick Westgate

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

/* Description: Language Card and Saturn 128K emulation
 *
 * Author: various
 */

#include "StdAfx.h"

#include "LanguageCard.h"
#include "Core.h"
#include "CPU.h"		// GH#700
#include "Log.h"
#include "Memory.h"
#include "YamlHelper.h"


const UINT LanguageCardUnit::kMemModeInitialState = MF_BANK2 | MF_WRITERAM;	// !INTCXROM

LanguageCardUnit::LanguageCardUnit(SS_CARDTYPE type/*=CT_LanguageCardIIe*/) :
	Card(type),
	m_uLastRamWrite(0)
{
	SetMemMainLanguageCard(NULL, true);
}

LanguageCardUnit::~LanguageCardUnit(void)
{
	SetMemMainLanguageCard(NULL);
}

void LanguageCardUnit::InitializeIO(void)
{
	RegisterIoHandler(kSlot0, &LanguageCardUnit::IO, &LanguageCardUnit::IO, NULL, NULL, this, NULL);
}

BYTE __stdcall LanguageCardUnit::IO(WORD PC, WORD uAddr, BYTE bWrite, BYTE uValue, ULONG nExecutedCycles)
{
	LanguageCardUnit* pLC = (LanguageCardUnit*) MemGetSlotParameters(kSlot0);

	DWORD memmode = GetMemMode();
	DWORD lastmemmode = memmode;
	memmode &= ~(MF_BANK2 | MF_HIGHRAM);

	if (!(uAddr & 8))
		memmode |= MF_BANK2;

	if (((uAddr & 2) >> 1) == (uAddr & 1))
		memmode |= MF_HIGHRAM;

	if (uAddr & 1)	// GH#392
	{
		if (!bWrite && pLC->GetLastRamWrite())
		{
			memmode |= MF_WRITERAM; // UTAIIe:5-23
		}
		else if (bWrite && pLC->GetLastRamWrite() && pLC->IsOpcodeRMWabs(uAddr))	// GH#700
		{
			memmode |= MF_WRITERAM;
		}
	}
	else
	{
		memmode &= ~MF_WRITERAM; // UTAIIe:5-23
	}

	pLC->SetLastRamWrite( ((uAddr & 1) && !bWrite) ); // UTAIIe:5-23
	SetMemMode(memmode);

	//

	if (MemOptimizeForModeChanging(PC, uAddr))
		return bWrite ? 0 : MemReadFloatingBus(nExecutedCycles);

	// IF THE MEMORY PAGING MODE HAS CHANGED, UPDATE OUR MEMORY IMAGES AND
	// WRITE TABLES.
	if (lastmemmode != memmode)
	{
		MemUpdatePaging(0);	// Initialize=0
	}

	return bWrite ? 0 : MemReadFloatingBus(nExecutedCycles);
}

bool LanguageCardUnit::IsOpcodeRMWabs(WORD addr)
{
	BYTE param1 = mem[(regs.pc - 2) & 0xffff];
	BYTE param2 = mem[(regs.pc - 1) & 0xffff];
	if (param1 != (addr & 0xff) || param2 != 0xC0)
		return false;

	// GH#404, GH#700: INC $C083,X/C08B,X (RMW) to write enable the LC (any 6502/65C02/816)
	BYTE opcode = mem[(regs.pc - 3) & 0xffff];
	if (opcode == 0xFE && regs.x == 0)	// INC abs,x
		return true;

	// GH#700: INC $C083/C08B (RMW) to write enable the LC (65C02 only)
	if (GetMainCpu() != CPU_65C02)
		return false;

	if (opcode == 0xEE ||	// INC abs
		opcode == 0xCE ||	// DEC abs
		opcode == 0x6E ||	// ROR abs
		opcode == 0x4E ||	// LSR abs
		opcode == 0x2E ||	// ROL abs
		opcode == 0x0E)		// ASL abs
		return true;

	if (opcode == 0x1C ||	// TRB abs
		opcode == 0x0C)		// TSB abs
		return true;

	return false;
}

//-------------------------------------

LanguageCardSlot0::LanguageCardSlot0(SS_CARDTYPE type/*=CT_LanguageCard*/)
	: LanguageCardUnit(type)
{
	m_pMemory = new BYTE[kMemBankSize];
	SetMemMainLanguageCard(m_pMemory);
}

LanguageCardSlot0::~LanguageCardSlot0(void)
{
	delete [] m_pMemory;
	m_pMemory = NULL;
}

//

static const UINT kUNIT_LANGUAGECARD_VER = 1;
static const UINT kSLOT_LANGUAGECARD = LanguageCardUnit::kSlot0;

#define SS_YAML_VALUE_CARD_LANGUAGECARD "Language Card"

#define SS_YAML_KEY_MEMORYMODE "Memory Mode"
#define SS_YAML_KEY_LASTRAMWRITE "Last RAM Write"

std::string LanguageCardSlot0::GetSnapshotMemStructName(void)
{
	static const std::string name("Memory Bank");
	return name;
}

std::string LanguageCardSlot0::GetSnapshotCardName(void)
{
	static const std::string name(SS_YAML_VALUE_CARD_LANGUAGECARD);
	return name;
}

void LanguageCardSlot0::SaveLCState(YamlSaveHelper& yamlSaveHelper)
{
	yamlSaveHelper.SaveHexUint32(SS_YAML_KEY_MEMORYMODE, GetMemMode() & (MF_WRITERAM|MF_HIGHRAM|MF_BANK2));
	yamlSaveHelper.SaveUint(SS_YAML_KEY_LASTRAMWRITE, GetLastRamWrite() ? 1 : 0);
}

void LanguageCardSlot0::LoadLCState(YamlLoadHelper& yamlLoadHelper)
{
	DWORD memMode     = yamlLoadHelper.LoadUint(SS_YAML_KEY_MEMORYMODE) & MF_LANGCARD_MASK;
	BOOL lastRamWrite = yamlLoadHelper.LoadUint(SS_YAML_KEY_LASTRAMWRITE) ? TRUE : FALSE;
	SetMemMode( (GetMemMode() & ~MF_LANGCARD_MASK) | memMode );
	SetLastRamWrite(lastRamWrite);
}

void LanguageCardSlot0::SaveSnapshot(YamlSaveHelper& yamlSaveHelper)
{
	if (!IsApple2PlusOrClone(GetApple2Type()))
	{
		_ASSERT(0);
		LogFileOutput("Warning: Save-state attempted to save %s for //e or above\n", GetSnapshotCardName().c_str());
		return;	// No Language Card support for //e and above
	}

	YamlSaveHelper::Slot slot(yamlSaveHelper, GetSnapshotCardName(), kSLOT_LANGUAGECARD, kUNIT_LANGUAGECARD_VER);
	YamlSaveHelper::Label state(yamlSaveHelper, "%s:\n", SS_YAML_KEY_STATE);

	SaveLCState(yamlSaveHelper);

	{
		YamlSaveHelper::Label state(yamlSaveHelper, "%s:\n", GetSnapshotMemStructName().c_str());
		yamlSaveHelper.SaveMemory(m_pMemory, kMemBankSize);
	}
}

bool LanguageCardSlot0::LoadSnapshot(YamlLoadHelper& yamlLoadHelper, UINT slot, UINT version)
{
	if (slot != kSLOT_LANGUAGECARD)
		throw std::string("Card: wrong slot");

	if (version != kUNIT_LANGUAGECARD_VER)
		throw std::string("Card: wrong version");

	// "State"
	LoadLCState(yamlLoadHelper);

	if (!m_pMemory)
	{
		m_pMemory = new BYTE[kMemBankSize];
	}

	if (!yamlLoadHelper.GetSubMap(GetSnapshotMemStructName()))
		throw std::string("Memory: Missing map name: " + GetSnapshotMemStructName());

	yamlLoadHelper.LoadMemory(m_pMemory, kMemBankSize);

	yamlLoadHelper.PopMap();

	// NB. MemUpdatePaging(TRUE) called at end of Snapshot_LoadState_v2()

	return true;
}

//-------------------------------------

Saturn128K::Saturn128K(UINT banks)
	: LanguageCardSlot0(CT_Saturn128K)
{
	m_uSaturnTotalBanks = (banks == 0) ? kMaxSaturnBanks : banks;
	m_uSaturnActiveBank = 0;

	for (UINT i=0; i<kMaxSaturnBanks; i++)
		m_aSaturnBanks[i] = NULL;

	m_aSaturnBanks[0] = m_pMemory;	// Reuse memory allocated in base ctor

	for (UINT i = 1; i < m_uSaturnTotalBanks; i++)
		m_aSaturnBanks[i] = new BYTE[kMemBankSize]; // Saturn banks are 16K, max 8 banks/card

	SetMemMainLanguageCard( m_aSaturnBanks[ m_uSaturnActiveBank ] );
}

Saturn128K::~Saturn128K(void)
{
	m_aSaturnBanks[0] = NULL;	// just zero this - deallocated in base ctor

	for (UINT i = 1; i < m_uSaturnTotalBanks; i++)
	{
		if (m_aSaturnBanks[i])
		{
			delete [] m_aSaturnBanks[i];
			m_aSaturnBanks[i] = NULL;
		}
	}
}

void Saturn128K::SetMemorySize(UINT banks)
{
	m_uSaturnTotalBanks = banks;
}

UINT Saturn128K::GetActiveBank(void)
{
	return m_uSaturnActiveBank;
}

void Saturn128K::InitializeIO(void)
{
	RegisterIoHandler(kSlot0, &Saturn128K::IO, &Saturn128K::IO, NULL, NULL, this, NULL);
}

BYTE __stdcall Saturn128K::IO(WORD PC, WORD uAddr, BYTE bWrite, BYTE uValue, ULONG nExecutedCycles)
{
/*
	Bin   Addr.
	      $C0N0 4K Bank A, RAM read, Write protect
	      $C0N1 4K Bank A, ROM read, Write enabled
	      $C0N2 4K Bank A, ROM read, Write protect
	      $C0N3 4K Bank A, RAM read, Write enabled
	0100  $C0N4 select 16K Bank 1
	0101  $C0N5 select 16K Bank 2
	0110  $C0N6 select 16K Bank 3
	0111  $C0N7 select 16K Bank 4
	      $C0N8 4K Bank B, RAM read, Write protect
	      $C0N9 4K Bank B, ROM read, Write enabled
	      $C0NA 4K Bank B, ROM read, Write protect
	      $C0NB 4K Bank B, RAM read, Write enabled
	1100  $C0NC select 16K Bank 5
	1101  $C0ND select 16K Bank 6
	1110  $C0NE select 16K Bank 7
	1111  $C0NF select 16K Bank 8
*/
	Saturn128K* pLC = (Saturn128K*) MemGetSlotParameters(kSlot0);

	_ASSERT(pLC->m_uSaturnTotalBanks);
	if (!pLC->m_uSaturnTotalBanks)
		return bWrite ? 0 : MemReadFloatingBus(nExecutedCycles);

	bool bBankChanged = false;
	DWORD memmode=0, lastmemmode=0;

	if (uAddr & (1<<2))
	{
		pLC->m_uSaturnActiveBank = 0 // Saturn 128K Language Card Bank 0 .. 7
			| (uAddr >> 1) & 4
			| (uAddr >> 0) & 3;

		if (pLC->m_uSaturnActiveBank >= pLC->m_uSaturnTotalBanks)
		{
			// EG. Run RAMTEST128K tests on a Saturn 64K card
			// TODO: Saturn::UpdatePaging() should deal with this case:
			// . Technically read floating-bus, write to nothing
			// . But the mem cache doesn't support floating-bus reads from non-I/O space
			pLC->m_uSaturnActiveBank = pLC->m_uSaturnTotalBanks-1;	// FIXME: just prevent crash for now!
		}

		SetMemMainLanguageCard( pLC->m_aSaturnBanks[ pLC->m_uSaturnActiveBank ] );
		bBankChanged = true;
	}
	else
	{
		memmode = GetMemMode();
		lastmemmode = memmode;
		memmode &= ~(MF_BANK2 | MF_HIGHRAM);

		if (!(uAddr & 8))
			memmode |= MF_BANK2;

		if (((uAddr & 2) >> 1) == (uAddr & 1))
			memmode |= MF_HIGHRAM;

		if (uAddr & 1 && pLC->GetLastRamWrite())// Saturn differs from Apple's 16K LC: any access (LC is read-only)
			memmode |= MF_WRITERAM;
		else
			memmode &= ~MF_WRITERAM;

		pLC->SetLastRamWrite(uAddr & 1);		// Saturn differs from Apple's 16K LC: any access (LC is read-only)
		SetMemMode(memmode);
	}

	// NB. Unlike LC, no need to check if next opcode is STA $C002-5, as Saturn is not for //e

	// IF THE MEMORY PAGING MODE HAS CHANGED, UPDATE OUR MEMORY IMAGES AND
	// WRITE TABLES.
	if ((lastmemmode != memmode) || bBankChanged)
	{
		MemUpdatePaging(0);	// Initialize=0
	}

	return bWrite ? 0 : MemReadFloatingBus(nExecutedCycles);
}

//

static const UINT kUNIT_SATURN_VER = 1;
static const UINT kSLOT_SATURN = LanguageCardUnit::kSlot0;

#define SS_YAML_VALUE_CARD_SATURN128 "Saturn 128"

#define SS_YAML_KEY_NUM_SATURN_BANKS "Num Saturn Banks"
#define SS_YAML_KEY_ACTIVE_SATURN_BANK "Active Saturn Bank"

std::string Saturn128K::GetSnapshotMemStructName(void)
{
	static const std::string name("Memory Bank");
	return name;
}

std::string Saturn128K::GetSnapshotCardName(void)
{
	static const std::string name(SS_YAML_VALUE_CARD_SATURN128);
	return name;
}

void Saturn128K::SaveSnapshot(YamlSaveHelper& yamlSaveHelper)
{
	if (!IsApple2PlusOrClone(GetApple2Type()))
	{
		_ASSERT(0);
		LogFileOutput("Warning: Save-state attempted to save %s for //e or above\n", GetSnapshotCardName().c_str());
		return;	// No Saturn support for //e and above
	}

	YamlSaveHelper::Slot slot(yamlSaveHelper, GetSnapshotCardName(), kSLOT_SATURN, kUNIT_SATURN_VER);
	YamlSaveHelper::Label state(yamlSaveHelper, "%s:\n", SS_YAML_KEY_STATE);

	SaveLCState(yamlSaveHelper);

	yamlSaveHelper.Save("%s: 0x%02X   # [1..8] 4=64K, 8=128K card\n", SS_YAML_KEY_NUM_SATURN_BANKS, m_uSaturnTotalBanks);
	yamlSaveHelper.Save("%s: 0x%02X # [0..7]\n", SS_YAML_KEY_ACTIVE_SATURN_BANK, m_uSaturnActiveBank);

	for(UINT uBank = 0; uBank < m_uSaturnTotalBanks; uBank++)
	{
		LPBYTE pMemBase = m_aSaturnBanks[uBank];
		YamlSaveHelper::Label state(yamlSaveHelper, "%s%02X:\n", GetSnapshotMemStructName().c_str(), uBank);
		yamlSaveHelper.SaveMemory(pMemBase, kMemBankSize);
	}
}

bool Saturn128K::LoadSnapshot(YamlLoadHelper& yamlLoadHelper, UINT slot, UINT version)
{
	if (slot != kSLOT_SATURN)	// fixme
		throw std::string("Card: wrong slot");

	if (version != kUNIT_SATURN_VER)
		throw std::string("Card: wrong version");

	// "State"
	LoadLCState(yamlLoadHelper);

	UINT numBanks   = yamlLoadHelper.LoadUint(SS_YAML_KEY_NUM_SATURN_BANKS);
	UINT activeBank = yamlLoadHelper.LoadUint(SS_YAML_KEY_ACTIVE_SATURN_BANK);

	if (numBanks < 1 || numBanks > kMaxSaturnBanks || activeBank >= numBanks)
		throw std::string(SS_YAML_KEY_UNIT ": Bad Saturn card state");

	m_uSaturnTotalBanks = numBanks;
	m_uSaturnActiveBank = activeBank;

	//

	for(UINT uBank = 0; uBank < m_uSaturnTotalBanks; uBank++)
	{
		if (!m_aSaturnBanks[uBank])
		{
			m_aSaturnBanks[uBank] = new BYTE[kMemBankSize];
		}

		// "Memory Bankxx"
		char szBank[3];
		sprintf(szBank, "%02X", uBank);
		std::string memName = GetSnapshotMemStructName() + szBank;

		if (!yamlLoadHelper.GetSubMap(memName))
			throw std::string("Memory: Missing map name: " + memName);

		yamlLoadHelper.LoadMemory(m_aSaturnBanks[uBank], kMemBankSize);

		yamlLoadHelper.PopMap();
	}

	SetMemMainLanguageCard( m_aSaturnBanks[ m_uSaturnActiveBank ] );

	// NB. MemUpdatePaging(TRUE) called at end of Snapshot_LoadState_v2()

	return true;
}
