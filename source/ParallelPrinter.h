#pragma once

void    PrintDestroy();
void    PrintLoadRom(LPBYTE);
void    PrintReset();
void    PrintUpdate(DWORD);

BYTE __stdcall PrintStatus (WORD pc, BYTE addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft);
BYTE __stdcall PrintTransmit (WORD pc, BYTE addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft);
