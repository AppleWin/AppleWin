#include "stdafx.h"

#include "../../source/Applewin.h"
#include "../../source/CPU.h"

// From Applewin.cpp
eCPU		g_ActiveCPU = CPU_6502;
enum AppMode_e g_nAppMode = MODE_RUNNING;

// From Memory.cpp
LPBYTE         memwrite[0x100];		// TODO: Init
LPBYTE         mem          = NULL;	// TODO: Init
LPBYTE         memdirty     = NULL;	// TODO: Init
iofunction		IORead[256] = {0};	// TODO: Init
iofunction		IOWrite[256] = {0};	// TODO: Init

// From Debugger_Types.h
	enum AddressingMode_e // ADDRESSING_MODES_e
	{
		  AM_IMPLIED // Note: SetDebugBreakOnInvalid() assumes this order of first 4 entries
		, AM_1    //    Invalid 1 Byte
		, AM_2    //    Invalid 2 Bytes
		, AM_3    //    Invalid 3 Bytes
	};

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

static const int IRQ_CHECK_TIMEOUT = 128;
static signed int g_nIrqCheckTimeout = IRQ_CHECK_TIMEOUT;

static __forceinline int Fetch(BYTE& iOpcode, ULONG uExecutedCycles)
{
	iOpcode = *(mem+regs.pc);
	regs.pc++;
	return 1;
}

#define INV IsDebugBreakOnInvalid(AM_1);
inline int IsDebugBreakOnInvalid( int iOpcodeType )
{
	return 0;
}

static __forceinline void DoIrqProfiling(DWORD uCycles)
{
}

static __forceinline void CheckInterruptSources(ULONG uExecutedCycles)
{
}

static __forceinline void NMI(ULONG& uExecutedCycles, UINT& uExtraCycles, BOOL& flagc, BOOL& flagn, BOOL& flagv, BOOL& flagz)
{
}

static __forceinline void IRQ(ULONG& uExecutedCycles, UINT& uExtraCycles, BOOL& flagc, BOOL& flagn, BOOL& flagv, BOOL& flagz)
{
}

void RequestDebugger()
{
}

// From Debug.h
inline int IsDebugBreakpointHit()
{
	return 0;
}

// From Debug.cpp
int g_bDebugBreakpointHit = 0;

// From z80.cpp
DWORD z80_mainloop(ULONG uTotalCycles, ULONG uExecutedCycles)
{
	return 0;
}

//-------------------------------------

#include "../../source/cpu/cpu_general.inl"
#include "../../source/cpu/cpu_instructions.inl"
#include "../../source/cpu/cpu6502.h"  // MOS 6502
#include "../../source/cpu/cpu65C02.h"  // WDC 65C02

void init(void)
{
	//mem = new BYTE[64*1024];
	mem = (LPBYTE)VirtualAlloc(NULL,64*1024,MEM_COMMIT,PAGE_READWRITE);

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

int test_GH264(void)
{
	reset();
	WORD abs = regs.pc+3;
	WORD dst = abs+2;
	mem[regs.pc+0] = 0x6c;	// JMP (IND) 
	mem[regs.pc+1] = abs&0xff;
	mem[regs.pc+2] = abs>>8;
	mem[regs.pc+3] = dst&0xff;
	mem[regs.pc+4] = dst>>8;

	DWORD cycles = Cpu6502(0);
	if (cycles != 5) return 1;
	if (regs.pc != dst) return 1;

	reset();
	cycles = Cpu65C02(0);
	if (cycles != 6) return 1;
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

int test_GH271(void)
{
	// asl abs,x
	{
		const WORD base = 0x20ff;
		const BYTE d = 0x40;

		// no page-cross
		{
			const BYTE x = 0;

			ASL_ABSX(x, base, d);
			if (Cpu6502(0) != 7) return 1;
			if (mem[base+x] != ((d<<1)&0xff)) return 1;

			ASL_ABSX(x, base, d);
			if (Cpu65C02(0) != 6) return 1;	// Non-PX case is optimised on 65C02
			if (mem[base+x] != ((d<<1)&0xff)) return 1;
		}

		// page-cross
		{
			const BYTE x = 1;

			ASL_ABSX(x, base, d);
			if (Cpu6502(0) != 7) return 1;
			if (mem[base+x] != ((d<<1)&0xff)) return 1;

			ASL_ABSX(x, base, d);
			if (Cpu65C02(0) != 7) return 1;
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
			if (Cpu6502(0) != 7) return 1;
			if (mem[base+x] != ((d-1)&0xff)) return 1;

			DEC_ABSX(x, base, d);
			if (Cpu65C02(0) != 7) return 1;	// NB. Not optimised for 65C02
			if (mem[base+x] != ((d-1)&0xff)) return 1;
		}

		// page-cross
		{
			const BYTE x = 1;

			DEC_ABSX(x, base, d);
			if (Cpu6502(0) != 7) return 1;
			if (mem[base+x] != ((d-1)&0xff)) return 1;

			DEC_ABSX(x, base, d);
			if (Cpu65C02(0) != 7) return 1;
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
			if (Cpu6502(0) != 7) return 1;
			if (mem[base+x] != ((d+1)&0xff)) return 1;

			INC_ABSX(x, base, d);
			if (Cpu65C02(0) != 7) return 1;	// NB. Not optimised for 65C02
			if (mem[base+x] != ((d+1)&0xff)) return 1;
		}

		// page-cross
		{
			const BYTE x = 1;

			INC_ABSX(x, base, d);
			if (Cpu6502(0) != 7) return 1;
			if (mem[base+x] != ((d+1)&0xff)) return 1;

			INC_ABSX(x, base, d);
			if (Cpu65C02(0) != 7) return 1;
			if (mem[base+x] != ((d+1)&0xff)) return 1;
		}
	}

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
	return Cpu6502(0);
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
	return Cpu6502(0);
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
	return Cpu6502(0);
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
	return Cpu6502(0);
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
	return Cpu6502(0);
}

int test_GH282(void)
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

int _tmain(int argc, _TCHAR* argv[])
{
	int res = 1;
	init();
	reset();

	res = test_GH264();
	if (res) return res;

	res = test_GH271();
	if (res) return res;

	res = test_GH282();
	if (res) return res;

	return 0;
}
