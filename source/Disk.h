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

#include "DiskLog.h"
#include "DiskFormatTrack.h"
#include "DiskImage.h"

extern class DiskIIInterfaceCard sg_DiskIICard;

// Floppy Disk Drives

enum Drive_e
{
	DRIVE_1 = 0,
	DRIVE_2,
	NUM_DRIVES
};

const bool IMAGE_USE_FILES_WRITE_PROTECT_STATUS = false;
const bool IMAGE_FORCE_WRITE_PROTECTED = true;
const bool IMAGE_DONT_CREATE = false;
const bool IMAGE_CREATE = true;

struct Disk_t
{
	TCHAR	imagename[ MAX_DISK_IMAGE_NAME + 1 ];	// <FILENAME> (ie. no extension)
	TCHAR	fullname [ MAX_DISK_FULL_NAME  + 1 ];	// <FILENAME.EXT> or <FILENAME.zip>  : This is persisted to the snapshot file
	std::string strFilenameInZip;					// ""             or <FILENAME.EXT>
	ImageInfo* imagehandle;							// Init'd by DiskInsert() -> ImageOpen()
	bool	bWriteProtected;
	//
	int		byte;
	int		nibbles;								// Init'd by ReadTrack() -> ImageReadTrack()
	LPBYTE	trackimage;
	bool	trackimagedata;
	bool	trackimagedirty;

	Disk_t()
	{
		clear();
	}

	void clear()
	{
		ZeroMemory(imagename, sizeof(imagename));
		ZeroMemory(fullname, sizeof(fullname));
		strFilenameInZip.clear();
		imagehandle = NULL;
		bWriteProtected = false;
		//
		byte = 0;
		nibbles = 0;
		trackimage = NULL;
		trackimagedata = false;
		trackimagedirty = false;
	}
};

struct Drive_t
{
	int		phase;
	int		track;
	DWORD	spinning;
	DWORD	writelight;
	Disk_t	disk;

	Drive_t()
	{
		clear();
	}

	void clear()
	{
		phase = 0;
		track = 0;
		spinning = 0;
		writelight = 0;
		disk.clear();
	}
};

class DiskIIInterfaceCard
{
public:
	DiskIIInterfaceCard(void);
	virtual ~DiskIIInterfaceCard(void){};

	void Initialize(void);	// 2x Initialize() funcs!
	void Initialize(LPBYTE pCxRomPeripheral, UINT uSlot);
	void Destroy(void);		// no, doesn't "destroy" the disk image.  DiskIIManagerShutdown()

	void Boot(void);
	void FlushCurrentTrack(const int iDrive);

	LPCTSTR GetDiskPathFilename(const int iDrive);
	LPCTSTR GetFullDiskFilename(const int iDrive);
	LPCTSTR GetFullName(const int iDrive);
	LPCTSTR GetBaseName(const int iDrive);
	void GetLightStatus (Disk_Status_e* pDisk1Status, Disk_Status_e* pDisk2Status);

	ImageError_e InsertDisk(const int iDrive, LPCTSTR pszImageFilename, const bool bForceWriteProtected, const bool bCreateIfNecessary);
	void EjectDisk(const int iDrive);

	bool IsConditionForFullSpeed(void);
	BOOL IsSpinning(void);
	void NotifyInvalidImage(const int iDrive, LPCTSTR pszImageFilename, const ImageError_e Error);
	void Reset(const bool bIsPowerCycle=false);
	bool GetProtect(const int iDrive);
	void SetProtect(const int iDrive, const bool bWriteProtect);
	int GetCurrentDrive(void);
	int GetCurrentTrack();
	int GetTrack(const int drive);
	int GetCurrentPhase(void);
	int GetCurrentOffset(void);
	LPCTSTR GetCurrentState(void);
	bool UserSelectNewDiskImage(const int iDrive, LPCSTR pszFilename="");
	void UpdateDriveState(DWORD);
	bool DriveSwap(void);

	std::string GetSnapshotCardName(void);
	void SaveSnapshot(class YamlSaveHelper& yamlSaveHelper);
	bool LoadSnapshot(class YamlLoadHelper& yamlLoadHelper, UINT slot, UINT version);

	void LoadLastDiskImage(const int iDrive);
	void SaveLastDiskImage(const int iDrive);

	bool IsDiskImageWriteProtected(const int iDrive);
	bool IsDriveEmpty(const int iDrive);

	bool GetEnhanceDisk(void);
	void SetEnhanceDisk(bool bEnhanceDisk);

	static BYTE __stdcall IORead(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nExecutedCycles);
	static BYTE __stdcall IOWrite(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nExecutedCycles);

private:
	void CheckSpinning(const ULONG nExecutedCycles);
	Disk_Status_e GetDriveLightStatus(const int iDrive);
	bool IsDriveValid(const int iDrive);
	void AllocTrack(const int iDrive);
	void ReadTrack(const int iDrive);
	void RemoveDisk(const int iDrive);
	void WriteTrack(const int iDrive);
	LPCTSTR DiskGetFullPathName(const int iDrive);
	void SaveSnapshotDisk2Unit(YamlSaveHelper& yamlSaveHelper, UINT unit);
	void LoadSnapshotDriveUnit(YamlLoadHelper& yamlLoadHelper, UINT unit);

	void __stdcall ControlStepper(WORD, WORD address, BYTE, BYTE, ULONG uExecutedCycles);
	void __stdcall ControlMotor(WORD, WORD address, BYTE, BYTE, ULONG uExecutedCycles);
	void __stdcall Enable(WORD, WORD address, BYTE, BYTE, ULONG uExecutedCycles);
	void __stdcall ReadWrite(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nExecutedCycles);
	void __stdcall LoadWriteProtect(WORD, WORD, BYTE write, BYTE value, ULONG);
	void __stdcall SetReadMode(WORD, WORD, BYTE, BYTE, ULONG);
	void __stdcall SetWriteMode(WORD, WORD, BYTE, BYTE, ULONG uExecutedCycles);

#if LOG_DISK_NIBBLES_WRITE
	bool LogWriteCheckSyncFF(ULONG& uCycleDelta);
#endif

	//

	WORD m_currDrive;
	Drive_t m_floppyDrive[NUM_DRIVES];
	BYTE m_floppyLatch;
	BOOL m_floppyMotorOn;
	BOOL m_floppyLoadMode;	// for efficiency this is not used; it's extremely unlikely to affect emulation (nickw)
	BOOL m_floppyWriteMode;
	WORD m_phases;			// state bits for stepper magnet phases 0 - 3
	bool m_saveDiskImage;
	UINT m_slot;
	unsigned __int64 m_diskLastCycle;
	unsigned __int64 m_diskLastReadLatchCycle;
	FormatTrack m_formatTrack;
	bool m_enhanceDisk;

	static const UINT SPINNING_CYCLES = 20000*64;	// 1280000 cycles = 1.25s
	static const UINT WRITELIGHT_CYCLES = 20000*64;	// 1280000 cycles = 1.25s

	// Debug:
#if LOG_DISK_NIBBLES_USE_RUNTIME_VAR
	bool m_bLogDisk_NibblesRW;	// From VS Debugger, change this to true/false during runtime for precise nibble logging
#endif
#if LOG_DISK_NIBBLES_WRITE
	UINT64 m_uWriteLastCycle;
	UINT m_uSyncFFCount;
#endif
};
