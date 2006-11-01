#pragma once

enum MemoryInitPattern_e
{
	  MIP_ZERO
	, MIP_FF_FF_00_00

	, NUM_MIP
};
extern MemoryInitPattern_e g_eMemoryInitPattern;

extern iofunction ioread[0x100];
extern iofunction iowrite[0x100];
extern LPBYTE     memshadow[MAXIMAGES][0x100];
extern LPBYTE     memwrite[MAXIMAGES][0x100];
extern DWORD      image;
extern DWORD      lastimage;
extern LPBYTE     mem;
extern LPBYTE     memdirty;

#ifdef RAMWORKS
extern UINT       g_uMaxExPages;	// user requested ram pages (from cmd line)
#endif

void    MemDestroy ();
bool    MemGet80Store();
LPBYTE  MemGetAuxPtr (WORD);
LPBYTE  MemGetMainPtr (WORD);
void    MemInitialize ();
BYTE    MemReadFloatingBus();
BYTE    MemReadFloatingBus(BYTE highbit);
void    MemReset ();
void    MemResetPaging ();
BYTE    MemReturnRandomData (BYTE highbit);
void    MemSetFastPaging (BOOL);
void    MemTrimImages ();
DWORD   MemGetSnapshot(SS_BaseMemory* pSS);
DWORD   MemSetSnapshot(SS_BaseMemory* pSS);

BYTE __stdcall CxReadFunc(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft);
BYTE __stdcall CxWriteFunc(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft);

BYTE __stdcall MemCheckPaging (WORD pc, BYTE addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft);
BYTE __stdcall MemSetPaging (WORD pc, BYTE addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft);
