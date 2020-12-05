#include "stdafx.h"

#include "../../source/Windows/AppleWin.h"
#include "../../source/CPU.h"
#include "../../source/Memory.h"
#include "../../source/SynchronousEventManager.h"

// From Applewin.cpp
bool g_bFullSpeed = false;
enum AppMode_e g_nAppMode = MODE_RUNNING;
SynchronousEventManager g_SynchronousEventMgr;

// From Memory.cpp
LPBYTE         memwrite[0x100];		// TODO: Init
LPBYTE         mem          = NULL;	// TODO: Init
LPBYTE         memdirty     = NULL;	// TODO: Init
iofunction		IORead[256] = {0};	// TODO: Init
iofunction		IOWrite[256] = {0};	// TODO: Init

BYTE __stdcall IO_F8xx(WORD programcounter, WORD address, BYTE write, BYTE value, ULONG nCycles)
{
	return 0;
}

// From CPU.cpp
#define	 AF_SIGN       0x80
#define	 AF_OVERFLOW   0x40
#define	 AF_RESERVED   0x20
#define	 AF_BREAK      0x10
#define	 AF_DECIMAL    0x08
#define	 AF_INTERRUPT  0x04
#define	 AF_ZERO       0x02
#define	 AF_CARRY      0x01

regsrec regs;

bool g_irqOnLastOpcodeCycle = false;

static eCpuType g_ActiveCPU = CPU_65C02;

eCpuType GetActiveCpu(void)
{
	return g_ActiveCPU;
}

bool g_bStopOnBRK = false;

static __forceinline int Fetch(BYTE& iOpcode, ULONG uExecutedCycles)
{
	iOpcode = *(mem+regs.pc);
	regs.pc++;

	if (iOpcode == 0x00 && g_bStopOnBRK)
		return 0;

	return 1;
}

static __forceinline void DoIrqProfiling(DWORD uCycles)
{
}

static __forceinline void CheckSynchronousInterruptSources(UINT cycles, ULONG uExecutedCycles)
{
}

static __forceinline void NMI(ULONG& uExecutedCycles, BOOL& flagc, BOOL& flagn, BOOL& flagv, BOOL& flagz)
{
}

static __forceinline void IRQ(ULONG& uExecutedCycles, BOOL& flagc, BOOL& flagn, BOOL& flagv, BOOL& flagz)
{
}

// From z80.cpp
DWORD z80_mainloop(ULONG uTotalCycles, ULONG uExecutedCycles)
{
	return 0;
}

// From NTSC.cpp
void NTSC_VideoUpdateCycles( long cycles6502 )
{
}

//-------------------------------------

#include "../../source/CPU/cpu_general.inl"
#include "../../source/CPU/cpu_instructions.inl"

#define READ _READ_WITH_IO_F8xx
#define WRITE(a) _WRITE_WITH_IO_F8xx(a)
#define HEATMAP_X(pc)

#include "../../source/CPU/cpu6502.h"  // MOS 6502

#undef READ
#undef WRITE

//-------

#define READ _READ
#define WRITE(a) _WRITE(a)

#include "../../source/CPU/cpu65C02.h"  // WDC 65C02

#undef READ
#undef WRITE
#undef HEATMAP_X

//-------------------------------------

void init(void)
{
	// memory must be zero initialised like MemInitiaize() does.
	mem = (LPBYTE)calloc(64, 1024);

	for (UINT i=0; i<256; i++)
		memwrite[i] = mem+i*256;

	memdirty = new BYTE[256];
}

void reset(void)
{
	regs.a  = 0;
	regs.x  = 0;
	regs.y  = 0;
	regs.pc = 0x300;
	regs.sp = 0x1FF;
	regs.ps = 0;
	regs.bJammed = 0;
}

//-------------------------------------

DWORD TestCpu6502(DWORD uTotalCycles)
{
	return Cpu6502(uTotalCycles, true);
}

DWORD TestCpu65C02(DWORD uTotalCycles)
{
	return Cpu65C02(uTotalCycles, true);
}

//-------------------------------------

int GH264_test(void)
{
	// No page-cross
	reset();
	regs.pc = 0x300;
	WORD abs = regs.pc+3;
	WORD dst = abs+2;
	mem[regs.pc+0] = 0x6c;	// JMP (IND) 
	mem[regs.pc+1] = abs&0xff;
	mem[regs.pc+2] = abs>>8;
	mem[regs.pc+3] = dst&0xff;
	mem[regs.pc+4] = dst>>8;

	DWORD cycles = TestCpu6502(0);
	if (cycles != 5) return 1;
	if (regs.pc != dst) return 1;

	reset();
	cycles = TestCpu65C02(0);
	if (cycles != 6) return 1;
	if (regs.pc != dst) return 1;

	// Page-cross
	reset();
	regs.pc = 0x3fc;		// 3FC: JMP (abs)
	abs = regs.pc+3;		// 3FF: lo(dst), hi(dst)
	dst = abs+2;
	mem[regs.pc+0] = 0x6c;	// JMP (IND)
	mem[regs.pc+1] = abs&0xff;
	mem[regs.pc+2] = abs>>8;
	mem[regs.pc+3] = dst&0xff;
	mem[regs.pc+4] = mem[regs.pc & ~0xff] = dst>>8;	// Allow for bug in 6502

	cycles = TestCpu6502(0);
	if (cycles != 5) return 1;
	if (regs.pc != dst) return 1;

	reset();
	regs.pc = 0x3fc;
	mem[regs.pc & ~0xff] = 0;	// Test that 65C02 fixes the bug in the 6502
	cycles = TestCpu65C02(0);
	if (cycles != 7) return 1;	// todo: is this 6 or 7?
	if (regs.pc != dst) return 1;

	return 0;
}

