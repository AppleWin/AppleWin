/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 1994-1996, Michael O'Brien
Copyright (C) 1999-2001, Oliver Schmidt
Copyright (C) 2002-2005, Tom Charlesworth
Copyright (C) 2006-2007, Tom Charlesworth, Michael Pohoreski

AppleWin is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

AppleWin is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with AppleWin; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/* Description: Memory emulation
 *
 * Author: Various
 *
 * In comments, UTAIIe is an abbreviation for a reference to "Understanding the Apple //e" by James Sather
 */

#include "StdAfx.h"

#include "Memory.h"
#include "Interface.h"
#include "Core.h"
#include "CardManager.h"
#include "CPU.h"
#include "Disk.h"
#include "Harddisk.h"
#include "Joystick.h"
#include "Keyboard.h"
#include "LanguageCard.h"
#include "Log.h"
#include "Mockingboard.h"
#include "MouseInterface.h"
#include "NTSC.h"
#include "NoSlotClock.h"
#include "ParallelPrinter.h"
#include "Registry.h"
#include "SAM.h"
#include "SerialComms.h"
#include "Speaker.h"
#include "Tape.h"
#include "RGBMonitor.h"

#include "z80emu.h"
#include "Z80VICE/z80.h"
#include "../resource/resource.h"
#include "Configuration/IPropertySheet.h"
#include "Debugger/DebugDefs.h"
#include "YamlHelper.h"

// In this file allocate the 64KB of RAM with aligned memory allocations (0x10000)
// to ease mapping between Apple ][ and host memory space (while debugging).

// this is not available in Visual Studio
// https://en.cppreference.com/w/c/memory/aligned_alloc

#ifdef _MSC_VER
// VirtualAlloc is aligned
#define ALIGNED_ALLOC(size) (LPBYTE)VirtualAlloc(NULL, size, MEM_COMMIT, PAGE_READWRITE)
#define ALIGNED_FREE(ptr) VirtualFree(ptr, 0, MEM_RELEASE)
#else
// use plain "new" in gcc (where debugging needs are less important)
#define ALIGNED_ALLOC(size) new BYTE[size]
#define ALIGNED_FREE(ptr) delete [] ptr
#endif


// UTAIIe:5-28 (GH#419)
// . Sather uses INTCXROM instead of SLOTCXROM' (used by the Apple//e Tech Ref Manual), so keep to this
//   convention too since UTAIIe is the reference for most of the logic that we implement in the emulator.

#define  SW_80STORE    (memmode & MF_80STORE)
#define  SW_ALTZP      (memmode & MF_ALTZP)
#define  SW_AUXREAD    (memmode & MF_AUXREAD)
#define  SW_AUXWRITE   (memmode & MF_AUXWRITE)
#define  SW_BANK2      (memmode & MF_BANK2)
#define  SW_HIGHRAM    (memmode & MF_HIGHRAM)
#define  SW_HIRES      (memmode & MF_HIRES)
#define  SW_PAGE2      (memmode & MF_PAGE2)
#define  SW_SLOTC3ROM  (memmode & MF_SLOTC3ROM)
#define  SW_INTCXROM   (memmode & MF_INTCXROM)
#define  SW_WRITERAM   (memmode & MF_WRITERAM)
#define  SW_IOUDIS     (memmode & MF_IOUDIS)
#define  SW_ALTROM0    (memmode & MF_ALTROM0)	// For Copam Base64A
#define  SW_ALTROM1    (memmode & MF_ALTROM1)	// For Copam Base64A

/*
MEMORY MANAGEMENT SOFT SWITCHES
 $C000   W       80STOREOFF      Allow page2 to switch video page1 page2
 $C001   W       80STOREON       Allow page2 to switch main & aux video memory
 $C002   W       RAMRDOFF        Read enable main memory from $0200-$BFFF
 $C003   W       RAMRDON         Read enable aux memory from $0200-$BFFF
 $C004   W       RAMWRTOFF       Write enable main memory from $0200-$BFFF
 $C005   W       RAMWRTON        Write enable aux memory from $0200-$BFFF
 $C006   W       INTCXROMOFF     Enable slot ROM from $C100-$C7FF (but $C800-$CFFF depends on INTC8ROM)
 $C007   W       INTCXROMON      Enable main ROM from $C100-$CFFF
 $C008   W       ALTZPOFF        Enable main memory from $0000-$01FF & avl BSR
 $C009   W       ALTZPON         Enable aux memory from $0000-$01FF & avl BSR
 $C00A   W       SLOTC3ROMOFF    Enable main ROM from $C300-$C3FF
 $C00B   W       SLOTC3ROMON     Enable slot ROM from $C300-$C3FF
 $C07E   W       IOUDIS          [//c] On: disable IOU access for addresses $C058 to $C05F; enable access to DHIRES switch
 $C07F   W       IOUDIS          [//c] Off: enable IOU access for addresses $C058 to $C05F; disable access to DHIRES switch

VIDEO SOFT SWITCHES
 $C00C   W       80COLOFF        Turn off 80 column display
 $C00D   W       80COLON         Turn on 80 column display
 $C00E   W       ALTCHARSETOFF   Turn off alternate characters
 $C00F   W       ALTCHARSETON    Turn on alternate characters
 $C050   R/W     TEXTOFF         Select graphics mode
 $C051   R/W     TEXTON          Select text mode
 $C052   R/W     MIXEDOFF        Use full screen for graphics
 $C053   R/W     MIXEDON         Use graphics with 4 lines of text
 $C054   R/W     PAGE2OFF        Select panel display (or main video memory)
 $C055   R/W     PAGE2ON         Select page2 display (or aux video memory)
 $C056   R/W     HIRESOFF        Select low resolution graphics
 $C057   R/W     HIRESON         Select high resolution graphics
 $C05E   R/W     DHIRESON        Select double (14M) resolution graphics
 $C05F   R/W     DHIRESOFF       Select single (7M) resolution graphics

SOFT SWITCH STATUS FLAGS
 $C010   R7      AKD             1=key pressed   0=keys free    (clears strobe)
 $C011   R7      BSRBANK2        1=bank2 available    0=bank1 available
 $C012   R7      BSRREADRAM      1=BSR active for read   0=$D000-$FFFF active (BSR = Bank Switch RAM)
 $C013   R7      RAMRD           0=main $0200-$BFFF active reads  1=aux active
 $C014   R7      RAMWRT          0=main $0200-$BFFF active writes 1=aux writes
 $C015   R7      INTCXROM        1=main $C100-$CFFF ROM active   0=slot active
 $C016   R7      ALTZP           1=aux $0000-$1FF+auxBSR    0=main available
 $C017   R7      SLOTC3ROM       1=slot $C3 ROM active   0=main $C3 ROM active
 $C018   R7      80STORE         1=page2 switches main/aux   0=page2 video
 $C019   R7      VERTBLANK       1=vertical retrace on   0=vertical retrace off
 $C01A   R7      TEXT            1=text mode is active   0=graphics mode active
 $C01B   R7      MIXED           1=mixed graphics & text    0=full screen
 $C01C   R7      PAGE2           1=video page2 selected or aux
 $C01D   R7      HIRES           1=high resolution graphics   0=low resolution
 $C01E   R7      ALTCHARSET      1=alt character set on    0=alt char set off
 $C01F   R7      80COL           1=80 col display on     0=80 col display off
 $C07E   R7      RDIOUDIS        [//c] 1=IOUDIS off     0=IOUDIS on
 $C07F   R7      RDDHIRES        [Enhanced //e? or //c] 1=DHIRES on     0=DHIRES off
*/



//-----------------------------------------------------------------------------

// Notes
// -----
//
// memimage
// - 64KB
//
// mem
// (a pointer to memimage 64KB)
// - a 64KB r/w memory cache
// - reflects the current readable memory in the 6502's 64K address space
//		. excludes $Cxxx I/O memory
//		. could be a mix of RAM/ROM, main/aux, etc
// - may also reflect the current writeable memory (on a 256-page granularity) if the write page addr == read page addr
//		. for this case, the memdirty flag(s) are valid
//		. when writes instead occur to backing-store, then memdirty flag(s) can be ignored
//
// memmain, memaux
// - physical contiguous 64KB "backing-store" for main & aux respectively
// - NB. 4K bank1 BSR is at $C000-$CFFF
//
// memwrite
// - 1 pointer entry per 256-byte page
// - used to write to a page
// - if RD & WR point to the same 256-byte RAM page, then memwrite will point to the page in 'mem'
//		. ie. when SW_AUXREAD==SW_AUXWRITE, or 4K-BSR is r/w, or 8K BSR is r/w, or SW_80STORE=1
//		. So 'mem' remains correct for both r&w operations, but the physical 64K mem block becomes stale
// - if RD & WR point to different 256-byte pages, then memwrite will point to the page in the physical 64K mem block
//		. writes will still set the dirty flag (but can be ignored)
//		. UpdatePaging() ignores this, as it only copies back to the physical 64K mem block when memshadow changes (for that 256-byte page)
//
// memdirty
// - 1 byte entry per 256-byte page
// - set when a write occurs to a 256-byte page
// - indicates that 'mem' (ie. the cache) is out-of-sync with the "physical" 64K backing-store memory
// - NB. a page's dirty flag is only useful(valid) when 'mem' is used for both read & write for the corresponding page
//   When they differ, then writes go directly to the backing-store.
//   . In this case, the dirty flag will just force a memcpy() to the same address in backing-store.
//
// memshadow
// - 1 pointer entry per 256-byte page
// - reflects how 'mem' is setup for read operations (at a 256-byte granularity)
//		. EG: if ALTZP=1, then:
//			. mem will have copies of memaux's ZP & stack
//			. memshadow[0] = &memaux[0x0000]
//			. memshadow[1] = &memaux[0x0100]
//

static LPBYTE  memshadow[0x100];
LPBYTE         memwrite[0x100];

iofunction		IORead[256];
iofunction		IOWrite[256];
static LPVOID	SlotParameters[NUM_SLOTS];

LPBYTE         mem          = NULL;

//

static LPBYTE  memaux       = NULL;
static LPBYTE  memmain      = NULL;

LPBYTE         memdirty     = NULL;
static LPBYTE  memrom       = NULL;

static LPBYTE  memimage     = NULL;

static LPBYTE	pCxRomInternal		= NULL;
static LPBYTE	pCxRomPeripheral	= NULL;

static LPBYTE g_pMemMainLanguageCard = NULL;

static DWORD   memmode      = LanguageCardUnit::kMemModeInitialState;
static BOOL    modechanging = 0;				// An Optimisation: means delay calling UpdatePaging() for 1 instruction

static UINT    memrompages = 1;

static CNoSlotClock* g_NoSlotClock = new CNoSlotClock;
static LanguageCardUnit* g_pLanguageCard = NULL;	// For all Apple II, //e and above

#ifdef RAMWORKS
static UINT		g_uMaxExPages = 1;				// user requested ram pages (default to 1 aux bank: so total = 128KB)
static UINT		g_uActiveBank = 0;				// 0 = aux 64K for: //e extended 80 Col card, or //c -- ALSO RAMWORKS
static LPBYTE	RWpages[kMaxExMemoryBanks];		// pointers to RW memory banks
#endif

static const UINT kNumAnnunciators = 4;
static bool g_Annunciator[kNumAnnunciators] = {};

BYTE __stdcall IO_Annunciator(WORD programcounter, WORD address, BYTE write, BYTE value, ULONG nCycles);

//=============================================================================

// Default memory types on a VM restart
// - can be overwritten by cmd-line or loading a save-state
static SS_CARDTYPE g_MemTypeAppleII = CT_Empty;
static SS_CARDTYPE g_MemTypeAppleIIPlus = CT_LanguageCard;	// Keep a copy so it's not lost if machine type changes, eg: A][ -> A//e -> A][
static SS_CARDTYPE g_MemTypeAppleIIe = CT_Extended80Col;	// Keep a copy so it's not lost if machine type changes, eg: A//e -> A][ -> A//e
static UINT g_uSaturnBanksFromCmdLine = 0;


const UINT CxRomSize = 4 * 1024;
const UINT Apple2RomSize = 12 * 1024;
const UINT Apple2eRomSize = Apple2RomSize + CxRomSize;
//const UINT Pravets82RomSize = 12*1024;
//const UINT Pravets8ARomSize = Pravets82RomSize+CxRomSize;
const UINT MaxRomPages = 4;		// For Copam Base64A
const UINT Base64ARomSize = MaxRomPages * Apple2RomSize;

// Called from MemLoadSnapshot()
static void ResetDefaultMachineMemTypes(void)
{
	g_MemTypeAppleII = CT_Empty;
	g_MemTypeAppleIIPlus = CT_LanguageCard;
	g_MemTypeAppleIIe = CT_Extended80Col;
}

// Called from MemInitialize(), MemLoadSnapshot()
static void SetExpansionMemTypeDefault(void)
{
	SS_CARDTYPE defaultType = IsApple2Original(GetApple2Type()) ? g_MemTypeAppleII
		: IsApple2PlusOrClone(GetApple2Type()) ? g_MemTypeAppleIIPlus
		: g_MemTypeAppleIIe;

	SetExpansionMemType(defaultType);
}

