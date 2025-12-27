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

#include "StdAfx.h"
#include "Config.h"
#include "../CardManager.h"
#include "../Interface.h"	// VideoRefreshRate_e, GetVideoRefreshRate()
#include "../Memory.h"
#include "../Uthernet2.h"
#include "../Tfe/PCapBackend.h"

// zero initialise
CConfigNeedingRestart::CConfigNeedingRestart()
	: m_parallelPrinterCard(SLOT1)	// slot not important
{
	m_Apple2Type = A2TYPE_APPLE2;
	m_CpuType = CPU_UNKNOWN;
	memset(m_Slot, 0, sizeof(m_Slot));
	m_SlotAux = CT_Empty;
	m_tfeVirtualDNS = false;
	m_bEnableTheFreezesF8Rom = 0;
	m_uSaveLoadStateMsg = 0;
	m_videoRefreshRate = VR_NONE;
	m_RamWorksMemorySize = 0;
}

// create from current global configuration
// . called from Snapshot_LoadState_v2()
CConfigNeedingRestart CConfigNeedingRestart::Create()
{
	CConfigNeedingRestart config;
	config.Reload();
	return config;
}

// update from current global configuration
void CConfigNeedingRestart::Reload()
{
	m_Apple2Type = GetApple2Type();
	m_CpuType = GetMainCpu();

	CardManager& cardManager = GetCardMgr();
	for (UINT slot = SLOT0; slot < NUM_SLOTS; slot++)
		m_Slot[slot] = cardManager.QuerySlot(slot);
	m_SlotAux = cardManager.QueryAux();

	for (UINT slot = SLOT0; slot < NUM_SLOTS; slot++)
	{
		if (m_Slot[slot] == CT_Uthernet || m_Slot[slot] == CT_Uthernet2)
		{
			// Assume only one CT_Uthernet or CT_Uthernet2 inserted
			m_tfeInterface = PCapBackend::GetRegistryInterface(slot);
			m_tfeVirtualDNS = Uthernet2::GetRegistryVirtualDNS(slot);
			break;
		}
	}
	m_bEnableTheFreezesF8Rom = GetPropertySheet().GetTheFreezesF8Rom();
	m_uSaveLoadStateMsg = 0;
	m_videoRefreshRate = GetVideo().GetVideoRefreshRate();
	m_RamWorksMemorySize = GetRamWorksMemorySize();

	if (cardManager.IsParallelPrinterCardInstalled())
		m_parallelPrinterCard = *cardManager.GetParallelPrinterCard();	// copy object

	if (cardManager.IsSSCInstalled())
		m_serialPortItem = cardManager.GetSSC()->GetSerialPortItem();
}

const CConfigNeedingRestart& CConfigNeedingRestart::operator= (const CConfigNeedingRestart& other)
{
	m_Apple2Type = other.m_Apple2Type;
	m_CpuType = other.m_CpuType;
	memcpy(m_Slot, other.m_Slot, sizeof(m_Slot));
	m_SlotAux = other.m_SlotAux;
	m_tfeInterface = other.m_tfeInterface;
	m_tfeVirtualDNS = other.m_tfeVirtualDNS;
	m_bEnableTheFreezesF8Rom = other.m_bEnableTheFreezesF8Rom;
	m_uSaveLoadStateMsg = other.m_uSaveLoadStateMsg;
	m_videoRefreshRate = other.m_videoRefreshRate;
	m_RamWorksMemorySize = other.m_RamWorksMemorySize;
	m_parallelPrinterCard = other.m_parallelPrinterCard;
	m_serialPortItem = other.m_serialPortItem;
	return *this;
}

bool CConfigNeedingRestart::operator== (const CConfigNeedingRestart& other) const
{
	return	m_Apple2Type == other.m_Apple2Type &&
		m_CpuType == other.m_CpuType &&
		memcmp(m_Slot, other.m_Slot, sizeof(m_Slot)) == 0 &&
		m_SlotAux == other.m_SlotAux &&
		m_tfeInterface == other.m_tfeInterface &&
		m_tfeVirtualDNS == other.m_tfeVirtualDNS &&
		m_bEnableTheFreezesF8Rom == other.m_bEnableTheFreezesF8Rom &&
		m_uSaveLoadStateMsg == other.m_uSaveLoadStateMsg &&
		m_videoRefreshRate == other.m_videoRefreshRate &&
		m_RamWorksMemorySize == other.m_RamWorksMemorySize &&
		m_parallelPrinterCard == other.m_parallelPrinterCard &&	// NB. no restart required if any of this changes
		m_serialPortItem == other.m_serialPortItem;
}

bool CConfigNeedingRestart::operator!= (const CConfigNeedingRestart& other) const
{
	return !operator==(other);
}