//-------------------------------------

void ASL_ABSX(BYTE x, WORD base, BYTE d)
{
	WORD addr = base+x;
	mem[addr] = d;

	reset();
	regs.x = x;
	mem[regs.pc+0] = 0x1e;
	mem[regs.pc+1] = base&0xff;
	mem[regs.pc+2] = base>>8;
}

void DEC_ABSX(BYTE x, WORD base, BYTE d)
{
	WORD addr = base+x;
	mem[addr] = d;

	reset();
	regs.x = x;
	mem[regs.pc+0] = 0xde;
	mem[regs.pc+1] = base&0xff;
	mem[regs.pc+2] = base>>8;
}

void INC_ABSX(BYTE x, WORD base, BYTE d)
{
	WORD addr = base+x;
	mem[addr] = d;

	reset();
	regs.x = x;
	mem[regs.pc+0] = 0xfe;
	mem[regs.pc+1] = base&0xff;
	mem[regs.pc+2] = base>>8;
}

int GH271_test(void)
{
	// asl abs,x
	{
		const WORD base = 0x20ff;
		const BYTE d = 0x40;

		// no page-cross
		{
			const BYTE x = 0;

			ASL_ABSX(x, base, d);
			if (TestCpu6502(0) != 7) return 1;
			if (mem[base+x] != ((d<<1)&0xff)) return 1;

			ASL_ABSX(x, base, d);
			if (TestCpu65C02(0) != 6) return 1;	// Non-PX case is optimised on 65C02
			if (mem[base+x] != ((d<<1)&0xff)) return 1;
		}

		// page-cross
		{
			const BYTE x = 1;

			ASL_ABSX(x, base, d);
			if (TestCpu6502(0) != 7) return 1;
			if (mem[base+x] != ((d<<1)&0xff)) return 1;

			ASL_ABSX(x, base, d);
			if (TestCpu65C02(0) != 7) return 1;
			if (mem[base+x] != ((d<<1)&0xff)) return 1;
		}
	}

	// dec abs,x
	{
		const WORD base = 0x20ff;
		const BYTE d = 0x40;

		// no page-cross
		{
			const BYTE x = 0;

			DEC_ABSX(x, base, d);
			if (TestCpu6502(0) != 7) return 1;
			if (mem[base+x] != ((d-1)&0xff)) return 1;

			DEC_ABSX(x, base, d);
			if (TestCpu65C02(0) != 7) return 1;	// NB. Not optimised for 65C02
			if (mem[base+x] != ((d-1)&0xff)) return 1;
		}

		// page-cross
		{
			const BYTE x = 1;

			DEC_ABSX(x, base, d);
			if (TestCpu6502(0) != 7) return 1;
			if (mem[base+x] != ((d-1)&0xff)) return 1;

			DEC_ABSX(x, base, d);
			if (TestCpu65C02(0) != 7) return 1;
			if (mem[base+x] != ((d-1)&0xff)) return 1;
		}
	}

	// inc abs,x
	{
		const WORD base = 0x20ff;
		const BYTE d = 0x40;

		// no page-cross
		{
			const BYTE x = 0;

			INC_ABSX(x, base, d);
			if (TestCpu6502(0) != 7) return 1;
			if (mem[base+x] != ((d+1)&0xff)) return 1;

			INC_ABSX(x, base, d);
			if (TestCpu65C02(0) != 7) return 1;	// NB. Not optimised for 65C02
			if (mem[base+x] != ((d+1)&0xff)) return 1;
		}

		// page-cross
		{
			const BYTE x = 1;

			INC_ABSX(x, base, d);
			if (TestCpu6502(0) != 7) return 1;
			if (mem[base+x] != ((d+1)&0xff)) return 1;

			INC_ABSX(x, base, d);
			if (TestCpu65C02(0) != 7) return 1;
			if (mem[base+x] != ((d+1)&0xff)) return 1;
		}
	}

	return 0;
}

//-------------------------------------

enum {CYC_6502=0, CYC_6502_PX, CYC_65C02, CYC_65C02_PX};