// Called from SetExpansionMemTypeDefault(), MemLoadSnapshotAux(), SaveState.cpp_ParseSlots(), cmd-line switch
void SetExpansionMemType(const SS_CARDTYPE type)
{
	SS_CARDTYPE newSlot0Card;
	SS_CARDTYPE newSlotAuxCard;

	// Set defaults:
	if (IsApple2Original(GetApple2Type()))
	{
		newSlot0Card = CT_Empty;
		newSlotAuxCard = CT_Empty;
	}
	else if (IsApple2PlusOrClone(GetApple2Type()))
	{
		newSlot0Card = CT_LanguageCard;
		newSlotAuxCard = CT_Empty;
	}
	else	// Apple //e or above
	{
		newSlot0Card = CT_Empty;		// NB. No slot0 for //e
		newSlotAuxCard = CT_Extended80Col;
	}

	if (type == CT_LanguageCard || type == CT_Saturn128K)
	{
		g_MemTypeAppleII = type;
		g_MemTypeAppleIIPlus = type;
		if (IsApple2PlusOrClone(GetApple2Type()))
			newSlot0Card = type;
		else
			newSlot0Card = CT_Empty;	// NB. No slot0 for //e
	}
	else if (type == CT_RamWorksIII)
	{
		g_MemTypeAppleIIe = type;
		if (IsApple2PlusOrClone(GetApple2Type()))
			newSlotAuxCard = CT_Empty;	// NB. No aux slot for ][ or ][+
		else
			newSlotAuxCard = type;
	}

	GetCardMgr().Insert(SLOT0, newSlot0Card);
	GetCardMgr().InsertAux(newSlotAuxCard);
}

void CreateLanguageCard(void)
{
	delete g_pLanguageCard;
	g_pLanguageCard = NULL;

	if (IsApple2PlusOrClone(GetApple2Type()))
	{
		if (GetCardMgr().QuerySlot(SLOT0) == CT_Saturn128K)
			g_pLanguageCard = new Saturn128K(g_uSaturnBanksFromCmdLine);
		else if (GetCardMgr().QuerySlot(SLOT0) == CT_LanguageCard)
			g_pLanguageCard = new LanguageCardSlot0;
		else
			g_pLanguageCard = NULL;
	}
	else
	{
		g_pLanguageCard = new LanguageCardUnit;
	}
}

SS_CARDTYPE GetCurrentExpansionMemType(void)
{
	if (IsApple2PlusOrClone(GetApple2Type()))
		return GetCardMgr().QuerySlot(SLOT0);
	else
		return GetCardMgr().QueryAux();
}

//

void SetRamWorksMemorySize(UINT pages)
{
	g_uMaxExPages = pages;
}

UINT GetRamWorksActiveBank(void)
{
	return g_uActiveBank;
}

void SetSaturnMemorySize(UINT banks)
{
	g_uSaturnBanksFromCmdLine = banks;
}

//

static BOOL GetLastRamWrite(void)
{
	if (g_pLanguageCard)
		return g_pLanguageCard->GetLastRamWrite();
	return 0;
}

static void SetLastRamWrite(BOOL count)
{
	if (g_pLanguageCard)
		g_pLanguageCard->SetLastRamWrite(count);
}

//

void SetMemMainLanguageCard(LPBYTE ptr, bool bMemMain /*=false*/)
{
	if (bMemMain)
		g_pMemMainLanguageCard = memmain+0xC000;
	else
		g_pMemMainLanguageCard = ptr;
}

LanguageCardUnit* GetLanguageCard(void)
{
	_ASSERT(g_pLanguageCard);
	return g_pLanguageCard;
}

LPBYTE GetCxRomPeripheral(void)
{
	return pCxRomPeripheral;	// Can be NULL if at MODE_LOGO
}

//=============================================================================

static BYTE __stdcall IORead_C00x(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nExecutedCycles)
{
	return KeybReadData();
}

static BYTE __stdcall IOWrite_C00x(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nExecutedCycles)
{
	if ((addr & 0xf) <= 0xB)
		return MemSetPaging(pc, addr, bWrite, d, nExecutedCycles);
	else
		return GetVideo().VideoSetMode(pc, addr, bWrite, d, nExecutedCycles);
}

//-------------------------------------

static BYTE __stdcall IORead_C01x(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nExecutedCycles)
{
	if (IS_APPLE2)	// Include Pravets machines too?
		return KeybReadFlag();

	bool res = false;
	switch (addr & 0xf)
	{
	case 0x0: return KeybReadFlag();
	case 0x1: res = SW_BANK2     ? true : false;		break;
	case 0x2: res = SW_HIGHRAM   ? true : false;		break;
	case 0x3: res = SW_AUXREAD   ? true : false;		break;
	case 0x4: res = SW_AUXWRITE  ? true : false;		break;
	case 0x5: res = SW_INTCXROM  ? true : false;		break;
	case 0x6: res = SW_ALTZP     ? true : false;		break;
	case 0x7: res = SW_SLOTC3ROM ? true : false;		break;
	case 0x8: res = SW_80STORE   ? true : false;		break;
	case 0x9: res = GetVideo().VideoGetVblBar(nExecutedCycles);	break;
	case 0xA: res = GetVideo().VideoGetSWTEXT();		break;
	case 0xB: res = GetVideo().VideoGetSWMIXED();		break;
	case 0xC: res = SW_PAGE2     ? true : false;		break;
	case 0xD: res = GetVideo().VideoGetSWHIRES();		break;
	case 0xE: res = GetVideo().VideoGetSWAltCharSet();	break;
	case 0xF: res = GetVideo().VideoGetSW80COL();		break;
	}

	return KeybGetKeycode() | (res ? 0x80 : 0);
}

static BYTE __stdcall IOWrite_C01x(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nExecutedCycles)
{
	return KeybReadFlag();
}

//-------------------------------------

static BYTE __stdcall IORead_C02x(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nExecutedCycles)
{
	return IO_Null(pc, addr, bWrite, d, nExecutedCycles);
}

static BYTE __stdcall IOWrite_C02x(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nExecutedCycles)
{
	return IO_Null(pc, addr, bWrite, d, nExecutedCycles);	// $C020 TAPEOUT
}

//-------------------------------------

static BYTE __stdcall IORead_C03x(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nExecutedCycles)
{
	return SpkrToggle(pc, addr, bWrite, d, nExecutedCycles);
}

static BYTE __stdcall IOWrite_C03x(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nExecutedCycles)
{
	return SpkrToggle(pc, addr, bWrite, d, nExecutedCycles);
}

//-------------------------------------

static BYTE __stdcall IORead_C04x(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nExecutedCycles)
{
	return IO_Null(pc, addr, bWrite, d, nExecutedCycles);
}

static BYTE __stdcall IOWrite_C04x(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nExecutedCycles)
{
	return IO_Null(pc, addr, bWrite, d, nExecutedCycles);
}

//-------------------------------------

static BYTE __stdcall IORead_C05x(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nExecutedCycles)
{
	switch (addr & 0xf)
	{
	case 0x0:	return GetVideo().VideoSetMode(pc, addr, bWrite, d, nExecutedCycles);
	case 0x1:	return GetVideo().VideoSetMode(pc, addr, bWrite, d, nExecutedCycles);
	case 0x2:	return GetVideo().VideoSetMode(pc, addr, bWrite, d, nExecutedCycles);
	case 0x3:	return GetVideo().VideoSetMode(pc, addr, bWrite, d, nExecutedCycles);
	case 0x4:	return MemSetPaging(pc, addr, bWrite, d, nExecutedCycles);
	case 0x5:	return MemSetPaging(pc, addr, bWrite, d, nExecutedCycles);
	case 0x6:	return MemSetPaging(pc, addr, bWrite, d, nExecutedCycles);
	case 0x7:	return MemSetPaging(pc, addr, bWrite, d, nExecutedCycles);
	case 0x8:	return IO_Annunciator(pc, addr, bWrite, d, nExecutedCycles);
	case 0x9:	return IO_Annunciator(pc, addr, bWrite, d, nExecutedCycles);
	case 0xA:	return IO_Annunciator(pc, addr, bWrite, d, nExecutedCycles);
	case 0xB:	return IO_Annunciator(pc, addr, bWrite, d, nExecutedCycles);
	case 0xC:	return IO_Annunciator(pc, addr, bWrite, d, nExecutedCycles);
	case 0xD:	return IO_Annunciator(pc, addr, bWrite, d, nExecutedCycles);
	case 0xE:	// fall through...
	case 0xF:	if (IsApple2PlusOrClone(GetApple2Type()))
					IO_Annunciator(pc, addr, bWrite, d, nExecutedCycles);
				else
					return (!SW_IOUDIS) ? GetVideo().VideoSetMode(pc, addr, bWrite, d, nExecutedCycles)
										: IO_Annunciator(pc, addr, bWrite, d, nExecutedCycles);
	}

	return 0;
}

static BYTE __stdcall IOWrite_C05x(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nExecutedCycles)
{
	switch (addr & 0xf)
	{
	case 0x0:	return GetVideo().VideoSetMode(pc, addr, bWrite, d, nExecutedCycles);
	case 0x1:	return GetVideo().VideoSetMode(pc, addr, bWrite, d, nExecutedCycles);
	case 0x2:	return GetVideo().VideoSetMode(pc, addr, bWrite, d, nExecutedCycles);
	case 0x3:	return GetVideo().VideoSetMode(pc, addr, bWrite, d, nExecutedCycles);
	case 0x4:	return MemSetPaging(pc, addr, bWrite, d, nExecutedCycles);
	case 0x5:	return MemSetPaging(pc, addr, bWrite, d, nExecutedCycles);
	case 0x6:	return MemSetPaging(pc, addr, bWrite, d, nExecutedCycles);
	case 0x7:	return MemSetPaging(pc, addr, bWrite, d, nExecutedCycles);
	case 0x8:	return IO_Annunciator(pc, addr, bWrite, d, nExecutedCycles);
	case 0x9:	return IO_Annunciator(pc, addr, bWrite, d, nExecutedCycles);
	case 0xA:	return IO_Annunciator(pc, addr, bWrite, d, nExecutedCycles);
	case 0xB:	return IO_Annunciator(pc, addr, bWrite, d, nExecutedCycles);
	case 0xC:	return IO_Annunciator(pc, addr, bWrite, d, nExecutedCycles);
	case 0xD:	return IO_Annunciator(pc, addr, bWrite, d, nExecutedCycles);
	case 0xE:	// fall through...
	case 0xF:	if (IsApple2PlusOrClone(GetApple2Type()))
					IO_Annunciator(pc, addr, bWrite, d, nExecutedCycles);
				else
					return (!SW_IOUDIS) ? GetVideo().VideoSetMode(pc, addr, bWrite, d, nExecutedCycles)
										: IO_Annunciator(pc, addr, bWrite, d, nExecutedCycles);
	}

	return 0;
}

//-------------------------------------

static BYTE __stdcall IORead_C06x(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nExecutedCycles)
{	
	switch (addr & 0x7) // address bit 4 is ignored (UTAIIe:7-5)
	{
	//In Pravets8A/C if SETMODE (8bit character encoding) is enabled, bit6 in $C060 is 0; Else it is 1
	//If (CAPS lOCK of Pravets8A/C is on or Shift is pressed) and (MODE is enabled), bit7 in $C000 is 1; Else it is 0
	//Writing into $C060 sets MODE on and off. If bit 0 is 0 the the MODE is set 0, if bit 0 is 1 then MODE is set to 1 (8-bit)

	case 0x0:	return TapeRead(pc, addr, bWrite, d, nExecutedCycles);			// $C060 TAPEIN
	case 0x1:	return JoyReadButton(pc, addr, bWrite, d, nExecutedCycles);		//$C061 Digital input 0 (If bit 7=1 then JoyButton 0 or OpenApple is pressed)
	case 0x2:	return JoyReadButton(pc, addr, bWrite, d, nExecutedCycles);		//$C062 Digital input 1 (If bit 7=1 then JoyButton 1 or ClosedApple is pressed)
	case 0x3:	return JoyReadButton(pc, addr, bWrite, d, nExecutedCycles);		//$C063 Digital input 2
	case 0x4:	return JoyReadPosition(pc, addr, bWrite, d, nExecutedCycles);	//$C064 Analog input 0
	case 0x5:	return JoyReadPosition(pc, addr, bWrite, d, nExecutedCycles);	//$C065 Analog input 1
	case 0x6:	return JoyReadPosition(pc, addr, bWrite, d, nExecutedCycles);	//$C066 Analog input 2
	case 0x7:	return JoyReadPosition(pc, addr, bWrite, d, nExecutedCycles);	//$C067 Analog input 3
	}

	return 0;
}

static BYTE __stdcall IOWrite_C06x(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nExecutedCycles)
{
	switch (addr & 0xf)
	{
	case 0x0:	
		if (g_Apple2Type == A2TYPE_PRAVETS8A)
			return TapeWrite (pc, addr, bWrite, d, nExecutedCycles);
		else
			return IO_Null(pc, addr, bWrite, d, nExecutedCycles); //Apple2 value
	}
	return IO_Null(pc, addr, bWrite, d, nExecutedCycles); //Apple2 value
}

//-------------------------------------

static BYTE __stdcall IORead_C07x(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nExecutedCycles)
{
	// Apple//e TRM, pg-258: "Reading or writing any address in the range $C070-$C07F also triggers the paddle timer and resets the VBLINT(*)." (*) //c only!
	JoyResetPosition(nExecutedCycles);  //$C07X Analog input reset

	switch (addr & 0xf)
	{
	case 0x0:	return IO_Null(pc, addr, bWrite, d, nExecutedCycles);
	case 0x1:	return IO_Null(pc, addr, bWrite, d, nExecutedCycles);
	case 0x2:	return IO_Null(pc, addr, bWrite, d, nExecutedCycles);
	case 0x3:	return IO_Null(pc, addr, bWrite, d, nExecutedCycles);
	case 0x4:	return IO_Null(pc, addr, bWrite, d, nExecutedCycles);
	case 0x5:	return IO_Null(pc, addr, bWrite, d, nExecutedCycles);
	case 0x6:	return IO_Null(pc, addr, bWrite, d, nExecutedCycles);
	case 0x7:	return IO_Null(pc, addr, bWrite, d, nExecutedCycles);
	case 0x8:	return IO_Null(pc, addr, bWrite, d, nExecutedCycles);
	case 0x9:	return IO_Null(pc, addr, bWrite, d, nExecutedCycles);
	case 0xA:	return IO_Null(pc, addr, bWrite, d, nExecutedCycles);
	case 0xB:	return IO_Null(pc, addr, bWrite, d, nExecutedCycles);
	case 0xC:	return IO_Null(pc, addr, bWrite, d, nExecutedCycles);
	case 0xD:	return IO_Null(pc, addr, bWrite, d, nExecutedCycles);
	case 0xE:	return IS_APPLE2C()			? MemReadFloatingBus(SW_IOUDIS ? true : false, nExecutedCycles)	// GH#636
											: IO_Null(pc, addr, bWrite, d, nExecutedCycles);
	case 0xF:	return IsEnhancedIIEorIIC()	? MemReadFloatingBus(GetVideo().VideoGetSWDHIRES(), nExecutedCycles)		// GH#636
											: IO_Null(pc, addr, bWrite, d, nExecutedCycles);
	}

	return 0;
}

