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

extern BOOL enhancedisk;
extern string DiskPathFilename[NUM_DRIVES];

void    DiskInitialize(void); // DiskIIManagerStartup()
void    DiskDestroy(void); // no, doesn't "destroy" the disk image.  DiskIIManagerShutdown()

void    DiskBoot(void);
void    DiskEject(const int iDrive);

LPCTSTR DiskGetFullName(const int iDrive);
LPCTSTR DiskGetFullDiskFilename(const int iDrive);
LPCTSTR DiskGetBaseName(const int iDrive);

void    DiskGetLightStatus (Disk_Status_e *pDisk1Status_, Disk_Status_e *pDisk2Status_);

ImageError_e DiskInsert(const int iDrive, LPCTSTR pszImageFilename, const bool bForceWriteProtected, const bool bCreateIfNecessary);
BOOL    DiskIsSpinning(void);
void    DiskNotifyInvalidImage(const int iDrive, LPCTSTR pszImageFilename, const ImageError_e Error);
void    DiskReset(void);
bool    DiskGetProtect(const int iDrive);
void    DiskSetProtect(const int iDrive, const bool bWriteProtect);
void    DiskSelect(const int iDrive);
void    DiskUpdatePosition(DWORD);
bool    DiskDriveSwap(void);
void    DiskLoadRom(LPBYTE pCxRomPeripheral, UINT uSlot);
DWORD   DiskGetSnapshot(SS_CARD_DISK2* pSS, DWORD dwSlot);
DWORD   DiskSetSnapshot(SS_CARD_DISK2* pSS, DWORD dwSlot);

void Disk_LoadLastDiskImage(const int iDrive);
void Disk_SaveLastDiskImage(const int iDrive);

bool Disk_ImageIsWriteProtected(const int iDrive);
bool Disk_IsDriveEmpty(const int iDrive);
