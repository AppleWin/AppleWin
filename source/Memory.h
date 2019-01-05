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
#define  MF_LANGCARD_MASK	(MF_WRITERAM|MF_HIGHRAM|MF_BANK2)


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

void	RegisterIoHandler(UINT uSlot, iofunction IOReadC0, iofunction IOWriteC0, iofunction IOReadCx, iofunction IOWriteCx, LPVOID lpSlotParameter, BYTE* pExpansionRom);

void    MemDestroy ();
bool	MemCheckSLOTC3ROM();
bool	MemCheckINTCXROM();
LPBYTE  MemGetAuxPtr(const WORD);
LPBYTE  MemGetMainPtr(const WORD);
LPBYTE  MemGetBankPtr(const UINT nBank);
LPBYTE  MemGetCxRomPeripheral();
DWORD   GetMemMode(void);
void    SetMemMode(DWORD memmode);
bool	MemOptimizeForModeChanging(WORD programcounter, WORD address);
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
std::string MemGetSnapshotUnitAuxSlotName(void);
void    MemSaveSnapshot(class YamlSaveHelper& yamlSaveHelper);
bool    MemLoadSnapshot(class YamlLoadHelper& yamlLoadHelper, UINT version);
void    MemSaveSnapshotAux(class YamlSaveHelper& yamlSaveHelper);
bool    MemLoadSnapshotAux(class YamlLoadHelper& yamlLoadHelper, UINT version);

BYTE __stdcall IO_Null(WORD programcounter, WORD address, BYTE write, BYTE value, ULONG nCycles);

BYTE __stdcall MemSetPaging(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nExecutedCycles);

enum SS_CARDTYPE;
void	SetExpansionMemType(const SS_CARDTYPE type);
SS_CARDTYPE GetCurrentExpansionMemType(void);
void	CreateLanguageCard(void);

void	SetRamWorksMemorySize(UINT pages);
UINT	GetRamWorksActiveBank(void);
void	SetSaturnMemorySize(UINT banks);
void	SetMemMainLanguageCard(LPBYTE ptr, bool bMemMain=false);
class LanguageCardUnit* GetLanguageCard(void);