const BYTE g_OpcodeTimings[256][4] =
{
// 6502 (no page-cross), 6502 (page-cross), 65C02 (no page-cross), 65C02 (page-cross)
	{7,7,7,7},	// 00
	{6,6,6,6},	// 01
	{2,2,2,2},	// 02
	{8,8,1,1},	// 03
	{3,3,5,5},	// 04
	{3,3,3,3},	// 05
	{5,5,5,5},	// 06
	{5,5,1,1},	// 07
	{3,3,3,3},	// 08
	{2,2,2,2},	// 09
	{2,2,2,2},	// 0A
	{2,2,1,1},	// 0B
	{4,5,6,6},	// 0C
	{4,4,4,4},	// 0D
	{6,6,6,6},	// 0E
	{6,6,1,1},	// 0F
	{3,3,3,3},	// 10
	{5,6,5,6},	// 11
	{2,2,5,5},	// 12
	{8,8,1,1},	// 13
	{4,4,5,5},	// 14
	{4,4,4,4},	// 15
	{6,6,6,6},	// 16
	{6,6,1,1},	// 17
	{2,2,2,2},	// 18
	{4,5,4,5},	// 19
	{2,2,2,2},	// 1A
	{7,7,1,1},	// 1B
	{4,5,6,6},	// 1C
	{4,5,4,5},	// 1D
	{7,7,6,7},	// 1E
	{7,7,1,1},	// 1F
	{6,6,6,6},	// 20
	{6,6,6,6},	// 21
	{2,2,2,2},	// 22
	{8,8,1,1},	// 23
	{3,3,3,3},	// 24
	{3,3,3,3},	// 25
	{5,5,5,5},	// 26
	{5,5,1,1},	// 27
	{4,4,4,4},	// 28
	{2,2,2,2},	// 29
	{2,2,2,2},	// 2A
	{2,2,1,1},	// 2B
	{4,4,4,4},	// 2C
	{4,4,4,4},	// 2D
	{6,6,6,6},	// 2E
	{6,6,1,1},	// 2F
	{2,2,2,2},	// 30
	{5,6,5,6},	// 31
	{2,2,5,5},	// 32
	{8,8,1,1},	// 33
	{4,4,4,4},	// 34
	{4,4,4,4},	// 35
	{6,6,6,6},	// 36
	{6,6,1,1},	// 37
	{2,2,2,2},	// 38
	{4,5,4,5},	// 39
	{2,2,2,2},	// 3A
	{7,7,1,1},	// 3B
	{4,5,4,5},	// 3C
	{4,5,4,5},	// 3D
	{7,7,6,7},	// 3E
	{7,7,1,1},	// 3F
	{6,6,6,6},	// 40
	{6,6,6,6},	// 41
	{2,2,2,2},	// 42
	{8,8,1,1},	// 43
	{3,3,3,3},	// 44
	{3,3,3,3},	// 45
	{5,5,5,5},	// 46
	{5,5,1,1},	// 47
	{3,3,3,3},	// 48
	{2,2,2,2},	// 49
	{2,2,2,2},	// 4A
	{2,2,1,1},	// 4B
	{3,3,3,3},	// 4C
	{4,4,4,4},	// 4D
	{6,6,6,6},	// 4E
	{6,6,1,1},	// 4F
	{3,3,3,3},	// 50
	{5,6,5,6},	// 51
	{2,2,5,5},	// 52
	{8,8,1,1},	// 53
	{4,4,4,4},	// 54
	{4,4,4,4},	// 55
	{6,6,6,6},	// 56
	{6,6,1,1},	// 57
	{2,2,2,2},	// 58
	{4,5,4,5},	// 59
	{2,2,3,3},	// 5A
	{7,7,1,1},	// 5B
	{4,5,8,8},	// 5C
	{4,5,4,5},	// 5D
	{7,7,6,7},	// 5E
	{7,7,1,1},	// 5F
	{6,6,6,6},	// 60
	{6,6,6,6},	// 61
	{2,2,2,2},	// 62
	{8,8,1,1},	// 63
	{3,3,3,3},	// 64
	{3,3,3,3},	// 65
	{5,5,5,5},	// 66
	{5,5,1,1},	// 67
	{4,4,4,4},	// 68
	{2,2,2,2},	// 69
	{2,2,2,2},	// 6A
	{2,2,1,1},	// 6B
	{5,5,7,7},	// 6C
	{4,4,4,4},	// 6D
	{6,6,6,6},	// 6E
	{6,6,1,1},	// 6F
	{2,2,2,2},	// 70
	{5,6,5,6},	// 71
	{2,2,5,5},	// 72
	{8,8,1,1},	// 73
	{4,4,4,4},	// 74
	{4,4,4,4},	// 75
	{6,6,6,6},	// 76
	{6,6,1,1},	// 77
	{2,2,2,2},	// 78
	{4,5,4,5},	// 79
	{2,2,4,4},	// 7A
	{7,7,1,1},	// 7B
	{4,5,6,6},	// 7C
	{4,5,4,5},	// 7D
	{7,7,6,7},	// 7E
	{7,7,1,1},	// 7F
	{2,2,3,3},	// 80
	{6,6,6,6},	// 81
	{2,2,2,2},	// 82
	{6,6,1,1},	// 83
	{3,3,3,3},	// 84
	{3,3,3,3},	// 85
	{3,3,3,3},	// 86
	{3,3,1,1},	// 87
	{2,2,2,2},	// 88
	{2,2,2,2},	// 89
	{2,2,2,2},	// 8A
	{2,2,1,1},	// 8B
	{4,4,4,4},	// 8C
	{4,4,4,4},	// 8D
	{4,4,4,4},	// 8E
	{4,4,1,1},	// 8F
	{3,3,3,3},	// 90
	{6,6,6,6},	// 91
	{2,2,5,5},	// 92
	{6,6,1,1},	// 93
	{4,4,4,4},	// 94
	{4,4,4,4},	// 95
	{4,4,4,4},	// 96
	{4,4,1,1},	// 97
	{2,2,2,2},	// 98
	{5,5,5,5},	// 99
	{2,2,2,2},	// 9A
	{5,5,1,1},	// 9B
	{5,5,4,4},	// 9C
	{5,5,5,5},	// 9D
	{5,5,5,5},	// 9E
	{5,5,1,1},	// 9F
	{2,2,2,2},	// A0
	{6,6,6,6},	// A1
	{2,2,2,2},	// A2
	{6,6,1,1},	// A3
	{3,3,3,3},	// A4
	{3,3,3,3},	// A5
	{3,3,3,3},	// A6
	{3,3,1,1},	// A7
	{2,2,2,2},	// A8
	{2,2,2,2},	// A9
	{2,2,2,2},	// AA
	{2,2,1,1},	// AB
	{4,4,4,4},	// AC
	{4,4,4,4},	// AD
	{4,4,4,4},	// AE
	{4,4,1,1},	// AF
	{2,2,2,2},	// B0
	{5,6,5,6},	// B1
	{2,2,5,5},	// B2
	{5,6,1,1},	// B3
	{4,4,4,4},	// B4
	{4,4,4,4},	// B5
	{4,4,4,4},	// B6
	{4,4,1,1},	// B7
	{2,2,2,2},	// B8
	{4,5,4,5},	// B9
	{2,2,2,2},	// BA
	{4,5,1,1},	// BB
	{4,5,4,5},	// BC
	{4,5,4,5},	// BD
	{4,5,4,5},	// BE
	{4,5,1,1},	// BF
	{2,2,2,2},	// C0
	{6,6,6,6},	// C1
	{2,2,2,2},	// C2
	{8,8,1,1},	// C3
	{3,3,3,3},	// C4
	{3,3,3,3},	// C5
	{5,5,5,5},	// C6
	{5,5,1,1},	// C7
	{2,2,2,2},	// C8
	{2,2,2,2},	// C9
	{2,2,2,2},	// CA
	{2,2,1,1},	// CB
	{4,4,4,4},	// CC
	{4,4,4,4},	// CD
	{6,6,6,6},	// CE
	{6,6,1,1},	// CF
	{3,3,3,3},	// D0
	{5,6,5,6},	// D1
	{2,2,5,5},	// D2
	{8,8,1,1},	// D3
	{4,4,4,4},	// D4
	{4,4,4,4},	// D5
	{6,6,6,6},	// D6
	{6,6,1,1},	// D7
	{2,2,2,2},	// D8
	{4,5,4,5},	// D9
	{2,2,3,3},	// DA
	{7,7,1,1},	// DB
	{4,5,4,4},	// DC
	{4,5,4,5},	// DD
	{7,7,7,7},	// DE
	{7,7,1,1},	// DF
	{2,2,2,2},	// E0
	{6,6,6,6},	// E1
	{2,2,2,2},	// E2
	{8,8,1,1},	// E3
	{3,3,3,3},	// E4
	{3,3,3,3},	// E5
	{5,5,5,5},	// E6
	{5,5,1,1},	// E7
	{2,2,2,2},	// E8
	{2,2,2,2},	// E9
	{2,2,2,2},	// EA
	{2,2,1,1},	// EB
	{4,4,4,4},	// EC
	{4,4,4,4},	// ED
	{6,6,6,6},	// EE
	{6,6,1,1},	// EF
	{2,2,2,2},	// F0
	{5,6,5,6},	// F1
	{2,2,5,5},	// F2
	{8,8,1,1},	// F3
	{4,4,4,4},	// F4
	{4,4,4,4},	// F5
	{6,6,6,6},	// F6
	{6,6,1,1},	// F7
	{2,2,2,2},	// F8
	{4,5,4,5},	// F9
	{2,2,4,4},	// FA
	{7,7,1,1},	// FB
	{4,5,4,4},	// FC
	{4,5,4,5},	// FD
	{7,7,7,7},	// FE
	{7,7,1,1},	// FF
};

