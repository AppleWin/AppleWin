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
 */

#include "StdAfx.h"
#pragma  hdrstop
#include "MouseInterface.h"
#include "..\resource\resource.h"

#define  MF_80STORE    0x00000001
#define  MF_ALTZP      0x00000002
#define  MF_AUXREAD    0x00000004
#define  MF_AUXWRITE   0x00000008
#define  MF_BANK2      0x00000010
#define  MF_HIGHRAM    0x00000020
#define  MF_HIRES      0x00000040
#define  MF_PAGE2      0x00000080
#define  MF_SLOTC3ROM  0x00000100
#define  MF_SLOTCXROM  0x00000200
#define  MF_WRITERAM   0x00000400
#define  MF_IMAGEMASK  0x000003F7

#define  SW_80STORE    (memmode & MF_80STORE)
#define  SW_ALTZP      (memmode & MF_ALTZP)
#define  SW_AUXREAD    (memmode & MF_AUXREAD)
#define  SW_AUXWRITE   (memmode & MF_AUXWRITE)
#define  SW_BANK2      (memmode & MF_BANK2)
#define  SW_HIGHRAM    (memmode & MF_HIGHRAM)
#define  SW_HIRES      (memmode & MF_HIRES)
#define  SW_PAGE2      (memmode & MF_PAGE2)
#define  SW_SLOTC3ROM  (memmode & MF_SLOTC3ROM)
#define  SW_SLOTCXROM  (memmode & MF_SLOTCXROM)
#define  SW_WRITERAM   (memmode & MF_WRITERAM)

//-----------------------------------------------------------------------------

//static DWORD   imagemode;
static LPBYTE  memshadow[0x100];
LPBYTE         memwrite[0x100];

iofunction		IORead[256];
iofunction		IOWrite[256];
static LPVOID	SlotParameters[NUM_SLOTS];

//static BOOL    fastpaging   = 0;	// Redundant: only ever set to 0, by MemSetFastPaging(0)
//static DWORD   image        = 0;
//static DWORD   lastimage    = 0;
static BOOL    lastwriteram = 0;

LPBYTE         mem          = NULL;

//

static LPBYTE  memaux       = NULL;
static LPBYTE  memmain      = NULL;

LPBYTE         memdirty     = NULL;
static LPBYTE  memrom       = NULL;

static LPBYTE  memimage     = NULL;

static LPBYTE	pCxRomInternal		= NULL;
static LPBYTE	pCxRomPeripheral	= NULL;

//

static DWORD   memmode      = MF_BANK2 | MF_SLOTCXROM | MF_WRITERAM;
static BOOL    modechanging = 0;

MemoryInitPattern_e g_eMemoryInitPattern = MIP_FF_FF_00_00;

#ifdef RAMWORKS
UINT			g_uMaxExPages	= 1;			// user requested ram pages
static LPBYTE	RWpages[128];					// pointers to RW memory banks
#endif

BYTE __stdcall IO_Annunciator(WORD programcounter, WORD address, BYTE write, BYTE value, ULONG nCycles);
void UpdatePaging(BOOL initialize, BOOL updatewriteonly);

//=============================================================================

static BYTE __stdcall IORead_C00x(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft)
{
	return KeybReadData(pc, addr, bWrite, d, nCyclesLeft);
}

static BYTE __stdcall IOWrite_C00x(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft)
{
	if ((addr & 0xf) <= 0xB)
		return MemSetPaging(pc, addr, bWrite, d, nCyclesLeft);
	else
		return VideoSetMode(pc, addr, bWrite, d, nCyclesLeft);
}

//-------------------------------------

static BYTE __stdcall IORead_C01x(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft)
{
	switch (addr & 0xf)
	{
	case 0x0:	return KeybReadFlag(pc, addr, bWrite, d, nCyclesLeft);
	case 0x1:	return MemCheckPaging(pc, addr, bWrite, d, nCyclesLeft);
	case 0x2:	return MemCheckPaging(pc, addr, bWrite, d, nCyclesLeft);
	case 0x3:	return MemCheckPaging(pc, addr, bWrite, d, nCyclesLeft);
	case 0x4:	return MemCheckPaging(pc, addr, bWrite, d, nCyclesLeft);
	case 0x5:	return MemCheckPaging(pc, addr, bWrite, d, nCyclesLeft);
	case 0x6:	return MemCheckPaging(pc, addr, bWrite, d, nCyclesLeft);
	case 0x7:	return MemCheckPaging(pc, addr, bWrite, d, nCyclesLeft);
	case 0x8:	return MemCheckPaging(pc, addr, bWrite, d, nCyclesLeft);
	case 0x9:	return VideoCheckVbl(pc, addr, bWrite, d, nCyclesLeft);
	case 0xA:	return VideoCheckMode(pc, addr, bWrite, d, nCyclesLeft);
	case 0xB:	return VideoCheckMode(pc, addr, bWrite, d, nCyclesLeft);
	case 0xC:	return MemCheckPaging(pc, addr, bWrite, d, nCyclesLeft);
	case 0xD:	return MemCheckPaging(pc, addr, bWrite, d, nCyclesLeft);
	case 0xE:	return VideoCheckMode(pc, addr, bWrite, d, nCyclesLeft);
	case 0xF:	return VideoCheckMode(pc, addr, bWrite, d, nCyclesLeft);
	}

	return 0;
}

static BYTE __stdcall IOWrite_C01x(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft)
{
	return KeybReadFlag(pc, addr, bWrite, d, nCyclesLeft);
}

//-------------------------------------

static BYTE __stdcall IORead_C02x(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft)
{
	return IO_Null(pc, addr, bWrite, d, nCyclesLeft);
}

static BYTE __stdcall IOWrite_C02x(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft)
{
	return IO_Null(pc, addr, bWrite, d, nCyclesLeft);
}

//-------------------------------------

static BYTE __stdcall IORead_C03x(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft)
{
	return SpkrToggle(pc, addr, bWrite, d, nCyclesLeft);
}

static BYTE __stdcall IOWrite_C03x(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft)
{
	return SpkrToggle(pc, addr, bWrite, d, nCyclesLeft);
}

//-------------------------------------

static BYTE __stdcall IORead_C04x(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft)
{
	return IO_Null(pc, addr, bWrite, d, nCyclesLeft);
}

static BYTE __stdcall IOWrite_C04x(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft)
{
	return IO_Null(pc, addr, bWrite, d, nCyclesLeft);
}

