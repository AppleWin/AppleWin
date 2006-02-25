#pragma once

bool    HD_CardIsEnabled();
void    HD_SetEnabled(bool bEnabled);
LPCTSTR HD_GetFullName (int drive);
VOID    HD_Load_Rom(LPBYTE lpMemRom);
VOID    HD_Cleanup();
BOOL    HD_InsertDisk2(int nDrive, LPCTSTR pszFilename);
BOOL    HD_InsertDisk(int nDrive, LPCTSTR imagefilename);
void    HD_Select(int nDrive);

BYTE __stdcall HD_IO_EMUL (WORD pc, BYTE addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft);
