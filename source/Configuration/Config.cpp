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
#include "../Interface.h"
#include "../Memory.h"
#include "../Registry.h"
#include "../SaveState.h"
#include "../Speaker.h"
#include "../Uthernet2.h"
#include "../Tfe/PCapBackend.h"
#include "../Windows/Win32Frame.h"


// Zero (or default) initialise - values not important, as they get set correctly in Reload()
CConfigNeedingRestart::CConfigNeedingRestart()
	: m_parallelPrinterCard(SLOT1)	// slot not important
{
	// Configuration
	m_Apple2Type = A2TYPE_APPLE2;
	m_CpuType = CPU_UNKNOWN;
	m_confirmReboot = false;
	m_masterVolume = 0;
	m_videoType = VT_DEFAULT;
	m_videoStyle = VS_NONE;
	m_videoRefreshRate = VR_NONE;
	m_monochromeRGB = Video::MONO_COLOR_DEFAULT;
	m_fullScreen_ShowSubunitStatus = false;
	m_enhanceDiskAccessSpeed = false;
	m_scrollLockToggle = 0;
	m_machineSpeed = 0;

	// Input
	m_joystickType[JN_JOYSTICK0] = 0;
	m_joystickType[JN_JOYSTICK1] = 0;
	m_pdlXTrim = 0;
	m_pdlYTrim = 0;
	m_autofire = 0;
	m_centeringControl = 0;
	m_cursorControl = 0;
	m_swapButtons0and1 = false;

	// Slots
	memset(m_Slot, 0, sizeof(m_Slot));
	m_SlotAux = CT_Empty;
	m_tfeVirtualDNS = false;
	m_RamWorksMemorySize = 0;
	m_serialPortItem = 0;

	// Advanced
	m_uSaveLoadStateMsg = 0;
	m_saveStateOnExit = false;
	m_enableTheFreezesF8Rom = 0;
	m_gameIOConnectorType = DT_EMPTY;
}

// Create from current global configuration
// . called from Snapshot_LoadState_v2()
CConfigNeedingRestart CConfigNeedingRestart::Create()
{
	CConfigNeedingRestart config;
	config.Reload();
	return config;
}

// Update from current global configuration
void CConfigNeedingRestart::Reload()
{
	// Configuration
	m_Apple2Type = GetApple2Type();
	m_CpuType = GetMainCpu();
	m_confirmReboot = GetFrame().g_bConfirmReboot;
	m_masterVolume = SpkrGetVolume();
	m_videoType = GetVideo().GetVideoType();
	m_videoStyle = GetVideo().GetVideoStyle();
	m_videoRefreshRate = GetVideo().GetVideoRefreshRate();
	m_monochromeRGB = GetVideo().GetMonochromeRGB();
	m_fullScreen_ShowSubunitStatus = Win32Frame::GetWin32Frame().GetFullScreenShowSubunitStatus();
	m_scrollLockToggle = GetPropertySheet().GetScrollLockToggle();
	m_enhanceDiskAccessSpeed = GetCardMgr().GetDisk2CardMgr().GetEnhanceDisk();
	m_machineSpeed = g_dwSpeed;

	// Input
	m_joystickType[JN_JOYSTICK0] = JoyGetJoyType(JN_JOYSTICK0);
	m_joystickType[JN_JOYSTICK1] = JoyGetJoyType(JN_JOYSTICK1);
	m_pdlXTrim = JoyGetTrim(true);
	m_pdlYTrim = JoyGetTrim(false);
	m_autofire = GetPropertySheet().GetAutofire();
	m_centeringControl = GetPropertySheet().GetJoystickCenteringControl();
	m_cursorControl = GetPropertySheet().GetJoystickCursorControl();
	m_swapButtons0and1 = GetPropertySheet().GetButtonsSwapState();

	// Slots
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
		}
		else if (m_Slot[slot] == CT_Disk2)
		{
			for (UINT i = DRIVE_1; i < NUM_DRIVES; i++)
				m_slotInfoForFDC[slot].pathname[i] = dynamic_cast<Disk2InterfaceCard&>(cardManager.GetRef(slot)).DiskGetFullPathName(i);
		}
		else if (m_Slot[slot] == CT_GenericHDD)
		{
			for (UINT i = HARDDISK_1; i < NUM_HARDDISKS; i++)
				m_slotInfoForHDC[slot].pathname[i] = dynamic_cast<HarddiskInterfaceCard&>(cardManager.GetRef(slot)).HarddiskGetFullPathName(i);
		}
	}

	m_RamWorksMemorySize = GetRamWorksMemorySize();

	if (cardManager.IsParallelPrinterCardInstalled())
		m_parallelPrinterCard = *cardManager.GetParallelPrinterCard();	// copy object

	if (cardManager.IsSSCInstalled())
		m_serialPortItem = cardManager.GetSSC()->GetSerialPortItem();

	// Advanced
	m_uSaveLoadStateMsg = 0;
	m_saveStateOnExit = GetSaveStateOnExit();
	char ciderPressPathname[MAX_PATH];
	RegLoadString(REG_CONFIG, REGVALUE_CIDERPRESSLOC, 1, ciderPressPathname, MAX_PATH, "");
	m_ciderPressPathname = ciderPressPathname;
	m_enableTheFreezesF8Rom = GetPropertySheet().GetTheFreezesF8Rom();
	m_gameIOConnectorType = GetCopyProtectionDongleType();
}

