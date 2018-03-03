#pragma once

extern BYTE __stdcall TapeRead(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nExecutedCycles);
extern BYTE __stdcall TapeWrite(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nExecutedCycles);
extern bool GetCapsLockAllowed(void);
