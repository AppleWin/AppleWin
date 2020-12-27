#include "stdafx.h"

#include "../../source/Windows/AppleWin.h"
#include "../../source/CPU.h"

#include "../../source/Debugger/Debugger_Types.h"
#include "../../source/Debugger/Debugger_Assembler.h"	// Pull in default args for _6502_GetTargets()

// NB. DebugDefs.h must come after Debugger_Types.h which declares these as extern
#include "../../source/Debugger/DebugDefs.h"

// From FrameBase
class FrameBase
{
public:
	FrameBase() { g_hFrameWindow = (HWND)0; }
	HWND   g_hFrameWindow;
};

// From Win32Frame
class Win32Frame : public FrameBase
{
};

// From AppleWin.cpp
FrameBase& GetFrame()
{
	static Win32Frame sg_Win32Frame;
	return sg_Win32Frame;
}

// From CPU.cpp
regsrec regs;

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

int GH445_test_abs(BYTE op)
{
	bool bRes;
	int TargetAddr[3];
	int TargetBytes;

	const WORD target2 = 0x1234;

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

	WORD target2 = 0x1234;

	mem[regs.pc] = OPCODE_JSR;
	mem[(regs.pc+1)&0xFFFF] = (BYTE) (target2&0xff);
	mem[(regs.pc+2)&0xFFFF] = (BYTE) ((target2>>8)&0xff);

	regs.sp = 0x1FF;
	bRes = _6502_GetTargets(regs.pc, &TargetAddr[0], &TargetAddr[1], &TargetAddr[2], &TargetBytes);
	if (!bRes || TargetAddr[0] != regs.sp || TargetAddr[1] != regs.sp-1 || TargetAddr[2] != target2) return 1;

	regs.sp = 0x100;
	bRes = _6502_GetTargets(regs.pc, &TargetAddr[0], &TargetAddr[1], &TargetAddr[2], &TargetBytes);
	if (!bRes || TargetAddr[0] != regs.sp || TargetAddr[1] != 0x1FF || TargetAddr[2] != target2) return 1;

	regs.sp = 0x101;
	bRes = _6502_GetTargets(regs.pc, &TargetAddr[0], &TargetAddr[1], &TargetAddr[2], &TargetBytes);
	if (!bRes || TargetAddr[0] != regs.sp || TargetAddr[1] != regs.sp-1 || TargetAddr[2] != target2) return 1;

	return 0;
}

int GH445_test_brk(void)
{
	bool bRes;
	int TargetAddr[3];
	int TargetBytes;

	mem[regs.pc] = OPCODE_BRK;

	regs.sp = 0x1FF;
	bRes = _6502_GetTargets(regs.pc, &TargetAddr[0], &TargetAddr[1], &TargetAddr[2], &TargetBytes);
	if (!bRes || TargetAddr[0] != regs.sp || TargetAddr[1] != regs.sp-1) return 1;

	regs.sp = 0x100;
	bRes = _6502_GetTargets(regs.pc, &TargetAddr[0], &TargetAddr[1], &TargetAddr[2], &TargetBytes);
	if (!bRes || TargetAddr[0] != regs.sp || TargetAddr[1] != 0x1FF) return 1;

	regs.sp = 0x101;
	bRes = _6502_GetTargets(regs.pc, &TargetAddr[0], &TargetAddr[1], &TargetAddr[2], &TargetBytes);
	if (!bRes || TargetAddr[0] != regs.sp || TargetAddr[1] != regs.sp-1) return 1;

	return 0;
}

