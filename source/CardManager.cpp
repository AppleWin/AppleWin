/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 1994-1996, Michael O'Brien
Copyright (C) 1999-2001, Oliver Schmidt
Copyright (C) 2002-2005, Tom Charlesworth
Copyright (C) 2006-2019, Tom Charlesworth, Michael Pohoreski, Nick Westgate

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

/* Description: Card Manager
 *
 * Author: Various
 *
 */

#include "StdAfx.h"

#include "CardManager.h"
#include "Registry.h"

#include "Disk.h"
#include "FourPlay.h"
#include "Harddisk.h"
#include "Mockingboard.h"
#include "MouseInterface.h"
#include "ParallelPrinter.h"
#include "SAM.h"
#include "SerialComms.h"
#include "SNESMAX.h"
#include "Uthernet1.h"
#include "Uthernet2.h"
#include "VidHD.h"
#include "LanguageCard.h"
#include "Memory.h"
#include "z80emu.h"

void CardManager::InsertInternal(UINT slot, SS_CARDTYPE type)
{
	RemoveInternal(slot);

	switch (type)
	{
	case CT_Empty:
		m_slot[slot] = new EmptyCard(slot);
		break;
	case CT_Disk2:
		m_slot[slot] = new Disk2InterfaceCard(slot);
		break;
	case CT_SSC:
		_ASSERT(m_pSSC == NULL);
		if (m_pSSC) break;	// Only support one SSC
		m_slot[slot] = m_pSSC = new CSuperSerialCard(slot);
		break;
	case CT_MockingboardC:
		m_slot[slot] = new MockingboardCard(slot, type);
		break;
	case CT_GenericPrinter:
		_ASSERT(m_pParallelPrinterCard == NULL);
		if (m_pParallelPrinterCard) break;	// Only support one Printer card
		m_slot[slot] = m_pParallelPrinterCard = new ParallelPrinterCard(slot);
		break;
	case CT_GenericHDD:
		m_slot[slot] = new HarddiskInterfaceCard(slot);
		break;
	case CT_GenericClock:
		m_slot[slot] = new DummyCard(type, slot);
		break;
	case CT_MouseInterface:
		_ASSERT(m_pMouseCard == NULL);
		if (m_pMouseCard) break;	// Only support one Mouse card
		m_slot[slot] = m_pMouseCard = new CMouseInterface(slot);
		break;
	case CT_Z80:
		_ASSERT(m_pZ80Card == NULL);
		if (m_pZ80Card) break;	// Only support one Z80 card
		m_slot[slot] = new Z80Card(slot);
		break;
	case CT_Phasor:
		m_slot[slot] = new MockingboardCard(slot, type);
		break;
	case CT_Echo:
		m_slot[slot] = new DummyCard(type, slot);
		break;
	case CT_SAM:
		m_slot[slot] = new SAMCard(slot);
		break;
	case CT_Uthernet:
		m_slot[slot] = new Uthernet1(slot);
		break;
	case CT_FourPlay:
		m_slot[slot] = new FourPlayCard(slot);
		break;
	case CT_SNESMAX:
		m_slot[slot] = new SNESMAXCard(slot);
		break;
	case CT_VidHD:
		m_slot[slot] = new VidHDCard(slot);
		break;
	case CT_Uthernet2:
		m_slot[slot] = new Uthernet2(slot);
		break;

	case CT_LanguageCard:
		_ASSERT(m_pLanguageCard == NULL);
		if (m_pLanguageCard) break;	// Only support one language card
		m_slot[slot] = m_pLanguageCard = LanguageCardSlot0::create(slot);
		break;
	case CT_Saturn128K:
		_ASSERT(m_pLanguageCard == NULL);
		if (m_pLanguageCard) break;	// Only support one language card
		m_slot[slot] = m_pLanguageCard = new Saturn128K(slot, Saturn128K::GetSaturnMemorySize());
		break;
	case CT_LanguageCardIIe:
		_ASSERT(m_pLanguageCard == NULL);
		if (m_pLanguageCard) break;	// Only support one language card
		m_slot[slot] = m_pLanguageCard = LanguageCardUnit::create(slot);
		break;

	default:
		_ASSERT(0);
		break;
	}

	if (m_slot[slot] == NULL)
		Remove(slot);			// creates a new EmptyCard
}

