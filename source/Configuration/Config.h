#pragma once

#include "../Core.h"
#include "../CPU.h"
#include "../Video.h"

class CConfigNeedingRestart
{
public:
	// zero initialise
	CConfigNeedingRestart();

	// create from current global configuration
	static CConfigNeedingRestart Create();

	// update from current global configuration
	void Reload();

	const CConfigNeedingRestart& operator= (const CConfigNeedingRestart& other);

	bool operator== (const CConfigNeedingRestart& other) const;

	bool operator!= (const CConfigNeedingRestart& other) const;

	eApple2Type	m_Apple2Type;
	eCpuType m_CpuType;
	SS_CARDTYPE m_Slot[NUM_SLOTS];
	SS_CARDTYPE m_SlotAux;
	std::string m_tfeInterface;
	bool m_tfeVirtualDNS;
	UINT m_bEnableTheFreezesF8Rom;
	UINT m_uSaveLoadStateMsg;
	VideoRefreshRate_e m_videoRefreshRate;
};