int GH445_test_rti_rts(WORD sp, const bool isRTI)
{
	bool bRes;
	int TargetAddr[3];
	int TargetBytes;

	mem[regs.pc] = isRTI ? OPCODE_RTI : OPCODE_RTS;
	regs.sp = sp;

	WORD sp_addr_p=0, sp_addr_l=0, sp_addr_h=0;
	if (isRTI)
	{
		sp_addr_p = 0x100 + ((regs.sp+1)&0xFF);
		sp_addr_l = 0x100 + ((regs.sp+2)&0xFF);
		sp_addr_h = 0x100 + ((regs.sp+3)&0xFF);
		mem[sp_addr_p] = 0xEA;
	}
	else
	{
		sp_addr_l = 0x100 + ((regs.sp+1)&0xFF);
		sp_addr_h = 0x100 + ((regs.sp+2)&0xFF);
	}

	WORD ret_addr = 0x1234;
	mem[sp_addr_l] = (BYTE) (ret_addr&0xFF);
	mem[sp_addr_h] = (BYTE) ((ret_addr>>8)&0xFF);

	if (!isRTI)
		ret_addr++;		// NB. return addr from stack is incremented before being transferred to PC

	bRes = _6502_GetTargets(regs.pc, &TargetAddr[0], &TargetAddr[1], &TargetAddr[2], &TargetBytes);
	if (!bRes || TargetAddr[0] != sp_addr_l || TargetAddr[1] != sp_addr_h || TargetAddr[2] != ret_addr) return 1;

	return 0;
}

int GH445_test_jmp(BYTE op)
{
	bool bRes;
	int TargetAddr[3];
	int TargetBytes;

	const WORD target16 = 0x1234;

	int target0=0, target1=0, target2=0;
	if (op == OPCODE_JMP_A)
	{
		target0 = NO_6502_TARGET;
		target1 = NO_6502_TARGET;
		target2 = target16;
	}
	else if (op == OPCODE_JMP_NA)
	{
		target0 = target16;
		target1 = (target16+1)&0xffff;
		target2 = 0x5678;
		mem[target0] = target2 & 0xff;
		mem[target1] = (target2>>8) & 0xff;
	}
	else if (op == OPCODE_JMP_IAX)
	{
		target0 = (target16+regs.x)&0xffff;
		target1 = (target16+regs.x+1)&0xffff;
		target2 = 0xABCD;
		mem[target0] = target2 & 0xff;
		mem[target1] = (target2>>8) & 0xff;
	}
	else
	{
		_ASSERT(0);
	}

	mem[regs.pc] = op;
	mem[(regs.pc+1)&0xFFFF] = (BYTE) (target16&0xff);
	mem[(regs.pc+2)&0xFFFF] = (BYTE) ((target16>>8)&0xff);
	bRes = _6502_GetTargets(regs.pc, &TargetAddr[0], &TargetAddr[1], &TargetAddr[2], &TargetBytes);
	if (!bRes || TargetAddr[0] != target0 || TargetAddr[1] != target1 || TargetAddr[2] != target2) return 1;

	return 0;
}

// bIgnoreBranch == true (default)
int GH445_test_Bcc(void)
{
	bool bRes;
	int TargetAddr[3];
	int TargetBytes;

	mem[regs.pc] = 0x10;	// BPL next-op
	mem[regs.pc+1] = 0;

	bRes = _6502_GetTargets(regs.pc, &TargetAddr[0], &TargetAddr[1], &TargetAddr[2], &TargetBytes);
	if (!bRes || TargetAddr[0] != NO_6502_TARGET || TargetAddr[1] != NO_6502_TARGET || TargetAddr[2] != NO_6502_TARGET) return 1;

	mem[regs.pc] = 0x10;	// BPL this-op
	mem[regs.pc+1] = 0xfe;

	bRes = _6502_GetTargets(regs.pc, &TargetAddr[0], &TargetAddr[1], &TargetAddr[2], &TargetBytes);
	if (!bRes || TargetAddr[0] != NO_6502_TARGET || TargetAddr[1] != NO_6502_TARGET || TargetAddr[2] != NO_6502_TARGET) return 1;

	return 0;
}