int GH278_Bcc_Sub(BYTE op, BYTE ps_not_taken, BYTE ps_taken, WORD pc)
{
	mem[pc+0] = op;
	mem[pc+1] = 0x01;
	const WORD dst_not_taken = pc+2;
	const WORD dst_taken     = pc+2 + mem[pc+1];

	const int pagecross = (((pc+2) ^ dst_taken) >> 8) & 1;

	// 6502

	reset();
	regs.pc = pc;
	regs.ps = ps_not_taken;
	if (TestCpu6502(0) != 2) return 1;
	if (regs.pc != dst_not_taken) return 1;

	reset();
	regs.pc = pc;
	regs.ps = ps_taken;
	if (TestCpu6502(0) != 3+pagecross) return 1;
	if (regs.pc != dst_taken) return 1;

	// 65C02

	reset();
	regs.pc = pc;
	regs.ps = ps_not_taken;
	if (TestCpu65C02(0) != 2) return 1;
	if (regs.pc != dst_not_taken) return 1;

	reset();
	regs.pc = pc;
	regs.ps = ps_taken;
	if (TestCpu65C02(0) != 3+pagecross) return 1;
	if (regs.pc != dst_taken) return 1;

	return 0;
}

int GH278_Bcc(BYTE op, BYTE ps_not_taken, BYTE ps_taken)
{
	if (GH278_Bcc_Sub(op, ps_not_taken, ps_taken, 0x300)) return 1;	// no page cross
	if (GH278_Bcc_Sub(op, ps_not_taken, ps_taken, 0x3FD)) return 1;	// page cross

	return 0;
}

int GH278_BRA(void)
{
	// No page-cross
	{
		WORD pc = 0x300;
		mem[pc+0] = 0x80;	// BRA
		mem[pc+1] = 0x01;
		const WORD dst_taken = pc+2 + mem[pc+1];

		reset();
		regs.pc = pc;
		if (TestCpu65C02(0) != 3) return 1;
		if (regs.pc != dst_taken) return 1;
	}

	// Page-cross
	{
		WORD pc = 0x3FD;
		mem[pc+0] = 0x80;	// BRA
		mem[pc+1] = 0x01;
		const WORD dst_taken = pc+2 + mem[pc+1];

		reset();
		regs.pc = pc;
		if (TestCpu65C02(0) != 4) return 1;
		if (regs.pc != dst_taken) return 1;
	}

	return 0;
}

