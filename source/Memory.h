#pragma once

// Memory Flag
#define  MF_80STORE    0x00000001
#define  MF_ALTZP      0x00000002
#define  MF_AUXREAD    0x00000004	// RAMRD
#define  MF_AUXWRITE   0x00000008	// RAMWRT
#define  MF_BANK2      0x00000010   // Language Card Bank 2 $D000..$DFFF
#define  MF_HIGHRAM    0x00000020   // Language Card RAM is active $D000..$DFFF
#define  MF_HIRES      0x00000040
#define  MF_PAGE2      0x00000080
#define  MF_SLOTC3ROM  0x00000100
#define  MF_INTCXROM   0x00000200
#define  MF_WRITERAM   0x00000400   // Language Card RAM is Write Enabled
#define  MF_IMAGEMASK  0x000003F7

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

enum MemoryType_e
{
	MEM_TYPE_NATIVE   = 0,
	MEM_TYPE_RAMWORKS = 1,
	MEM_TYPE_SATURN   = 2,
	NUM_MEM_TYPE      = 3
};

typedef BYTE (__stdcall *iofunction)(WORD nPC, WORD nAddr, BYTE nWriteFlag, BYTE nWriteValue, ULONG nExecutedCycles);

extern iofunction IORead[256];
extern iofunction IOWrite[256];
extern LPBYTE     memwrite[0x100];
extern LPBYTE     mem;
extern LPBYTE     memdirty;

#ifdef RAMWORKS
const UINT kMaxExMemoryBanks = 127;	// 127 * aux mem(64K) + main mem(64K) = 8MB
extern UINT       g_uMaxExPages;	// user requested ram pages (from cmd line)
extern UINT       g_uActiveBank;
#endif

const UINT kMaxSaturnBanks = 8;		// 8 * 16K = 128K

void	RegisterIoHandler(UINT uSlot, iofunction IOReadC0, iofunction IOWriteC0, iofunction IOReadCx, iofunction IOWriteCx, LPVOID lpSlotParameter, BYTE* pExpansionRom);

void    MemDestroy ();
bool	MemCheckSLOTC3ROM();
bool	MemCheckINTCXROM();
LPBYTE  MemGetAuxPtr(const WORD);
LPBYTE  MemGetMainPtr(const WORD);
LPBYTE  MemGetBankPtr(const UINT nBank);
LPBYTE  MemGetCxRomPeripheral();
DWORD   GetMemMode(void);
bool    MemIsAddrCodeMemory(const USHORT addr);
void    MemInitialize ();
void    MemInitializeROM(void);
void    MemInitializeCustomF8ROM(void);
void    MemInitializeIO(void);
void    MemInitializeCardExpansionRomFromSnapshot(void);
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

BYTE __stdcall IO_Null(WORD programcounter, WORD address, BYTE write, BYTE value, ULONG nCycles);

BYTE __stdcall MemSetPaging(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nExecutedCycles);

void	SetExpansionMemType(MemoryType_e type);
MemoryType_e GetCurrentExpansionMemType(void);
void	SetRamWorksMemorySize(UINT pages);
UINT	GetRamWorksActiveBank(void);
void	SetSaturnMemorySize(UINT banks);
UINT	GetSaturnActiveBank(void);
std::string Saturn_GetSnapshotCardName(void);
void Saturn_SaveSnapshot(YamlSaveHelper& yamlSaveHelper);
bool Saturn_LoadSnapshot(YamlLoadHelper& yamlLoadHelper, UINT slot, UINT version);
