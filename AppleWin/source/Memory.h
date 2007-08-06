#pragma once

enum MemoryInitPattern_e
{
	  MIP_ZERO
	, MIP_FF_FF_00_00

	, NUM_MIP
};
extern MemoryInitPattern_e g_eMemoryInitPattern;

extern iofunction IORead[256];
extern iofunction IOWrite[256];
extern LPBYTE     memwrite[0x100];
extern LPBYTE     mem;
extern LPBYTE     memdirty;

#ifdef RAMWORKS
extern UINT       g_uMaxExPages;	// user requested ram pages (from cmd line)
#endif

void	RegisterIoHandler(UINT uSlot, iofunction IOReadC0, iofunction IOWriteC0, iofunction IOReadCx, iofunction IOWriteCx, LPVOID lpSlotParameter, BYTE* pExpansionRom);

void    MemDestroy ();
bool    MemGet80Store();
bool	MemCheckSLOTCXROM();
LPBYTE  MemGetAuxPtr (WORD);
LPBYTE  MemGetMainPtr (WORD);
LPBYTE  MemGetCxRomPeripheral();
void	MemPreInitialize ();
void    MemInitialize ();
BYTE    MemReadFloatingBus(const ULONG uExecutedCycles);
BYTE    MemReadFloatingBus(const BYTE highbit, const ULONG uExecutedCycles);
void    MemReset ();
void    MemResetPaging ();
BYTE    MemReturnRandomData (BYTE highbit);
void    MemSetFastPaging (BOOL);
void    MemTrimImages ();
LPVOID	MemGetSlotParameters (UINT uSlot);
DWORD   MemGetSnapshot(SS_BaseMemory* pSS);
DWORD   MemSetSnapshot(SS_BaseMemory* pSS);

BYTE __stdcall IO_Null(WORD programcounter, WORD address, BYTE write, BYTE value, ULONG nCycles);

BYTE __stdcall MemCheckPaging (WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft);
BYTE __stdcall MemSetPaging(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft);
