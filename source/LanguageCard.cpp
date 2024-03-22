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
 *
 * Note: Here's what isn't fully emulated:
 * From UTAII 5-42 (Application Note: Multiple RAM Card Configurations)
 * . For II/II, INHIBIT' (disable motherboard ROM for $D000-$FFFF) and Apple's 16K RAM card isn't correct:
 *   . "If the expansion RAM is not enabled on a RAM card, the ROM on the card will respond to $F800-$FFFF addressing - period."
 *   . In UTAIIe 5-24, Sather describes this as "a particularly nettlesome associated fact"!
 *     . NB. "When INHIBIT' is low on the Apple IIe, all motherboard ROM is disabled, including high RAM."
 *     . Note: I assume a Saturn card "will release the $F800-$FFFF range when RAM on the card is disabled", since there's no F8 ROM on the Saturn.
 *   . Summary: for a II/II+ with an *Apple* 16K RAM card in slot 0, when (High) RAM is disabled, then:
 *     . ROM on the slot 0 card will respond, along with any Saturn card(s) in other slots which pull INHIBIT' low.
 *     . *** AppleWin emulates a slot 0 LC as if the Sather h/w mod had been applied.
 * . [UTAII 5-42] "Enable two RAM cards for writing simultaneously..."
 *     "both RAM cards will accept the data from a single store instruction to the $D000-$FFFF range"
 *   *** AppleWin only stores to the last accessed RAM card.
 * . Presumably enabling two RAM cards for reading RAM will both respond and the result is the OR-sum?
 *   *** AppleWin only loads from the last accessed RAM card.
 * . The 16K RAM card has a socket for an F8 ROM, whereas the Saturn card doesn't.
 * Also see UTAII 6-6, where Firmware card and 16K RAM card are described.
 * . Sather refers to the Apple 16K RAM card, which is just the Apple Language Card.
 *
 */

#include "StdAfx.h"

#include "LanguageCard.h"
#include "CardManager.h"
#include "Core.h"
#include "CPU.h"		// GH#700
#include "Log.h"
#include "Memory.h"
#include "YamlHelper.h"


const UINT LanguageCardUnit::kMemModeInitialState = MF_BANK2 | MF_WRITERAM;	// !INTCXROM

LanguageCardUnit * LanguageCardUnit::create(UINT slot)
{
	return new LanguageCardUnit(CT_LanguageCardIIe, slot);
}

LanguageCardUnit::LanguageCardUnit(SS_CARDTYPE type, UINT slot) :
	Card(type, slot),
	m_uLastRamWrite(0),
	m_memMode(kMemModeInitialState),
	m_pMemory(NULL)
{
	if (type != CT_Saturn128K && m_slot != LanguageCardUnit::kSlot0)
		ThrowErrorInvalidSlot();

	if (m_slot == SLOT0)
		SetMemMainLanguageCard(NULL, SLOT0, true);
}

LanguageCardUnit::~LanguageCardUnit(void)
{
	// Nothing to do for SetMemMainLanguageCard():
	// . if //e, then no ptr to clean up (since just using memmain)
	// . else: subclass will do ptr clean up
}

void LanguageCardUnit::Reset(const bool powerCycle)
{
	// For power on: card's ctor will have set card's local memmode to LanguageCardUnit::kMemModeInitialState.
	// For reset: II/II+ unaffected, so only for //e or above.
	if (IsAppleIIeOrAbove(GetApple2Type()))
		SetLCMemMode(LanguageCardUnit::kMemModeInitialState);
}

void LanguageCardUnit::InitializeIO(LPBYTE pCxRomPeripheral)
{
	RegisterIoHandler(m_slot, &LanguageCardUnit::IO, &LanguageCardUnit::IO, NULL, NULL, this, NULL);
}

