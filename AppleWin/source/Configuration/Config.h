#pragma once

#include "..\HardDisk.h"

class CConfigNeedingRestart
{
public:
	CConfigNeedingRestart(UINT bEnableTheFreezesF8Rom = false) :
		m_Apple2Type(g_Apple2Type),
		m_bEnhanceDisk(enhancedisk),
		m_bDoBenchmark(false),
		m_uSaveLoadStateMsg(0)
	{
		m_bEnableHDD = HD_CardIsEnabled();
		m_bEnableTheFreezesF8Rom = bEnableTheFreezesF8Rom;
		memset(&m_Slot, 0, sizeof(m_Slot));
		m_Slot[4] = g_Slot4;
		m_Slot[5] = g_Slot5;
	}

	const CConfigNeedingRestart& operator= (const CConfigNeedingRestart& other)
	{
		m_Apple2Type = other.m_Apple2Type;
		memcpy(m_Slot, other.m_Slot, sizeof(m_Slot));
		m_bEnhanceDisk = other.m_bEnhanceDisk;
		m_bEnableHDD = other.m_bEnableHDD;
		m_bEnableTheFreezesF8Rom = other.m_bEnableTheFreezesF8Rom;
		m_bDoBenchmark = other.m_bDoBenchmark;
		m_uSaveLoadStateMsg = other.m_uSaveLoadStateMsg;
		return *this;
	}

	bool operator== (const CConfigNeedingRestart& other) const
	{
		return	m_Apple2Type == other.m_Apple2Type &&
				memcmp(m_Slot, other.m_Slot, sizeof(m_Slot)) == 0 &&
				m_bEnhanceDisk == other.m_bEnhanceDisk &&
				m_bEnableHDD == other.m_bEnableHDD &&
				m_bEnableTheFreezesF8Rom == other.m_bEnableTheFreezesF8Rom &&
				m_bDoBenchmark == other.m_bDoBenchmark &&
				m_uSaveLoadStateMsg == other.m_uSaveLoadStateMsg;
	}

	bool operator!= (const CConfigNeedingRestart& other) const
	{
		return !operator==(other);
	}

	eApple2Type	m_Apple2Type;
	SS_CARDTYPE m_Slot[NUM_SLOTS];	// 0..7
	BOOL m_bEnhanceDisk;
	bool m_bEnableHDD;
	UINT m_bEnableTheFreezesF8Rom;
	bool m_bDoBenchmark;
	UINT m_uSaveLoadStateMsg;
};