static BYTE __stdcall IOWrite_C07x(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nExecutedCycles)
{
	// Apple//e TRM, pg-258: "Reading or writing any address in the range $C070-$C07F also triggers the paddle timer and resets the VBLINT(*)." (*) //c only!
	JoyResetPosition(nExecutedCycles);  //$C07X Analog input reset

	switch (addr & 0xf)
	{
	case 0x0:	return IO_Null(pc, addr, bWrite, d, nExecutedCycles);
#ifdef RAMWORKS
	case 0x1:	return MemSetPaging(pc, addr, bWrite, d, nExecutedCycles);	// extended memory card set page
	case 0x2:	return IO_Null(pc, addr, bWrite, d, nExecutedCycles);
	case 0x3:	return MemSetPaging(pc, addr, bWrite, d, nExecutedCycles);	// Ramworks III set page
#else
	case 0x1:	return IO_Null(pc, addr, bWrite, d, nExecutedCycles);
	case 0x2:	return IO_Null(pc, addr, bWrite, d, nExecutedCycles);
	case 0x3:	return IO_Null(pc, addr, bWrite, d, nExecutedCycles);
#endif
	case 0x4:	return IO_Null(pc, addr, bWrite, d, nExecutedCycles);
	case 0x5:	return IO_Null(pc, addr, bWrite, d, nExecutedCycles);
	case 0x6:	return IO_Null(pc, addr, bWrite, d, nExecutedCycles);
	case 0x7:	return IO_Null(pc, addr, bWrite, d, nExecutedCycles);
	case 0x8:	return IO_Null(pc, addr, bWrite, d, nExecutedCycles);
	case 0x9:	return IO_Null(pc, addr, bWrite, d, nExecutedCycles);
	case 0xA:	return IO_Null(pc, addr, bWrite, d, nExecutedCycles);
	case 0xB:	return IO_Null(pc, addr, bWrite, d, nExecutedCycles);
	case 0xC:	return IO_Null(pc, addr, bWrite, d, nExecutedCycles);
	case 0xD:	return IO_Null(pc, addr, bWrite, d, nExecutedCycles);
	case 0xE:	if (IS_APPLE2C())
					SetMemMode(memmode & ~MF_IOUDIS);	// disable IOU access for addresses $C058 to $C05F; enable access to DHIRES switch
				else
					return IO_Null(pc, addr, bWrite, d, nExecutedCycles);
				break;
	case 0xF:	if (IS_APPLE2C())
					SetMemMode(memmode | MF_IOUDIS);	// enable IOU access for addresses $C058 to $C05F; disable access to DHIRES switch
				else
					return IO_Null(pc, addr, bWrite, d, nExecutedCycles);
				break;
	}

	return 0;
}

//-----------------------------------------------------------------------------

static iofunction IORead_C0xx[8] =
{
	IORead_C00x,		// Keyboard
	IORead_C01x,		// Memory/Video
	IORead_C02x,		// Cassette
	IORead_C03x,		// Speaker
	IORead_C04x,
	IORead_C05x,		// Video
	IORead_C06x,		// Joystick
	IORead_C07x,		// Joystick/Video
};

static iofunction IOWrite_C0xx[8] =
{
	IOWrite_C00x,		// Memory/Video
	IOWrite_C01x,		// Keyboard
	IOWrite_C02x,		// Cassette
	IOWrite_C03x,		// Speaker
	IOWrite_C04x,
	IOWrite_C05x,		// Video/Memory
	IOWrite_C06x,
	IOWrite_C07x,		// Joystick/Ramworks
};

static BYTE IO_SELECT = 0;
static bool INTC8ROM = false;	// UTAIIe:5-28

static BYTE* ExpansionRom[NUM_SLOTS];

enum eExpansionRomType {eExpRomNull=0, eExpRomInternal, eExpRomPeripheral};
static eExpansionRomType g_eExpansionRomType = eExpRomNull;
static UINT	g_uPeripheralRomSlot = 0;

//=============================================================================

BYTE __stdcall IO_Null(WORD programcounter, WORD address, BYTE write, BYTE value, ULONG nExecutedCycles)
{
	if (!write)
		return MemReadFloatingBus(nExecutedCycles);
	else
		return 0;
}

BYTE __stdcall IO_Annunciator(WORD programcounter, WORD address, BYTE write, BYTE value, ULONG nExecutedCycles)
{
	// Apple//e ROM:
	// . $FA6F: LDA $C058 (SETAN0) ; AN0 = TTL LO
	// . $FA72: LDA $C05A (SETAN1) ; AN1 = TTL LO
	// . $C2B5: LDA $C05D (CLRAN2) ;SETUP
	// . $C2B8: LDA $C05F (CLRAN3) ; ANNUNCIATORS

	// NB. AN3: For //e & //c these locations are now used to enabled/disabled DHIRES

	g_Annunciator[(address>>1) & 3] = (address&1) ? true : false;

	if (address >= 0xC058 && address <= 0xC05B)
		JoyportControl(address & 0x3);	// AN0 and AN1 control

	if (address >= 0xC058 && address <= 0xC05B && IsCopamBase64A(GetApple2Type()))
		MemSetPaging(programcounter, address, write, value, nExecutedCycles);

	if (address >= 0xC05C && address <= 0xC05D && IsApple2JPlus(GetApple2Type()))
		NTSC_VideoInitAppleType();		// AN2 switches between Katakana & ASCII video rom chars (GH#773)

	if (!write)
		return MemReadFloatingBus(nExecutedCycles);
	else
		return 0;
}

inline bool IsPotentialNoSlotClockAccess(const WORD address)
{
	// UAIIe:5-28
	const BYTE AddrHi = address >>  8;
	return ( ((SW_INTCXROM || !SW_SLOTC3ROM) && (AddrHi == 0xC3)) ||	// Internal ROM at [$C100-CFFF or $C300-C3FF] && AddrHi == $C3
			  (SW_INTCXROM && (AddrHi == 0xC8)) );						// Internal ROM at [$C100-CFFF]               && AddrHi == $C8
}

static bool IsCardInSlot(const UINT uSlot);

// Enabling expansion ROM ($C800..$CFFF]:
// . Enable if: Enable1 && Enable2
// . Enable1 = I/O SELECT' (6502 accesses $Csxx)
//   - Reset when 6502 accesses $CFFF
// . Enable2 = I/O STROBE' (6502 accesses [$C800..$CFFF])

// TODO:
// . IO_SELECT and INTC8ROM are sticky - they only getting reset by $CFFF and MemReset()
// . Check Sather UAIIe, but I assume that a 6502 access to a non-$Csxx (and non-expansion ROM) location will clear IO_SELECT

// NB. ProDOS boot sets IO_SELECT=0x04 (its scan for boot devices?), as slot2 contains a card (ie. SSC) with an expansion ROM.

//
// -----------
// UTAIIe:5-28
//                       $C100-C2FF
// INTCXROM   SLOTC3ROM  $C400-CFFF $C300-C3FF
//    0           0         slot     internal
//    0           1         slot       slot
//    1           0       internal   internal
//    1           1       internal   internal
//
// NB. if (INTCXROM || INTC8ROM) == true then internal ROM
//
// -----------
//
// INTC8ROM: Unreadable soft switch (UTAIIe:5-28)
// . Set:   On access to $C3XX with SLOTC3ROM reset
//			- "From this point, $C800-$CFFF will stay assigned to motherboard ROM until
//			   an access is made to $CFFF or until the MMU detects a system reset."
// . Reset: On access to $CFFF or an MMU reset
//

static BYTE __stdcall IO_Cxxx(WORD programcounter, WORD address, BYTE write, BYTE value, ULONG nExecutedCycles)
{
	if (address == 0xCFFF)
	{
		// Disable expansion ROM at [$C800..$CFFF]
		// . SSC will disable on an access to $CFxx - but ROM only accesses $CFFF, so it doesn't matter
		IO_SELECT = 0;
		INTC8ROM = false;
		g_uPeripheralRomSlot = 0;

		if (!SW_INTCXROM)
		{
			// NB. SW_INTCXROM==1 ensures that internal rom stays switched in
			memset(pCxRomPeripheral+0x800, 0, FIRMWARE_EXPANSION_SIZE);
			memset(mem+FIRMWARE_EXPANSION_BEGIN, 0, FIRMWARE_EXPANSION_SIZE);
			g_eExpansionRomType = eExpRomNull;
		}

		// NB. IO_SELECT won't get set, so ROM won't be switched back in...
	}

	//

	BYTE IO_STROBE = 0;

	if (IS_APPLE2 || !SW_INTCXROM)
	{
		if ((address >= APPLE_SLOT_BEGIN) && (address <= APPLE_SLOT_END))
		{
			const UINT uSlot = (address>>8)&0x7;
			if (uSlot != 3)
			{
				if (ExpansionRom[uSlot])
					IO_SELECT |= 1<<uSlot;
			}
			else	// slot3
			{
				if ((SW_SLOTC3ROM) && ExpansionRom[uSlot])
					IO_SELECT |= 1<<uSlot;		// Slot3 & Peripheral ROM
				else if (!SW_SLOTC3ROM)
					INTC8ROM = true;			// Slot3 & Internal ROM
			}
		}
		else if ((address >= FIRMWARE_EXPANSION_BEGIN) && (address <= FIRMWARE_EXPANSION_END))
		{
			if (!INTC8ROM)	// [GH#423] UTAIIe:5-28: if INTCXROM or INTC8ROM is configured for internal response,
							// then access to $C800-$CFFF results in ROMEN1' low (active) and I/O STROBE' high (inactive)
				IO_STROBE = 1;
		}

		//

		if (IO_SELECT && IO_STROBE)
		{
			// Enable Peripheral Expansion ROM
			UINT uSlot=1;
			for (; uSlot<NUM_SLOTS; uSlot++)
			{
				if (IO_SELECT & (1<<uSlot))
				{
					BYTE RemainingSelected = IO_SELECT & ~(1<<uSlot);
					_ASSERT(RemainingSelected == 0);
					break;
				}
			}

			if (ExpansionRom[uSlot] && (g_uPeripheralRomSlot != uSlot))
			{
				memcpy(pCxRomPeripheral+0x800, ExpansionRom[uSlot], FIRMWARE_EXPANSION_SIZE);
				memcpy(mem+FIRMWARE_EXPANSION_BEGIN, ExpansionRom[uSlot], FIRMWARE_EXPANSION_SIZE);
				g_eExpansionRomType = eExpRomPeripheral;
				g_uPeripheralRomSlot = uSlot;
			}
		}
		else if (INTC8ROM && (g_eExpansionRomType != eExpRomInternal))
		{
			// Enable Internal ROM
			// . Get this for PR#3
			memcpy(mem+FIRMWARE_EXPANSION_BEGIN, pCxRomInternal+0x800, FIRMWARE_EXPANSION_SIZE);
			g_eExpansionRomType = eExpRomInternal;
			g_uPeripheralRomSlot = 0;
		}
	}

	// NSC only for //e at internal C3/C8 ROMs, as II/II+ has no internal ROM here! (GH#827)
	if (!IS_APPLE2 && g_NoSlotClock && IsPotentialNoSlotClockAccess(address))
	{
		if (g_NoSlotClock->ReadWrite(address, value, write))
			return value;
	}

	if (!IS_APPLE2 && SW_INTCXROM)
	{
		// !SW_SLOTC3ROM = Internal ROM: $C300-C3FF
		//  SW_INTCXROM  = Internal ROM: $C100-CFFF

		if ((address >= 0xC300) && (address <= 0xC3FF))
		{
			if (!SW_SLOTC3ROM)	// GH#423
				INTC8ROM = true;
		}
		else if ((address >= FIRMWARE_EXPANSION_BEGIN) && (address <= FIRMWARE_EXPANSION_END))
		{
			if (!INTC8ROM)		// GH#423
				IO_STROBE = 1;
		}

		if (INTC8ROM && (g_eExpansionRomType != eExpRomInternal))
		{
			// Enable Internal ROM
			memcpy(mem+FIRMWARE_EXPANSION_BEGIN, pCxRomInternal+0x800, FIRMWARE_EXPANSION_SIZE);
			g_eExpansionRomType = eExpRomInternal;
			g_uPeripheralRomSlot = 0;
		}
	}

	if (address >= APPLE_SLOT_BEGIN && address <= APPLE_SLOT_END)
	{
		const UINT uSlot = (address>>8)&0x7;
		const bool bPeripheralSlotRomEnabled = IS_APPLE2 ? true	// A][
													     :		// A//e or above
			  ( !SW_INTCXROM   &&					// Peripheral (card) ROMs enabled in $C100..$C7FF
		      !(!SW_SLOTC3ROM  && uSlot == 3) );	// Internal C3 ROM disabled in $C300 when slot == 3

		// Fix for GH#149 and GH#164
		if (bPeripheralSlotRomEnabled && !IsCardInSlot(uSlot))	// Slot is empty
		{
			return IO_Null(programcounter, address, write, value, nExecutedCycles);
		}
	}

	if ((g_eExpansionRomType == eExpRomNull) && (address >= FIRMWARE_EXPANSION_BEGIN))
		return IO_Null(programcounter, address, write, value, nExecutedCycles);

	return mem[address];
}