//-------------------------------------

static BYTE __stdcall IORead_C05x(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft)
{
	switch (addr & 0xf)
	{
	case 0x0:	return VideoSetMode(pc, addr, bWrite, d, nCyclesLeft);
	case 0x1:	return VideoSetMode(pc, addr, bWrite, d, nCyclesLeft);
	case 0x2:	return VideoSetMode(pc, addr, bWrite, d, nCyclesLeft);
	case 0x3:	return VideoSetMode(pc, addr, bWrite, d, nCyclesLeft);
	case 0x4:	return MemSetPaging(pc, addr, bWrite, d, nCyclesLeft);
	case 0x5:	return MemSetPaging(pc, addr, bWrite, d, nCyclesLeft);
	case 0x6:	return MemSetPaging(pc, addr, bWrite, d, nCyclesLeft);
	case 0x7:	return MemSetPaging(pc, addr, bWrite, d, nCyclesLeft);
	case 0x8:	return IO_Annunciator(pc, addr, bWrite, d, nCyclesLeft);
	case 0x9:	return IO_Annunciator(pc, addr, bWrite, d, nCyclesLeft);
	case 0xA:	return IO_Annunciator(pc, addr, bWrite, d, nCyclesLeft);
	case 0xB:	return IO_Annunciator(pc, addr, bWrite, d, nCyclesLeft);
	case 0xC:	return IO_Annunciator(pc, addr, bWrite, d, nCyclesLeft);
	case 0xD:	return IO_Annunciator(pc, addr, bWrite, d, nCyclesLeft);
	case 0xE:	return VideoSetMode(pc, addr, bWrite, d, nCyclesLeft);
	case 0xF:	return VideoSetMode(pc, addr, bWrite, d, nCyclesLeft);
	}

	return 0;
}

static BYTE __stdcall IOWrite_C05x(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft)
{
	switch (addr & 0xf)
	{
	case 0x0:	return VideoSetMode(pc, addr, bWrite, d, nCyclesLeft);
	case 0x1:	return VideoSetMode(pc, addr, bWrite, d, nCyclesLeft);
	case 0x2:	return VideoSetMode(pc, addr, bWrite, d, nCyclesLeft);
	case 0x3:	return VideoSetMode(pc, addr, bWrite, d, nCyclesLeft);
	case 0x4:	return MemSetPaging(pc, addr, bWrite, d, nCyclesLeft);
	case 0x5:	return MemSetPaging(pc, addr, bWrite, d, nCyclesLeft);
	case 0x6:	return MemSetPaging(pc, addr, bWrite, d, nCyclesLeft);
	case 0x7:	return MemSetPaging(pc, addr, bWrite, d, nCyclesLeft);
	case 0x8:	return IO_Annunciator(pc, addr, bWrite, d, nCyclesLeft);
	case 0x9:	return IO_Annunciator(pc, addr, bWrite, d, nCyclesLeft);
	case 0xA:	return IO_Annunciator(pc, addr, bWrite, d, nCyclesLeft);
	case 0xB:	return IO_Annunciator(pc, addr, bWrite, d, nCyclesLeft);
	case 0xC:	return IO_Annunciator(pc, addr, bWrite, d, nCyclesLeft);
	case 0xD:	return IO_Annunciator(pc, addr, bWrite, d, nCyclesLeft);
	case 0xE:	return VideoSetMode(pc, addr, bWrite, d, nCyclesLeft);
	case 0xF:	return VideoSetMode(pc, addr, bWrite, d, nCyclesLeft);
	}

	return 0;
}

//-------------------------------------

static BYTE __stdcall IORead_C06x(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft)
{
	switch (addr & 0xf)
	{
	case 0x0:	return IO_Null(pc, addr, bWrite, d, nCyclesLeft);
	case 0x1:	return JoyReadButton(pc, addr, bWrite, d, nCyclesLeft);
	case 0x2:	return JoyReadButton(pc, addr, bWrite, d, nCyclesLeft);
	case 0x3:	return JoyReadButton(pc, addr, bWrite, d, nCyclesLeft);
	case 0x4:	return JoyReadPosition(pc, addr, bWrite, d, nCyclesLeft);
	case 0x5:	return JoyReadPosition(pc, addr, bWrite, d, nCyclesLeft);
	case 0x6:	return JoyReadPosition(pc, addr, bWrite, d, nCyclesLeft);
	case 0x7:	return JoyReadPosition(pc, addr, bWrite, d, nCyclesLeft);
	case 0x8:	return IO_Null(pc, addr, bWrite, d, nCyclesLeft);
	case 0x9:	return IO_Null(pc, addr, bWrite, d, nCyclesLeft);
	case 0xA:	return IO_Null(pc, addr, bWrite, d, nCyclesLeft);
	case 0xB:	return IO_Null(pc, addr, bWrite, d, nCyclesLeft);
	case 0xC:	return IO_Null(pc, addr, bWrite, d, nCyclesLeft);
	case 0xD:	return IO_Null(pc, addr, bWrite, d, nCyclesLeft);
	case 0xE:	return IO_Null(pc, addr, bWrite, d, nCyclesLeft);
	case 0xF:	return IO_Null(pc, addr, bWrite, d, nCyclesLeft);
	}

	return 0;
}

static BYTE __stdcall IOWrite_C06x(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft)
{
	return IO_Null(pc, addr, bWrite, d, nCyclesLeft);
}

//-------------------------------------

static BYTE __stdcall IORead_C07x(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft)
{
	switch (addr & 0xf)
	{
	case 0x0:	return JoyResetPosition(pc, addr, bWrite, d, nCyclesLeft);
	case 0x1:	return IO_Null(pc, addr, bWrite, d, nCyclesLeft);
	case 0x2:	return IO_Null(pc, addr, bWrite, d, nCyclesLeft);
	case 0x3:	return IO_Null(pc, addr, bWrite, d, nCyclesLeft);
	case 0x4:	return IO_Null(pc, addr, bWrite, d, nCyclesLeft);
	case 0x5:	return IO_Null(pc, addr, bWrite, d, nCyclesLeft);
	case 0x6:	return IO_Null(pc, addr, bWrite, d, nCyclesLeft);
	case 0x7:	return IO_Null(pc, addr, bWrite, d, nCyclesLeft);
	case 0x8:	return IO_Null(pc, addr, bWrite, d, nCyclesLeft);
	case 0x9:	return IO_Null(pc, addr, bWrite, d, nCyclesLeft);
	case 0xA:	return IO_Null(pc, addr, bWrite, d, nCyclesLeft);
	case 0xB:	return IO_Null(pc, addr, bWrite, d, nCyclesLeft);
	case 0xC:	return IO_Null(pc, addr, bWrite, d, nCyclesLeft);
	case 0xD:	return IO_Null(pc, addr, bWrite, d, nCyclesLeft);
	case 0xE:	return IO_Null(pc, addr, bWrite, d, nCyclesLeft);
	case 0xF:	return VideoCheckMode(pc, addr, bWrite, d, nCyclesLeft);
	}

	return 0;
}

