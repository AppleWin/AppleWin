#pragma once

BYTE __stdcall TapeRead(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nExecutedCycles);
BYTE __stdcall TapeWrite(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nExecutedCycles);
