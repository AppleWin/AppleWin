#pragma once

extern BYTE __stdcall TapeRead (WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft);
extern BYTE __stdcall TapeWrite (WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft);
extern bool __stdcall GetCapsLockAllowed ();