static BYTE __stdcall IOWrite_C07x(WORD pc, WORD addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft)
{
	switch (addr & 0xf)
	{
	case 0x0:	return JoyResetPosition(pc, addr, bWrite, d, nCyclesLeft);
#ifdef RAMWORKS
	case 0x1:	return MemSetPaging(pc, addr, bWrite, d, nCyclesLeft);	// extended memory card set page
	case 0x2:	return IO_Null(pc, addr, bWrite, d, nCyclesLeft);
	case 0x3:	return MemSetPaging(pc, addr, bWrite, d, nCyclesLeft);	// Ramworks III set page
#else
	case 0x1:	return IO_Null(pc, addr, bWrite, d, nCyclesLeft);
	case 0x2:	return IO_Null(pc, addr, bWrite, d, nCyclesLeft);
	case 0x3:	return IO_Null(pc, addr, bWrite, d, nCyclesLeft);
#endif
	case 0x4:	return IO_Null(pc, addr, bWrite, d, nCyclesLeft);
	case 0x5:	return IO_Null(pc, addr, bWrite, d, nCyclesLeft);
	case 0x6:	return IO_Null(pc, addr, bWrite, d, nCyclesLeft);
	case 0x7:	return IO_Null(pc, addr, bWrite, d, nCyclesLeft);
	case 0x8:	return IO_Null(pc, addr, bWrite, d, nCyclesLeft);
	case 0x9:	return IO_Null(pc, addr, bWrite, d, nCyclesLeft);
	case 0xA:	return IO_Null(pc, addr, bWrite, d, nCyclesLeft);
	case 0xB:	return IO_Null(pc, addr, bWrite, d, nCyclesLeft);
	case 0xC:	return IO_Null(pc, addr, bWrite, d, nCyclesLeft);
	case 0xD:	return IO_Null(pc, addr, bWrite, d, nCyclesLeft);
	case 0xE:	return IO_Null(pc, addr, bWrite, d, nCyclesLeft);
	case 0xF:	return IO_Null(pc, addr, bWrite, d, nCyclesLeft);
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

static BYTE IO_SELECT;
static BYTE IO_SELECT_InternalROM;

static BYTE* ExpansionRom[NUM_SLOTS];

enum eExpansionRomType {eExpRomNull=0, eExpRomInternal, eExpRomPeripheral};
static eExpansionRomType g_eExpansionRomType = eExpRomNull;
static UINT	g_uPeripheralRomSlot = 0;

//=============================================================================

BYTE __stdcall IO_Null(WORD programcounter, WORD address, BYTE write, BYTE value, ULONG nCyclesLeft)
{
	if (!write)
		return MemReadFloatingBus(nCyclesLeft);
	else
		return 0;
}

BYTE __stdcall IO_Annunciator(WORD programcounter, WORD address, BYTE write, BYTE value, ULONG nCyclesLeft)
{
	// Apple//e ROM:
	// . PC=FA6F: LDA $C058 (SETAN0)
	// . PC=FA72: LDA $C05A (SETAN1)
	// . PC=C2B5: LDA $C05D (CLRAN2)

	// NB. AN3: For //e & //c these locations are now used to enabled/disabled DHIRES
	return 0;
}

// Enabling expansion ROM ($C800..$CFFF]:
// . Enable if: Enable1 && Enable2
// . Enable1 = I/O SELECT' (6502 accesses $Csxx)
//   - Reset when 6502 accesses $CFFF
// . Enable2 = I/O STROBE' (6502 accesses [$C800..$CFFF])

BYTE __stdcall IORead_Cxxx(WORD programcounter, WORD address, BYTE write, BYTE value, ULONG nCyclesLeft)
{
	if (address == 0xCFFF)
	{
		// Disable expansion ROM at [$C800..$CFFF]
		// . SSC will disable on an access to $CFxx - but ROM only writes to $CFFF, so it doesn't matter
		IO_SELECT = 0;
		IO_SELECT_InternalROM = 0;
		g_uPeripheralRomSlot = 0;

		if (SW_SLOTCXROM)
		{
			// NB. SW_SLOTCXROM==0 ensures that internal rom stays switched in
			memset(pCxRomPeripheral+0x800, 0, 0x800);
			memset(mem+0xC800, 0, 0x800);
			g_eExpansionRomType = eExpRomNull;
		}

		// NB. IO_SELECT won't get set, so ROM won't be switched back in...
	}

	//

	BYTE IO_STROBE = 0;

	if (IS_APPLE2 || SW_SLOTCXROM)
	{
		if ((address >= 0xC100) && (address <= 0xC7FF))
		{
			const UINT uSlot = (address >> 8) & 0xF;
			if ((uSlot != 3) && ExpansionRom[uSlot])
				IO_SELECT |= 1<<uSlot;
			else if ((SW_SLOTC3ROM) && ExpansionRom[uSlot])
				IO_SELECT |= 1<<uSlot;		// Slot3 & Peripheral ROM
			else if (!SW_SLOTC3ROM)
				IO_SELECT_InternalROM = 1;	// Slot3 & Internal ROM
		}
		else if ((address >= 0xC800) && (address <= 0xCFFF))
		{
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
				memcpy(pCxRomPeripheral+0x800, ExpansionRom[uSlot], 0x800);
				memcpy(mem+0xC800, ExpansionRom[uSlot], 0x800);
				g_eExpansionRomType = eExpRomPeripheral;
				g_uPeripheralRomSlot = uSlot;
			}
		}
		else if (IO_SELECT_InternalROM && IO_STROBE && (g_eExpansionRomType != eExpRomInternal))
		{
			// Enable Internal ROM
			// . Get this for PR#3
			memcpy(mem+0xC800, pCxRomInternal+0x800, 0x800);
			g_eExpansionRomType = eExpRomInternal;
			g_uPeripheralRomSlot = 0;
		}
	}

	if (!IS_APPLE2 && !SW_SLOTCXROM)
	{
		// !SW_SLOTC3ROM = Internal ROM: $C300-C3FF
		// !SW_SLOTCXROM = Internal ROM: $C100-CFFF

		if ((address >= 0xC100) && (address <= 0xC7FF))	// Don't care about state of SW_SLOTC3ROM
			IO_SELECT_InternalROM = 1;
		else if ((address >= 0xC800) && (address <= 0xCFFF))
			IO_STROBE = 1;

		if (!SW_SLOTCXROM && IO_SELECT_InternalROM && IO_STROBE && (g_eExpansionRomType != eExpRomInternal))
		{
			// Enable Internal ROM
			memcpy(mem+0xC800, pCxRomInternal+0x800, 0x800);
			g_eExpansionRomType = eExpRomInternal;
			g_uPeripheralRomSlot = 0;
		}
	}

	if ((g_eExpansionRomType == eExpRomNull) && (address >= 0xC800))
		return IO_Null(programcounter, address, write, value, nCyclesLeft);
	else
		return mem[address];
}

BYTE __stdcall IOWrite_Cxxx(WORD programcounter, WORD address, BYTE write, BYTE value, ULONG nCyclesLeft)
{
	return 0;
}

//===========================================================================

static BYTE g_bmSlotInit = 0;

static void InitIoHandlers()
{
	g_bmSlotInit = 0;
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
		IORead[i]	= IORead_Cxxx;
		IOWrite[i]	= IOWrite_Cxxx;
	}

	//

	IO_SELECT = 0;
	IO_SELECT_InternalROM = 0;
	g_eExpansionRomType = eExpRomNull;
	g_uPeripheralRomSlot = 0;

	for (i=0; i<NUM_SLOTS; i++)
		ExpansionRom[i] = NULL;
}

