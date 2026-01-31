#pragma once

#include "../CopyProtectionDongles.h"
#include "../Core.h"
#include "../CPU.h"
#include "../Disk.h"
#include "../Harddisk.h"
#include "../Joystick.h"
#include "../ParallelPrinter.h"
#include "../SerialComms.h"
#include "../Video.h"

struct SlotInfoForFDC
{
	std::string pathname[NUM_DRIVES];
};

struct SlotInfoForHDC
{
	std::string pathname[NUM_HARDDISKS];
};

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

	// Configuration
	eApple2Type	m_Apple2Type;
	eCpuType m_CpuType;
	bool m_confirmReboot;
	uint32_t m_masterVolume;
	VideoType_e m_videoType;
	VideoStyle_e m_videoStyle;
	VideoRefreshRate_e m_videoRefreshRate;
	COLORREF m_monochromeRGB;
	bool m_fullScreen_ShowSubunitStatus;
	bool m_enhanceDiskAccessSpeed;
	UINT m_scrollLockToggle;
	UINT m_machineSpeed;

	// Input
	uint32_t m_joystickType[JN_NUM];
	short m_pdlXTrim;
	short m_pdlYTrim;
	UINT m_autofire;
	UINT m_centeringControl;
	UINT m_cursorControl;
	bool m_swapButtons0and1;

	// Slots
	SS_CARDTYPE m_Slot[NUM_SLOTS];
	SS_CARDTYPE m_SlotAux;
	std::string m_tfeInterface;
	bool m_tfeVirtualDNS;
	uint32_t m_RamWorksMemorySize;	// Size in 64K banks
	ParallelPrinterCard m_parallelPrinterCard;	// Use entire card object, as there are many config vars
	UINT m_serialPortItem;	// SSC: Just one config var for this card (at the moment)
	SlotInfoForFDC m_slotInfoForFDC[NUM_SLOTS];
	SlotInfoForHDC m_slotInfoForHDC[NUM_SLOTS];

	// Advanced
	UINT m_uSaveLoadStateMsg;	// WM_USER_LOADSTATE or WM_USER_SAVESTATE
	bool m_saveStateOnExit;
	std::string m_ciderPressPathname;
	UINT m_enableTheFreezesF8Rom;
	DONGLETYPE m_gameIOConnectorType;
};
