#pragma once

#include "../Core.h"
#include "../CardManager.h"
#include "../CPU.h"
#include "../DiskImage.h"	// Disk_Status_e
#include "../Harddisk.h"	// HD_CardIsEnabled()
#include "../Interface.h"	// VideoRefreshRate_e, GetVideoRefreshRate()
#include "../Tfe/tfe.h"

class CConfigNeedingRestart
{
public:
	CConfigNeedingRestart()
	{
		// do not call Reload() here
		// it causes a circular loop in the initialisation of CPropertySheet
		m_Apple2Type = A2TYPE_APPLE2;
		m_CpuType = CPU_UNKNOWN;
		memset(m_Slot, 0, sizeof(m_Slot));
		m_SlotAux = CT_Empty;
		m_bEnableHDD = false;
		m_tfeEnabled = false;;
		m_bEnableTheFreezesF8Rom = 0;
		m_uSaveLoadStateMsg = 0;
		m_videoRefreshRate = VR_NONE;
	}

	// create and initialise to current state
	// do not use in static variables
	static CConfigNeedingRestart Create()
	{
		CConfigNeedingRestart config;
		config.Reload();
		return config;
	}

	void Reload()
	{
		m_uSaveLoadStateMsg = 0;

		m_Apple2Type = GetApple2Type();
		m_CpuType = GetMainCpu();
		CardManager& cardManager = GetCardMgr();
		for (int i = SLOT0; i < NUM_SLOTS; ++i)
		{
			m_Slot[i] = cardManager.QuerySlot(i);
		}
		m_bEnableHDD = HD_CardIsEnabled();
		m_bEnableTheFreezesF8Rom = GetPropertySheet().GetTheFreezesF8Rom();
		m_videoRefreshRate = GetVideo().GetVideoRefreshRate();
		m_tfeEnabled = get_tfe_enabled();
		m_tfeInterface = get_tfe_interface();
		m_SlotAux = CT_Empty;
	}

	const CConfigNeedingRestart& operator= (const CConfigNeedingRestart& other)
	{
		m_Apple2Type = other.m_Apple2Type;
		m_CpuType = other.m_CpuType;
		memcpy(m_Slot, other.m_Slot, sizeof(m_Slot));
		m_SlotAux = other.m_SlotAux;
		m_bEnableHDD = other.m_bEnableHDD;
		m_tfeEnabled = other.m_tfeEnabled;
		m_tfeInterface = other.m_tfeInterface;
		m_bEnableTheFreezesF8Rom = other.m_bEnableTheFreezesF8Rom;
		m_uSaveLoadStateMsg = other.m_uSaveLoadStateMsg;
		m_videoRefreshRate = other.m_videoRefreshRate;
		return *this;
	}

	bool operator== (const CConfigNeedingRestart& other) const
	{
		return	m_Apple2Type == other.m_Apple2Type &&
			m_CpuType == other.m_CpuType &&
			memcmp(m_Slot, other.m_Slot, sizeof(m_Slot)) == 0 &&
			m_SlotAux == other.m_SlotAux &&
			m_bEnableHDD == other.m_bEnableHDD &&
			m_tfeEnabled == other.m_tfeEnabled &&
			m_tfeInterface == other.m_tfeInterface &&
			m_bEnableTheFreezesF8Rom == other.m_bEnableTheFreezesF8Rom &&
			m_uSaveLoadStateMsg == other.m_uSaveLoadStateMsg &&
			m_videoRefreshRate == other.m_videoRefreshRate;
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
	int m_tfeEnabled;
	std::string m_tfeInterface;
	UINT m_bEnableTheFreezesF8Rom;
	UINT m_uSaveLoadStateMsg;
	VideoRefreshRate_e m_videoRefreshRate;
};
