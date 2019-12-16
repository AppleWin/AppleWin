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

#include "AppleWin.h"
#include "CardManager.h"

#include "Disk.h"
#include "MouseInterface.h"
#include "SerialComms.h"

void CardManager::Insert(UINT slot, SS_CARDTYPE type)
{
	if (type == CT_Empty)
		return Remove(slot);

	delete m_slot[slot];
	m_slot[slot] = NULL;

	switch (type)
	{
	case CT_Disk2:
		m_slot[slot] = new Disk2InterfaceCard;
		break;
	case CT_SSC:
		m_slot[slot] = new CSuperSerialCard;
		break;
	case CT_MockingboardC:
		m_slot[slot] = new DummyCard(type);
		break;
	case CT_GenericPrinter:
		m_slot[slot] = new DummyCard(type);
		break;
	case CT_GenericHDD:
		m_slot[slot] = new DummyCard(type);
		break;
	case CT_GenericClock:
		m_slot[slot] = new DummyCard(type);
		break;
	case CT_MouseInterface:
		m_slot[slot] = new CMouseInterface;
		break;
	case CT_Z80:
		m_slot[slot] = new DummyCard(type);
		break;
	case CT_Phasor:
		m_slot[slot] = new DummyCard(type);
		break;
	case CT_Echo:
		m_slot[slot] = new DummyCard(type);
		break;
	case CT_SAM:
		m_slot[slot] = new DummyCard(type);
		break;
	case CT_Uthernet:
		m_slot[slot] = new DummyCard(type);
		break;

	case CT_LanguageCard:
	case CT_LanguageCardIIe:
	case CT_Saturn128K:
		{
			if (slot != 0)
			{
				_ASSERT(0);
				break;
			}
		}
		m_slot[slot] = new DummyCard(type);
		break;

	default:
		_ASSERT(0);
		break;
	}

	if (m_slot[slot] == NULL)
		m_slot[slot] = new EmptyCard;
}

void CardManager::Remove(UINT slot)
{
	delete m_slot[slot];
	m_slot[slot] = new EmptyCard;
}

void CardManager::InsertAux(SS_CARDTYPE type)
{
	switch (type)
	{
	case CT_80Col:
		m_aux = new DummyCard(type);
		break;
	case CT_Extended80Col:
		m_aux = new DummyCard(type);
		break;
	case CT_RamWorksIII:
		m_aux = new DummyCard(type);
		break;
	default:
		_ASSERT(0);
		break;
	}
}

void CardManager::RemoveAux(void)
{
	delete m_aux;
	m_aux = new EmptyCard;
}

//

bool CardManager::Disk2IsConditionForFullSpeed(void)
{
	for (UINT i = 0; i < NUM_SLOTS; i++)
	{
		if (m_slot[i]->QueryType() == CT_Disk2)
		{
			if (dynamic_cast<Disk2InterfaceCard*>(GetObj(i))->IsConditionForFullSpeed())
				return true;	// if any card is true then the condition for full-speed is true
		}
	}

	return false;
}

void CardManager::Disk2UpdateDriveState(UINT cycles)
{
	for (UINT i = 0; i < NUM_SLOTS; i++)
	{
		if (m_slot[i]->QueryType() == CT_Disk2)
		{
			dynamic_cast<Disk2InterfaceCard*>(GetObj(i))->UpdateDriveState(cycles);
		}
	}
}

void CardManager::Disk2Reset(const bool powerCycle /*=false*/)
{
	for (UINT i = 0; i < NUM_SLOTS; i++)
	{
		if (m_slot[i]->QueryType() == CT_Disk2)
		{
			dynamic_cast<Disk2InterfaceCard*>(GetObj(i))->Reset(powerCycle);
		}
	}
}

bool CardManager::Disk2GetEnhanceDisk(void)
{
	for (UINT i = 0; i < NUM_SLOTS; i++)
	{
		if (m_slot[i]->QueryType() == CT_Disk2)
		{
			// All Disk2 cards should have the same setting, so just return the state of the first card
			return dynamic_cast<Disk2InterfaceCard*>(GetObj(i))->GetEnhanceDisk();
		}
	}
	return false;
}

void CardManager::Disk2SetEnhanceDisk(bool enhanceDisk)
{
	for (UINT i = 0; i < NUM_SLOTS; i++)
	{
		if (m_slot[i]->QueryType() == CT_Disk2)
		{
			dynamic_cast<Disk2InterfaceCard*>(GetObj(i))->SetEnhanceDisk(enhanceDisk);
		}
	}
}

void CardManager::Disk2LoadLastDiskImage(void)
{
	for (UINT i = 0; i < NUM_SLOTS; i++)
	{
		if (i != SLOT6) continue;	// FIXME

		if (m_slot[i]->QueryType() == CT_Disk2)
		{
			dynamic_cast<Disk2InterfaceCard*>(GetObj(i))->LoadLastDiskImage(DRIVE_1);
			dynamic_cast<Disk2InterfaceCard*>(GetObj(i))->LoadLastDiskImage(DRIVE_2);
		}
	}
}

void CardManager::Disk2Destroy(void)
{
	for (UINT i = 0; i < NUM_SLOTS; i++)
	{
		if (m_slot[i]->QueryType() == CT_Disk2)
		{
			dynamic_cast<Disk2InterfaceCard*>(GetObj(i))->Destroy();
		}
	}
}