int GH445_test_sub(bool bIs65C02)
{
	int res;

	mem[0x10000] = 0xDD;	// Bad data if 64K wrap not working
	mem[0x10001] = 0xDD;

	mem[0x200] = 0xDD;		// Bad data if SP wrap not working
	mem[0x201] = 0xDD;

	regs.pc = 0x300;

	//
	// PHn/PLn

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
	// LDA abs

	regs.pc = 0xFFFD;
	res = GH445_test_abs(OPCODE_LDA_A);	// LDA ABS
	if (res) return res;

	regs.pc = 0xFFFE;
	res = GH445_test_abs(OPCODE_LDA_A);	// LDA ABS
	if (res) return res;

	regs.pc = 0xFFFF;
	res = GH445_test_abs(OPCODE_LDA_A);	// LDA ABS
	if (res) return res;

	//
	// JSR abs

	res = GH445_test_jsr();
	if (res) return res;

	//
	// BRK

	mem[_6502_BRK_VECTOR+0] = 0x40;		// BRK vector: $FA40
	mem[_6502_BRK_VECTOR+1] = 0xFA;

	regs.pc = 0x300;
	res = GH445_test_brk();
	if (res) return res;

	//
	// RTI

	res = GH445_test_rti_rts(0x1FE, true);
	if (res) return res;
	res = GH445_test_rti_rts(0x1FF, true);
	if (res) return res;
	res = GH445_test_rti_rts(0x100, true);
	if (res) return res;

	//
	// RTS

	res = GH445_test_rti_rts(0x1FE, false);
	if (res) return res;
	res = GH445_test_rti_rts(0x1FF, false);
	if (res) return res;
	res = GH445_test_rti_rts(0x100, false);
	if (res) return res;

	//
	// JMP

	res = GH445_test_jmp(OPCODE_JMP_A);			// JMP abs
	if (res) return res;

	res = GH445_test_jmp(OPCODE_JMP_NA);		// JMP (abs)
	if (res) return res;

	if (bIs65C02)
	{
		regs.x = 0xff;
		res = GH445_test_jmp(OPCODE_JMP_IAX);	// JMP (abs,x)
		if (res) return res;
	}

	//
	// Bcc

	res = GH445_test_Bcc();
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

//
// bIncludeNextOpcodeAddress == false, check that:
// . TargetAddr[2] gets set, eg. for LDA abs
// . TargetAddr[2] == NO_6502_TARGET for control flow instructions, eg. BRK,RTI,RTS,JSR,JMP,Bcc
//

int GH451_test_abs(BYTE op)
{
	bool bRes;
	int TargetAddr[3];
	int TargetBytes;

	const WORD target2 = 0x1234;

	mem[regs.pc] = op;
	mem[(regs.pc+1)&0xFFFF] = (BYTE) (target2&0xff);
	mem[(regs.pc+2)&0xFFFF] = (BYTE) ((target2>>8)&0xff);
	bRes = _6502_GetTargets(regs.pc, &TargetAddr[0], &TargetAddr[1], &TargetAddr[2], &TargetBytes, true, false);
	if (!bRes || TargetAddr[2] != target2) return 1;

	return 0;
}

int GH451_test_jsr(void)
{
	bool bRes;
	int TargetAddr[3];
	int TargetBytes;

	mem[regs.pc] = OPCODE_JSR;

	regs.sp = 0x1FF;
	bRes = _6502_GetTargets(regs.pc, &TargetAddr[0], &TargetAddr[1], &TargetAddr[2], &TargetBytes, true, false);
	if (!bRes || TargetAddr[0] != regs.sp || TargetAddr[1] != regs.sp-1 || TargetAddr[2] != NO_6502_TARGET) return 1;

	regs.sp = 0x100;
	bRes = _6502_GetTargets(regs.pc, &TargetAddr[0], &TargetAddr[1], &TargetAddr[2], &TargetBytes, true, false);
	if (!bRes || TargetAddr[0] != regs.sp || TargetAddr[1] != 0x1FF || TargetAddr[2] != NO_6502_TARGET) return 1;

	regs.sp = 0x101;
	bRes = _6502_GetTargets(regs.pc, &TargetAddr[0], &TargetAddr[1], &TargetAddr[2], &TargetBytes, true, false);
	if (!bRes || TargetAddr[0] != regs.sp || TargetAddr[1] != regs.sp-1 || TargetAddr[2] != NO_6502_TARGET) return 1;

	return 0;
}

int GH451_test_brk(void)
{
	bool bRes;
	int TargetAddr[3];
	int TargetBytes;

	mem[regs.pc] = OPCODE_BRK;

	regs.sp = 0x1FF;
	bRes = _6502_GetTargets(regs.pc, &TargetAddr[0], &TargetAddr[1], &TargetAddr[2], &TargetBytes, true, false);
	if (!bRes || TargetAddr[0] != regs.sp || TargetAddr[1] != regs.sp-1 || TargetAddr[2] != NO_6502_TARGET) return 1;

	regs.sp = 0x100;
	bRes = _6502_GetTargets(regs.pc, &TargetAddr[0], &TargetAddr[1], &TargetAddr[2], &TargetBytes, true, false);
	if (!bRes || TargetAddr[0] != regs.sp || TargetAddr[1] != 0x1FF || TargetAddr[2] != NO_6502_TARGET) return 1;

	regs.sp = 0x101;
	bRes = _6502_GetTargets(regs.pc, &TargetAddr[0], &TargetAddr[1], &TargetAddr[2], &TargetBytes, true, false);
	if (!bRes || TargetAddr[0] != regs.sp || TargetAddr[1] != regs.sp-1 || TargetAddr[2] != NO_6502_TARGET) return 1;

	return 0;
}

int GH451_test_rti_rts(WORD sp, const bool isRTI)
{
	bool bRes;
	int TargetAddr[3];
	int TargetBytes;

	mem[regs.pc] = isRTI ? OPCODE_RTI : OPCODE_RTS;
	regs.sp = sp;

	WORD sp_addr_p=0, sp_addr_l=0, sp_addr_h=0;
	if (isRTI)
	{
		sp_addr_p = 0x100 + ((regs.sp+1)&0xFF);
		sp_addr_l = 0x100 + ((regs.sp+2)&0xFF);
		sp_addr_h = 0x100 + ((regs.sp+3)&0xFF);
		mem[sp_addr_p] = 0xEA;
	}
	else
	{
		sp_addr_l = 0x100 + ((regs.sp+1)&0xFF);
		sp_addr_h = 0x100 + ((regs.sp+2)&0xFF);
	}

	WORD ret_addr = 0x1234;
	mem[sp_addr_l] = (BYTE) (ret_addr&0xFF);
	mem[sp_addr_h] = (BYTE) ((ret_addr>>8)&0xFF);

	if (!isRTI)
		ret_addr++;		// NB. return addr from stack is incremented before being transferred to PC

	bRes = _6502_GetTargets(regs.pc, &TargetAddr[0], &TargetAddr[1], &TargetAddr[2], &TargetBytes, true, false);
	if (!bRes || TargetAddr[0] != sp_addr_l || TargetAddr[1] != sp_addr_h || TargetAddr[2] != NO_6502_TARGET) return 1;

	return 0;
}

int GH451_test_jmp(BYTE op)
{
	bool bRes;
	int TargetAddr[3];
	int TargetBytes;

	const WORD target16 = 0x1234;

	int target0=0, target1=0;
	if (op == OPCODE_JMP_A)
	{
		target0 = NO_6502_TARGET;
		target1 = NO_6502_TARGET;
	}
	else if (op == OPCODE_JMP_NA)
	{
		target0 = target16;
		target1 = (target16+1)&0xffff;
	}
	else if (op == OPCODE_JMP_IAX)
	{
		target0 = (target16+regs.x)&0xffff;
		target1 = (target16+regs.x+1)&0xffff;
	}
	else
	{
		_ASSERT(0);
	}

	mem[regs.pc] = op;
	mem[(regs.pc+1)&0xFFFF] = (BYTE) (target16&0xff);
	mem[(regs.pc+2)&0xFFFF] = (BYTE) ((target16>>8)&0xff);
	bRes = _6502_GetTargets(regs.pc, &TargetAddr[0], &TargetAddr[1], &TargetAddr[2], &TargetBytes, true, false);
	if (!bRes || TargetAddr[0] != target0 || TargetAddr[1] != target1 || TargetAddr[2] != NO_6502_TARGET) return 1;

	return 0;
}

// bIgnoreBranch == true
int GH451_test_Bcc(void)
{
	bool bRes;
	int TargetAddr[3];
	int TargetBytes;

	mem[regs.pc] = 0x10;	// BPL next-op
	mem[regs.pc+1] = 0;

	bRes = _6502_GetTargets(regs.pc, &TargetAddr[0], &TargetAddr[1], &TargetAddr[2], &TargetBytes, true, false);
	if (!bRes || TargetAddr[0] != NO_6502_TARGET || TargetAddr[1] != NO_6502_TARGET || TargetAddr[2] != NO_6502_TARGET) return 1;

	mem[regs.pc] = 0x10;	// BPL this-op
	mem[regs.pc+1] = 0xfe;

	bRes = _6502_GetTargets(regs.pc, &TargetAddr[0], &TargetAddr[1], &TargetAddr[2], &TargetBytes, true, false);
	if (!bRes || TargetAddr[0] != NO_6502_TARGET || TargetAddr[1] != NO_6502_TARGET || TargetAddr[2] != NO_6502_TARGET) return 1;

	return 0;
}

int GH451_test_sub(bool bIs65C02)
{
	int res;

	mem[0x10000] = 0xDD;	// Bad data if 64K wrap not working
	mem[0x10001] = 0xDD;

	mem[0x200] = 0xDD;		// Bad data if SP wrap not working
	mem[0x201] = 0xDD;

	regs.pc = 0x300;

	//
	// LDA abs

	res = GH451_test_abs(OPCODE_LDA_A);
	if (res) return res;

	//
	// JSR abs

	res = GH451_test_jsr();
	if (res) return res;

	//
	// BRK

	mem[_6502_BRK_VECTOR+0] = 0x40;		// BRK vector: $FA40
	mem[_6502_BRK_VECTOR+1] = 0xFA;

	res = GH451_test_brk();
	if (res) return res;

	//
	// RTI

	res = GH451_test_rti_rts(0x1FE, true);
	if (res) return res;
	res = GH451_test_rti_rts(0x1FF, true);
	if (res) return res;
	res = GH451_test_rti_rts(0x100, true);
	if (res) return res;

	//
	// RTS

	res = GH451_test_rti_rts(0x1FE, false);
	if (res) return res;
	res = GH451_test_rti_rts(0x1FF, false);
	if (res) return res;
	res = GH451_test_rti_rts(0x100, false);
	if (res) return res;

	//
	// JMP

	res = GH451_test_jmp(OPCODE_JMP_A);			// JMP abs
	if (res) return res;

	res = GH451_test_jmp(OPCODE_JMP_NA);		// JMP (abs)
	if (res) return res;

	if (bIs65C02)
	{
		regs.x = 0xff;
		res = GH451_test_jmp(OPCODE_JMP_IAX);	// JMP (abs),x
		if (res) return res;
	}

	//
	// Bcc

	res = GH451_test_Bcc();
	if (res) return res;

	return 0;
}

// debugger command 'bpm[r|w] addr16': JSR abs should not trigger a breakpoint at addr16
// . similarly for all other control flow opcodes (eg. Bcc, BRK, JMP, RTI, RTS)
int GH451_test(void)
{
	int res;

	g_aOpcodes = g_aOpcodes65C02;
	res = GH451_test_sub(true);
	if (res) return res;

	g_aOpcodes = g_aOpcodes6502;
	res = GH451_test_sub(false);

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

	res = GH451_test();
	if (res) return res;

	return 0;
}