BYTE __stdcall IO_F8xx(WORD programcounter, WORD address, BYTE write, BYTE value, ULONG nCycles)	// NSC for Apple II/II+ (GH#827)
{
	if (IS_APPLE2 && g_NoSlotClock && !SW_HIGHRAM)
	{
		if (g_NoSlotClock->ReadWrite(address, value, write))
			return value;
	}

	//

	if (!write)
	{
		return *(mem+address);
	}
	else
	{
		memdirty[address >> 8] = 0xFF;
		LPBYTE page = memwrite[address >> 8];
		if (page)
			*(page+(address & 0xFF)) = value;
		return 0;
	}
}

//===========================================================================

static struct SlotInfo
{
	bool bHasCard;
	iofunction IOReadCx;
	iofunction IOWriteCx;
} g_SlotInfo[NUM_SLOTS] = {0};

static void InitIoHandlers()
{
	UINT i=0;

	for (; i<8; i++)	// C00x..C07x
	{
		IORead[i]	= IORead_C0xx[i];
		IOWrite[i]	= IOWrite_C0xx[i];
	}

	for (; i<16; i++)	// C08x..C0Fx
	{
		IORead[i]	= IO_Null;
		IOWrite[i]	= IO_Null;
	}

	//

	for (; i<256; i++)	// C10x..CFFx
	{
		IORead[i]	= IO_Cxxx;
		IOWrite[i]	= IO_Cxxx;
	}

	//

	for (i=0; i<NUM_SLOTS; i++)
	{
		g_SlotInfo[i].bHasCard = false;
		g_SlotInfo[i].IOReadCx = IO_Cxxx;
		g_SlotInfo[i].IOWriteCx = IO_Cxxx;
		ExpansionRom[i] = NULL;
	}
}

// All slots [0..7] must register their handlers
void RegisterIoHandler(UINT uSlot, iofunction IOReadC0, iofunction IOWriteC0, iofunction IOReadCx, iofunction IOWriteCx, LPVOID lpSlotParameter, BYTE* pExpansionRom)
{
	_ASSERT(uSlot < NUM_SLOTS);
	SlotParameters[uSlot] = lpSlotParameter;

	IORead[uSlot+8]		= IOReadC0;
	IOWrite[uSlot+8]	= IOWriteC0;

	if (uSlot == 0)		// Don't trash C0xx handlers
		return;

	if (IOReadCx == NULL)	IOReadCx = IO_Cxxx;
	if (IOWriteCx == NULL)	IOWriteCx = IO_Cxxx;

	for (UINT i=0; i<16; i++)
	{
		IORead[uSlot*16+i]	= IOReadCx;
		IOWrite[uSlot*16+i]	= IOWriteCx;
	}

	g_SlotInfo[uSlot].bHasCard = true;
	g_SlotInfo[uSlot].IOReadCx = IOReadCx;
	g_SlotInfo[uSlot].IOWriteCx = IOWriteCx;

	// What about [$C80x..$CFEx]? - Do any cards use this as I/O memory?
	ExpansionRom[uSlot] = pExpansionRom;
}

// From UTAIIe:5-28: Since INTCXROM==1 then state of SLOTC3ROM is not important
static void IoHandlerCardsOut(void)
{
	_ASSERT( SW_INTCXROM );

	for (UINT uSlot=1; uSlot<NUM_SLOTS; uSlot++)
	{
		for (UINT i=0; i<16; i++)
		{
			IORead[uSlot*16+i]	= IO_Cxxx;
			IOWrite[uSlot*16+i]	= IO_Cxxx;
		}
	}
}

static void IoHandlerCardsIn(void)
{
	_ASSERT( !SW_INTCXROM );

	for (UINT uSlot=1; uSlot<NUM_SLOTS; uSlot++)
	{
		iofunction ioreadcx  = g_SlotInfo[uSlot].IOReadCx;
		iofunction iowritecx = g_SlotInfo[uSlot].IOWriteCx;

		if (uSlot == 3 && !SW_SLOTC3ROM)
		{
			// From UTAIIe:5-28: If INTCXROM==0 && SLOTC3ROM==0 Then $C300-C3FF is internal ROM
			ioreadcx  = IO_Cxxx;
			iowritecx = IO_Cxxx;
		}

		for (UINT i=0; i<16; i++)
		{
			IORead[uSlot*16+i]	= ioreadcx;
			IOWrite[uSlot*16+i]	= iowritecx;
		}
	}
}

static bool IsCardInSlot(const UINT uSlot)
{
	return g_SlotInfo[uSlot].bHasCard;
}

//===========================================================================

DWORD GetMemMode(void)
{
	return memmode;
}

void SetMemMode(DWORD uNewMemMode)
{
#if defined(_DEBUG) && 0
	static DWORD dwOldDiff = 0;
	DWORD dwDiff = memmode ^ uNewMemMode;
	dwDiff &= ~(MF_SLOTC3ROM | MF_INTCXROM);
	if (dwOldDiff != dwDiff)
	{
		dwOldDiff = dwDiff;
		char szStr[100];
		char* psz = szStr;
		psz += sprintf(psz, "diff = %08X ", dwDiff);
		psz += sprintf(psz, "80=%d "   , SW_80STORE   ? 1 : 0);
		psz += sprintf(psz, "ALTZP=%d ", SW_ALTZP     ? 1 : 0);
		psz += sprintf(psz, "AUXR=%d " , SW_AUXREAD   ? 1 : 0);
		psz += sprintf(psz, "AUXW=%d " , SW_AUXWRITE  ? 1 : 0);
		psz += sprintf(psz, "BANK2=%d ", SW_BANK2     ? 1 : 0);
		psz += sprintf(psz, "HIRAM=%d ", SW_HIGHRAM   ? 1 : 0);
		psz += sprintf(psz, "HIRES=%d ", SW_HIRES     ? 1 : 0);
		psz += sprintf(psz, "PAGE2=%d ", SW_PAGE2     ? 1 : 0);
		psz += sprintf(psz, "C3=%d "   , SW_SLOTC3ROM ? 1 : 0);
		psz += sprintf(psz, "CX=%d "   , SW_INTCXROM  ? 1 : 0);
		psz += sprintf(psz, "WRAM=%d " , SW_WRITERAM  ? 1 : 0);
		psz += sprintf(psz, "\n");
		OutputDebugString(szStr);
	}
#endif
	memmode = uNewMemMode;
}

//===========================================================================

static void ResetPaging(BOOL initialize);
static void UpdatePaging(BOOL initialize);

// Call by:
// . CtrlReset() Soft-reset (Ctrl+Reset) for //e
void MemResetPaging()
{
	ResetPaging(0);		// Initialize=0
}

static void ResetPaging(BOOL initialize)
{
	SetLastRamWrite(0);

	if (IsApple2PlusOrClone(GetApple2Type()) && GetCardMgr().QuerySlot(SLOT0) == CT_Empty)
		SetMemMode(0);
	else
		SetMemMode(LanguageCardUnit::kMemModeInitialState);

	UpdatePaging(initialize);
}

//===========================================================================

void MemUpdatePaging(BOOL initialize)
{
	UpdatePaging(initialize);
}

static void UpdatePaging(BOOL initialize)
{
	modechanging = 0;

	// SAVE THE CURRENT PAGING SHADOW TABLE
	LPBYTE oldshadow[256];
	if (!initialize)
		memcpy(oldshadow,memshadow,256*sizeof(LPBYTE));

	// UPDATE THE PAGING TABLES BASED ON THE NEW PAGING SWITCH VALUES
	UINT loop;
	if (initialize)
	{
		for (loop = 0x00; loop < 0xC0; loop++)
			memwrite[loop] = mem+(loop << 8);

		for (loop = 0xC0; loop < 0xD0; loop++)
			memwrite[loop] = NULL;
	}

	for (loop = 0x00; loop < 0x02; loop++)
		memshadow[loop] = SW_ALTZP ? memaux+(loop << 8) : memmain+(loop << 8);

	for (loop = 0x02; loop < 0xC0; loop++)
	{
		memshadow[loop] = SW_AUXREAD ? memaux+(loop << 8)
			: memmain+(loop << 8);

		memwrite[loop]  = ((SW_AUXREAD != 0) == (SW_AUXWRITE != 0))
			? mem+(loop << 8)
			: SW_AUXWRITE	? memaux+(loop << 8)
							: memmain+(loop << 8);
	}

	for (loop = 0xC0; loop < 0xC8; loop++)
	{
		memdirty[loop] = 0;	// mem(cache) can't be dirty for ROM (but STA $Csnn will set the dirty flag)
		const UINT uSlotOffset = (loop & 0x0f) * 0x100;
		if (loop == 0xC3)
			memshadow[loop] = (SW_SLOTC3ROM && !SW_INTCXROM)	? pCxRomPeripheral+uSlotOffset	// C300..C3FF - Slot 3 ROM (all 0x00's)
																: pCxRomInternal+uSlotOffset;	// C300..C3FF - Internal ROM
		else
			memshadow[loop] = !SW_INTCXROM	? pCxRomPeripheral+uSlotOffset						// C000..C7FF - SSC/Disk][/etc
											: pCxRomInternal+uSlotOffset;						// C000..C7FF - Internal ROM
	}

	for (loop = 0xC8; loop < 0xD0; loop++)
	{
		memdirty[loop] = 0;	// mem(cache) can't be dirty for ROM (but STA $Cnnn will set the dirty flag)
		const UINT uRomOffset = (loop & 0x0f) * 0x100;
		memshadow[loop] = (!SW_INTCXROM && !INTC8ROM)	? pCxRomPeripheral+uRomOffset			// C800..CFFF - Peripheral ROM (GH#486)
														: pCxRomInternal+uRomOffset;			// C800..CFFF - Internal ROM
	}

	const int selectedrompage = (SW_ALTROM0 ? 1 : 0) | (SW_ALTROM1 ? 2 : 0);
#ifdef _DEBUG
	if (selectedrompage) { _ASSERT(IsCopamBase64A(GetApple2Type())); }
#endif
	const int romoffset = (selectedrompage % memrompages) * Apple2RomSize;	// Only Copam Base64A has a non-zero romoffset
	for (loop = 0xD0; loop < 0xE0; loop++)
	{
		const int bankoffset = (SW_BANK2 ? 0 : 0x1000);
		memshadow[loop] = SW_HIGHRAM ? SW_ALTZP	? memaux+(loop << 8)-bankoffset
												: g_pMemMainLanguageCard+((loop-0xC0)<<8)-bankoffset
									 : memrom+((loop-0xD0) * 0x100)+romoffset;

		memwrite[loop]  = SW_WRITERAM	? SW_HIGHRAM	? mem+(loop << 8)
														: SW_ALTZP	? memaux+(loop << 8)-bankoffset
																	: g_pMemMainLanguageCard+((loop-0xC0)<<8)-bankoffset
										: NULL;
	}

	for (loop = 0xE0; loop < 0x100; loop++)
	{
		memshadow[loop] = SW_HIGHRAM	? SW_ALTZP	? memaux+(loop << 8)
													: g_pMemMainLanguageCard+((loop-0xC0)<<8)
										: memrom+((loop-0xD0) * 0x100)+romoffset;

		memwrite[loop]  = SW_WRITERAM	? SW_HIGHRAM	? mem+(loop << 8)
														: SW_ALTZP	? memaux+(loop << 8)
																	: g_pMemMainLanguageCard+((loop-0xC0)<<8)
										: NULL;
	}

	if (SW_80STORE)
	{
		for (loop = 0x04; loop < 0x08; loop++)
		{
			memshadow[loop] = SW_PAGE2	? memaux+(loop << 8)
										: memmain+(loop << 8);
			memwrite[loop]  = mem+(loop << 8);
		}

		if (SW_HIRES)
		{
			for (loop = 0x20; loop < 0x40; loop++)
			{
				memshadow[loop] = SW_PAGE2	? memaux+(loop << 8)
											: memmain+(loop << 8);
				memwrite[loop]  = mem+(loop << 8);
			}
		}
	}

	// MOVE MEMORY BACK AND FORTH AS NECESSARY BETWEEN THE SHADOW AREAS AND
	// THE MAIN RAM IMAGE TO KEEP BOTH SETS OF MEMORY CONSISTENT WITH THE NEW
	// PAGING SHADOW TABLE
	//
	// NB. the condition 'loop <= 1' is there because:
	// . Page0 (ZP)    : memdirty[0] is set when the 6502 CPU does a ZP-write, but perhaps older versions didn't set this flag (eg. the asm version?).
	// . Page1 (stack) : memdirty[1] is NOT set when the 6502 CPU writes to this page with JSR, etc.

	for (loop = 0x00; loop < 0x100; loop++)
	{
		if (initialize || (oldshadow[loop] != memshadow[loop]))
		{
			if (!initialize &&
				((*(memdirty+loop) & 1) || (loop <= 1)))
			{
				*(memdirty+loop) &= ~1;
				memcpy(oldshadow[loop],mem+(loop << 8),256);
			}

			memcpy(mem+(loop << 8),memshadow[loop],256);
		}
	}
}

//
// ----- ALL GLOBALLY ACCESSIBLE FUNCTIONS ARE BELOW THIS LINE -----
//

//===========================================================================

