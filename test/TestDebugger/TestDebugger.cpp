#include "stdafx.h"

#include "../../source/Applewin.h"
#include "../../source/CPU.h"

#include "../../source/Debugger/Debugger_Types.h"
#include "../../source/Debugger/Debugger_Assembler.h"	// Pull in default args for _6502_GetTargets()

// NB. DebugDefs.h must come after Debugger_Types.h which declares these as extern
#include "../../source/Debugger/DebugDefs.h"

// From CPU.cpp
regsrec regs;

// From Frame.cpp
HWND   g_hFrameWindow   = (HWND)0;

// From Memory.cpp
LPBYTE         mem          = NULL;	// TODO: Init
LPBYTE         memdirty     = NULL;	// TODO: Init

//-------------------------------------

// From Debugger_Console.cpp
		char      g_aConsolePrompt[] = ">!"; // input, assembler // NUM_PROMPTS
		char      g_sConsolePrompt[] = ">"; // No, NOT Integer Basic!  The nostalgic '*' "Monitor" doesn't look as good, IMHO. :-(

Update_t ConsoleUpdate ()
{
	return 0;
}

bool ConsoleBufferPush ( const char * pText )
{
	return false;
}

bool ConsoleBufferPushVa ( char* buf, size_t bufsz, const char * pFormat, va_list va )
{
	return false;
}

// From Debugger_DisassemblerData.cpp
DisasmData_t* Disassembly_IsDataAddress ( WORD nAddress )
{
	return NULL;
}

// From Debugger_Parser.cpp
	int   g_nArgRaw;
	Arg_t g_aArgRaw[ MAX_ARGS ]; // pre-processing
	Arg_t g_aArgs  [ MAX_ARGS ]; // post-processing (cooked)

bool ArgsGetValue ( Arg_t *pArg, WORD * pAddressValue_, const int nBase )
{
	return false;
}

// From Debugger_Symbols.cpp
bool FindAddressFromSymbol ( const char* pSymbol, WORD * pAddress_, int * iTable_ )
{
	return false;
}

//-------------------------------------

