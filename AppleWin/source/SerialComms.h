#pragma once

extern DWORD      serialport;

void    CommDestroy ();
void    CommReset ();
void    CommSetSerialPort (HWND,DWORD);
void    CommUpdate (DWORD);
bool	CommThInit();
void	CommThUninit();
DWORD   CommGetSnapshot(SS_IO_Comms* pSS);
DWORD   CommSetSnapshot(SS_IO_Comms* pSS);

BYTE __stdcall CommCommand (WORD pc, BYTE addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft);
BYTE __stdcall CommControl (WORD pc, BYTE addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft);
BYTE __stdcall CommDipSw (WORD pc, BYTE addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft);
BYTE __stdcall CommReceive (WORD pc, BYTE addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft);
BYTE __stdcall CommStatus (WORD pc, BYTE addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft);
BYTE __stdcall CommTransmit (WORD pc, BYTE addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft);
