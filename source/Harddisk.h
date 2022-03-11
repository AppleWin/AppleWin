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
		m_imagename.clear();
		m_fullname.clear();
		m_strFilenameInZip.clear();
		m_imagehandle = NULL;
		m_bWriteProtected = false;
		//
		m_error = 0;
		m_memblock = 0;
		m_diskblock = 0;
		m_buf_ptr = 0;
		m_imageloaded = false;
		memset(m_buf, 0, sizeof(m_buf));
		m_status_next = DISK_STATUS_OFF;
		m_status_prev = DISK_STATUS_OFF;
	}

	// From FloppyDisk
	std::string	m_imagename;	// <FILENAME> (ie. no extension)
	std::string m_fullname;	// <FILENAME.EXT> or <FILENAME.zip>
	std::string m_strFilenameInZip;					// ""             or <FILENAME.EXT> [not used]
	ImageInfo* m_imagehandle;			// Init'd by HD_Insert() -> ImageOpen()
	bool m_bWriteProtected;			// Needed for ImageOpen() [otherwise not used]
	//
	BYTE m_error;		// NB. Firmware requires that b0=0 (OK) or b0=1 (Error)
	WORD m_memblock;
	UINT m_diskblock;
	WORD m_buf_ptr;
	bool m_imageloaded;
	BYTE m_buf[HD_BLOCK_SIZE];

	Disk_Status_e m_status_next;
	Disk_Status_e m_status_prev;
};

class HarddiskInterfaceCard : public Card
{
public:
	HarddiskInterfaceCard(UINT slot);
	virtual ~HarddiskInterfaceCard(void);

	virtual void Reset(const bool powerCycle);
	virtual void Update(const ULONG nExecutedCycles) {}

	virtual void InitializeIO(LPBYTE pCxRomPeripheral);
	virtual void Destroy(void);
	const std::string& GetFullName(const int iDrive);
	const std::string& HarddiskGetFullPathName(const int iDrive);
	void GetFilenameAndPathForSaveState(std::string& filename, std::string& path);
	bool Select(const int iDrive);
	bool Insert(const int iDrive, const std::string& pathname);
	void Unplug(const int iDrive);
	bool IsDriveUnplugged(const int iDrive);
	void LoadLastDiskImage(const int iDrive);

	void GetLightStatus(Disk_Status_e* pDisk1Status);
	bool ImageSwap(void);

	static const std::string& GetSnapshotCardName(void);
	virtual void SaveSnapshot(YamlSaveHelper& yamlSaveHelper);
	virtual bool LoadSnapshot(YamlLoadHelper& yamlLoadHelper, UINT version);

	static BYTE __stdcall IORead(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nExecutedCycles);
	static BYTE __stdcall IOWrite(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nExecutedCycles);

private:
	void CleanupDriveInternal(const int iDrive);
	void CleanupDrive(const int iDrive);
	void NotifyInvalidImage(TCHAR* pszImageFilename);
	void SaveLastDiskImage(const int drive);
	const std::string& DiskGetBaseName(const int iDrive);
	bool SelectImage(const int drive, LPCSTR pszFilename);
	void UpdateLightStatus(HardDiskDrive* pHDD);

	void SaveSnapshotHDDUnit(YamlSaveHelper& yamlSaveHelper, UINT unit);
	bool LoadSnapshotHDDUnit(YamlLoadHelper& yamlLoadHelper, UINT unit);

	//

	BYTE m_unitNum;			// b7=unit
	BYTE m_command;
	UINT64 m_notBusyCycle;

	bool m_saveDiskImage;	// Save the DiskImage name to Registry

	HardDiskDrive m_hardDiskDrive[NUM_HARDDISKS];
};
