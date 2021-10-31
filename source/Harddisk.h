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

#include "Card.h"
#include "DiskImage.h"
#include "DiskImageHelper.h"

// 1.19.0.0 Hard Disk Status/Indicator Light
#define HD_LED 1

enum HardDrive_e
{
	HARDDISK_1 = 0,
	HARDDISK_2,
	NUM_HARDDISKS
};

class HardDiskDrive
{
public:
	HardDiskDrive(void)
	{
		clear();
	}
	~HardDiskDrive(void) {}

	void clear()
	{
		imagename.clear();
		fullname.clear();
		strFilenameInZip.clear();
		imagehandle = NULL;
		bWriteProtected = false;
		//
		hd_error = 0;
		hd_memblock = 0;
		hd_diskblock = 0;
		hd_buf_ptr = 0;
		hd_imageloaded = false;
		memset(hd_buf, 0, sizeof(hd_buf));
#if HD_LED
		hd_status_next = DISK_STATUS_OFF;
		hd_status_prev = DISK_STATUS_OFF;
#endif
	}

	// From FloppyDisk
	std::string	imagename;	// <FILENAME> (ie. no extension)
	std::string fullname;	// <FILENAME.EXT> or <FILENAME.zip>
	std::string strFilenameInZip;					// ""             or <FILENAME.EXT> [not used]
	ImageInfo* imagehandle;			// Init'd by HD_Insert() -> ImageOpen()
	bool bWriteProtected;			// Needed for ImageOpen() [otherwise not used]
	//
	BYTE hd_error;		// NB. Firmware requires that b0=0 (OK) or b0=1 (Error)
	WORD hd_memblock;
	UINT hd_diskblock;
	WORD hd_buf_ptr;
	bool hd_imageloaded;
	BYTE hd_buf[HD_BLOCK_SIZE + 1];	// Why +1? Probably for erroreous reads beyond the block size (ie. reads from I/O addr 0xC0F8)

#if HD_LED
	Disk_Status_e hd_status_next;
	Disk_Status_e hd_status_prev;
#endif
};

class HarddiskInterfaceCard : public Card
{
public:
	HarddiskInterfaceCard(UINT slot);
	virtual ~HarddiskInterfaceCard(void);

	virtual void Init(void) {}
	virtual void Reset(const bool powerCycle);

	void Initialize(const LPBYTE pCxRomPeripheral);
	void HD_Destroy(void);
	const std::string& HD_GetFullName(const int iDrive);
	const std::string& HD_GetFullPathName(const int iDrive);
	void HD_GetFilenameAndPathForSaveState(std::string& filename, std::string& path);
	bool HD_Select(const int iDrive);
	BOOL HD_Insert(const int iDrive, const std::string& pathname);
	void HD_Unplug(const int iDrive);
	bool HD_IsDriveUnplugged(const int iDrive);
	void HD_LoadLastDiskImage(const int iDrive);

	void HD_GetLightStatus(Disk_Status_e* pDisk1Status);
	bool HD_ImageSwap(void);

	static std::string HD_GetSnapshotCardName(void);
	void HD_SaveSnapshot(class YamlSaveHelper& yamlSaveHelper);
	bool HD_LoadSnapshot(class YamlLoadHelper& yamlLoadHelper, UINT slot, UINT version, const std::string& strSaveStatePath);

	static BYTE __stdcall IORead(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nExecutedCycles);
	static BYTE __stdcall IOWrite(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nExecutedCycles);

private:
	void HD_CleanupDrive(const int iDrive);
	void NotifyInvalidImage(TCHAR* pszImageFilename);
	void HD_SaveLastDiskImage(const int drive);
	const std::string& HD_DiskGetBaseName(const int iDrive);
	bool HD_SelectImage(const int drive, LPCSTR pszFilename);

	void HD_SaveSnapshotHDDUnit(YamlSaveHelper& yamlSaveHelper, UINT unit);
	bool HD_LoadSnapshotHDDUnit(YamlLoadHelper& yamlLoadHelper, UINT unit);

	//

	BYTE g_nHD_UnitNum;		// b7=unit
	BYTE g_nHD_Command;

	bool g_bSaveDiskImage;	// Save the DiskImage name to Registry
	UINT m_slot;

	HardDiskDrive m_hardDiskDrive[NUM_HARDDISKS];
};
