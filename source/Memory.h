#pragma once

#include "Common.h"
#include "Card.h"
#include "MemoryDefs.h"

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
#define  MF_IOUDIS     0x00000800   // Disable IOU access for addresses $C058 to $C05F; enable access to DHIRES switch (0=on) (//c only)
#define  MF_ALTROM0    0x00001000   // Use alternate ROM for $D000 to $FFFF. Two bits for up to 4 pages
#define  MF_ALTROM1    0x00002000   // Use alternate ROM, second bit to have four pages
#define  MF_IMAGEMASK  0x000003F7
#define  MF_LANGCARD_MASK	(MF_WRITERAM|MF_HIGHRAM|MF_BANK2)


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

// For Cpu6502_altRead() & Cpu65C02_altRead()
enum { MEM_Normal = 0, MEM_IORead, MEM_FloatingBus, MEM_Aux1K, MEM_NoSlotClock };

typedef BYTE (__stdcall *iofunction)(WORD nPC, WORD nAddr, BYTE nWriteFlag, BYTE nWriteValue, ULONG nExecutedCycles);

extern iofunction IORead[256];
extern iofunction IOWrite[256];
extern LPBYTE     memshadow[0x100];
extern LPBYTE     memwrite[0x100];
extern BYTE       memreadPageType[0x100];
extern LPBYTE     mem;
extern LPBYTE     memdirty;
extern LPBYTE     memVidHD;

#ifdef RAMWORKS
const UINT kMaxExMemoryBanks = 256;	// 256 * aux mem(64K) + main mem(64K) = 16MB + 64K
const UINT kDefaultExMemoryBanksRealRW3 = 16;	// Real RamWorks III would default to 1MB
#endif

void	RegisterIoHandler(UINT uSlot, iofunction IOReadC0, iofunction IOWriteC0, iofunction IOReadCx, iofunction IOWriteCx, LPVOID lpSlotParameter, BYTE* pExpansionRom);
void	UnregisterIoHandler(UINT uSlot);

void    MemDestroy ();
bool	MemCheckSLOTC3ROM();
bool	MemCheckINTCXROM();
LPBYTE  MemGetAuxPtr(const WORD);
LPBYTE  MemGetMainPtrWithLC(const WORD);
LPBYTE  MemGetMainPtr(const WORD);
LPBYTE  MemGetBankPtr(const UINT nBank, const bool isSaveSnapshotOrDebugging = true);
LPBYTE  MemGetCxRomPeripheral();
uint32_t   GetMemMode(void);
void    SetMemMode(uint32_t memmode);
bool    MemIsWriteAux(uint32_t memMode);
bool    IsIIeWithoutAuxMem(void);
bool	MemOptimizeForModeChanging(WORD programcounter, WORD address);
bool    MemIsAddrCodeMemory(const USHORT addr);
void    MemInitialize ();
void    MemInitializeROM(void);
void    MemInitializeCustomROM(void);
void    MemInitializeCustomF8ROM(void);
void    MemInitializeIO(void);
void    MemInitializeFromSnapshot(void);
BYTE    MemReadFloatingBus(const ULONG uExecutedCycles);
BYTE    MemReadFloatingBus(const BYTE highbit, const ULONG uExecutedCycles);
BYTE    MemReadFloatingBusFromNTSC(void);
void    MemReset ();
void    MemResetPaging ();
void    MemUpdatePaging(BOOL initialize);
LPVOID	MemGetSlotParameters (UINT uSlot);
void	MemAnnunciatorReset(void);
bool    MemGetAnnunciator(UINT annunciator);
bool    MemHasNoSlotClock(void);
void    MemInsertNoSlotClock(void);
void    MemRemoveNoSlotClock(void);
const std::string& MemGetSnapshotUnitAuxSlotName(void);
void    MemSaveSnapshot(class YamlSaveHelper& yamlSaveHelper);
bool    MemLoadSnapshot(class YamlLoadHelper& yamlLoadHelper, UINT unitVersion);
void    MemSaveSnapshotAux(class YamlSaveHelper& yamlSaveHelper);
bool    MemLoadSnapshotAux(class YamlLoadHelper& yamlLoadHelper, UINT unitVersion);
void    NoSlotClockSaveSnapshot(YamlSaveHelper& yamlSaveHelper);
void    NoSlotClockLoadSnapshot(YamlLoadHelper& yamlLoadHelper);

BYTE __stdcall IO_Null(WORD programcounter, WORD address, BYTE write, BYTE value, ULONG nCycles);

BYTE __stdcall MemSetPaging(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nExecutedCycles);

BYTE __stdcall IO_F8xx(WORD programcounter, WORD address, BYTE write, BYTE value, ULONG nCycles);

void	SetExpansionMemType(const SS_CARDTYPE type, bool updateRegistry=true);
SS_CARDTYPE GetCurrentExpansionMemType(void);

void	SetRamWorksMemorySize(UINT banks, bool updateRegistry=true);
UINT	GetRamWorksActiveBank(void);
void	SetMemMainLanguageCard(LPBYTE ptr, UINT slot, bool bMemMain=false);
void	SetRegistryAuxNumberOfBanks(void);

LPBYTE GetCxRomPeripheral(void);
bool GetIsMemCacheValid(void);
uint8_t ReadByteFromMemory(uint16_t addr);
uint16_t ReadWordFromMemory(uint16_t addr);
void WriteByteToMemory(uint16_t addr, uint8_t data);
void CopyBytesFromMemoryPage(uint8_t* pDst, uint16_t srcAddr, size_t size);
bool IsZeroPageFloatingBus(void);
