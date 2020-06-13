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
extern int32_t g_aMemoryHeatmapPtr_R[];
extern int32_t g_aMemoryHeatmapPtr_W[];
extern int32_t g_iMemoryHeatmapValue;

// heatmap macros for Read/Write/Execute used in 6502.h and 65c02.h
#define HEATMAP_W(addr) if (bMemoryHeatmap) { g_aMemoryHeatmap_W[ g_aMemoryHeatmapPtr_W[addr>>8] + (addr & 0xFF) ] = g_iMemoryHeatmapValue; }
#define HEATMAP_R(addr) if (bMemoryHeatmap) { g_aMemoryHeatmap_R[ g_aMemoryHeatmapPtr_R[addr>>8] + (addr & 0xFF) ] = g_iMemoryHeatmapValue; }
#define HEATMAP_X(addr) if (bMemoryHeatmap) { g_aMemoryHeatmap_X[ g_aMemoryHeatmapPtr_R[addr>>8] + (addr & 0xFF) ] = ++g_iMemoryHeatmapValue; }


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

void	CpuEnableHeatmapGeneration(bool enable);

bool Is6502InterruptEnabled(void);
void ResetCyclesExecutedForDebugger(void);
void SetMouseCardInstalled(bool installed);

