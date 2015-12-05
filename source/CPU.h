#pragma once

struct regsrec
{
  BYTE a;   // accumulator
  BYTE x;   // index X
  BYTE y;   // index Y
  BYTE ps;  // processor status
  WORD pc;  // program counter
  WORD sp;  // stack pointer
  BYTE bJammed; // CPU has crashed (NMOS 6502 only)
};

extern regsrec    regs;
extern unsigned __int64 g_nCumulativeCycles;

void    CpuDestroy ();
void    CpuCalcCycles(ULONG nExecutedCycles);
DWORD   CpuExecute (DWORD);
ULONG   CpuGetCyclesThisVideoFrame(ULONG nExecutedCycles);
void    CpuInitialize ();
void    CpuSetupBenchmark ();
void	CpuIrqReset();
void	CpuIrqAssert(eIRQSRC Device);
void	CpuIrqDeassert(eIRQSRC Device);
void	CpuNmiReset();
void	CpuNmiAssert(eIRQSRC Device);
void	CpuNmiDeassert(eIRQSRC Device);
void    CpuReset ();
void    CpuSetSnapshot_v1(const BYTE A, const BYTE X, const BYTE Y, const BYTE P, const BYTE SP, const USHORT PC, const unsigned __int64 CumulativeCycles);
void    CpuSaveSnapshot(class YamlSaveHelper& yamlSaveHelper);
void    CpuLoadSnapshot(class YamlLoadHelper& yamlLoadHelper);
void    CpuGetSnapshot(struct SS_CPU6502_v2& CPU);
void    CpuSetSnapshot(const struct SS_CPU6502_v2& CPU);

BYTE	CpuRead(USHORT addr, ULONG uExecutedCycles);
void	CpuWrite(USHORT addr, BYTE a, ULONG uExecutedCycles);

DWORD   CpuGetEmulationTime_ms(void);