int GH278_JMP_INDX(void)
{
	// No page-cross
	reset();
	regs.pc = 0x300;
	WORD abs = regs.pc+3;
	WORD dst = abs+2;
	mem[regs.pc+0] = 0x7c;	// JMP (IND,X)
	mem[regs.pc+1] = abs&0xff;
	mem[regs.pc+2] = abs>>8;
	mem[regs.pc+3] = dst&0xff;
	mem[regs.pc+4] = dst>>8;

	DWORD cycles = TestCpu65C02(0);
	if (cycles != 6) return 1;
	if (regs.pc != dst) return 1;

	// Page-cross (case 1)
	reset();
	regs.pc = 0x3fc;
	abs = regs.pc+3;
	dst = abs+2;
	mem[regs.pc+0] = 0x7c;	// JMP (IND,X)
	mem[regs.pc+1] = abs&0xff;
	mem[regs.pc+2] = abs>>8;
	mem[regs.pc+3] = dst&0xff;
	mem[regs.pc+4] = dst>>8;

	cycles = TestCpu65C02(0);
	if (cycles != 6) return 1;	// todo: is this 6 or 7?
	if (regs.pc != dst) return 1;

	// Page-cross (case 2)
	reset();
	regs.x = 1;
	regs.pc = 0x3fa;
	abs = regs.pc+3;
	dst = abs+2 + regs.x;
	mem[regs.pc+0] = 0x7c;	// JMP (IND,X)
	mem[regs.pc+1] = abs&0xff;
	mem[regs.pc+2] = abs>>8;
	mem[regs.pc+3] = 0xcc;	// unused
	mem[regs.pc+4] = dst&0xff;
	mem[regs.pc+5] = dst>>8;

	cycles = TestCpu65C02(0);
	if (cycles != 6) return 1;	// todo: is this 6 or 7?
	if (regs.pc != dst) return 1;

	return 0;
}

int GH278_ADC_SBC(UINT op)
{
	const WORD base = 0x20ff;
	reset();
	mem[regs.pc+0] = op;
	mem[regs.pc+1] = base&0xff;
	mem[regs.pc+2] = base>>8;
	mem[0xff] = 0xff; mem[0x00] = 0x00;	// For: OPCODE (zp),Y

	// No page-cross
	reset();
	regs.ps = AF_DECIMAL;
	DWORD cycles = TestCpu6502(0);
	if (g_OpcodeTimings[op][CYC_6502] != cycles) return 1;

	reset();
	regs.ps = AF_DECIMAL;
	cycles = TestCpu65C02(0);
	if (g_OpcodeTimings[op][CYC_65C02]+1 != cycles) return 1;	// CMOS is +1 cycles in decimal mode

	// Page-cross
	reset();
	regs.ps = AF_DECIMAL;
	regs.x = 1;
	regs.y = 1;
	cycles = TestCpu6502(0);
	if (g_OpcodeTimings[op][CYC_6502_PX] != cycles) return 1;

	reset();
	regs.ps = AF_DECIMAL;
	regs.x = 1;
	regs.y = 1;
	cycles = TestCpu65C02(0);
	if (g_OpcodeTimings[op][CYC_65C02_PX]+1 != cycles) return 1;	// CMOS is +1 cycles in decimal mode

	return 0;
}

int GH278_ADC(void)
{
	const BYTE adc[] = {0x61,0x65,0x69,0x6D,0x71,0x72,0x75,0x79,0x7D};

	for (UINT i = 0; i<sizeof(adc); i++)
	{
		if (GH278_ADC_SBC(adc[i])) return 1;
	}

	return 0;
}

int GH278_SBC(void)
{
	const BYTE sbc[] = {0xE1,0xE5,0xE9,0xED,0xF1,0xF2,0xF5,0xF9,0xFD};

	for (UINT i = 0; i<sizeof(sbc); i++)
	{
		if (GH278_ADC_SBC(sbc[i])) return 1;
	}

	return 0;
}

int GH278_test(void)
{
	int variant = 0;

	//
	// 6502
	//

	// No page-cross
	for (UINT op=0; op<256; op++)
	{
		reset();
		WORD base = 0x20ff;
		mem[regs.pc+0] = op;
		mem[regs.pc+1] = base&0xff;
		mem[regs.pc+2] = base>>8;
		DWORD cycles = TestCpu6502(0);
		if (g_OpcodeTimings[op][variant] != cycles) return 1;
	}

	variant++;

	// Page-cross
	for (UINT op=0; op<256; op++)
	{
		reset();
		regs.x = 1;
		regs.y = 1;
		WORD base = 0x20ff;
		mem[regs.pc+0] = op;
		mem[regs.pc+1] = base&0xff;
		mem[regs.pc+2] = base>>8;
		mem[0xff] = 0xff; mem[0x00] = 0x00;	// For: OPCODE (zp),Y
		DWORD cycles = TestCpu6502(0);
		if (g_OpcodeTimings[op][variant] != cycles) return 1;
	}

	variant++;

	//
	// 65C02
	//

	// No page-cross
	for (UINT op=0; op<256; op++)
	{
		reset();
		WORD base = 0x20ff;
		mem[regs.pc+0] = op;
		mem[regs.pc+1] = base&0xff;
		mem[regs.pc+2] = base>>8;
		DWORD cycles = TestCpu65C02(0);
		if (g_OpcodeTimings[op][variant] != cycles) return 1;
	}

	variant++;

	// Page-cross
	for (UINT op=0; op<256; op++)
	{
		reset();
		regs.x = 1;
		regs.y = 1;
		WORD base = 0x20ff;
		mem[regs.pc+0] = op;
		mem[regs.pc+1] = base&0xff;
		mem[regs.pc+2] = base>>8;
		mem[0xff] = 0xff; mem[0x00] = 0x00;	// For: OPCODE (zp),Y
		DWORD cycles = TestCpu65C02(0);
		if (g_OpcodeTimings[op][variant] != cycles) return 1;
	}

	//
	// Bcc
	//

	if (GH278_Bcc(0x10, AF_SIGN, 0)) return 1;		// BPL
	if (GH278_Bcc(0x30, 0, AF_SIGN)) return 1;		// BMI
	if (GH278_Bcc(0x50, AF_OVERFLOW, 0)) return 1;	// BVC
	if (GH278_Bcc(0x70, 0, AF_OVERFLOW)) return 1;	// BVS
	if (GH278_Bcc(0x90, AF_CARRY, 0)) return 1;		// BCC
	if (GH278_Bcc(0xB0, 0, AF_CARRY)) return 1;		// BCS
	if (GH278_Bcc(0xD0, AF_ZERO, 0)) return 1;		// BNE
	if (GH278_Bcc(0xF0, 0, AF_ZERO)) return 1;		// BEQ
	if (GH278_BRA()) return 1;						// BRA

	//
	// JMP (IND) and JMP (IND,X)
	// . NB. GH264_test() tests JMP (IND)
	//

	if (GH278_JMP_INDX()) return 1;

	//
	// ADC/SBC CMOS decimal mode is +1 cycles
	//

	if (GH278_ADC()) return 1;
	if (GH278_SBC()) return 1;

	return 0;
}