// All slots [0..7] must register their handlers
void RegisterIoHandler(UINT uSlot, iofunction IOReadC0, iofunction IOWriteC0, iofunction IOReadCx, iofunction IOWriteCx, LPVOID lpSlotParameter, BYTE* pExpansionRom)
{
	_ASSERT(uSlot < NUM_SLOTS);
	g_bmSlotInit |= 1<<uSlot;
	SlotParameters[uSlot] = lpSlotParameter;

	IORead[uSlot+8]		= IOReadC0;
	IOWrite[uSlot+8]	= IOWriteC0;

	if (uSlot == 0)		// Don't trash C0xx handlers
		return;

	if (IOReadCx == NULL)	IOReadCx = IORead_Cxxx;
	if (IOWriteCx == NULL)	IOWriteCx = IOWrite_Cxxx;

	for (UINT i=0; i<16; i++)
	{
		IORead[uSlot*16+i]	= IOReadCx;
		IOWrite[uSlot*16+i]	= IOWriteCx;
	}

	// What about [$C80x..$CFEx]? - Do any cards use this as I/O memory?
	ExpansionRom[uSlot] = pExpansionRom;
}

//===========================================================================

//// Only called by MemSetFastPaging()
//void BackMainImage ()
//{
//	for (UINT loop = 0; loop < 256; loop++)
//	{
//		if (memshadow[loop] && ((*(memdirty+loop) & 1) || (loop <= 1)))
//			CopyMemory(memshadow[loop],memimage+(loop << 8),256);
//
//		*(memdirty+loop) &= ~1;
//	}
//}

//===========================================================================

void ResetPaging (BOOL initialize)
{
	//if (!initialize)
	//  MemSetFastPaging(0);

	lastwriteram = 0;
	memmode      = MF_BANK2 | MF_SLOTCXROM | MF_WRITERAM;
	UpdatePaging(initialize, 0);
}

//===========================================================================
//void UpdateFastPaging () {
//  BOOL  found    = 0;
//  DWORD imagenum = 0;
//  do
//    if ((imagemode[imagenum] == memmode) ||
//        ((lastimage >= 3) &&
//         ((imagemode[imagenum] & MF_IMAGEMASK) == (memmode & MF_IMAGEMASK))))
//      found = 1;
//    else
//      ++imagenum;
//  while ((imagenum <= lastimage) && !found);
//  if (found) {
//    image = imagenum;
//    mem   = memimage+(image << 16);
//    if (imagemode[image] != memmode) {
//      imagemode[image] = memmode;
//      UpdatePaging(0,1);
//    }
//  }
//  else {
//    if (lastimage < MAXIMAGES-1) {
//      imagenum = ++lastimage;
//      if (lastimage >= 3)
//        VirtualAlloc(memimage+lastimage*0x10000,0x10000,MEM_COMMIT,PAGE_READWRITE);
//    }
//    else {
//      static DWORD nextimage = 0;
//      if (nextimage > lastimage)
//        nextimage = 0;
//      imagenum = nextimage++;
//    }
//    imagemode[image = imagenum] = memmode;
//    mem = memimage+(image << 16);
//    UpdatePaging(1,0);
//  }
//}

//===========================================================================

