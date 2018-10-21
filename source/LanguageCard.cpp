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

#include "Applewin.h"
#include "LanguageCard.h"
#include "Log.h"
#include "Memory.h"
#include "YamlHelper.h"


const UINT LanguageCardUnit::kMemModeInitialState = MF_BANK2 | MF_WRITERAM;	// !INTCXROM

LanguageCardUnit::LanguageCardUnit(void) :
	m_uLastRamWrite(0),
	m_type(MEM_TYPE_LANGUAGECARD_IIE)
{
}

DWORD LanguageCardUnit::SetPaging(WORD address, DWORD memmode, BOOL& modechanging, bool write)
{
	memmode &= ~(MF_BANK2 | MF_HIGHRAM);

	if (!(address & 8))
		memmode |= MF_BANK2;

	if (((address & 2) >> 1) == (address & 1))
		memmode |= MF_HIGHRAM;

	if (address & 1)	// GH#392
	{
		if (!write && GetLastRamWrite())
		{
			memmode |= MF_WRITERAM; // UTAIIe:5-23
		}
	}
	else
	{
		memmode &= ~MF_WRITERAM; // UTAIIe:5-23
	}

	SetLastRamWrite( ((address & 1) && !write) ); // UTAIIe:5-23

	return memmode;
}

//-------------------------------------

LanguageCardSlot0::LanguageCardSlot0(void)
{
	m_type = MEM_TYPE_LANGUAGECARD_SLOT0;
	m_pMemory = (LPBYTE) VirtualAlloc(NULL, kMemBankSize, MEM_COMMIT, PAGE_READWRITE);
	SetMemMainLanguageCard(m_pMemory);
}

LanguageCardSlot0::~LanguageCardSlot0(void)
{
	VirtualFree(m_pMemory, 0, MEM_RELEASE);
	m_pMemory = NULL;
}

//

static const UINT kUNIT_LANGUAGECARD_VER = 1;
static const UINT kSLOT_LANGUAGECARD = 0;

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
		m_pMemory = (LPBYTE) VirtualAlloc(NULL, kMemBankSize, MEM_COMMIT, PAGE_READWRITE);
		if (!m_pMemory)
			throw std::string("Card: mem alloc failed");
	}

	if (!yamlLoadHelper.GetSubMap(GetSnapshotMemStructName()))
		throw std::string("Memory: Missing map name: " + GetSnapshotMemStructName());

	yamlLoadHelper.LoadMemory(m_pMemory, kMemBankSize);

	yamlLoadHelper.PopMap();

	//

	SetExpansionMemType(MEM_TYPE_LANGUAGECARD_SLOT0);
	SetMemMainLanguageCard(m_pMemory);

	// NB. MemUpdatePaging(TRUE) called at end of Snapshot_LoadState_v2()

	return true;
}

//-------------------------------------

Saturn128K::Saturn128K(UINT banks)
{
	m_type = MEM_TYPE_SATURN;
	m_uSaturnTotalBanks = (banks == 0) ? kMaxSaturnBanks : banks;
	m_uSaturnActiveBank = 0;

	for (UINT i=0; i<kMaxSaturnBanks; i++)
		m_aSaturnBanks[i] = NULL;

	for (UINT i = 0; i < m_uSaturnTotalBanks; i++)
		m_aSaturnBanks[i] = (LPBYTE) VirtualAlloc(NULL, kMemBankSize, MEM_COMMIT, PAGE_READWRITE); // Saturn banks are 16K, max 8 banks/card

	SetMemMainLanguageCard( m_aSaturnBanks[ m_uSaturnActiveBank ] );
}

Saturn128K::~Saturn128K(void)
{
	for (UINT i = 0; i < m_uSaturnTotalBanks; i++)
	{
		if (m_aSaturnBanks[i])
		{
			VirtualFree(m_aSaturnBanks[i], 0, MEM_RELEASE);
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

DWORD Saturn128K::SetPaging(WORD address, DWORD memmode, BOOL& modechanging, bool /*write*/)
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
	_ASSERT(m_uSaturnTotalBanks);
	if (!m_uSaturnTotalBanks)
		return memmode;

	if (address & (1<<2))
	{
		m_uSaturnActiveBank = 0 // Saturn 128K Language Card Bank 0 .. 7
			| (address >> 1) & 4
			| (address >> 0) & 3;

		if (m_uSaturnActiveBank >= m_uSaturnTotalBanks)
		{
			// EG. Run RAMTEST128K tests on a Saturn 64K card
			// TODO: Saturn::UpdatePaging() should deal with this case:
			// . Technically read floating-bus, write to nothing
			// . But the mem cache doesn't support floating-bus reads from non-I/O space
			m_uSaturnActiveBank = m_uSaturnTotalBanks-1;	// FIXME: just prevent crash for now!
		}

		SetMemMainLanguageCard( m_aSaturnBanks[ m_uSaturnActiveBank ] );

		modechanging = 1;
	}
	else
	{
		memmode &= ~(MF_BANK2 | MF_HIGHRAM);

		if (!(address & 8))
			memmode |= MF_BANK2;

		if (((address & 2) >> 1) == (address & 1))
			memmode |= MF_HIGHRAM;

		if (address & 1 && GetLastRamWrite())	// Saturn differs from Apple's 16K LC: any access (LC is read-only)
			memmode |= MF_WRITERAM;
		else
			memmode &= ~MF_WRITERAM;

		SetLastRamWrite(address & 1);	// Saturn differs from Apple's 16K LC: any access (LC is read-only)
	}

	return memmode;
}

//

static const UINT kUNIT_SATURN_VER = 1;
static const UINT kSLOT_SATURN = 0;

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
		LPBYTE pBank = m_aSaturnBanks[uBank];
		if (!pBank)
		{
			pBank = m_aSaturnBanks[uBank] = (LPBYTE) VirtualAlloc(NULL, kMemBankSize, MEM_COMMIT, PAGE_READWRITE);
			if (!pBank)
				throw std::string("Card: mem alloc failed");
		}

		// "Memory Bankxx"
		char szBank[3];
		sprintf(szBank, "%02X", uBank);
		std::string memName = GetSnapshotMemStructName() + szBank;

		if (!yamlLoadHelper.GetSubMap(memName))
			throw std::string("Memory: Missing map name: " + memName);

		yamlLoadHelper.LoadMemory(pBank, kMemBankSize);

		yamlLoadHelper.PopMap();
	}

	SetExpansionMemType(MEM_TYPE_SATURN);
	SetMemMainLanguageCard( m_aSaturnBanks[ m_uSaturnActiveBank ] );

	// NB. MemUpdatePaging(TRUE) called at end of Snapshot_LoadState_v2()

	return true;
}