void init(void)
{
	mem = (LPBYTE)VirtualAlloc(NULL,128*1024,MEM_COMMIT,PAGE_READWRITE);	// alloc >64K to test wrap-around at 64K boundary
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

int GH445_test_PHn(BYTE op)
{
	bool bRes;
	int TargetAddr[3];
	int TargetBytes;

	mem[regs.pc] = op;

	regs.sp = 0x1FE;
	bRes = _6502_GetTargets(regs.pc, &TargetAddr[0], &TargetAddr[1], &TargetAddr[2], &TargetBytes);
	if (!bRes || TargetAddr[2] != regs.sp) return 1;

	regs.sp = 0x1FF;
	bRes = _6502_GetTargets(regs.pc, &TargetAddr[0], &TargetAddr[1], &TargetAddr[2], &TargetBytes);
	if (!bRes || TargetAddr[2] != regs.sp) return 1;

	regs.sp = 0x100;
	bRes = _6502_GetTargets(regs.pc, &TargetAddr[0], &TargetAddr[1], &TargetAddr[2], &TargetBytes);
	if (!bRes || TargetAddr[2] != regs.sp) return 1;

	return 0;
}

int GH445_test_PLn(BYTE op)
{
	bool bRes;
	int TargetAddr[3];
	int TargetBytes;

	mem[regs.pc] = op;

	regs.sp = 0x1FE;
	bRes = _6502_GetTargets(regs.pc, &TargetAddr[0], &TargetAddr[1], &TargetAddr[2], &TargetBytes);
	if (!bRes || TargetAddr[2] != (regs.sp+1)) return 1;

	regs.sp = 0x1FF;
	bRes = _6502_GetTargets(regs.pc, &TargetAddr[0], &TargetAddr[1], &TargetAddr[2], &TargetBytes);
	if (!bRes || TargetAddr[2] != 0x100) return 1;

	regs.sp = 0x100;
	bRes = _6502_GetTargets(regs.pc, &TargetAddr[0], &TargetAddr[1], &TargetAddr[2], &TargetBytes);
	if (!bRes || TargetAddr[2] != (regs.sp+1)) return 1;

	return 0;
}

int GH445_test_abs(BYTE op, WORD target2)
{
	bool bRes;
	int TargetAddr[3];
	int TargetBytes;

	mem[regs.pc] = op;
	mem[(regs.pc+1)&0xFFFF] = (BYTE) (target2&0xff);
	mem[(regs.pc+2)&0xFFFF] = (BYTE) ((target2>>8)&0xff);
	bRes = _6502_GetTargets(regs.pc, &TargetAddr[0], &TargetAddr[1], &TargetAddr[2], &TargetBytes);
	if (!bRes || TargetAddr[2] != target2) return 1;

	return 0;
}

int GH445_test_jsr(void)
{
	bool bRes;
	int TargetAddr[3];
	int TargetBytes;

	mem[regs.pc] = 0x20;

	regs.sp = 0x1FF;
	bRes = _6502_GetTargets(regs.pc, &TargetAddr[0], &TargetAddr[1], &TargetAddr[2], &TargetBytes, false);
	if (!bRes || TargetAddr[0] != regs.sp || TargetAddr[1] != regs.sp-1) return 1;

	regs.sp = 0x100;
	bRes = _6502_GetTargets(regs.pc, &TargetAddr[0], &TargetAddr[1], &TargetAddr[2], &TargetBytes, false);
	if (!bRes || TargetAddr[0] != regs.sp || TargetAddr[1] != 0x1FF) return 1;

	regs.sp = 0x101;
	bRes = _6502_GetTargets(regs.pc, &TargetAddr[0], &TargetAddr[1], &TargetAddr[2], &TargetBytes, false);
	if (!bRes || TargetAddr[0] != regs.sp || TargetAddr[1] != regs.sp-1) return 1;

	return 0;
}

int GH445_test_rts(WORD sp)
{
	bool bRes;
	int TargetAddr[3];
	int TargetBytes;

	mem[regs.pc] = 0x60;

	regs.sp = sp;
	WORD sp_addr_l = 0x100 + ((regs.sp+1)&0xFF);
	WORD sp_addr_h = 0x100 + ((regs.sp+2)&0xFF);
	WORD rts_addr = 0x1234;
	mem[sp_addr_l] = (BYTE) (rts_addr&0xFF);
	mem[sp_addr_h] = (BYTE) ((rts_addr>>8)&0xFF);
	rts_addr++;		// NB. return addr from stack is incremented before being transferred to PC
	bRes = _6502_GetTargets(regs.pc, &TargetAddr[0], &TargetAddr[1], &TargetAddr[2], &TargetBytes, false);
	if (!bRes || TargetAddr[0] != sp_addr_l || TargetAddr[1] != sp_addr_h || TargetAddr[2] != rts_addr) return 1;

	return 0;
}

int GH445_test_sub(bool bIs65C02)
{
	int res;

	mem[0x10000] = 0xDD;	// Bad data if 64K wrap not working
	mem[0x10001] = 0xDD;

	mem[0x200] = 0xDD;		// Bad data if SP wrap not working
	mem[0x201] = 0xDD;

	//
	// PHn/PLn

	regs.pc = 0x300;

	res = GH445_test_PHn(0x08);	// PHP
	if (res) return res;
	res = GH445_test_PHn(0x48);	// PHA
	if (res) return res;

	if (bIs65C02)
	{
		res = GH445_test_PHn(0x5A);	// PHY
		if (res) return res;
		res = GH445_test_PHn(0xDA);	// PHX
		if (res) return res;
	}

	//

	res = GH445_test_PLn(0x28);	// PLP
	if (res) return res;
	res = GH445_test_PLn(0x68);	// PLA
	if (res) return res;

	if (bIs65C02)
	{
		res = GH445_test_PLn(0x7A);	// PLY
		if (res) return res;
		res = GH445_test_PLn(0xFA);	// PLX
		if (res) return res;
	}

	//
	// ABS

	regs.pc = 0xFFFD;
	res = GH445_test_abs(0xAD, 0x1234);	// LDA ABS
	if (res) return res;

	regs.pc = 0xFFFE;
	res = GH445_test_abs(0xAD, 0x1234);	// LDA ABS
	if (res) return res;

	regs.pc = 0xFFFF;
	res = GH445_test_abs(0xAD, 0x1234);	// LDA ABS
	if (res) return res;

	//
	// JSR ABS

	res = GH445_test_jsr();
	if (res) return res;

	//
	// RTS

	res = GH445_test_rts(0x1FE);
	if (res) return res;
	res = GH445_test_rts(0x1FF);
	if (res) return res;
	res = GH445_test_rts(0x100);
	if (res) return res;

	return 0;
}

int GH445_test(void)
{
	int res;

	g_aOpcodes = g_aOpcodes65C02;
	res = GH445_test_sub(true);
	if (res) return res;

	g_aOpcodes = g_aOpcodes6502;
	res = GH445_test_sub(false);

	return res;
}

//-------------------------------------

int _tmain(int argc, _TCHAR* argv[])
{
	int res = 1;
	init();
	reset();

	res = GH445_test();
	if (res) return res;

	return 0;
}