static void UpdatePaging (BOOL initialize, BOOL updatewriteonly)
{
	// SAVE THE CURRENT PAGING SHADOW TABLE
	LPBYTE oldshadow[256];
	if (!(initialize || updatewriteonly /*|| fastpaging*/ ))
		CopyMemory(oldshadow,memshadow,256*sizeof(LPBYTE));

	// UPDATE THE PAGING TABLES BASED ON THE NEW PAGING SWITCH VALUES
	UINT loop;
	if (initialize)
	{
		for (loop = 0x00; loop < 0xC0; loop++)
			memwrite[loop] = mem+(loop << 8);

		for (loop = 0xC0; loop < 0xD0; loop++)
			memwrite[loop] = NULL;
	}

	if (!updatewriteonly)
	{
		for (loop = 0x00; loop < 0x02; loop++)
			memshadow[loop] = SW_ALTZP ? memaux+(loop << 8) : memmain+(loop << 8);
	}

	for (loop = 0x02; loop < 0xC0; loop++)
	{
		memshadow[loop] = SW_AUXREAD ? memaux+(loop << 8)
			: memmain+(loop << 8);

		memwrite[loop]  = ((SW_AUXREAD != 0) == (SW_AUXWRITE != 0))
			? mem+(loop << 8)
			: SW_AUXWRITE	? memaux+(loop << 8)
							: memmain+(loop << 8);
	}

	if (!updatewriteonly)
	{
		for (loop = 0xC0; loop < 0xC8; loop++)
		{
			const UINT uSlotOffset = (loop & 0x0f) * 0x100;
			if (loop == 0xC3)
				memshadow[loop] = (SW_SLOTC3ROM && SW_SLOTCXROM)	? pCxRomPeripheral+uSlotOffset	// C300..C3FF - Slot 3 ROM (all 0x00's)
																	: pCxRomInternal+uSlotOffset;	// C300..C3FF - Internal ROM
			else
				memshadow[loop] = SW_SLOTCXROM	? pCxRomPeripheral+uSlotOffset						// C000..C7FF - SSC/Disk][/etc
												: pCxRomInternal+uSlotOffset;						// C000..C7FF - Internal ROM
		}

		for (loop = 0xC8; loop < 0xD0; loop++)
		{
			const UINT uRomOffset = (loop & 0x0f) * 0x100;
			memshadow[loop] = pCxRomInternal+uRomOffset;											// C800..CFFF - Internal ROM
		}
	}

	for (loop = 0xD0; loop < 0xE0; loop++)
	{
		int bankoffset = (SW_BANK2 ? 0 : 0x1000);
		memshadow[loop] = SW_HIGHRAM ? SW_ALTZP	? memaux+(loop << 8)-bankoffset
												: memmain+(loop << 8)-bankoffset
									: memrom+((loop-0xD0) * 0x100);

		memwrite[loop]  = SW_WRITERAM	? SW_HIGHRAM	? mem+(loop << 8)
														: SW_ALTZP	? memaux+(loop << 8)-bankoffset
																	: memmain+(loop << 8)-bankoffset
										: NULL;
	}

	for (loop = 0xE0; loop < 0x100; loop++)
	{
		memshadow[loop] = SW_HIGHRAM	? SW_ALTZP	? memaux+(loop << 8)
													: memmain+(loop << 8)
										: memrom+((loop-0xD0) * 0x100);

		memwrite[loop]  = SW_WRITERAM	? SW_HIGHRAM	? mem+(loop << 8)
														: SW_ALTZP	? memaux+(loop << 8)
																	: memmain+(loop << 8)
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
	if (!updatewriteonly)
	{
		for (loop = 0x00; loop < 0x100; loop++)
		{
			if (initialize || (oldshadow[loop] != memshadow[loop]))
			{
				if ((!(initialize/* || fastpaging*/)) &&
					((*(memdirty+loop) & 1) || (loop <= 1)))
				{
					*(memdirty+loop) &= ~1;
					CopyMemory(oldshadow[loop],mem+(loop << 8),256);
				}

				CopyMemory(mem+(loop << 8),memshadow[loop],256);
			}
		}
	}

}

//
// ----- ALL GLOBALLY ACCESSIBLE FUNCTIONS ARE BELOW THIS LINE -----
//

//===========================================================================

// TODO: >= Apple2e only?
BYTE __stdcall MemCheckPaging (WORD, WORD address, BYTE, BYTE, ULONG)
{
	address &= 0xFF;
	BOOL result = 0;
	switch (address)
	{
	case 0x11: result = SW_BANK2;       break;
	case 0x12: result = SW_HIGHRAM;     break;
	case 0x13: result = SW_AUXREAD;     break;
	case 0x14: result = SW_AUXWRITE;    break;
	case 0x15: result = !SW_SLOTCXROM;  break;
	case 0x16: result = SW_ALTZP;       break;
	case 0x17: result = SW_SLOTC3ROM;   break;
	case 0x18: result = SW_80STORE;     break;
	case 0x1C: result = SW_PAGE2;       break;
	case 0x1D: result = SW_HIRES;       break;
	}
	return KeybGetKeycode() | (result ? 0x80 : 0);
}

//===========================================================================

void MemDestroy ()
{
  //if (fastpaging)
  //  MemSetFastPaging(0);
//  VirtualFree(memimage,MAX(0x30000,0x10000*1),MEM_DECOMMIT);

	VirtualFree(memaux  ,0,MEM_RELEASE);
	VirtualFree(memmain ,0,MEM_RELEASE);
	VirtualFree(memdirty,0,MEM_RELEASE);
	VirtualFree(memrom  ,0,MEM_RELEASE);
	VirtualFree(memimage,0,MEM_RELEASE);

	VirtualFree(pCxRomInternal,0,MEM_RELEASE);
	VirtualFree(pCxRomPeripheral,0,MEM_RELEASE);

#ifdef RAMWORKS
	for (UINT i=1; i<g_uMaxExPages; i++)
	{
		if (RWpages[i])
		{
			VirtualFree(RWpages[i], 0, MEM_RELEASE);
			RWpages[i] = NULL;
		}
	}
	RWpages[0]=NULL;
#endif

	memaux   = NULL;
	memmain  = NULL;
	memdirty = NULL;
	memrom   = NULL;
	memimage = NULL;

	pCxRomInternal		= NULL;
	pCxRomPeripheral	= NULL;

	mem      = NULL;

	ZeroMemory(memwrite, sizeof(memwrite));
	ZeroMemory(memshadow,sizeof(memshadow));
}

//===========================================================================

bool MemGet80Store()
{
	return SW_80STORE != 0;
}

//===========================================================================

bool MemCheckSLOTCXROM()
{
	return SW_SLOTCXROM ? true : false;
}

//===========================================================================
LPBYTE MemGetAuxPtr (WORD offset)
{
	LPBYTE lpMem = (memshadow[(offset >> 8)] == (memaux+(offset & 0xFF00)))
			? mem+offset
			: memaux+offset;

#ifdef RAMWORKS
	if ( ((SW_PAGE2 && SW_80STORE) || VideoGetSW80COL()) &&
		( ( ((offset & 0xFF00)>=0x0400) &&
		((offset & 0xFF00)<=0700) ) ||
		( SW_HIRES && ((offset & 0xFF00)>=0x2000) &&
		((offset & 0xFF00)<=0x3F00) ) ) ) {
		lpMem = (memshadow[(offset >> 8)] == (RWpages[0]+(offset & 0xFF00)))
			? mem+offset
			: RWpages[0]+offset;
	}
#endif

	return lpMem;
}

//===========================================================================
LPBYTE MemGetMainPtr (WORD offset)
{
  return (memshadow[(offset >> 8)] == (memmain+(offset & 0xFF00)))
           ? mem+offset
           : memmain+offset;
}

//===========================================================================

LPBYTE MemGetCxRomPeripheral()
{
	return pCxRomPeripheral;
}

//===========================================================================

void MemPreInitialize ()
{
	// Init the I/O handlers
	InitIoHandlers();
}

//===========================================================================

void MemInitialize()
{
	const UINT CxRomSize = 4*1024;
	const UINT Apple2RomSize = 12*1024;
	const UINT Apple2eRomSize = Apple2RomSize+CxRomSize;

	// ALLOCATE MEMORY FOR THE APPLE MEMORY IMAGE AND ASSOCIATED DATA STRUCTURES
	memaux   = (LPBYTE)VirtualAlloc(NULL,_6502_MEM_END+1,MEM_COMMIT,PAGE_READWRITE);
	memmain  = (LPBYTE)VirtualAlloc(NULL,_6502_MEM_END+1,MEM_COMMIT,PAGE_READWRITE);
	memdirty = (LPBYTE)VirtualAlloc(NULL,0x100  ,MEM_COMMIT,PAGE_READWRITE);
	memrom   = (LPBYTE)VirtualAlloc(NULL,0x5000 ,MEM_COMMIT,PAGE_READWRITE);
//  // THE MEMIMAGE BUFFER CAN CONTAIN EITHER MULTIPLE MEMORY IMAGES OR ONE MEMORY IMAGE WITH COMPILER DATA
//  memimage = (LPBYTE)VirtualAlloc(NULL,
//                                  MAX(0x30000,MAXIMAGES*0x10000),
//                                  MEM_RESERVE,PAGE_NOACCESS);
	memimage = (LPBYTE)VirtualAlloc(NULL,_6502_MEM_END+1,MEM_RESERVE,PAGE_NOACCESS);

	pCxRomInternal		= (LPBYTE) VirtualAlloc(NULL, CxRomSize, MEM_COMMIT, PAGE_READWRITE);
	pCxRomPeripheral	= (LPBYTE) VirtualAlloc(NULL, CxRomSize, MEM_COMMIT, PAGE_READWRITE);

	if (!memaux || !memdirty || !memimage || !memmain || !memrom || !pCxRomInternal || !pCxRomPeripheral)
	{
		MessageBox(
			GetDesktopWindow(),
			TEXT("The emulator was unable to allocate the memory it ")
			TEXT("requires.  Further execution is not possible."),
			g_pAppTitle,
			MB_ICONSTOP | MB_SETFOREGROUND);
		ExitProcess(1);
	}

//	LPVOID newloc = VirtualAlloc(memimage,0x30000,MEM_COMMIT,PAGE_READWRITE);
	LPVOID newloc = VirtualAlloc(memimage,_6502_MEM_END+1,MEM_COMMIT,PAGE_READWRITE);
	if (newloc != memimage)
		MessageBox(
			GetDesktopWindow(),
			TEXT("The emulator has detected a bug in your operating ")
			TEXT("system.  While changing the attributes of a memory ")
			TEXT("object, the operating system also changed its ")
			TEXT("location."),
			g_pAppTitle,
			MB_ICONEXCLAMATION | MB_SETFOREGROUND);

#ifdef RAMWORKS
	// allocate memory for RAMWorks III - up to 8MB
	RWpages[0] = memaux;
	UINT i = 1;
	while ((i < g_uMaxExPages) && (RWpages[i] = (LPBYTE) VirtualAlloc(NULL,_6502_MEM_END+1,MEM_COMMIT,PAGE_READWRITE)))
		i++;
#endif

	// READ THE APPLE FIRMWARE ROMS INTO THE ROM IMAGE
	UINT ROM_SIZE = 0;
	HRSRC hResInfo = NULL;
	switch (g_Apple2Type)
	{
	case A2TYPE_APPLE2:			hResInfo = FindResource(NULL, MAKEINTRESOURCE(IDR_APPLE2_ROM), "ROM"); ROM_SIZE = Apple2RomSize; break;
	case A2TYPE_APPLE2PLUS:		hResInfo = FindResource(NULL, MAKEINTRESOURCE(IDR_APPLE2_PLUS_ROM), "ROM"); ROM_SIZE = Apple2RomSize; break;
	case A2TYPE_APPLE2E:		hResInfo = FindResource(NULL, MAKEINTRESOURCE(IDR_APPLE2E_ROM), "ROM"); ROM_SIZE = Apple2eRomSize; break;
	case A2TYPE_APPLE2EEHANCED:	hResInfo = FindResource(NULL, MAKEINTRESOURCE(IDR_APPLE2E_ENHANCED_ROM), "ROM"); ROM_SIZE = Apple2eRomSize; break;
	}

	if(hResInfo == NULL)
	{
		TCHAR sRomFileName[ MAX_PATH ];
		switch (g_Apple2Type)
		{
		case A2TYPE_APPLE2:			_tcscpy(sRomFileName, TEXT("APPLE2.ROM")); break;
		case A2TYPE_APPLE2PLUS:		_tcscpy(sRomFileName, TEXT("APPLE2_PLUS.ROM")); break;
		case A2TYPE_APPLE2E:		_tcscpy(sRomFileName, TEXT("APPLE2E.ROM")); break;
		case A2TYPE_APPLE2EEHANCED:	_tcscpy(sRomFileName, TEXT("APPLE2E_ENHANCED.ROM")); break;
		}

		TCHAR sText[ MAX_PATH ];
		wsprintf( sText, TEXT("Unable to open the required firmware ROM data file.\n\nFile: %s"), sRomFileName );

		MessageBox(
			GetDesktopWindow(),
			sText,
			g_pAppTitle,
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

	//

	memset(pCxRomInternal,0,CxRomSize);
	memset(pCxRomPeripheral,0,CxRomSize);

	if (ROM_SIZE == Apple2eRomSize)
	{
		memcpy(pCxRomInternal, pData, CxRomSize);
		pData += CxRomSize;
		ROM_SIZE -= CxRomSize;
	}

	_ASSERT(ROM_SIZE == Apple2RomSize);
	memcpy(memrom, pData, Apple2RomSize);			// ROM at $D000...$FFFF 

	const UINT F8RomSize = 0x800;
	if (g_hCustomRomF8 != INVALID_HANDLE_VALUE)
	{
		SetFilePointer(g_hCustomRomF8, 0, NULL, FILE_BEGIN);
		DWORD uNumBytesRead;
		BOOL bRes = ReadFile(g_hCustomRomF8, memrom+Apple2RomSize-F8RomSize, F8RomSize, &uNumBytesRead, NULL);
		if (uNumBytesRead != F8RomSize)
		{
			memcpy(memrom, pData, Apple2RomSize);	// ROM at $D000...$FFFF 
			bRes = FALSE;
		}

		if (!bRes)
		{
			MessageBox( g_hFrameWindow, "Failed to read custom F8 rom", TEXT("AppleWin Error"), MB_OK );
			CloseHandle(g_hCustomRomF8);
			g_hCustomRomF8 = INVALID_HANDLE_VALUE;
			// Failed, so use default rom...
		}
	}

	if (g_uTheFreezesF8Rom)
	{
		hResInfo = FindResource(NULL, MAKEINTRESOURCE(IDR_FREEZES_F8_ROM), "ROM");

		if (hResInfo && (SizeofResource(NULL, hResInfo) == 0x800) && (hResData = LoadResource(NULL, hResInfo)) && (pData = (BYTE*) LockResource(hResData)))
		{
			memcpy(memrom+Apple2RomSize-F8RomSize, pData, F8RomSize);
		}
	}

	//

	const UINT uSlot = 0;
	RegisterIoHandler(uSlot, MemSetPaging, MemSetPaging, NULL, NULL, NULL, NULL);

	PrintLoadRom(pCxRomPeripheral, 1);				// $C100 : Parallel printer f/w
	sg_SSC.CommInitialize(pCxRomPeripheral, 2);		// $C200 : SSC
	if (g_Slot4 == CT_MouseInterface)
		sg_Mouse.Initialize(pCxRomPeripheral, 4);	// $C400 : Mouse f/w
	DiskLoadRom(pCxRomPeripheral, 6);				// $C600 : Disk][ f/w
	HD_Load_Rom(pCxRomPeripheral, 7);				// $C700 : HDD f/w

	MemReset();
}

//===========================================================================

// Called by:
// . MemInitialize()
// . ResetMachineState()	eg. Power-cycle ('Apple-Go' button)
// . Snapshot_LoadState()
void MemReset ()
{
	//// TURN OFF FAST PAGING IF IT IS CURRENTLY ACTIVE
	//MemSetFastPaging(0);

	// INITIALIZE THE PAGING TABLES
	ZeroMemory(memshadow,256*sizeof(LPBYTE));
	ZeroMemory(memwrite ,256*sizeof(LPBYTE));

	// INITIALIZE THE RAM IMAGES
	ZeroMemory(memaux ,0x10000);

	ZeroMemory(memmain,0x10000);

	int iByte;

	if (g_eMemoryInitPattern == MIP_FF_FF_00_00)
	{
		for( iByte = 0x0000; iByte < 0xC000; )
		{
			memmain[ iByte++ ] = 0xFF;
			memmain[ iByte++ ] = 0xFF;

			iByte++;
			iByte++;
		}
	}

	// SET UP THE MEMORY IMAGE
	mem   = memimage;
	//image = 0;

	// INITIALIZE PAGING, FILLING IN THE 64K MEMORY IMAGE
	ResetPaging(1);

	// INITIALIZE & RESET THE CPU
	// . Do this after ROM has been copied back to mem[], so that PC is correctly init'ed from 6502's reset vector
	CpuInitialize();
}

//===========================================================================

// Call by:
// . Soft-reset (Ctrl+Reset)
// . Snapshot_LoadState()
void MemResetPaging ()
{
  ResetPaging(0);
}

//===========================================================================

// Called by Disk][ I/O only
BYTE MemReturnRandomData (BYTE highbit)
{
  static const BYTE retval[16] = {0x00,0x2D,0x2D,0x30,0x30,0x32,0x32,0x34,
                                  0x35,0x39,0x43,0x43,0x43,0x60,0x7F,0x7F};
  BYTE r = (BYTE)(rand() & 0xFF);
  if (r <= 170)
    return 0x20 | (highbit ? 0x80 : 0);
  else
    return retval[r & 15] | (highbit ? 0x80 : 0);
}

//===========================================================================

BYTE MemReadFloatingBus(const ULONG uExecutedCycles)
{
  return*(LPBYTE)(mem + VideoGetScannerAddress(NULL, uExecutedCycles));
}

//===========================================================================

BYTE MemReadFloatingBus(const BYTE highbit, const ULONG uExecutedCycles)
{
  BYTE r = *(LPBYTE)(mem + VideoGetScannerAddress(NULL, uExecutedCycles));
  return (r & ~0x80) | ((highbit) ? 0x80 : 0);
}

//===========================================================================
//void MemSetFastPaging (BOOL on) {
//  if (fastpaging && modechanging) {
//    modechanging = 0;
//    UpdateFastPaging();
//  }
//  else if (!fastpaging) {
//    BackMainImage();
//    if (lastimage >= 3)
//      VirtualFree(memimage+0x30000,(lastimage-2) << 16,MEM_DECOMMIT);
//  }
//  fastpaging   = on;
//  image        = 0;
//  mem          = memimage;
//  lastimage    = 0;
//  imagemode[0] = memmode;
//  if (!fastpaging)
//    UpdatePaging(1,0);
//}

//===========================================================================
BYTE __stdcall MemSetPaging (WORD programcounter, WORD address, BYTE write, BYTE value, ULONG nCyclesLeft)
{
  address &= 0xFF;
  DWORD lastmemmode = memmode;

  // DETERMINE THE NEW MEMORY PAGING MODE.
  if ((address >= 0x80) && (address <= 0x8F))
  {
    BOOL writeram = (address & 1);
    memmode &= ~(MF_BANK2 | MF_HIGHRAM | MF_WRITERAM);
    lastwriteram = 1; // note: because diags.do doesn't set switches twice!
    if (lastwriteram && writeram)
      memmode |= MF_WRITERAM;
    if (!(address & 8))
      memmode |= MF_BANK2;
    if (((address & 2) >> 1) == (address & 1))
      memmode |= MF_HIGHRAM;
    lastwriteram = writeram;
  }
  else if (!IS_APPLE2)
  {
    switch (address)
	{
		case 0x00: memmode &= ~MF_80STORE;    break;
		case 0x01: memmode |=  MF_80STORE;    break;
		case 0x02: memmode &= ~MF_AUXREAD;    break;
		case 0x03: memmode |=  MF_AUXREAD;    break;
		case 0x04: memmode &= ~MF_AUXWRITE;   break;
		case 0x05: memmode |=  MF_AUXWRITE;   break;
		case 0x06: memmode |=  MF_SLOTCXROM;  break;
		case 0x07: memmode &= ~MF_SLOTCXROM;  break;
		case 0x08: memmode &= ~MF_ALTZP;      break;
		case 0x09: memmode |=  MF_ALTZP;      break;
		case 0x0A: memmode &= ~MF_SLOTC3ROM;  break;
		case 0x0B: memmode |=  MF_SLOTC3ROM;  break;
		case 0x54: memmode &= ~MF_PAGE2;      break;
		case 0x55: memmode |=  MF_PAGE2;      break;
		case 0x56: memmode &= ~MF_HIRES;      break;
		case 0x57: memmode |=  MF_HIRES;      break;
#ifdef RAMWORKS
		case 0x71: // extended memory aux page number
		case 0x73: // Ramworks III set aux page number
			if ((value < g_uMaxExPages) && RWpages[value])
			{
				memaux = RWpages[value];
				//memmode &= ~MF_RWPMASK;
				//memmode |= value;
				//if (fastpaging)
				//	UpdateFastPaging();
				//else
					UpdatePaging(0,0);
			}
			break;
#endif
	}
  }

  // IF THE EMULATED PROGRAM HAS JUST UPDATE THE MEMORY WRITE MODE AND IS
  // ABOUT TO UPDATE THE MEMORY READ MODE, HOLD OFF ON ANY PROCESSING UNTIL
  // IT DOES SO.
  if ((address >= 4) && (address <= 5) &&
      ((*(LPDWORD)(mem+programcounter) & 0x00FFFEFF) == 0x00C0028D)) {
    modechanging = 1;
    return write ? 0 : MemReadFloatingBus(1, nCyclesLeft);
  }
  if ((address >= 0x80) && (address <= 0x8F) && (programcounter < 0xC000) &&
      (((*(LPDWORD)(mem+programcounter) & 0x00FFFEFF) == 0x00C0048D) ||
       ((*(LPDWORD)(mem+programcounter) & 0x00FFFEFF) == 0x00C0028D))) {
    modechanging = 1;
    return write ? 0 : MemReadFloatingBus(1, nCyclesLeft);
  }

  // IF THE MEMORY PAGING MODE HAS CHANGED, UPDATE OUR MEMORY IMAGES AND
  // WRITE TABLES.
  if ((lastmemmode != memmode) || modechanging)
  {
    modechanging = 0;

	if ((lastmemmode & MF_SLOTCXROM) != (memmode & MF_SLOTCXROM))
	{
		if (SW_SLOTCXROM)
		{
			// Disable Internal ROM
			// . Similar to $CFFF access
			// . None of the peripheral cards can be driving the bus - so use the null ROM
			memset(pCxRomPeripheral+0x800, 0, 0x800);
			memset(mem+0xC800, 0, 0x800);
			g_eExpansionRomType = eExpRomNull;
			g_uPeripheralRomSlot = 0;
		}
		else
		{
			// Enable Internal ROM
			memcpy(mem+0xC800, pCxRomInternal+0x800, 0x800);
			g_eExpansionRomType = eExpRomInternal;
			g_uPeripheralRomSlot = 0;
		}
	}

    //// IF FAST PAGING IS ACTIVE, WE KEEP MULTIPLE COMPLETE MEMORY IMAGES
    //// AND WRITE TABLES, AND SWITCH BETWEEN THEM.  THE FAST PAGING VERSION
    //// OF THE CPU EMULATOR KEEPS ALL OF THE IMAGES COHERENT.
    //if (fastpaging)
    //  UpdateFastPaging();

    // IF FAST PAGING IS NOT ACTIVE THEN WE KEEP ONLY ONE MEMORY IMAGE AND
    // WRITE TABLE, AND UPDATE THEM EVERY TIME PAGING IS CHANGED.
    //else
      UpdatePaging(0,0);

  }

  if ((address <= 1) || ((address >= 0x54) && (address <= 0x57)))
    return VideoSetMode(programcounter,address,write,value,nCyclesLeft);

  return write ? 0 : MemReadFloatingBus(nCyclesLeft);
}

//===========================================================================
//void MemTrimImages () {
//  if (fastpaging && (lastimage > 2))
//  {
//    if (modechanging) {
//      modechanging = 0;
//      UpdateFastPaging();
//    }
//    static DWORD trimnumber = 0;
//    if ((image != trimnumber) &&
//        (image != lastimage) &&
//        (trimnumber < lastimage)) {
//      imagemode[trimnumber] = imagemode[lastimage];
//      VirtualFree(memimage+(lastimage-- << 16),0x10000,MEM_DECOMMIT);
//      DWORD realimage = image;
//      image   = trimnumber;
//      mem     = memimage+(image << 16);
//      memmode = imagemode[image];
//      UpdatePaging(1,0);
//      image   = realimage;
//      mem     = memimage+(image << 16);
//      memmode = imagemode[image];
//    }
//    if (++trimnumber >= lastimage)
//      trimnumber = 0;
//  }
//}

//===========================================================================

LPVOID MemGetSlotParameters (UINT uSlot)
{
	_ASSERT(uSlot < NUM_SLOTS);
	return SlotParameters[uSlot];
}

//===========================================================================

DWORD MemGetSnapshot(SS_BaseMemory* pSS)
{
	pSS->dwMemMode = memmode;
	pSS->bLastWriteRam = lastwriteram;

	for(DWORD dwOffset = 0x0000; dwOffset < 0x10000; dwOffset+=0x100)
	{
		memcpy(pSS->nMemMain+dwOffset, MemGetMainPtr((WORD)dwOffset), 0x100);
		memcpy(pSS->nMemAux+dwOffset, MemGetAuxPtr((WORD)dwOffset), 0x100);
	}

	return 0;
}

DWORD MemSetSnapshot(SS_BaseMemory* pSS)
{
	memmode = pSS->dwMemMode;
	lastwriteram = pSS->bLastWriteRam;

	memcpy(memmain, pSS->nMemMain, nMemMainSize);
	memcpy(memaux, pSS->nMemAux, nMemAuxSize);

	//

	modechanging = 0;

	UpdatePaging(1,0);		// Initialize=1, UpdateWriteOnly=0

	return 0;
}
