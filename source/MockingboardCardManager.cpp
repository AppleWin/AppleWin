/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 1994-1996, Michael O'Brien
Copyright (C) 1999-2001, Oliver Schmidt
Copyright (C) 2002-2005, Tom Charlesworth
Copyright (C) 2006-2022, Tom Charlesworth, Michael Pohoreski, Nick Westgate

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

/* Description: Mockingboard/Phasor Card Manager
 *
 * Author: Various
 *
 */

#include "StdAfx.h"

#include "MockingboardCardManager.h"
#include "Core.h"
#include "CardManager.h"
#include "Mockingboard.h"

bool MockingboardCardManager::IsMockingboard(UINT slot)
{
	SS_CARDTYPE type = GetCardMgr().QuerySlot(slot);
	return type == CT_MockingboardC || type == CT_Phasor;
}

void MockingboardCardManager::ReinitializeClock(void)
{
	for (UINT i = SLOT0; i < NUM_SLOTS; i++)
	{
		if (IsMockingboard(i))
			dynamic_cast<MockingboardCard&>(GetCardMgr().GetRef(i)).ReinitializeClock();
	}
}

void MockingboardCardManager::InitializeForLoadingSnapshot(void)
{
	for (UINT i = SLOT0; i < NUM_SLOTS; i++)
	{
		if (IsMockingboard(i))
			dynamic_cast<MockingboardCard&>(GetCardMgr().GetRef(i)).InitializeForLoadingSnapshot();
	}
}

void MockingboardCardManager::MuteControl(bool mute)
{
	for (UINT i = SLOT0; i < NUM_SLOTS; i++)
	{
		if (IsMockingboard(i))
			dynamic_cast<MockingboardCard&>(GetCardMgr().GetRef(i)).MuteControl(mute);
	}
}

void MockingboardCardManager::SetCumulativeCycles(void)
{
	for (UINT i = SLOT0; i < NUM_SLOTS; i++)
	{
		if (IsMockingboard(i))
			dynamic_cast<MockingboardCard&>(GetCardMgr().GetRef(i)).SetCumulativeCycles();
	}
}

void MockingboardCardManager::UpdateCycles(ULONG executedCycles)
{
	for (UINT i = SLOT0; i < NUM_SLOTS; i++)
	{
		if (IsMockingboard(i))
			dynamic_cast<MockingboardCard&>(GetCardMgr().GetRef(i)).UpdateCycles(executedCycles);
	}
}

bool MockingboardCardManager::IsActive(void)
{
	for (UINT i = SLOT0; i < NUM_SLOTS; i++)
	{
		if (IsMockingboard(i))
			if (dynamic_cast<MockingboardCard&>(GetCardMgr().GetRef(i)).IsActive())
				return true;	// if any card is true then the condition for active is true
	}

	return false;
}

DWORD MockingboardCardManager::GetVolume(void)
{
	return m_userVolume;
}

void MockingboardCardManager::SetVolume(DWORD volume, DWORD volumeMax)
{
	m_userVolume = volume;

	for (UINT i = SLOT0; i < NUM_SLOTS; i++)
	{
		if (IsMockingboard(i))
			dynamic_cast<MockingboardCard&>(GetCardMgr().GetRef(i)).SetVolume(volume, volumeMax);
	}
}

#ifdef _DEBUG
void MockingboardCardManager::CheckCumulativeCycles(void)
{
	for (UINT i = SLOT0; i < NUM_SLOTS; i++)
	{
		if (IsMockingboard(i))
			dynamic_cast<MockingboardCard&>(GetCardMgr().GetRef(i)).CheckCumulativeCycles();
	}
}

void MockingboardCardManager::Get6522IrqDescription(std::string& desc)
{
	for (UINT i = SLOT0; i < NUM_SLOTS; i++)
	{
		if (IsMockingboard(i))
			dynamic_cast<MockingboardCard&>(GetCardMgr().GetRef(i)).Get6522IrqDescription(desc);
	}
}
#endif
