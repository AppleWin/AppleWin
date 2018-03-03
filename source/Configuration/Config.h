#pragma once

#include "../Applewin.h"
#include "../CPU.h"
#include "../DiskImage.h"	// Disk_Status_e
#include "../Harddisk.h"	// HD_CardIsEnabled()

class CConfigNeedingRestart
{
public:
	CConfigNeedingRestart(UINT bEnableTheFreezesF8Rom = false) :
		m_Apple2Type( GetApple2Type() ),
		m_CpuType( GetMainCpu() ),
		m_uSaveLoadStateMsg(0)
	{
		m_bEnableHDD = HD_CardIsEnabled();
		m_bEnableTheFreezesF8Rom = bEnableTheFreezesF8Rom;
		memset(&m_Slot, 0, sizeof(m_Slot));
		m_SlotAux = CT_Empty;
		m_Slot[4] = g_Slot4;
		m_Slot[5] = g_Slot5;
	}

	const CConfigNeedingRestart& operator= (const CConfigNeedingRestart& other)
	{
		m_Apple2Type = other.m_Apple2Type;
		m_CpuType = other.m_CpuType;
		memcpy(m_Slot, other.m_Slot, sizeof(m_Slot));
		m_bEnableHDD = other.m_bEnableHDD;
		m_bEnableTheFreezesF8Rom = other.m_bEnableTheFreezesF8Rom;
		m_uSaveLoadStateMsg = other.m_uSaveLoadStateMsg;
		return *this;
	}

	bool operator== (const CConfigNeedingRestart& other) const
	{
		return	m_Apple2Type == other.m_Apple2Type &&
				m_CpuType == other.m_CpuType &&
				memcmp(m_Slot, other.m_Slot, sizeof(m_Slot)) == 0 &&
				m_bEnableHDD == other.m_bEnableHDD &&
				m_bEnableTheFreezesF8Rom == other.m_bEnableTheFreezesF8Rom &&
				m_uSaveLoadStateMsg == other.m_uSaveLoadStateMsg;
	}

	bool operator!= (const CConfigNeedingRestart& other) const
	{
		return !operator==(other);
	}

	eApple2Type	m_Apple2Type;
	eCpuType m_CpuType;
	SS_CARDTYPE m_Slot[NUM_SLOTS];	// 0..7
	SS_CARDTYPE m_SlotAux;
	bool m_bEnableHDD;
	UINT m_bEnableTheFreezesF8Rom;
	UINT m_uSaveLoadStateMsg;
};