void MemDestroy()
{
	ALIGNED_FREE(memaux);
	ALIGNED_FREE(memmain);
	ALIGNED_FREE(memimage);

	delete [] memdirty;
	delete [] memrom;

	delete [] pCxRomInternal;
	delete [] pCxRomPeripheral;

#ifdef RAMWORKS
	for (UINT i=1; i<g_uMaxExPages; i++)
	{
		if (RWpages[i])
		{
			ALIGNED_FREE(RWpages[i]);
			RWpages[i] = NULL;
		}
	}
	RWpages[0]=NULL;
#endif

	delete g_pLanguageCard;
	g_pLanguageCard = NULL;

	memaux   = NULL;
	memmain  = NULL;
	memdirty = NULL;
	memrom   = NULL;
	memimage = NULL;

	pCxRomInternal		= NULL;
	pCxRomPeripheral	= NULL;

	mem      = NULL;

	memset(memwrite,  0, sizeof(memwrite));
	memset(memshadow, 0, sizeof(memshadow));
}

//===========================================================================

bool MemCheckSLOTC3ROM()
{
	return SW_SLOTC3ROM ? true : false;
}

bool MemCheckINTCXROM()
{
	return SW_INTCXROM ? true : false;
}

//===========================================================================

static void BackMainImage(void)
{
	for (UINT loop = 0; loop < 256; loop++)
	{
		if (memshadow[loop] && ((*(memdirty+loop) & 1) || (loop <= 1)))
			memcpy(memshadow[loop], mem+(loop << 8), 256);

		*(memdirty+loop) &= ~1;
	}
}

//===========================================================================

static LPBYTE MemGetPtrBANK1(const WORD offset, const LPBYTE pMemBase)
{
	if ((offset & 0xF000) != 0xC000)	// Requesting RAM at physical addr $Cxxx (ie. 4K RAM BANK1)
		return NULL;

	// NB. This works for memaux when set to any RWpages[] value, ie. RamWork III "just works"
	const BYTE bank1page = (offset >> 8) & 0xF;
	return (memshadow[0xD0+bank1page] == pMemBase+(0xC0+bank1page)*256)
		? mem+offset+0x1000				// Return ptr to $Dxxx address - 'mem' has (a potentially dirty) 4K RAM BANK1 mapped in at $D000
		: pMemBase+offset;				// Else return ptr to $Cxxx address
}

//-------------------------------------

LPBYTE MemGetAuxPtr(const WORD offset)
{
	LPBYTE lpMem = MemGetPtrBANK1(offset, memaux);
	if (lpMem)
		return lpMem;

	lpMem = (memshadow[(offset >> 8)] == (memaux+(offset & 0xFF00)))
			? mem+offset				// Return 'mem' copy if possible, as page could be dirty
			: memaux+offset;

#ifdef RAMWORKS
	// Video scanner (for 14M video modes) always fetches from 1st 64K aux bank (UTAIIe ref?)
	if (((SW_PAGE2 && SW_80STORE) || GetVideo().VideoGetSW80COL()) &&
			(
				(             ((offset & 0xFF00)>=0x0400) && ((offset & 0xFF00)<=0x0700) ) ||
				( SW_HIRES && ((offset & 0xFF00)>=0x2000) && ((offset & 0xFF00)<=0x3F00) )
			)
		)
	{
		lpMem = (memshadow[(offset >> 8)] == (RWpages[0]+(offset & 0xFF00)))
			? mem+offset
			: RWpages[0]+offset;
	}
#endif

	return lpMem;
}

//-------------------------------------

// if memshadow == memmain
//	  so RD memmain
//	  case: RD == WR
//		so RD(mem),WR(mem)
//		so 64K memmain could be incorrect
//		*therefore mem is correct
//	  case: RD != WR
//		so RD(mem),WR(memaux)
//		doesn't matter since RD != WR, then it's guaranteed that memaux is correct
//		*therefore either mem or memmain is correct
// else ; memshadow != memmain
//	  so RD memaux (or ROM)
//	  case: RD == WR
//		so RD(mem),WR(mem)
//		so 64K memaux could be incorrect
//		*therefore memmain is correct
//	  case: RD != WR
//		so RD(mem),WR(memmain)
//		doesn't matter since RD != WR, then it's guaranteed that memmain is correct
//		*therefore memmain is correct
//
// *OR*
//
// Is the mem(cache) setup to read (via memshadow) from memmain?
// . if yes, then return the mem(cache) address as writes (via memwrite) may've made the mem(cache) dirty.
// . if no, then return memmain, as the mem(cache) isn't involved in memmain (any writes will go directly to this backing-store).
//

LPBYTE MemGetMainPtr(const WORD offset)
{
	LPBYTE lpMem = MemGetPtrBANK1(offset, memmain);
	if (lpMem)
		return lpMem;

	return (memshadow[(offset >> 8)] == (memmain+(offset & 0xFF00)))
			? mem+offset				// Return 'mem' copy if possible, as page could be dirty
			: memmain+offset;
}

//===========================================================================

// Used by:
// . Savestate: MemSaveSnapshotMemory(), MemLoadSnapshotAux()
// . Debugger : CmdMemorySave(), CmdMemoryLoad()
LPBYTE MemGetBankPtr(const UINT nBank)
{
	BackMainImage();	// Flush any dirty pages to back-buffer

#ifdef RAMWORKS
	if (nBank > g_uMaxExPages)
		return NULL;

	if (nBank == 0)
		return memmain;

	return RWpages[nBank-1];
#else
	return	(nBank == 0) ? memmain :
			(nBank == 1) ? memaux :
			NULL;
#endif
}

//===========================================================================

LPBYTE MemGetCxRomPeripheral()
{
	return pCxRomPeripheral;
}

//===========================================================================

// Post:
// . true:  code memory
// . false: I/O memory or floating bus
bool MemIsAddrCodeMemory(const USHORT addr)
{
	if (addr < 0xC000 || addr > FIRMWARE_EXPANSION_END)	// Assume all A][ types have at least 48K
		return true;

	if (addr < APPLE_SLOT_BEGIN)		// [$C000..C0FF]
		return false;

	if (!IS_APPLE2 && SW_INTCXROM)		// [$C100..C7FF] //e or Enhanced //e internal ROM
		return true;

	if (!IS_APPLE2 && !SW_SLOTC3ROM && (addr >> 8) == 0xC3)	// [$C300..C3FF] //e or Enhanced //e internal ROM
		return true;

	if (addr <= APPLE_SLOT_END)			// [$C100..C7FF]
	{
		const UINT uSlot = (addr >> 8) & 0x7;
		return IsCardInSlot(uSlot);
	}

	// [$C800..CFFF]

	if (g_eExpansionRomType == eExpRomNull)
	{
		if (IO_SELECT || INTC8ROM)	// Was at $Csxx and now in [$C800..$CFFF]
			return true;

		return false;
	}

	return true;
}

//===========================================================================

void MemInitialize()
{
	// ALLOCATE MEMORY FOR THE APPLE MEMORY IMAGE AND ASSOCIATED DATA STRUCTURES
	memaux   = ALIGNED_ALLOC(_6502_MEM_LEN);
	memmain  = ALIGNED_ALLOC(_6502_MEM_LEN);
	memimage = ALIGNED_ALLOC(_6502_MEM_LEN);

	memdirty = new BYTE[0x100];
	memrom   = new BYTE[0x3000 * MaxRomPages];

	pCxRomInternal		= new BYTE[CxRomSize];
	pCxRomPeripheral	= new BYTE[CxRomSize];

	if (!memaux || !memdirty || !memimage || !memmain || !memrom || !pCxRomInternal || !pCxRomPeripheral)
	{
		MessageBox(
			GetDesktopWindow(),
			TEXT("The emulator was unable to allocate the memory it ")
			TEXT("requires.  Further execution is not possible."),
			g_pAppTitle.c_str(),
			MB_ICONSTOP | MB_SETFOREGROUND);
		ExitProcess(1);
	}

	RWpages[0] = memaux;

	SetExpansionMemTypeDefault();

#ifdef RAMWORKS
	if (GetCardMgr().QueryAux() == CT_RamWorksIII)
	{
		// allocate memory for RAMWorks III - up to 8MB
		g_uActiveBank = 0;

		UINT i = 1;
		while ((i < g_uMaxExPages) && (RWpages[i] = ALIGNED_ALLOC(_6502_MEM_LEN)))
			i++;
		while (i < kMaxExMemoryBanks)
			RWpages[i++] = NULL;
	}
#endif

	//

	CreateLanguageCard();

	MemInitializeROM();
	MemInitializeCustomROM();
	MemInitializeCustomF8ROM();
	MemInitializeIO();
	MemReset();
}

void MemInitializeROM(void)
{
	// READ THE APPLE FIRMWARE ROMS INTO THE ROM IMAGE
	UINT ROM_SIZE = 0;
	HRSRC hResInfo = NULL;
	switch (g_Apple2Type)
	{
	case A2TYPE_APPLE2:         hResInfo = FindResource(NULL, MAKEINTRESOURCE(IDR_APPLE2_ROM          ), "ROM"); ROM_SIZE = Apple2RomSize ; break;
	case A2TYPE_APPLE2PLUS:     hResInfo = FindResource(NULL, MAKEINTRESOURCE(IDR_APPLE2_PLUS_ROM     ), "ROM"); ROM_SIZE = Apple2RomSize ; break;
	case A2TYPE_APPLE2JPLUS:    hResInfo = FindResource(NULL, MAKEINTRESOURCE(IDR_APPLE2_JPLUS_ROM    ), "ROM"); ROM_SIZE = Apple2RomSize ; break;
	case A2TYPE_APPLE2E:        hResInfo = FindResource(NULL, MAKEINTRESOURCE(IDR_APPLE2E_ROM         ), "ROM"); ROM_SIZE = Apple2eRomSize; break;
	case A2TYPE_APPLE2EENHANCED:hResInfo = FindResource(NULL, MAKEINTRESOURCE(IDR_APPLE2E_ENHANCED_ROM), "ROM"); ROM_SIZE = Apple2eRomSize; break;
	case A2TYPE_PRAVETS82:      hResInfo = FindResource(NULL, MAKEINTRESOURCE(IDR_PRAVETS_82_ROM      ), "ROM"); ROM_SIZE = Apple2RomSize ; break;
	case A2TYPE_PRAVETS8M:      hResInfo = FindResource(NULL, MAKEINTRESOURCE(IDR_PRAVETS_8M_ROM      ), "ROM"); ROM_SIZE = Apple2RomSize ; break;
	case A2TYPE_PRAVETS8A:      hResInfo = FindResource(NULL, MAKEINTRESOURCE(IDR_PRAVETS_8C_ROM      ), "ROM"); ROM_SIZE = Apple2eRomSize; break;
	case A2TYPE_TK30002E:       hResInfo = FindResource(NULL, MAKEINTRESOURCE(IDR_TK3000_2E_ROM       ), "ROM"); ROM_SIZE = Apple2eRomSize; break;
	case A2TYPE_BASE64A:        hResInfo = FindResource(NULL, MAKEINTRESOURCE(IDR_BASE_64A_ROM        ), "ROM"); ROM_SIZE = Base64ARomSize; break;
	}

	if (hResInfo == NULL)
	{
		TCHAR sRomFileName[ MAX_PATH ];
		switch (g_Apple2Type)
		{
		case A2TYPE_APPLE2:         _tcscpy(sRomFileName, TEXT("APPLE2.ROM"          )); break;
		case A2TYPE_APPLE2PLUS:     _tcscpy(sRomFileName, TEXT("APPLE2_PLUS.ROM"     )); break;
		case A2TYPE_APPLE2JPLUS:    _tcscpy(sRomFileName, TEXT("APPLE2_JPLUS.ROM"    )); break;
		case A2TYPE_APPLE2E:        _tcscpy(sRomFileName, TEXT("APPLE2E.ROM"         )); break;
		case A2TYPE_APPLE2EENHANCED:_tcscpy(sRomFileName, TEXT("APPLE2E_ENHANCED.ROM")); break;
		case A2TYPE_PRAVETS82:      _tcscpy(sRomFileName, TEXT("PRAVETS82.ROM"       )); break;
		case A2TYPE_PRAVETS8M:      _tcscpy(sRomFileName, TEXT("PRAVETS8M.ROM"       )); break;
		case A2TYPE_PRAVETS8A:      _tcscpy(sRomFileName, TEXT("PRAVETS8C.ROM"       )); break;
		case A2TYPE_TK30002E:       _tcscpy(sRomFileName, TEXT("TK3000e.ROM"         )); break;
		case A2TYPE_BASE64A:        _tcscpy(sRomFileName, TEXT("BASE64A.ROM"         )); break;
		default:
			{
				_tcscpy(sRomFileName, TEXT("Unknown type!"));
				GetPropertySheet().ConfigSaveApple2Type(A2TYPE_APPLE2EENHANCED);
			}
		}

		TCHAR sText[MAX_PATH];
		StringCbPrintf(sText, sizeof(sText), TEXT("Unable to open the required firmware ROM data file.\n\nFile: %s"), sRomFileName);

		LogFileOutput("%s\n", sText);

		MessageBox(
			GetDesktopWindow(),
			sText,
			g_pAppTitle.c_str(),
			MB_ICONSTOP | MB_SETFOREGROUND);

		ExitProcess(1);
	}

	DWORD dwResSize = SizeofResource(NULL, hResInfo);
	if(dwResSize != ROM_SIZE)
		return;

	HGLOBAL hResData = LoadResource(NULL, hResInfo);
	if(hResData == NULL)
		return;

	BYTE* pData = (BYTE*) LockResource(hResData);	// NB. Don't need to unlock resource
	if (pData == NULL)
		return;

	memset(pCxRomInternal,0,CxRomSize);
	memset(pCxRomPeripheral,0,CxRomSize);

	if (ROM_SIZE == Apple2eRomSize)
	{
		memcpy(pCxRomInternal, pData, CxRomSize);
		pData += CxRomSize;
		ROM_SIZE -= CxRomSize;
	}

	memrompages = MAX(MaxRomPages, ROM_SIZE / Apple2RomSize);
	_ASSERT(ROM_SIZE % Apple2RomSize == 0);
	memcpy(memrom, pData, ROM_SIZE);			// ROM at $D000...$FFFF, one or several pages
}