BYTE __stdcall LanguageCardUnit::IO(WORD PC, WORD uAddr, BYTE bWrite, BYTE uValue, ULONG nExecutedCycles)
{
	UINT uSlot = ((uAddr & 0xff) >> 4) - 8;
	LanguageCardUnit* pLC = (LanguageCardUnit*) MemGetSlotParameters(uSlot);
	_ASSERT(uSlot == SLOT0);

	UINT memmode = pLC->GetLCMemMode();
	UINT lastmemmode = memmode;
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
	pLC->SetLCMemMode(memmode);

	const bool bCardChanged = GetCardMgr().GetLanguageCardMgr().GetLastSlotToSetMainMemLC() != SLOT0;
	if (bCardChanged)
	{
		if (pLC->QueryType() == CT_LanguageCardIIe)
			SetMemMainLanguageCard(NULL, SLOT0, true);
		else // CT_LanguageCard
			SetMemMainLanguageCard(pLC->m_pMemory, SLOT0);
	}

	//

	if (MemOptimizeForModeChanging(PC, uAddr))
		return bWrite ? 0 : MemReadFloatingBus(nExecutedCycles);

	// IF THE MEMORY PAGING MODE HAS CHANGED, UPDATE OUR MEMORY IMAGES AND
	// WRITE TABLES.
	if ((lastmemmode != memmode) || bCardChanged)
	{
		// NB. Always SetMemMode() - locally may be same, but card may've changed
		SetMemMode((GetMemMode() & ~MF_LANGCARD_MASK) | (memmode & MF_LANGCARD_MASK));
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

void LanguageCardUnit::SetGlobalLCMemMode(void)
{
	SetMemMode((GetMemMode() & ~MF_LANGCARD_MASK) | (GetLCMemMode() & MF_LANGCARD_MASK));
}

//-------------------------------------

LanguageCardSlot0 * LanguageCardSlot0::create(UINT slot)
{
	return new LanguageCardSlot0(CT_LanguageCard, slot);
}

LanguageCardSlot0::LanguageCardSlot0(SS_CARDTYPE type, UINT slot)
	: LanguageCardUnit(type, slot)
{
	m_pMemory = new BYTE[kMemBankSize];
	if (m_slot == SLOT0)
		SetMemMainLanguageCard(m_pMemory, SLOT0);
}

LanguageCardSlot0::~LanguageCardSlot0(void)
{
	delete [] m_pMemory;
	m_pMemory = NULL;
	if (m_slot == SLOT0)
		SetMemMainLanguageCard(NULL, SLOT0);
}

//

static const UINT kUNIT_LANGUAGECARD_VER = 1;

#define SS_YAML_VALUE_CARD_LANGUAGECARD "Language Card"

#define SS_YAML_KEY_MEMORYMODE "Memory Mode"
#define SS_YAML_KEY_LASTRAMWRITE "Last RAM Write"

const std::string& LanguageCardSlot0::GetSnapshotMemStructName(void)
{
	static const std::string name("Memory Bank");
	return name;
}

const std::string& LanguageCardSlot0::GetSnapshotCardName(void)
{
	static const std::string name(SS_YAML_VALUE_CARD_LANGUAGECARD);
	return name;
}

void LanguageCardSlot0::SaveLCState(YamlSaveHelper& yamlSaveHelper)
{
	yamlSaveHelper.SaveHexUint32(SS_YAML_KEY_MEMORYMODE, GetLCMemMode() & MF_LANGCARD_MASK);
	yamlSaveHelper.SaveUint(SS_YAML_KEY_LASTRAMWRITE, GetLastRamWrite() ? 1 : 0);
}

void LanguageCardSlot0::LoadLCState(YamlLoadHelper& yamlLoadHelper)
{
	UINT memMode      = yamlLoadHelper.LoadUint(SS_YAML_KEY_MEMORYMODE) & MF_LANGCARD_MASK;
	BOOL lastRamWrite = yamlLoadHelper.LoadUint(SS_YAML_KEY_LASTRAMWRITE) ? TRUE : FALSE;
	SetLCMemMode(memMode);
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

	YamlSaveHelper::Slot slot(yamlSaveHelper, GetSnapshotCardName(), m_slot, kUNIT_LANGUAGECARD_VER);
	YamlSaveHelper::Label state(yamlSaveHelper, "%s:\n", SS_YAML_KEY_STATE);

	SaveLCState(yamlSaveHelper);

	{
		YamlSaveHelper::Label state(yamlSaveHelper, "%s:\n", GetSnapshotMemStructName().c_str());
		yamlSaveHelper.SaveMemory(m_pMemory, kMemBankSize);
	}
}

bool LanguageCardSlot0::LoadSnapshot(YamlLoadHelper& yamlLoadHelper, UINT version)
{
	if (version != kUNIT_LANGUAGECARD_VER)
		ThrowErrorInvalidVersion(version);

	// "State"
	LoadLCState(yamlLoadHelper);

	if (!m_pMemory)
	{
		m_pMemory = new BYTE[kMemBankSize];
	}

	if (!yamlLoadHelper.GetSubMap(GetSnapshotMemStructName()))
		throw std::runtime_error("Memory: Missing map name: " + GetSnapshotMemStructName());

	yamlLoadHelper.LoadMemory(m_pMemory, kMemBankSize);

	yamlLoadHelper.PopMap();

	// NB. MemUpdatePaging(TRUE) called at end of Snapshot_LoadState_v2()

	return true;
}

//-------------------------------------

UINT Saturn128K::g_uSaturnBanksFromCmdLine = 0;

Saturn128K::Saturn128K(UINT slot, UINT banks)
	: LanguageCardSlot0(CT_Saturn128K, slot)
{
	m_uSaturnTotalBanks = (banks == 0) ? kMaxSaturnBanks : banks;
	m_uSaturnActiveBank = 0;

	for (UINT i=0; i<kMaxSaturnBanks; i++)
		m_aSaturnBanks[i] = NULL;

	m_aSaturnBanks[0] = m_pMemory;	// Reuse memory allocated in base ctor

	for (UINT i = 1; i < m_uSaturnTotalBanks; i++)
		m_aSaturnBanks[i] = new BYTE[kMemBankSize]; // Saturn banks are 16K, max 8 banks/card

	if (slot == SLOT0)
		::SetMemMainLanguageCard(m_aSaturnBanks[m_uSaturnActiveBank], SLOT0);
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

	// NB. want the Saturn128K object that set the ptr via ::SetMemMainLanguageCard() to now set it to NULL (may be from SLOT0 or another slot)
	// In reality, dtor only called when whole VM is being destroyed, so won't have have use-after-frees.
}

UINT Saturn128K::GetActiveBank(void)
{
	return m_uSaturnActiveBank;
}

void Saturn128K::InitializeIO(LPBYTE pCxRomPeripheral)
{
	RegisterIoHandler(m_slot, &Saturn128K::IO, &Saturn128K::IO, NULL, NULL, this, NULL);
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
	UINT uSlot = ((uAddr & 0xff) >> 4) - 8;
	Saturn128K* pLC = (Saturn128K*) MemGetSlotParameters(uSlot);

	_ASSERT(pLC->m_uSaturnTotalBanks);
	if (!pLC->m_uSaturnTotalBanks)
		return bWrite ? 0 : MemReadFloatingBus(nExecutedCycles);

	bool bBankChanged = false;
	UINT memmode = pLC->GetLCMemMode();
	UINT lastmemmode = memmode;

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

		::SetMemMainLanguageCard(pLC->m_aSaturnBanks[pLC->m_uSaturnActiveBank], uSlot);
		bBankChanged = true;
	}
	else
	{
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
		pLC->SetLCMemMode(memmode);

		bBankChanged = GetCardMgr().GetLanguageCardMgr().GetLastSlotToSetMainMemLC() != uSlot;
		if (bBankChanged)
		{
			::SetMemMainLanguageCard(pLC->m_aSaturnBanks[pLC->m_uSaturnActiveBank], uSlot);
		}
	}

	// NB. Saturn can be put in any slot but MemOptimizeForModeChanging() currently only supports LC in slot 0.
	// . This optimization (check if next opcode is STA $C002-5) isn't essential, so skip it for now.

	// IF THE MEMORY PAGING MODE HAS CHANGED, UPDATE OUR MEMORY IMAGES AND
	// WRITE TABLES.
	if ((lastmemmode != memmode) || bBankChanged)
	{
		// NB. Always SetMemMode() - locally may be same, but card or bank may've changed
		SetMemMode((GetMemMode() & ~MF_LANGCARD_MASK) | (memmode & MF_LANGCARD_MASK));
		MemUpdatePaging(0);	// Initialize=0
	}

	return bWrite ? 0 : MemReadFloatingBus(nExecutedCycles);
}

//

static const UINT kUNIT_SATURN_VER = 1;

#define SS_YAML_VALUE_CARD_SATURN128 "Saturn 128"

#define SS_YAML_KEY_NUM_SATURN_BANKS "Num Saturn Banks"
#define SS_YAML_KEY_ACTIVE_SATURN_BANK "Active Saturn Bank"

const std::string& Saturn128K::GetSnapshotMemStructName(void)
{
	static const std::string name("Memory Bank");
	return name;
}

const std::string& Saturn128K::GetSnapshotCardName(void)
{
	static const std::string name(SS_YAML_VALUE_CARD_SATURN128);
	return name;
}

void Saturn128K::SaveSnapshot(YamlSaveHelper& yamlSaveHelper)
{
	YamlSaveHelper::Slot slot(yamlSaveHelper, GetSnapshotCardName(), m_slot, kUNIT_SATURN_VER);
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

bool Saturn128K::LoadSnapshot(YamlLoadHelper& yamlLoadHelper, UINT version)
{
	if (version != kUNIT_SATURN_VER)
		ThrowErrorInvalidVersion(version);

	// "State"
	LoadLCState(yamlLoadHelper);

	UINT numBanks   = yamlLoadHelper.LoadUint(SS_YAML_KEY_NUM_SATURN_BANKS);
	UINT activeBank = yamlLoadHelper.LoadUint(SS_YAML_KEY_ACTIVE_SATURN_BANK);

	if (numBanks < 1 || numBanks > kMaxSaturnBanks || activeBank >= numBanks)
		throw std::runtime_error(SS_YAML_KEY_UNIT ": Bad Saturn card state");

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
		std::string memName = GetSnapshotMemStructName() + ByteToHexStr(uBank);

		if (!yamlLoadHelper.GetSubMap(memName))
			throw std::runtime_error("Memory: Missing map name: " + memName);

		yamlLoadHelper.LoadMemory(m_aSaturnBanks[uBank], kMemBankSize);

		yamlLoadHelper.PopMap();
	}

	// NB. MemInitializeFromSnapshot() called at end of Snapshot_LoadState_v2():
	// . SetMemMainLanguageCard() for the slot/card that last set the 16KB LC bank
	// . MemUpdatePaging(TRUE)

	return true;
}

void Saturn128K::SetMemMainLanguageCard(void)
{
	::SetMemMainLanguageCard(m_aSaturnBanks[m_uSaturnActiveBank], m_slot);
}

void Saturn128K::SetSaturnMemorySize(UINT banks)
{
	g_uSaturnBanksFromCmdLine = banks;
}

UINT Saturn128K::GetSaturnMemorySize()
{
	return g_uSaturnBanksFromCmdLine;
}

//-------------------------------------

/*
* LangauageCardManager:
* . manage reset for all cards (eg. II/II+'s LC is unaffected, whereas //e's LC is)
* . manage lastSlotToSetMainMemLC
* . TODO: assist with debugger's display of "sNN" for active 16K bank
*/

void LanguageCardManager::Reset(const bool powerCycle /*=false*/)
{
	if (IsApple2PlusOrClone(GetApple2Type()) && !powerCycle)	// For reset : II/II+ unaffected
		return;

	if (GetLanguageCard())
		GetLanguageCard()->SetLastRamWrite(0);

	if (IsApple2PlusOrClone(GetApple2Type()) && GetCardMgr().QuerySlot(SLOT0) == CT_Empty)
		SetMemMode(0);
	else
		SetMemMode(LanguageCardUnit::kMemModeInitialState);
}

void LanguageCardManager::SetMemModeFromSnapshot(void)
{
	// If multiple "Language Cards" (eg. LC+Saturn or 2xSaturn) then setup via the last card that selected the 16KB LC bank.
	// NB. Skip if not Saturn card (ie. a LC), since LC's are only in slot0 and in the ctor it has called SetMainMemLanguageCard()
	if (GetCardMgr().QuerySlot(m_lastSlotToSetMainMemLCFromSnapshot) == CT_Saturn128K)
	{
		Saturn128K& saturn = dynamic_cast<Saturn128K&>(GetCardMgr().GetRef(m_lastSlotToSetMainMemLCFromSnapshot));
		saturn.SetMemMainLanguageCard();
	}

	if (GetCardMgr().QuerySlot(m_lastSlotToSetMainMemLCFromSnapshot) != CT_Empty)
		dynamic_cast<LanguageCardUnit&>(GetCardMgr().GetRef(m_lastSlotToSetMainMemLCFromSnapshot)).SetGlobalLCMemMode();
}

bool LanguageCardManager::SetLanguageCard(SS_CARDTYPE type)
{
	if (type == CT_Empty)
	{
		m_pLanguageCard = NULL;
		return true;
	}

	_ASSERT(GetLanguageCard() == NULL);
	if (GetLanguageCard())
		return false;	// Only support one language card

	switch (type)
	{
	case CT_LanguageCard:
		m_pLanguageCard = LanguageCardSlot0::create(SLOT0);
		break;
	case CT_LanguageCardIIe:
		m_pLanguageCard = LanguageCardUnit::create(SLOT0);
		break;
	case CT_Saturn128K:
		m_pLanguageCard = new Saturn128K(SLOT0, Saturn128K::GetSaturnMemorySize());
		break;
	default:
		_ASSERT(0);
		return false;
	}

	return true;
}
