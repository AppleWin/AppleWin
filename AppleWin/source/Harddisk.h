#pragma once

enum HardDrive_e
{
	HARDDISK_1 = 0,
	HARDDISK_2,
	NUM_HARDDISKS
};

bool HD_CardIsEnabled(void);
void HD_SetEnabled(const bool bEnabled);
LPCTSTR HD_GetFullName(const int iDrive);
VOID HD_Load_Rom(const LPBYTE pCxRomPeripheral, const UINT uSlot);
VOID HD_Cleanup(void);
BOOL HD_InsertDisk2(const int iDrive, LPCTSTR pszFilename);
BOOL HD_InsertDisk(const int iDrive, LPCTSTR pszImageFilename);
void HD_Select(const int iDrive);
void HD_Unplug(const int iDrive);
bool HD_IsDriveUnplugged(const int iDrive);
