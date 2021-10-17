#pragma once

#include "../Core.h"
#include "../CardManager.h"
#include "../CPU.h"
#include "../DiskImage.h"	// Disk_Status_e
#include "../Harddisk.h"
#include "../Interface.h"	// VideoRefreshRate_e, GetVideoRefreshRate()
#include "../Tfe/tfe.h"

class CConfigNeedingRestart
{
public:
	CConfigNeedingRestart(UINT bEnableTheFreezesF8Rom = false) :
		m_Apple2Type( GetApple2Type() ),
		m_CpuType( GetMainCpu() ),
		m_uSaveLoadStateMsg(0),
		m_videoRefreshRate( GetVideo().GetVideoRefreshRate() )
	{
		m_bEnableTheFreezesF8Rom = bEnableTheFreezesF8Rom;

		for (UINT slot = SLOT0; slot < NUM_SLOTS; slot++)
			m_Slot[slot] = GetCardMgr().QuerySlot(slot);
		m_SlotAux = GetCardMgr().QueryAux();

		m_tfeInterface = get_tfe_interface();
	}

	const CConfigNeedingRestart& operator= (const CConfigNeedingRestart& other)
	{
		m_Apple2Type = other.m_Apple2Type;
		m_CpuType = other.m_CpuType;
		memcpy(m_Slot, other.m_Slot, sizeof(m_Slot));
		m_SlotAux = other.m_SlotAux;
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
	SS_CARDTYPE m_Slot[NUM_SLOTS];
	SS_CARDTYPE m_SlotAux;
	std::string m_tfeInterface;
	UINT m_bEnableTheFreezesF8Rom;
	UINT m_uSaveLoadStateMsg;
	VideoRefreshRate_e m_videoRefreshRate;
};
