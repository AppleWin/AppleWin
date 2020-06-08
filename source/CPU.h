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

extern int32_t MemGetBank(int32_t addr, bool write);

// Memory heatmap for debug purpose
// Displayed as 256x256 64K memory access
extern int32_t g_aMemoryHeatmap_R[];
extern int32_t g_aMemoryHeatmap_W[];
extern int32_t g_aMemoryHeatmap_X[];
extern int32_t g_iMemoryHeatmapValue;

// heatmap macros for Read/Write/Execute used in 6502.h and 65c02.h
#define HEATMAP_W(addr) g_aMemoryHeatmap_W[ MemGetBank(addr, true)  ] = g_iMemoryHeatmapValue
#define HEATMAP_R(addr) g_aMemoryHeatmap_R[ MemGetBank(addr, false) ] = g_iMemoryHeatmapValue
#define HEATMAP_X(addr) g_aMemoryHeatmap_X[ MemGetBank(addr, false) ] = ++g_iMemoryHeatmapValue


void    CpuAdjustIrqCheck(UINT uCyclesUntilInterrupt);
void    CpuDestroy ();
void    CpuCalcCycles(ULONG nExecutedCycles);
DWORD   CpuExecute(const DWORD uCycles, const bool bVideoUpdate);
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
void    CpuSaveSnapshot(class YamlSaveHelper& yamlSaveHelper);
void    CpuLoadSnapshot(class YamlLoadHelper& yamlLoadHelper, UINT version);

BYTE	CpuRead(USHORT addr, ULONG uExecutedCycles);
void	CpuWrite(USHORT addr, BYTE a, ULONG uExecutedCycles);

enum eCpuType {CPU_UNKNOWN=0, CPU_6502=1, CPU_65C02, CPU_Z80};	// Don't change! Persisted to Registry

eCpuType GetMainCpu(void);
void     SetMainCpu(eCpuType cpu);
eCpuType ProbeMainCpuDefault(eApple2Type apple2Type);
void     SetMainCpuDefault(eApple2Type apple2Type);
eCpuType GetActiveCpu(void);
void     SetActiveCpu(eCpuType cpu);

bool Is6502InterruptEnabled(void);
void ResetCyclesExecutedForDebugger(void);
void SetMouseCardInstalled(bool installed);