//-------------------------------------

DWORD AXA_ZPY(BYTE a, BYTE x, BYTE y, WORD base)
{
	reset();
	mem[0xfe] = base&0xff;
	mem[0xff] = base>>8;
	regs.a = a;
	regs.x = x;
	regs.y = y;
	mem[regs.pc+0] = 0x93;
	mem[regs.pc+1] = 0xfe;
	return TestCpu6502(0);
}

DWORD AXA_ABSY(BYTE a, BYTE x, BYTE y, WORD base)
{
	reset();
	regs.a = a;
	regs.x = x;
	regs.y = y;
	mem[regs.pc+0] = 0x9f;
	mem[regs.pc+1] = base&0xff;
	mem[regs.pc+2] = base>>8;
	return TestCpu6502(0);
}

DWORD SAY_ABSX(BYTE a, BYTE x, BYTE y, WORD base)
{
	reset();
	regs.a = a;
	regs.x = x;
	regs.y = y;
	mem[regs.pc+0] = 0x9c;
	mem[regs.pc+1] = base&0xff;
	mem[regs.pc+2] = base>>8;
	return TestCpu6502(0);
}

DWORD TAS_ABSY(BYTE a, BYTE x, BYTE y, WORD base)
{
	reset();
	regs.a = a;
	regs.x = x;
	regs.y = y;
	mem[regs.pc+0] = 0x9b;
	mem[regs.pc+1] = base&0xff;
	mem[regs.pc+2] = base>>8;
	return TestCpu6502(0);
}

DWORD XAS_ABSY(BYTE a, BYTE x, BYTE y, WORD base)
{
	reset();
	regs.a = a;
	regs.x = x;
	regs.y = y;
	mem[regs.pc+0] = 0x9e;
	mem[regs.pc+1] = base&0xff;
	mem[regs.pc+2] = base>>8;
	return TestCpu6502(0);
}

int GH282_test(void)
{
	// axa (zp),y
	{
		WORD base = 0x20ff, addr = 0x20ff;
		mem[addr] = 0xcc;
		BYTE a = 0xea, x = 0xff, y = 0;
		DWORD cycles = AXA_ZPY(a, x, y, base);
		if (cycles != 6) return 1;
		if (mem[addr] != (a & x & ((base>>8)+1))) return 1;
	}

	// axa (zp),y (page-cross)
	{
		WORD base = 0x20ff, addr = 0x2000;
		mem[addr] = 0xcc;
		BYTE a = 0xea, x = 0xff, y = 1;
		DWORD cycles = AXA_ZPY(a, x, y, base);
		if (cycles != 6) return 1;
		if (mem[addr] != (a & x & ((base>>8)+1))) return 1;
	}

	//

	// axa abs,y
	{
		WORD base = 0x20ff, addr = 0x20ff;
		mem[addr] = 0xcc;
		BYTE a = 0xea, x = 0xff, y = 0;
		DWORD cycles = AXA_ABSY(a, x, y, base);
		if (cycles != 5) return 1;
		if (mem[addr] != (a & x & ((base>>8)+1))) return 1;
	}

	// axa abs,y (page-cross)
	{
		WORD base = 0x20ff, addr = 0x2000;
		mem[addr] = 0xcc;
		BYTE a = 0xea, x = 0xff, y = 1;
		DWORD cycles = AXA_ABSY(a, x, y, base);
		if (cycles != 5) return 1;
		if (mem[addr] != (a & x & ((base>>8)+1))) return 1;
	}

	//

	// say abs,x
	{
		WORD base = 0x20ff, addr = 0x20ff;
		mem[addr] = 0xcc;
		BYTE a = 0xea, x = 0, y=0x20;
		DWORD cycles = SAY_ABSX(a, x, y, base);
		if (cycles != 5) return 1;
		if (mem[addr] != (y & ((base>>8)+1))) return 1;
	}

	// say abs,x (page-cross)
	{
		WORD base = 0x20ff, addr = 0x2000;
		mem[addr] = 0xcc;
		BYTE a = 0xea, x = 1, y=0x20;
		DWORD cycles = SAY_ABSX(a, x, y, base);
		if (cycles != 5) return 1;
		if (mem[addr] != (y & ((base>>8)+1))) return 1;
	}

	//

	// tas abs,y
	{
		WORD base = 0x20ff, addr = 0x20ff;
		mem[addr] = 0xcc;
		BYTE a = 0xea, x = 0xff, y = 0;
		DWORD cycles = TAS_ABSY(a, x, y, base);
		if (cycles != 5) return 1;
		if (mem[addr] != (a & x & ((base>>8)+1))) return 1;
		if (regs.sp != (0x100 | (a & x))) return 1;
	}

	// tas abs,y (page-cross)
	{
		WORD base = 0x20ff, addr = 0x2000;
		mem[addr] = 0xcc;
		BYTE a = 0xea, x = 0xff, y = 1;
		DWORD cycles = TAS_ABSY(a, x, y, base);
		if (cycles != 5) return 1;
		if (mem[addr] != (a & x & ((base>>8)+1))) return 1;
		if (regs.sp != (0x100 | (a & x))) return 1;
	}

	//

	// xas abs,y
	{
		WORD base = 0x20ff, addr = 0x20ff;
		mem[addr] = 0xcc;
		BYTE a = 0xea, x = 0x20, y = 0;
		DWORD cycles = XAS_ABSY(a, x, y, base);
		if (cycles != 5) return 1;
		if (mem[addr] != (x & ((base>>8)+1))) return 1;
	}

	// xas abs,y (page-cross)
	{
		WORD base = 0x20ff, addr = 0x2000;
		mem[addr] = 0xcc;
		BYTE a = 0xea, x = 0x20, y = 1;
		DWORD cycles = XAS_ABSY(a, x, y, base);
		if (cycles != 5) return 1;
		if (mem[addr] != (x & ((base>>8)+1))) return 1;
	}

	return 0;
}

