#pragma once

enum
{
	// Note: All are in bytes!
	APPLE_SLOT_SIZE          = 0x0100, // 1 page  = $Cx00 .. $CxFF (slot 1 .. 7)
	APPLE_SLOT_BEGIN         = 0xC100, // each slot has 1 page reserved for it
	APPLE_SLOT_END           = 0xC7FF, //

	FIRMWARE_EXPANSION_SIZE  = 0x0800, // 8 pages = $C800 .. $CFFF
	FIRMWARE_EXPANSION_BEGIN = 0xC800, // [C800,CFFF)
	FIRMWARE_EXPANSION_END   = 0xCFFF //
};

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