const CConfigNeedingRestart& CConfigNeedingRestart::operator= (const CConfigNeedingRestart& other)
{
	// Configuration
	m_Apple2Type = other.m_Apple2Type;
	m_CpuType = other.m_CpuType;
	m_confirmReboot = other.m_confirmReboot;
	m_masterVolume = other.m_masterVolume;
	m_videoType = other.m_videoType;
	m_videoStyle = other.m_videoStyle;
	m_videoRefreshRate = other.m_videoRefreshRate;
	m_monochromeRGB = other.m_monochromeRGB;
	m_fullScreen_ShowSubunitStatus = other.m_fullScreen_ShowSubunitStatus;
	m_enhanceDiskAccessSpeed = other.m_enhanceDiskAccessSpeed;
	m_scrollLockToggle = other.m_scrollLockToggle;
	m_machineSpeed = other.m_machineSpeed;

	// Input
	m_joystickType[JN_JOYSTICK0] = other.m_joystickType[JN_JOYSTICK0];
	m_joystickType[JN_JOYSTICK1] = other.m_joystickType[JN_JOYSTICK1];
	m_pdlXTrim = other.m_pdlXTrim;
	m_pdlYTrim = other.m_pdlYTrim;
	m_autofire = other.m_autofire;
	m_centeringControl = other.m_centeringControl;
	m_cursorControl = other.m_cursorControl;
	m_swapButtons0and1 = other.m_swapButtons0and1;

	// Slots
	memcpy(m_Slot, other.m_Slot, sizeof(m_Slot));
	m_SlotAux = other.m_SlotAux;
	m_tfeInterface = other.m_tfeInterface;
	m_tfeVirtualDNS = other.m_tfeVirtualDNS;
	m_RamWorksMemorySize = other.m_RamWorksMemorySize;
	m_parallelPrinterCard = other.m_parallelPrinterCard;
	m_serialPortItem = other.m_serialPortItem;
	for (UINT slot = SLOT0; slot < NUM_SLOTS; slot++)
	{
		for (UINT i = DRIVE_1; i < NUM_DRIVES; i++)
			m_slotInfoForFDC[slot].pathname[i] = other.m_slotInfoForFDC[slot].pathname[i];
		for (UINT i = HARDDISK_1; i < NUM_HARDDISKS; i++)
			m_slotInfoForHDC[slot].pathname[i] = other.m_slotInfoForHDC[slot].pathname[i];
	}

	// Advanced
	m_uSaveLoadStateMsg = other.m_uSaveLoadStateMsg;
	m_saveStateOnExit = other.m_saveStateOnExit;
	m_ciderPressPathname = other.m_ciderPressPathname;
	m_enableTheFreezesF8Rom = other.m_enableTheFreezesF8Rom;
	m_gameIOConnectorType = other.m_gameIOConnectorType;
	return *this;
}

bool CConfigNeedingRestart::operator== (const CConfigNeedingRestart& other) const
{
	// Ignore: (as they don't require the VM to be restarted)
	// . [Config] m_confirmReboot, m_masterVolume
	// . [Config] m_videoType, m_videoStyle, m_monochromeRGB, m_fullScreen_ShowSubunitStatus
	// . [Config] m_enhanceDiskAccessSpeed, m_scrollLockToggle, m_machineSpeed
	// . [Input] m_autofire, m_centeringControl, m_cursorControl, m_swapButtons0and1
	// . [Input] m_joystickType[], m_pdlXTrim, m_pdlYTrim
	// . [Slots] m_parallelPrinterCard
	// . [Advanced] m_uSaveLoadStateMsg, m_saveStateOnExit, m_ciderPressPathname, m_gameIOConnectorType

	return	m_Apple2Type == other.m_Apple2Type &&
		m_CpuType == other.m_CpuType &&
		m_videoRefreshRate == other.m_videoRefreshRate &&
		memcmp(m_Slot, other.m_Slot, sizeof(m_Slot)) == 0 &&
		m_SlotAux == other.m_SlotAux &&
		m_tfeInterface == other.m_tfeInterface &&
		m_tfeVirtualDNS == other.m_tfeVirtualDNS &&
		m_RamWorksMemorySize == other.m_RamWorksMemorySize &&
		m_serialPortItem == other.m_serialPortItem &&
		m_enableTheFreezesF8Rom == other.m_enableTheFreezesF8Rom;
}

bool CConfigNeedingRestart::operator!= (const CConfigNeedingRestart& other) const
{
	return !operator==(other);
}