//-------------------------------------

int g_fn_C000_count = 0;

BYTE __stdcall fn_C000(WORD, WORD, BYTE, BYTE, ULONG)
{
	g_fn_C000_count++;
	return 42;
}

int GH292_test(void)
{
	// Undocumented 65C02 NOPs: 1 cycle & 1 byte
	for (UINT op=0; op<256; op+=0x10)
	{
		reset();
		WORD base=regs.pc;

		mem[regs.pc] = op+0x03; if (TestCpu65C02(0) != 1 || regs.pc != base+1) return 1;
		mem[regs.pc] = op+0x07; if (TestCpu65C02(0) != 1 || regs.pc != base+2) return 1;
		mem[regs.pc] = op+0x0B; if (TestCpu65C02(0) != 1 || regs.pc != base+3) return 1;
		mem[regs.pc] = op+0x0F; if (TestCpu65C02(0) != 1 || regs.pc != base+4) return 1;
	}

	//

	// Undocumented 65C02 NOP: LDD - LoaD and Discard
	IORead[0] = fn_C000;

	reset();
	WORD base = regs.pc;
	mem[regs.pc+0] = 0xDC;
	mem[regs.pc+1] = 0x00;
	mem[regs.pc+2] = 0xC0;
	if (TestCpu65C02(0) != 4 || regs.pc != base+3 || g_fn_C000_count != 1 || regs.a != 0) return 1;

	reset();
	base = regs.pc;
	mem[regs.pc+0] = 0xFC;
	mem[regs.pc+1] = 0x00;
	mem[regs.pc+2] = 0xC0;
	if (TestCpu65C02(0) != 4 || regs.pc != base+3 || g_fn_C000_count != 2 || regs.a != 0) return 1;

	IORead[0] = NULL;

	return 0;
}

//-------------------------------------

const BYTE g_GH321_code[] =
{
// org $f156
0xA9, 0x00,			//   lda #0
0x8D, 0x7E, 0xF7,	//   sta $f77e
0xA9, 0x7F,			//   lda #$7f
0x85, 0x0B,			//   sta $0b

// f15f:
0xA9, 0x00,			//   lda #0
0x85, 0x0C,			//   sta $0c
0xA5, 0x0B,			//   lda $0b	; 0x7F
0xCD, 0x19, 0xC0,	// l1: cmp $c019
0x10, 0xFB,			//     bpl l1
0xA5, 0x0B,			//   lda $0b
0xCD, 0x19, 0xC0,	// l2: cmp $c019
0x30, 0xFB,			//     bmi l2
0xEE, 0x7E, 0xF7,	// l3: inc $f77e
0xA2, 0x09,			//     ldx #9
0xCA,				// l4:   dex
0xD0, 0xFD,			//       bne l4
0xA5, 0x0B,			//     lda $0b
0xA5, 0x0B,			//     lda $0b
0xCD, 0x19, 0xC0,	//     cmp $c019
0x10, 0xEF,			//     bpl l3
0xAD, 0x7E, 0xF7,	//   lda $f77e
0xC9, 0x47,			//   cmp #$47	; 262-191 = 71 = 0x47
0xB0, 0x07,			//   bcs l5
0xA9, 0x01,			//     lda #1		; NTSC
0x85, 0x0A,			//     sta $0a
0x4C, 0x94, 0xF1,	//   jmp $f194
0xA9, 0x00,			// l5: lda #0		; PAL
0x85, 0x0A,			//     sta $0a

// f194:
0x00
};

DWORD g_dwCyclesThisFrame = 0;	// # cycles executed in frame before Cpu65C02() was called

ULONG CpuGetCyclesThisVideoFrame(ULONG nExecutedCycles)
{
	return g_dwCyclesThisFrame + nExecutedCycles;
}

// video scanner constants
int const kHBurstClock      =    53; // clock when Color Burst starts
int const kHBurstClocks     =     4; // clocks per Color Burst duration
int const kHClock0State     =  0x18; // H[543210] = 011000
int const kHClocks          =    65; // clocks per horizontal scan (including HBL)
int const kHPEClock         =    40; // clock when HPE (horizontal preset enable) goes low
int const kHPresetClock     =    41; // clock when H state presets
int const kHSyncClock       =    49; // clock when HSync starts
int const kHSyncClocks      =     4; // clocks per HSync duration
int const kNTSCScanLines    =   262; // total scan lines including VBL (NTSC)
int const kNTSCVSyncLine    =   224; // line when VSync starts (NTSC)
int const kPALScanLines     =   312; // total scan lines including VBL (PAL)
int const kPALVSyncLine     =   264; // line when VSync starts (PAL)
int const kVLine0State      = 0x100; // V[543210CBA] = 100000000
int const kVPresetLine      =   256; // line when V state presets
int const kVSyncLines       =     4; // lines per VSync duration