void MemInitializeCustomF8ROM(void)
{
	const UINT F8RomSize = 0x800;
	const UINT F8RomOffset = Apple2RomSize-F8RomSize;

	if (IsApple2Original(GetApple2Type()) && GetCardMgr().QuerySlot(SLOT0) == CT_LanguageCard)
	{
		try
		{
			HRSRC hResInfo = FindResource(NULL, MAKEINTRESOURCE(IDR_APPLE2_PLUS_ROM), "ROM");
			if (hResInfo == NULL)
				throw false;

			DWORD dwResSize = SizeofResource(NULL, hResInfo);
			if(dwResSize != Apple2RomSize)
				throw false;

			HGLOBAL hResData = LoadResource(NULL, hResInfo);
			if(hResData == NULL)
				throw false;

			BYTE* pData = (BYTE*) LockResource(hResData);	// NB. Don't need to unlock resource
			if (pData == NULL)
				throw false;

			memcpy(memrom+F8RomOffset, pData+F8RomOffset, F8RomSize);
		}
		catch (bool)
		{
			MessageBox( GetFrame().g_hFrameWindow, "Failed to read F8 (auto-start) ROM for language card in original Apple][", TEXT("AppleWin Error"), MB_OK );
		}
	}

	if (g_hCustomRomF8 != INVALID_HANDLE_VALUE)
	{
		std::vector<BYTE> oldRom(memrom, memrom+Apple2RomSize);	// range ctor: [first,last)

		SetFilePointer(g_hCustomRomF8, 0, NULL, FILE_BEGIN);
		DWORD uNumBytesRead;
		BOOL bRes = ReadFile(g_hCustomRomF8, memrom+F8RomOffset, F8RomSize, &uNumBytesRead, NULL);
		if (uNumBytesRead != F8RomSize)
		{
			memcpy(memrom, &oldRom[0], Apple2RomSize);	// ROM at $D000...$FFFF
			bRes = FALSE;
		}

		// NB. If succeeded, then keep g_hCustomRomF8 handle open - so that any next restart can load it again

		if (!bRes)
		{
			MessageBox( GetFrame().g_hFrameWindow, "Failed to read custom F8 rom", TEXT("AppleWin Error"), MB_OK );
			CloseHandle(g_hCustomRomF8);
			g_hCustomRomF8 = INVALID_HANDLE_VALUE;
			// Failed, so use default rom...
		}
	}

	if (GetPropertySheet().GetTheFreezesF8Rom() && IS_APPLE2)
	{
		HGLOBAL hResData = NULL;
		BYTE* pData = NULL;

		HRSRC hResInfo = FindResource(NULL, MAKEINTRESOURCE(IDR_FREEZES_F8_ROM), "ROM");

		if (hResInfo && (SizeofResource(NULL, hResInfo) == 0x800) && (hResData = LoadResource(NULL, hResInfo)) && (pData = (BYTE*) LockResource(hResData)))
		{
			memcpy(memrom+Apple2RomSize-F8RomSize, pData, F8RomSize);
		}
	}
}

void MemInitializeCustomROM(void)
{
	if (g_hCustomRom == INVALID_HANDLE_VALUE)
		return;

	SetFilePointer(g_hCustomRom, 0, NULL, FILE_BEGIN);
	DWORD uNumBytesRead;
	BOOL bRes = TRUE;

	if (GetFileSize(g_hCustomRom, NULL) == Apple2eRomSize)
	{
		std::vector<BYTE> oldRomC0(pCxRomInternal, pCxRomInternal+CxRomSize);	// range ctor: [first,last)
		bRes = ReadFile(g_hCustomRom, pCxRomInternal, CxRomSize, &uNumBytesRead, NULL);
		if (uNumBytesRead != CxRomSize)
		{
			memcpy(pCxRomInternal, &oldRomC0[0], CxRomSize);	// ROM at $C000...$CFFF
			bRes = FALSE;
		}
	}

	if (bRes)
	{
		std::vector<BYTE> oldRom(memrom, memrom+Apple2RomSize);	// range ctor: [first,last)
		bRes = ReadFile(g_hCustomRom, memrom, Apple2RomSize, &uNumBytesRead, NULL);
		if (uNumBytesRead != Apple2RomSize)
		{
			memcpy(memrom, &oldRom[0], Apple2RomSize);	// ROM at $D000...$FFFF
			bRes = FALSE;
		}
	}

	// NB. If succeeded, then keep g_hCustomRom handle open - so that any next restart can load it again

	if (!bRes)
	{
		MessageBox( GetFrame().g_hFrameWindow, "Failed to read custom rom", TEXT("AppleWin Error"), MB_OK );
		CloseHandle(g_hCustomRom);
		g_hCustomRom = INVALID_HANDLE_VALUE;
		// Failed, so use default rom...
	}
}

// Called by:
// . MemInitialize()
// . Snapshot_LoadState_v2()
//
// Since called by LoadState(), then this must not init any cards
// - it should only init the card I/O hooks
void MemInitializeIO(void)
{
	InitIoHandlers();

	if (g_pLanguageCard)
		g_pLanguageCard->InitializeIO();
	else
		RegisterIoHandler(LanguageCardUnit::kSlot0, IO_Null, IO_Null, NULL, NULL, NULL, NULL);

	if (GetCardMgr().QuerySlot(SLOT1) == CT_GenericPrinter)
		PrintLoadRom(pCxRomPeripheral, SLOT1);				// $C100 : Parallel printer f/w

	if (GetCardMgr().QuerySlot(SLOT2) == CT_SSC)
		dynamic_cast<CSuperSerialCard&>(GetCardMgr().GetRef(SLOT2)).CommInitialize(pCxRomPeripheral, SLOT2);	// $C200 : SSC

	if (GetCardMgr().QuerySlot(SLOT3) == CT_Uthernet)
	{
		// Slot 3 contains the Uthernet card (which can coexist with an 80-col+Ram card in AUX slot)
		// . Uthernet card has no ROM and only IO mapped at $C0Bx
		// NB. I/O handlers setup via tfe_init() & update_tfe_interface()
	}

	// Apple//e: Auxilary slot contains Extended 80 Column card or RamWorksIII card

	if (GetCardMgr().QuerySlot(SLOT4) == CT_MouseInterface)
	{
		dynamic_cast<CMouseInterface&>(GetCardMgr().GetRef(SLOT4)).Initialize(pCxRomPeripheral, SLOT4);	// $C400 : Mouse f/w
	}
	else if (GetCardMgr().QuerySlot(SLOT4) == CT_MockingboardC || GetCardMgr().QuerySlot(SLOT4) == CT_Phasor)
	{
		MB_InitializeIO(pCxRomPeripheral, SLOT4, SLOT5);
	}
	else if (GetCardMgr().QuerySlot(SLOT4) == CT_Z80)
	{
		ConfigureSoftcard(pCxRomPeripheral, SLOT4);		// $C400 : Z80 card
	}
//	else if (GetCardMgr().QuerySlot(SLOT4) == CT_GenericClock)
//	{
//		LoadRom_Clock_Generic(pCxRomPeripheral, SLOT4);
//	}

	if (GetCardMgr().QuerySlot(SLOT5) == CT_Z80)
	{
		ConfigureSoftcard(pCxRomPeripheral, SLOT5);		// $C500 : Z80 card
	}
	else if (GetCardMgr().QuerySlot(SLOT5) == CT_SAM)
	{
		ConfigureSAM(pCxRomPeripheral, SLOT5);			// $C500 : Z80 card
	}
	else if (GetCardMgr().QuerySlot(SLOT5) == CT_Disk2)
	{
		dynamic_cast<Disk2InterfaceCard&>(GetCardMgr().GetRef(SLOT5)).Initialize(pCxRomPeripheral, SLOT5);	// $C500 : Disk][ card
	}

	if (GetCardMgr().QuerySlot(SLOT6) == CT_Disk2)
		dynamic_cast<Disk2InterfaceCard&>(GetCardMgr().GetRef(SLOT6)).Initialize(pCxRomPeripheral, SLOT6);	// $C600 : Disk][ card

	if (GetCardMgr().QuerySlot(SLOT7) == CT_GenericHDD)
		HD_Load_Rom(pCxRomPeripheral, SLOT7);			// $C700 : HDD f/w
}

// Called by:
// . Snapshot_LoadState_v2()
void MemInitializeCardSlotAndExpansionRomFromSnapshot(void)
{
	// Remove all the cards' ROMs at $Csnn if internal ROM is enabled
	if (IsAppleIIeOrAbove(GetApple2Type()) && SW_INTCXROM)
		IoHandlerCardsOut();

	// Potentially init a card's expansion ROM
	const UINT uSlot = g_uPeripheralRomSlot;

	if (ExpansionRom[uSlot] == NULL)
		return;

	_ASSERT(g_eExpansionRomType == eExpRomPeripheral);

	memcpy(pCxRomPeripheral+0x800, ExpansionRom[uSlot], FIRMWARE_EXPANSION_SIZE);
	// NB. Copied to /mem/ by UpdatePaging(TRUE)
}

inline DWORD getRandomTime()
{
	return rand() ^ timeGetTime(); // We can't use g_nCumulativeCycles as it will be zero on a fresh execution.
}

//===========================================================================

// Called by:
// . MemInitialize()		eg. on AppleWin start & restart (eg. h/w config changes)
// . ResetMachineState()	eg. Power-cycle ('Apple-Go' button)
// . Snapshot_LoadState_v2()
void MemReset()
{
	// INITIALIZE THE PAGING TABLES
	memset(memshadow, 0, 256*sizeof(LPBYTE));
	memset(memwrite , 0, 256*sizeof(LPBYTE));

	// INITIALIZE THE RAM IMAGES
	memset(memaux , 0, 0x10000);
	memset(memmain, 0, 0x10000);

	// Init the I/O ROM vars
	IO_SELECT = 0;
	INTC8ROM = false;
	g_eExpansionRomType = eExpRomNull;
	g_uPeripheralRomSlot = 0;

	memset(memdirty, 0, 0x100);

	//

	int iByte;

	// Memory is pseudo-initialized across various models of Apple ][ //e //c
	// We chose a random one for nostalgia's sake
	// To inspect:
	//   F2. Ctrl-F2. CALL-151, C050 C053 C057
	// OR
	//   F2, Ctrl-F2, F7, HGR
	DWORD randTime = getRandomTime();
	MemoryInitPattern_e eMemoryInitPattern = static_cast<MemoryInitPattern_e>(g_nMemoryClearType);

	if (g_nMemoryClearType < 0)	// random
	{
		eMemoryInitPattern = static_cast<MemoryInitPattern_e>( randTime % NUM_MIP );

		// Don't use unless manually specified as a
		// few badly written programs will not work correctly
		// due to buffer overflows or not initializig memory before using.
		if( eMemoryInitPattern == MIP_PAGE_ADDRESS_LOW )
			eMemoryInitPattern = MIP_FF_FF_00_00;
	}

	switch( eMemoryInitPattern )
	{
		case MIP_FF_FF_00_00:
			for( iByte = 0x0000; iByte < 0xC000; iByte += 4 ) // NB. ODD 16-bit words are zero'd above...
			{
				memmain[ iByte+0 ] = 0xFF;
				memmain[ iByte+1 ] = 0xFF;
			}

			// Exceptions: xx28 xx29 xx68 xx69 Apple //e
			for( iByte = 0x0000; iByte < 0xC000; iByte += 512 )
			{
				randTime = getRandomTime();
				memmain[ iByte + 0x28 ] = (randTime >>  0) & 0xFF;
				memmain[ iByte + 0x29 ] = (randTime >>  8) & 0xFF;
				randTime = getRandomTime();
				memmain[ iByte + 0x68 ] = (randTime >>  0) & 0xFF;
				memmain[ iByte + 0x69 ] = (randTime >>  8) & 0xFF;
			}
			break;
		
		case MIP_FF_00_FULL_PAGE:
			// https://github.com/AppleWin/AppleWin/issues/225
			// AppleWin 1.25 RC2 fails to boot Castle Wolfenstein #225
			// This causes Castle Wolfenstein to not boot properly 100% with an error:
			// ?OVERFLOW ERROR IN 10
			// http://mirrors.apple2.org.za/ftp.apple.asimov.net/images/games/action/wolfenstein/castle_wolfenstein-fixed.dsk
			for( iByte = 0x0000; iByte < 0xC000; iByte += 512 )
			{
				memset( &memmain[ iByte ], 0xFF, 256 );

				// Exceptions: xx28: 00  xx68:00  Apple //e Platinum NTSC
				memmain[ iByte + 0x28 ] = 0x00;
				memmain[ iByte + 0x68 ] = 0x00;
			}
			break;

		case MIP_00_FF_HALF_PAGE: 
			for( iByte = 0x0080; iByte < 0xC000; iByte += 256 ) // NB. start = 0x80, delta = 0x100 !
				memset( &memmain[ iByte ], 0xFF, 128 );
			break;

		case MIP_FF_00_HALF_PAGE:
			for( iByte = 0x0000; iByte < 0xC000; iByte += 256 )
				memset( &memmain[ iByte ], 0xFF, 128 );
			break;

		case MIP_RANDOM:
			unsigned char random[ 256 ];
			for( iByte = 0x0000; iByte < 0xC000; iByte += 256 )
			{
				for( int i = 0; i < 256; i++ )
				{
					randTime = getRandomTime();
					random[ (i+0) & 0xFF ] = (randTime >>  0) & 0xFF;
					random[ (i+1) & 0xFF ] = (randTime >> 11) & 0xFF;
				}

				memcpy( &memmain[ iByte ], random, 256 );
			}
			break;

		case MIP_PAGE_ADDRESS_LOW:
			for( iByte = 0x0000; iByte < 0xC000; iByte++ )
				memmain[ iByte ] = iByte & 0xFF;
			break;

		case MIP_PAGE_ADDRESS_HIGH:
			for( iByte = 0x0000; iByte < 0xC000; iByte += 256 )
				memset( &memmain[ iByte ], (iByte >> 8), 256 );
			break;

		default: // MIP_ZERO -- nothing to do
			break;
	}

	// https://github.com/AppleWin/AppleWin/issues/206
	// Work-around for a cold-booting bug in "Pooyan" which expects RNDL and RNDH to be non-zero.
	randTime = getRandomTime();
	memmain[ 0x4E ] = 0x20 | (randTime >> 0) & 0xFF;
	memmain[ 0x4F ] = 0x20 | (randTime >> 8) & 0xFF;

	// https://github.com/AppleWin/AppleWin/issues/222
	// MIP_PAGE_ADDRESS_LOW breaks a few badly written programs!
	// "Beautiful Boot by Mini Appler" reads past $61FF into $6200
	// - "BeachParty-PoacherWars-DaytonDinger-BombsAway.dsk"
	// - "Dung Beetles, Ms. PacMan, Pooyan, Star Cruiser, Star Thief, Invas. Force.dsk"
	memmain[ 0x620B ] = 0x0;

	// https://github.com/AppleWin/AppleWin/issues/222
	// MIP_PAGE_ADDRESS_LOW
	// "Copy II+ v5.0.dsk"
	// There is a strange memory checker from $1B03 .. $1C25
	// Stuck in loop at $1BC2: JSR $F88E INSDS2 before crashing to $0: 00 BRK
	memmain[ 0xBFFD ] = 0;
	memmain[ 0xBFFE ] = 0;
	memmain[ 0xBFFF ] = 0;

	// SET UP THE MEMORY IMAGE
	mem = memimage;

	// INITIALIZE PAGING, FILLING IN THE 64K MEMORY IMAGE
	ResetPaging(TRUE);		// Initialize=1, init memmode
	MemAnnunciatorReset();

	// INITIALIZE & RESET THE CPU
	// . Do this after ROM has been copied back to mem[], so that PC is correctly init'ed from 6502's reset vector
	CpuInitialize();
	//Sets Caps Lock = false (Pravets 8A/C only)

	z80_reset();	// NB. Also called above in CpuInitialize()

	if (g_NoSlotClock)
		g_NoSlotClock->Reset();	// NB. Power-cycle, but not RESET signal
}

