#pragma once

#define  CPU_COMPILING     0
#define  CPU_INTERPRETIVE  1
#define  CPU_FASTPAGING    2

typedef struct _regsrec {
  BYTE a;   // accumulator
  BYTE x;   // index X
  BYTE y;   // index Y
  BYTE ps;  // processor status
  WORD pc;  // program counter
  WORD sp;  // stack pointer
  BYTE bIRQ;	// IRQ asserted flag 
} regsrec, *regsptr;

extern DWORD      cpuemtype;
extern regsrec    regs;
extern unsigned __int64 g_nCumulativeCycles;

void    CpuDestroy ();
void    CpuCalcCycles(ULONG nCyclesLeft);
DWORD   CpuExecute (DWORD);
void    CpuGetCode (WORD,LPBYTE *,DWORD *);
ULONG   CpuGetCyclesThisFrame();
void    CpuInitialize ();
void    CpuReinitialize ();
void    CpuResetCompilerData ();
void    CpuSetupBenchmark ();
BOOL    CpuSupportsFastPaging ();
void    CpuIRQ ();
DWORD   CpuGetSnapshot(SS_CPU6502* pSS);
DWORD   CpuSetSnapshot(SS_CPU6502* pSS);
