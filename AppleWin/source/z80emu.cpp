/*  Emulador do computador TK3000 //e (Microdigital)
 *  por F�bio Belavenuto - Copyright (C) 2004
 *
 *  Adaptado do emulador Applewin por Michael O'Brien
 *  Part of code is Copyright (C) 2003-2004 Tom Charlesworth
 *
 *  Este arquivo � distribuido pela Licen�a P�blica Geral GNU.
 *  Veja o arquivo Licenca.txt distribuido com este software.
 *
 *  ESTE SOFTWARE N�O OFERECE NENHUMA GARANTIA
 *
 */

// Emula a CPU Z80

#include "StdAfx.h"
#include "z80emu.h"

// Variaveis
static int g_uCPMZ80Slot = 0;

BYTE __stdcall CPMZ80_IONull(WORD PC, WORD uAddr, BYTE bWrite, BYTE uValue, ULONG nCyclesLeft)
{
	return IO_Null(PC, uAddr, bWrite, uValue, nCyclesLeft);
}

BYTE __stdcall CPMZ80_IOWrite(WORD PC, WORD uAddr, BYTE bWrite, BYTE uValue, ULONG nCyclesLeft)
{
	if ((uAddr & 0xFF00) == (0xC000 + (g_uCPMZ80Slot << 8)))
		g_ActiveCPU = (g_ActiveCPU == CPU_6502) ? CPU_Z80 : CPU_6502;

	return IO_Null(PC, uAddr, bWrite, uValue, nCyclesLeft);
}

//===========================================================================

void ConfigureSoftcard(LPBYTE pCxRomPeripheral, UINT uSlot, UINT bEnable)
{	
	memset(pCxRomPeripheral + (uSlot << 8), 0xFF, APPLE_SLOT_SIZE);
	
	g_uCPMZ80Slot = uSlot;

	RegisterIoHandler(uSlot, CPMZ80_IONull, CPMZ80_IONull, CPMZ80_IONull, bEnable ? CPMZ80_IOWrite : NULL, NULL, NULL);
}
