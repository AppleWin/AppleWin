#pragma once

/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 1994-1996, Michael O'Brien
Copyright (C) 1999-2001, Oliver Schmidt
Copyright (C) 2002-2005, Tom Charlesworth
Copyright (C) 2006-2010, Tom Charlesworth, Michael Pohoreski

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

#include "DiskImage.h"

// 1.19.0.0 Hard Disk Status/Indicator Light
#define HD_LED 1

	enum HardDrive_e
	{
		HARDDISK_1 = 0,
		HARDDISK_2,
		NUM_HARDDISKS
	};

	void HD_Destroy(void);
	bool HD_CardIsEnabled(void);
	void HD_SetEnabled(const bool bEnabled);
	const std::string & HD_GetFullName(const int iDrive);
	const std::string & HD_GetFullPathName(const int iDrive);
	void HD_GetFilenameAndPathForSaveState(std::string& filename, std::string& path);
	void HD_Reset(void);
	void HD_Load_Rom(const LPBYTE pCxRomPeripheral, const UINT uSlot);
	bool HD_Select(const int iDrive);
	BOOL HD_Insert(const int iDrive, const std::string & pszImageFilename);
	void HD_Unplug(const int iDrive);
	bool HD_IsDriveUnplugged(const int iDrive);
	void HD_LoadLastDiskImage(const int iDrive);

	// 1.19.0.0 Hard Disk Status/Indicator Light
	void HD_GetLightStatus (Disk_Status_e *pDisk1Status_);
	bool HD_ImageSwap(void);

	std::string HD_GetSnapshotCardName(void);
	void HD_SaveSnapshot(class YamlSaveHelper& yamlSaveHelper);
	bool HD_LoadSnapshot(class YamlLoadHelper& yamlLoadHelper, UINT slot, UINT version, const std::string strSaveStatePath);