//===========================================================================

BYTE MemReadFloatingBus(const ULONG uExecutedCycles)
{
	return mem[ NTSC_VideoGetScannerAddress(uExecutedCycles) ];		// OK: This does the 2-cycle adjust for ANSI STORY (End Credits)
}

//===========================================================================

BYTE MemReadFloatingBus(const BYTE highbit, const ULONG uExecutedCycles)
{
	BYTE r = MemReadFloatingBus(uExecutedCycles);
	return (r & ~0x80) | (highbit ? 0x80 : 0);
}

//===========================================================================

//#define DEBUG_FLIP_TIMINGS

#if defined(_DEBUG) && defined(DEBUG_FLIP_TIMINGS)
static void DebugFlip(WORD address, ULONG nExecutedCycles)
{
	static unsigned __int64 uLastFlipCycle = 0;
	static unsigned int uLastPage = -1;

	if (address != 0x54 && address != 0x55)
		return;

	const unsigned int uNewPage = address & 1;
	if (uLastPage == uNewPage)
		return;
	uLastPage = uNewPage;

	CpuCalcCycles(nExecutedCycles);	// Update g_nCumulativeCycles

	const unsigned int uCyclesBetweenFlips = (unsigned int) (uLastFlipCycle ? g_nCumulativeCycles - uLastFlipCycle : 0);
	uLastFlipCycle = g_nCumulativeCycles;

	if (!uCyclesBetweenFlips)
		return;					// 1st time in func

	const double fFreq = CLK_6502 / (double)uCyclesBetweenFlips;

	char szStr[100];
	sprintf(szStr, "Cycles between flips = %d (%f Hz)\n", uCyclesBetweenFlips, fFreq);
	OutputDebugString(szStr);
}
#endif

BYTE __stdcall MemSetPaging(WORD programcounter, WORD address, BYTE write, BYTE value, ULONG nExecutedCycles)
{
	address &= 0xFF;
	DWORD lastmemmode = memmode;
#if defined(_DEBUG) && defined(DEBUG_FLIP_TIMINGS)
	DebugFlip(address, nExecutedCycles);
#endif

	// DETERMINE THE NEW MEMORY PAGING MODE.
	if (!IS_APPLE2)
	{
		switch (address)
		{
			case 0x00: SetMemMode(memmode & ~MF_80STORE);    break;
			case 0x01: SetMemMode(memmode |  MF_80STORE);    break;
			case 0x02: SetMemMode(memmode & ~MF_AUXREAD);    break;
			case 0x03: SetMemMode(memmode |  MF_AUXREAD);    break;
			case 0x04: SetMemMode(memmode & ~MF_AUXWRITE);   break;
			case 0x05: SetMemMode(memmode |  MF_AUXWRITE);   break;
			case 0x06: SetMemMode(memmode & ~MF_INTCXROM);   break;
			case 0x07: SetMemMode(memmode |  MF_INTCXROM);   break;
			case 0x08: SetMemMode(memmode & ~MF_ALTZP);      break;
			case 0x09: SetMemMode(memmode |  MF_ALTZP);      break;
			case 0x0A: SetMemMode(memmode & ~MF_SLOTC3ROM);  break;
			case 0x0B: SetMemMode(memmode |  MF_SLOTC3ROM);  break;
			case 0x54: SetMemMode(memmode & ~MF_PAGE2);      break;
			case 0x55: SetMemMode(memmode |  MF_PAGE2);      break;
			case 0x56: SetMemMode(memmode & ~MF_HIRES);      break;
			case 0x57: SetMemMode(memmode |  MF_HIRES);      break;
#ifdef RAMWORKS
			case 0x71: // extended memory aux page number
			case 0x73: // Ramworks III set aux page number
				if ((value < g_uMaxExPages) && RWpages[value])
				{
					g_uActiveBank = value;
					memaux = RWpages[g_uActiveBank];
					UpdatePaging(FALSE);	// Initialize=FALSE
				}
				break;
#endif
		}
	}

	if (IsCopamBase64A(GetApple2Type()))
	{
		switch (address)
		{
			case 0x58: SetMemMode(memmode & ~MF_ALTROM0);  break;
			case 0x59: SetMemMode(memmode |  MF_ALTROM0);  break;
			case 0x5A: SetMemMode(memmode & ~MF_ALTROM1);  break;
			case 0x5B: SetMemMode(memmode |  MF_ALTROM1);  break;
		}
	}

	if (MemOptimizeForModeChanging(programcounter, address))
		return write ? 0 : MemReadFloatingBus(nExecutedCycles);

	// IF THE MEMORY PAGING MODE HAS CHANGED, UPDATE OUR MEMORY IMAGES AND
	// WRITE TABLES.
	if ((lastmemmode != memmode) || modechanging)
	{
		// NB. Must check MF_SLOTC3ROM too, as IoHandlerCardsIn() depends on both MF_INTCXROM|MF_SLOTC3ROM
		if ((lastmemmode & (MF_INTCXROM|MF_SLOTC3ROM)) != (memmode & (MF_INTCXROM|MF_SLOTC3ROM)))
		{
			if (!SW_INTCXROM)
			{
				if (!INTC8ROM)	// GH#423
				{
					// Disable Internal ROM
					// . Similar to $CFFF access
					// . None of the peripheral cards can be driving the bus - so use the null ROM
					memset(pCxRomPeripheral+0x800, 0, FIRMWARE_EXPANSION_SIZE);
					memset(mem+FIRMWARE_EXPANSION_BEGIN, 0, FIRMWARE_EXPANSION_SIZE);
					g_eExpansionRomType = eExpRomNull;
					g_uPeripheralRomSlot = 0;
				}
				IoHandlerCardsIn();
			}
			else
			{
				// Enable Internal ROM
				memcpy(mem+0xC800, pCxRomInternal+0x800, FIRMWARE_EXPANSION_SIZE);
				g_eExpansionRomType = eExpRomInternal;
				g_uPeripheralRomSlot = 0;
				IoHandlerCardsOut();
			}
		}

		UpdatePaging(0);	// Initialize=0
	}

	// Replicate 80STORE, PAGE2 and HIRES to video sub-system
	if ((address <= 1) || ((address >= 0x54) && (address <= 0x57)))
		return GetVideo().VideoSetMode(programcounter,address,write,value,nExecutedCycles);

	return write ? 0 : MemReadFloatingBus(nExecutedCycles);
}

//===========================================================================

bool MemOptimizeForModeChanging(WORD programcounter, WORD address)
{
	if (IsAppleIIeOrAbove(GetApple2Type()))
	{
		// IF THE EMULATED PROGRAM HAS JUST UPDATED THE MEMORY WRITE MODE AND IS
		// ABOUT TO UPDATE THE MEMORY READ MODE, HOLD OFF ON ANY PROCESSING UNTIL
		// IT DOES SO.
		//
		// NB. A 6502 interrupt occurring between these memory write & read updates could lead to incorrect behaviour.
		// - although any data-race is probably a bug in the 6502 code too.
		if ((address >= 4) && (address <= 5) &&									// Now:  RAMWRTOFF or RAMWRTON
			((*(LPDWORD)(mem+programcounter) & 0x00FFFEFF) == 0x00C0028D))		// Next: STA $C002(RAMRDOFF) or STA $C003(RAMRDON)
		{
				modechanging = 1;
				return true;
		}

		if ((address >= 0x80) && (address <= 0x8F) && (programcounter < 0xC000) &&	// Now: LC
			(((*(LPDWORD)(mem+programcounter) & 0x00FFFEFF) == 0x00C0048D) ||		// Next: STA $C004(RAMWRTOFF) or STA $C005(RAMWRTON)
			 ((*(LPDWORD)(mem+programcounter) & 0x00FFFEFF) == 0x00C0028D)))		//    or STA $C002(RAMRDOFF)  or STA $C003(RAMRDON)
		{
				modechanging = 1;
				return true;
		}
	}

	return false;
}

//===========================================================================

LPVOID MemGetSlotParameters(UINT uSlot)
{
	_ASSERT(uSlot < NUM_SLOTS);
	return SlotParameters[uSlot];
}

//===========================================================================

void MemAnnunciatorReset(void)
{
	for (UINT i=0; i<kNumAnnunciators; i++)
		g_Annunciator[i] = 0;

	if (IsCopamBase64A(GetApple2Type()))
	{
		SetMemMode(memmode & ~(MF_ALTROM0|MF_ALTROM1));
		UpdatePaging(FALSE);	// Initialize=FALSE
	}
}

bool MemGetAnnunciator(UINT annunciator)
{
	return g_Annunciator[annunciator];
}

//===========================================================================

bool MemHasNoSlotClock(void)
{
	return g_NoSlotClock != NULL;
}

void MemInsertNoSlotClock(void)
{
	if (!MemHasNoSlotClock())
		g_NoSlotClock = new CNoSlotClock;
	g_NoSlotClock->Reset();
}

void MemRemoveNoSlotClock(void)
{
	delete g_NoSlotClock;
	g_NoSlotClock = NULL;
}

//===========================================================================

// NB. Don't need to save 'modechanging', as this is just an optimisation to save calling UpdatePaging() twice.
// . If we were to save the state when 'modechanging' is set, then on restoring the state, the 6502 code will immediately update the read memory mode.
// . This will work correctly.

#define SS_YAML_KEY_MEMORYMODE "Memory Mode"
#define SS_YAML_KEY_LASTRAMWRITE "Last RAM Write"
#define SS_YAML_KEY_IOSELECT "IO_SELECT"
#define SS_YAML_KEY_IOSELECT_INT "IO_SELECT_InternalROM"	// INTC8ROM
#define SS_YAML_KEY_EXPANSIONROMTYPE "Expansion ROM Type"
#define SS_YAML_KEY_PERIPHERALROMSLOT "Peripheral ROM Slot"
#define SS_YAML_KEY_ANNUNCIATOR "Annunciator"

//

// Unit version history:
// 2: Added version field to card's state
static const UINT kUNIT_AUXSLOT_VER = 2;

// Unit version history:
// 2: Added: RGB card state
// 3: Extended: RGB card state ('80COL changed')
static const UINT kUNIT_CARD_VER = 3;

#define SS_YAML_VALUE_CARD_80COL "80 Column"
#define SS_YAML_VALUE_CARD_EXTENDED80COL "Extended 80 Column"
#define SS_YAML_VALUE_CARD_RAMWORKSIII "RamWorksIII"

#define SS_YAML_KEY_NUMAUXBANKS "Num Aux Banks"
#define SS_YAML_KEY_ACTIVEAUXBANK "Active Aux Bank"

static std::string MemGetSnapshotStructName(void)
{
	static const std::string name("Memory");
	return name;
}

std::string MemGetSnapshotUnitAuxSlotName(void)
{
	static const std::string name("Auxiliary Slot");
	return name;
}

static std::string MemGetSnapshotMainMemStructName(void)
{
	static const std::string name("Main Memory");
	return name;
}