void CardManager::Insert(UINT slot, SS_CARDTYPE type, bool updateRegistry/*=true*/)
{
	InsertInternal(slot, type);
	if (updateRegistry)
		RegSetConfigSlotNewCardType(slot, type);
}

void CardManager::RemoveInternal(UINT slot)
{
	if (m_slot[slot])
	{
		// NB. object deleted below: delete m_slot[slot]
		switch (m_slot[slot]->QueryType())
		{
		case CT_MouseInterface:
			m_pMouseCard = NULL;
			break;
		case CT_SSC:
			m_pSSC = NULL;
			break;
		case CT_GenericPrinter:
			m_pParallelPrinterCard = NULL;
			break;
		case CT_LanguageCard:
		case CT_Saturn128K:
		case CT_LanguageCardIIe:
			m_pLanguageCard = NULL;
			break;
		}

		UnregisterIoHandler(slot);
		delete m_slot[slot];
		m_slot[slot] = NULL;
	}
}

void CardManager::Remove(UINT slot, bool updateRegistry/*=true*/)
{
	Insert(slot, CT_Empty, updateRegistry);
}

void CardManager::InsertAuxInternal(SS_CARDTYPE type)
{
	RemoveAuxInternal();

	switch (type)
	{
	case CT_Empty:
		m_aux = new EmptyCard(SLOT_AUX);
		break;
	case CT_80Col:
		m_aux = new DummyCard(type, SLOT_AUX);
		break;
	case CT_Extended80Col:
		m_aux = new DummyCard(type, SLOT_AUX);
		break;
	case CT_RamWorksIII:
		m_aux = new DummyCard(type, SLOT_AUX);
		break;
	default:
		_ASSERT(0);
		break;
	}

	// for consistency m_aux must never be NULL
	_ASSERT(m_aux != NULL);
	if (m_aux == NULL)
		RemoveAux();	// creates a new EmptyCard
}

void CardManager::InsertAux(SS_CARDTYPE type)
{
	InsertAuxInternal(type);
	RegSetConfigSlotNewCardType(SLOT_AUX, type);
}

void CardManager::RemoveAuxInternal()
{
	delete m_aux;
	m_aux = NULL;
}

void CardManager::RemoveAux(void)
{
	InsertAux(CT_Empty);
}

void CardManager::InitializeIO(LPBYTE pCxRomPeripheral)
{
	// if it is a //e then SLOT0 must be CT_LanguageCardIIe (and the other way round)
	_ASSERT(IsApple2PlusOrClone(GetApple2Type()) != (m_slot[SLOT0]->QueryType() == CT_LanguageCardIIe));

	for (UINT i = SLOT0; i < NUM_SLOTS; ++i)
	{
		if (m_slot[i])
		{
			m_slot[i]->InitializeIO(pCxRomPeripheral);
		}
	}
}

void CardManager::Destroy()
{
	for (UINT i = SLOT0; i < NUM_SLOTS; ++i)
	{
		if (m_slot[i])
		{
			m_slot[i]->Destroy();
		}
	}

	GetMockingboardCardMgr().Destroy();
}

void CardManager::Reset(const bool powerCycle)
{
	for (UINT i = SLOT0; i < NUM_SLOTS; ++i)
	{
		if (m_slot[i])
		{
			m_slot[i]->Reset(powerCycle);
		}
	}

	GetMockingboardCardMgr().Reset(powerCycle);
}

void CardManager::Update(const ULONG nExecutedCycles)
{
	for (UINT i = SLOT0; i < NUM_SLOTS; ++i)
	{
		if (m_slot[i])
		{
			m_slot[i]->Update(nExecutedCycles);
		}
	}

	GetMockingboardCardMgr().Update(nExecutedCycles);
}

void CardManager::SaveSnapshot(YamlSaveHelper& yamlSaveHelper)
{
	for (UINT i = SLOT0; i < NUM_SLOTS; ++i)
	{
		if (m_slot[i])
		{
			m_slot[i]->SaveSnapshot(yamlSaveHelper);
		}
	}
}
