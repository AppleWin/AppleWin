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
	, MIP_RANDOM
	, MIP_FF_FF_00_00
	, MIP_FF_00_FULL_PAGE
	, MIP_00_FF_HALF_PAGE
	, MIP_FF_00_HALF_PAGE
	, MIP_PAGE_ADDRESS_LOW
	, MIP_PAGE_ADDRESS_HIGH
	, NUM_MIP
};

extern iofunction IORead[256];
extern iofunction IOWrite[256];
extern LPBYTE     memwrite[0x100];
extern LPBYTE     mem;
extern LPBYTE     memdirty;

#ifdef RAMWORKS
const UINT kMaxExMemoryBanks = 127;	// 127 * aux mem(64K) + main mem(64K) = 8MB
extern UINT       g_uMaxExPages;	// user requested ram pages (from cmd line)
#endif

void	RegisterIoHandler(UINT uSlot, iofunction IOReadC0, iofunction IOWriteC0, iofunction IOReadCx, iofunction IOWriteCx, LPVOID lpSlotParameter, BYTE* pExpansionRom);

void    MemDestroy ();
bool	MemCheckSLOTCXROM();
LPBYTE  MemGetAuxPtr(const WORD);
LPBYTE  MemGetMainPtr(const WORD);
LPBYTE  MemGetBankPtr(const UINT nBank);
LPBYTE  MemGetCxRomPeripheral();
void    MemInitialize ();
void    MemInitializeROM(void);
void    MemInitializeCustomF8ROM(void);
void    MemInitializeIO(void);
BYTE    MemReadFloatingBus(const ULONG uExecutedCycles);
BYTE    MemReadFloatingBus(const BYTE highbit, const ULONG uExecutedCycles);
void    MemReset ();
void    MemResetPaging ();
void    MemUpdatePaging(BOOL initialize);
LPVOID	MemGetSlotParameters (UINT uSlot);
void    MemSetSnapshot_v1(const DWORD MemMode, const BOOL LastWriteRam, const BYTE* const pMemMain, const BYTE* const pMemAux);
std::string MemGetSnapshotUnitAuxSlotName(void);
void    MemSaveSnapshot(class YamlSaveHelper& yamlSaveHelper);
bool    MemLoadSnapshot(class YamlLoadHelper& yamlLoadHelper);
void    MemSaveSnapshotAux(class YamlSaveHelper& yamlSaveHelper);
bool    MemLoadSnapshotAux(class YamlLoadHelper& yamlLoadHelper, UINT version);
void    MemGetSnapshot(struct SS_BaseMemory_v2& Memory);
void    MemSetSnapshot(const struct SS_BaseMemory_v2& Memory);
void    MemGetSnapshotAux(const HANDLE hFile);
void    MemSetSnapshotAux(const HANDLE hFile);

BYTE __stdcall IO_Null(WORD programcounter, WORD address, BYTE write, BYTE value, ULONG nCycles);

BYTE __stdcall MemCheckPaging (WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft);
BYTE __stdcall MemSetPaging(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft);
