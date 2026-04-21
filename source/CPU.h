#pragma once

#include "Common.h"

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

// 6502 Processor Status flags
enum {
	AF_SIGN = 0x80,
	AF_OVERFLOW = 0x40,
	AF_RESERVED = 0x20,
	AF_BREAK = 0x10,
	AF_DECIMAL = 0x08,
	AF_INTERRUPT = 0x04,
	AF_ZERO = 0x02,
	AF_CARRY = 0x01
};

extern regsrec    regs;
extern unsigned __int64 g_nCumulativeCycles;

void    CpuDestroy ();
void    CpuCalcCycles(ULONG nExecutedCycles);
DWORD   CpuExecute(const DWORD uCycles, const bool bVideoUpdate);
ULONG   CpuGetCyclesThisVideoFrame(ULONG nExecutedCycles);
void    CpuCreateCriticalSection(void);
void    CpuInitialize(void);
void    CpuSetupBenchmark ();
void	CpuIrqReset();
void	CpuIrqAssert(eIRQSRC Device);
void	CpuIrqDeassert(eIRQSRC Device);
void	CpuNmiReset();
void	CpuNmiAssert(eIRQSRC Device);
void	CpuNmiDeassert(eIRQSRC Device);
void    CpuReset ();
void    CpuSaveSnapshot(class YamlSaveHelper& yamlSaveHelper);
void    CpuLoadSnapshot(class YamlLoadHelper& yamlLoadHelper, UINT version);

BYTE	CpuRead(USHORT addr, ULONG uExecutedCycles);
void	CpuWrite(USHORT addr, BYTE value, ULONG uExecutedCycles);

enum eCpuType {CPU_UNKNOWN=0, CPU_6502=1, CPU_65C02, CPU_Z80};	// Don't change! Persisted to Registry

eCpuType GetMainCpu(void);
void     SetMainCpu(eCpuType cpu);
eCpuType ProbeMainCpuDefault(eApple2Type apple2Type);
void     SetMainCpuDefault(eApple2Type apple2Type);
eCpuType GetActiveCpu(void);
void     SetActiveCpu(eCpuType cpu);

bool IsIrqAsserted(void);
bool Is6502InterruptEnabled(void);
void ResetCyclesExecutedForDebugger(void);
bool IsInterruptInLastExecution(void);
void SetIrqOnLastOpcodeCycle(void);
