#pragma once

#define  DRIVE_1  0
#define  DRIVE_2  1

#define  DRIVES   2
#define  TRACKS   35

extern BOOL       enhancedisk;

void    DiskInitialize (); // DiskManagerStartup()
void    DiskDestroy (); // no, doesn't "destroy" the disk image.  DiskManagerShutdown()

void    DiskBoot ();
void    DiskEject( const int iDrive );
LPCTSTR DiskGetFullName (int);
void    DiskGetLightStatus (int *,int *);
LPCTSTR DiskGetName (int);
int     DiskInsert (int,LPCTSTR,BOOL,BOOL);
BOOL    DiskIsSpinning ();
void    DiskNotifyInvalidImage (LPCTSTR,int);
void    DiskReset ();
void    DiskProtect( const int iDrive, const bool bWriteProtect );
void    DiskSelect (int);
void    DiskUpdatePosition (DWORD);
bool    DiskDriveSwap();
DWORD   DiskGetSnapshot(SS_CARD_DISK2* pSS, DWORD dwSlot);
DWORD   DiskSetSnapshot(SS_CARD_DISK2* pSS, DWORD dwSlot);

BYTE __stdcall DiskControlMotor (WORD pc, BYTE addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft);
BYTE __stdcall DiskControlStepper (WORD pc, BYTE addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft);
BYTE __stdcall DiskEnable (WORD pc, BYTE addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft);
BYTE __stdcall DiskReadWrite (WORD pc, BYTE addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft);
BYTE __stdcall DiskSetLatchValue (WORD pc, BYTE addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft);
BYTE __stdcall DiskSetReadMode (WORD pc, BYTE addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft);
BYTE __stdcall DiskSetWriteMode (WORD pc, BYTE addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft);