static std::string MemGetSnapshotAuxMemStructName(void)
{
	static const std::string name("Auxiliary Memory Bank");
	return name;
}

static void MemSaveSnapshotMemory(YamlSaveHelper& yamlSaveHelper, bool bIsMainMem, UINT bank=0, UINT size=64*1024)
{
	LPBYTE pMemBase = MemGetBankPtr(bank);

	if (bIsMainMem)
	{
		YamlSaveHelper::Label state(yamlSaveHelper, "%s:\n", MemGetSnapshotMainMemStructName().c_str());
		yamlSaveHelper.SaveMemory(pMemBase, size);
	}
	else
	{
		YamlSaveHelper::Label state(yamlSaveHelper, "%s%02X:\n", MemGetSnapshotAuxMemStructName().c_str(), bank-1);
		yamlSaveHelper.SaveMemory(pMemBase, size);
	}
}

void MemSaveSnapshot(YamlSaveHelper& yamlSaveHelper)
{
	// Scope so that "Memory" & "Main Memory" are at same indent level
	{
		YamlSaveHelper::Label state(yamlSaveHelper, "%s:\n", MemGetSnapshotStructName().c_str());
		DWORD saveMemMode = memmode;
		if (IsApple2PlusOrClone(GetApple2Type()))
			saveMemMode &= ~MF_LANGCARD_MASK;		// For II,II+: clear LC bits - set later by slot-0 LC or Saturn
		yamlSaveHelper.SaveHexUint32(SS_YAML_KEY_MEMORYMODE, saveMemMode);
		if (!IsApple2PlusOrClone(GetApple2Type()))	// NB. This is set later for II,II+ by slot-0 LC or Saturn
			yamlSaveHelper.SaveUint(SS_YAML_KEY_LASTRAMWRITE, GetLastRamWrite() ? 1 : 0);
		yamlSaveHelper.SaveHexUint8(SS_YAML_KEY_IOSELECT, IO_SELECT);
		yamlSaveHelper.SaveHexUint8(SS_YAML_KEY_IOSELECT_INT, INTC8ROM ? 1 : 0);
		yamlSaveHelper.SaveUint(SS_YAML_KEY_EXPANSIONROMTYPE, (UINT) g_eExpansionRomType);
		yamlSaveHelper.SaveUint(SS_YAML_KEY_PERIPHERALROMSLOT, g_uPeripheralRomSlot);

		for (UINT i=0; i<kNumAnnunciators; i++)
		{
			std::string annunciator = SS_YAML_KEY_ANNUNCIATOR + std::string(1,'0'+i);
			yamlSaveHelper.SaveBool(annunciator.c_str(), g_Annunciator[i]);
		}
	}

	if (IsApple2PlusOrClone(GetApple2Type()))
		MemSaveSnapshotMemory(yamlSaveHelper, true, 0, 48*1024);	// NB. Language Card/Saturn provides the remaining 16K (or multiple) bank(s)
	else
		MemSaveSnapshotMemory(yamlSaveHelper, true);
}

bool MemLoadSnapshot(YamlLoadHelper& yamlLoadHelper, UINT unitVersion)
{
	if (!yamlLoadHelper.GetSubMap(MemGetSnapshotStructName()))
		return false;

	// Create default LC type for AppleII machine (do prior to loading saved LC state)
	ResetDefaultMachineMemTypes();
	if (unitVersion == 1)
		g_MemTypeAppleII = CT_LanguageCard;	// version=1: original Apple II always has a LC
	else
		g_MemTypeAppleIIPlus = CT_Empty;	// version=2+: Apple II/II+ initially start with slot-0 empty
	SetExpansionMemTypeDefault();
	CreateLanguageCard();	// Create default LC now for: (a) //e which has no slot-0 LC (so this is final)
							//							  (b) II/II+ which get re-created later if slot-0 has a card

	//

	IO_SELECT = (BYTE) yamlLoadHelper.LoadUint(SS_YAML_KEY_IOSELECT);
	INTC8ROM = yamlLoadHelper.LoadUint(SS_YAML_KEY_IOSELECT_INT) ? true : false;
	g_eExpansionRomType = (eExpansionRomType) yamlLoadHelper.LoadUint(SS_YAML_KEY_EXPANSIONROMTYPE);
	g_uPeripheralRomSlot = yamlLoadHelper.LoadUint(SS_YAML_KEY_PERIPHERALROMSLOT);

	if (unitVersion == 1)
	{
		SetMemMode( yamlLoadHelper.LoadUint(SS_YAML_KEY_MEMORYMODE) ^ MF_INTCXROM );	// Convert from SLOTCXROM to INTCXROM
		SetLastRamWrite( yamlLoadHelper.LoadUint(SS_YAML_KEY_LASTRAMWRITE) ? TRUE : FALSE );
	}
	else
	{
		UINT uMemMode = yamlLoadHelper.LoadUint(SS_YAML_KEY_MEMORYMODE);
		if (IsApple2PlusOrClone(GetApple2Type()))
			uMemMode &= ~MF_LANGCARD_MASK;	// For II,II+: clear LC bits - set later by slot-0 LC or Saturn
		SetMemMode(uMemMode);

		if (!IsApple2PlusOrClone(GetApple2Type()))
			SetLastRamWrite( yamlLoadHelper.LoadUint(SS_YAML_KEY_LASTRAMWRITE) ? TRUE : FALSE );	// NB. This is set later for II,II+ by slot-0 LC or Saturn
	}

	if (unitVersion >= 3)
	{
		for (UINT i=0; i<kNumAnnunciators; i++)
		{
			std::string annunciator = SS_YAML_KEY_ANNUNCIATOR + std::string(1,'0'+i);
			g_Annunciator[i] = yamlLoadHelper.LoadBool(annunciator.c_str());
		}
	}

	yamlLoadHelper.PopMap();

	//

	if (!yamlLoadHelper.GetSubMap( MemGetSnapshotMainMemStructName() ))
		throw std::string("Card: Expected key: ") + MemGetSnapshotMainMemStructName();

	memset(memmain+0xC000, 0, LanguageCardSlot0::kMemBankSize);	// Clear it, as high 16K may not be in the save-state's "Main Memory" (eg. the case of II+ Saturn replacing //e LC)

	yamlLoadHelper.LoadMemory(memmain, _6502_MEM_LEN);
	if (unitVersion == 1 && IsApple2PlusOrClone(GetApple2Type()))
	{
		// v1 for II/II+ doesn't have a dedicated slot-0 LC, instead the 16K is stored as the top 16K of memmain
		memcpy(g_pMemMainLanguageCard, memmain+0xC000, LanguageCardSlot0::kMemBankSize);
		memset(memmain+0xC000, 0, LanguageCardSlot0::kMemBankSize);
	}
	memset(memdirty, 0, 0x100);

	yamlLoadHelper.PopMap();

	//

	// NB. MemUpdatePaging(TRUE) called at end of Snapshot_LoadState_v2()
	UpdatePaging(1);	// Initialize=1 (Still needed, even with call to MemUpdatePaging() - why?)
						// TC-TODO: At this point, the cards haven't been loaded, so the card's expansion ROM is unknown - so pointless(?) calling this now

	return true;
}

// TODO: Switch from checking 'g_uMaxExPages == n' to using g_SlotAux
void MemSaveSnapshotAux(YamlSaveHelper& yamlSaveHelper)
{
	if (IS_APPLE2)
	{
		return;	// No Aux slot for AppleII
	}

	if (IS_APPLE2C())
	{
		_ASSERT(g_uMaxExPages == 1);
	}

	yamlSaveHelper.UnitHdr(MemGetSnapshotUnitAuxSlotName(), kUNIT_AUXSLOT_VER);

	// Unit state
	{
		YamlSaveHelper::Label unitState(yamlSaveHelper, "%s:\n", SS_YAML_KEY_STATE);

		std::string card = 	g_uMaxExPages == 0 ?	SS_YAML_VALUE_CARD_80COL :			// todo: support empty slot
							g_uMaxExPages == 1 ?	SS_YAML_VALUE_CARD_EXTENDED80COL :
													SS_YAML_VALUE_CARD_RAMWORKSIII;

		yamlSaveHelper.SaveString(SS_YAML_KEY_CARD, card.c_str());
		yamlSaveHelper.Save("%s: %d\n", SS_YAML_KEY_VERSION, kUNIT_CARD_VER);

		// Card state
		{
			YamlSaveHelper::Label cardState(yamlSaveHelper, "%s:\n", SS_YAML_KEY_STATE);

			yamlSaveHelper.Save("%s: 0x%02X   # [0,1..7F] 0=no aux mem, 1=128K system, etc\n", SS_YAML_KEY_NUMAUXBANKS, g_uMaxExPages);
			yamlSaveHelper.Save("%s: 0x%02X # [  0..7E] 0=memaux\n", SS_YAML_KEY_ACTIVEAUXBANK, g_uActiveBank);

			for(UINT uBank = 1; uBank <= g_uMaxExPages; uBank++)
			{
				MemSaveSnapshotMemory(yamlSaveHelper, false, uBank);
			}

			RGB_SaveSnapshot(yamlSaveHelper);
		}
	}
}

static void MemLoadSnapshotAuxCommon(YamlLoadHelper& yamlLoadHelper, const std::string& card)
{
	// "State"
	UINT numAuxBanks   = yamlLoadHelper.LoadUint(SS_YAML_KEY_NUMAUXBANKS);
	UINT activeAuxBank = yamlLoadHelper.LoadUint(SS_YAML_KEY_ACTIVEAUXBANK);

	SS_CARDTYPE type = CT_Empty;
	if (card == SS_YAML_VALUE_CARD_80COL)
	{
		type = CT_80Col;
		if (numAuxBanks != 0 || activeAuxBank != 0)
			throw std::string(SS_YAML_KEY_UNIT ": AuxSlot: Bad aux slot card state");
	}
	else if (card == SS_YAML_VALUE_CARD_EXTENDED80COL)
	{
		type = CT_Extended80Col;
		if (numAuxBanks != 1 || activeAuxBank != 0)
			throw std::string(SS_YAML_KEY_UNIT ": AuxSlot: Bad aux slot card state");
	}
	else if (card == SS_YAML_VALUE_CARD_RAMWORKSIII)
	{
		type = CT_RamWorksIII;
		if (numAuxBanks < 2 || numAuxBanks > 0x7F || (activeAuxBank+1) > numAuxBanks)
			throw std::string(SS_YAML_KEY_UNIT ": AuxSlot: Bad aux slot card state");
	}
	else
	{
		// todo: support empty slot
		type = CT_Empty;
		throw std::string(SS_YAML_KEY_UNIT ": AuxSlot: Unknown card: " + card);
	}

	g_uMaxExPages = numAuxBanks;
	g_uActiveBank = activeAuxBank;

	//

	for(UINT uBank = 1; uBank <= g_uMaxExPages; uBank++)
	{
		LPBYTE pBank = MemGetBankPtr(uBank);
		if (!pBank)
		{
			pBank = RWpages[uBank-1] = ALIGNED_ALLOC(_6502_MEM_LEN);
		}

		// "Auxiliary Memory Bankxx"
		char szBank[3];
		sprintf(szBank, "%02X", uBank-1);
		std::string auxMemName = MemGetSnapshotAuxMemStructName() + szBank;

		if (!yamlLoadHelper.GetSubMap(auxMemName))
			throw std::string("Memory: Missing map name: " + auxMemName);

		yamlLoadHelper.LoadMemory(pBank, _6502_MEM_LEN);

		yamlLoadHelper.PopMap();
	}

	GetCardMgr().Remove(SLOT0);
	GetCardMgr().InsertAux(type);

	memaux = RWpages[g_uActiveBank];
	// NB. MemUpdatePaging(TRUE) called at end of Snapshot_LoadState_v2()
}

static void MemLoadSnapshotAuxVer1(YamlLoadHelper& yamlLoadHelper)
{
	std::string card = yamlLoadHelper.LoadString(SS_YAML_KEY_CARD);
	MemLoadSnapshotAuxCommon(yamlLoadHelper, card);
}

static void MemLoadSnapshotAuxVer2(YamlLoadHelper& yamlLoadHelper)
{
	std::string card = yamlLoadHelper.LoadString(SS_YAML_KEY_CARD);
	UINT cardVersion = yamlLoadHelper.LoadUint(SS_YAML_KEY_VERSION);

	if (!yamlLoadHelper.GetSubMap(std::string(SS_YAML_KEY_STATE)))
		throw std::string(SS_YAML_KEY_UNIT ": Expected sub-map name: " SS_YAML_KEY_STATE);

	MemLoadSnapshotAuxCommon(yamlLoadHelper, card);

	RGB_LoadSnapshot(yamlLoadHelper, cardVersion);
}

bool MemLoadSnapshotAux(YamlLoadHelper& yamlLoadHelper, UINT unitVersion)
{
	if (unitVersion < 1 || unitVersion > kUNIT_AUXSLOT_VER)
		throw std::string(SS_YAML_KEY_UNIT ": AuxSlot: Version mismatch");

	if (unitVersion == 1)
		MemLoadSnapshotAuxVer1(yamlLoadHelper);
	else
		MemLoadSnapshotAuxVer2(yamlLoadHelper);

	return true;
}

void NoSlotClockSaveSnapshot(YamlSaveHelper& yamlSaveHelper)
{
	if (g_NoSlotClock)
		g_NoSlotClock->SaveSnapshot(yamlSaveHelper);
}

void NoSlotClockLoadSnapshot(YamlLoadHelper& yamlLoadHelper)
{
	if (!g_NoSlotClock)
		g_NoSlotClock = new CNoSlotClock;

	g_NoSlotClock->LoadSnapshot(yamlLoadHelper);
}