bool bVideoScannerNTSC = true;

// Derived from VideoGetScannerAddress()
bool VideoGetVbl(const DWORD uExecutedCycles)
{
    // get video scanner position
    //
    int nCycles = CpuGetCyclesThisVideoFrame(uExecutedCycles);

    // calculate video parameters according to display standard
    //
    int nScanLines  = bVideoScannerNTSC ? kNTSCScanLines : kPALScanLines;
    int nScanCycles = nScanLines * kHClocks;
    nCycles %= nScanCycles;

    // calculate vertical scanning state
    //
    int nVLine  = nCycles / kHClocks; // which vertical scanning line
    int nVState = kVLine0State + nVLine; // V state bits
    if ((nVLine >= kVPresetLine)) // check for previous vertical state preset
    {
        nVState -= nScanLines; // compensate for preset
    }
    int v_3 = (nVState >> 6) & 1;
    int v_4 = (nVState >> 7) & 1;

    // update VBL' state
    //
	if (v_4 & v_3) // VBL?
	{
		return false; // Y: VBL' is false
	}
	else
	{
		return true; // N: VBL' is true
	}
}

BYTE __stdcall fn_C010(WORD nPC, WORD nAddr, BYTE nWriteFlag, BYTE nWriteValue, ULONG uExecutedCycles)
{
	if (nAddr != 0xC019)
		return 0;

	if (nWriteFlag)
		return 0;

	return VideoGetVbl(uExecutedCycles) ? 0x80 : 0;
}

int GH321_test()
{
	const UINT org = 0xf156;
	memcpy(mem+org, g_GH321_code, sizeof(g_GH321_code));
	reset();

	IORead[1] = fn_C010;
	g_bStopOnBRK = true;

	// 65C02 - CMP; CYC(4)         : Fails every 7th cycle, ie: 6, 13, 20, ...
	// 65C02 - CYC(4); CMP         : Fails every 7th cycle, ie: 2,  9, 16, ...
	// 65C02 - CYC(3); CMP; CYC(1) : Fails every 7th cycle, ie: 3, 10, 17, ...
	BYTE res[kHClocks] = {0xFF};

	UINT startCycle = 0;
	for (; startCycle < kHClocks; startCycle++)
	{
		g_dwCyclesThisFrame = startCycle;

		regs.pc = org;
		ULONG uExecutedCycles = TestCpu65C02(2 * kNTSCScanLines * kHClocks);

		res[startCycle] = mem[0x000a];
		//if (mem[0x000a] == 0)
		//	break;
	}

	//

	IORead[1] = NULL;
	g_bStopOnBRK = false;

	return mem[0x000a] == 0 ? 1 : 0;
}

//-------------------------------------

int testCB(int id, int cycles, ULONG uExecutedCycles)
{
	return 0;
}

int SyncEvents_test(void)
{
	SyncEvent syncEvent0(0, 0x10, testCB);
	SyncEvent syncEvent1(1, 0x20, testCB);
	SyncEvent syncEvent2(2, 0x30, testCB);
	SyncEvent syncEvent3(3, 0x40, testCB);

	g_SynchronousEventMgr.Insert(&syncEvent0);
	g_SynchronousEventMgr.Insert(&syncEvent1);
	g_SynchronousEventMgr.Insert(&syncEvent2);
	g_SynchronousEventMgr.Insert(&syncEvent3);
	// id0 -> id1 -> id2 -> id3
	if (syncEvent0.m_cyclesRemaining != 0x10) return 1;
	if (syncEvent1.m_cyclesRemaining != 0x10) return 1;
	if (syncEvent2.m_cyclesRemaining != 0x10) return 1;
	if (syncEvent3.m_cyclesRemaining != 0x10) return 1;

	g_SynchronousEventMgr.Remove(1);
	g_SynchronousEventMgr.Remove(3);
	g_SynchronousEventMgr.Remove(0);
	if (syncEvent2.m_cyclesRemaining != 0x30) return 1;
	g_SynchronousEventMgr.Remove(2);

	//

	syncEvent0.m_cyclesRemaining = 0x40;
	syncEvent1.m_cyclesRemaining = 0x30;
	syncEvent2.m_cyclesRemaining = 0x20;
	syncEvent3.m_cyclesRemaining = 0x10;

	g_SynchronousEventMgr.Insert(&syncEvent0);
	g_SynchronousEventMgr.Insert(&syncEvent1);
	g_SynchronousEventMgr.Insert(&syncEvent2);
	g_SynchronousEventMgr.Insert(&syncEvent3);
	// id3 -> id2 -> id1 -> id0
	if (syncEvent0.m_cyclesRemaining != 0x10) return 1;
	if (syncEvent1.m_cyclesRemaining != 0x10) return 1;
	if (syncEvent2.m_cyclesRemaining != 0x10) return 1;
	if (syncEvent3.m_cyclesRemaining != 0x10) return 1;

	g_SynchronousEventMgr.Remove(3);
	g_SynchronousEventMgr.Remove(0);
	g_SynchronousEventMgr.Remove(1);
	if (syncEvent2.m_cyclesRemaining != 0x20) return 1;
	g_SynchronousEventMgr.Remove(2);

	return 0;
}

//-------------------------------------

int _tmain(int argc, _TCHAR* argv[])
{
	int res = 1;
	init();
	reset();

//	res = GH321_test();
//	if (res) return res;

	res = GH264_test();
	if (res) return res;

	res = GH271_test();
	if (res) return res;

	res = GH278_test();
	if (res) return res;

	res = GH282_test();
	if (res) return res;

	res = GH292_test();
	if (res) return res;

	res = SyncEvents_test();
	if (res) return res;

	return 0;
}
