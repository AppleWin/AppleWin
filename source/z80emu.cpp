/*  Emulador do computador TK3000 //e (Microdigital)
 *  por Fábio Belavenuto - Copyright (C) 2004
 *
 *  Adaptado do emulador Applewin por Michael O'Brien
 *  Part of code is Copyright (C) 2003-2004 Tom Charlesworth
 *
 *  Este arquivo é distribuido pela Licença Pública Geral GNU.
 *  Veja o arquivo Licenca.txt distribuido com este software.
 *
 *  ESTE SOFTWARE NÃO OFERECE NENHUMA GARANTIA
 *
 */

// Emula a CPU Z80

#include "StdAfx.h"

#include "z80emu.h"
#include "CPU.h"
#include "Memory.h"

// Variaveis
static int g_uCPMZ80Slot = 0;

BYTE __stdcall CPMZ80_IONull(WORD PC, WORD uAddr, BYTE bWrite, BYTE uValue, ULONG nExecutedCycles)
{
	return IO_Null(PC, uAddr, bWrite, uValue, nExecutedCycles);
}

BYTE __stdcall CPMZ80_IOWrite(WORD PC, WORD uAddr, BYTE bWrite, BYTE uValue, ULONG nExecutedCycles)
{
	if ((uAddr & 0xFF00) == (0xC000 + (g_uCPMZ80Slot << 8)))
		SetActiveCpu( GetActiveCpu() == CPU_Z80 ? GetMainCpu() : CPU_Z80 );

	return IO_Null(PC, uAddr, bWrite, uValue, nExecutedCycles);
}

//===========================================================================

void ConfigureSoftcard(LPBYTE pCxRomPeripheral, UINT uSlot)
{	
	memset(pCxRomPeripheral + (uSlot << 8), 0xFF, APPLE_SLOT_SIZE);
	
	g_uCPMZ80Slot = uSlot;

	RegisterIoHandler(uSlot, CPMZ80_IONull, CPMZ80_IONull, CPMZ80_IONull, CPMZ80_IOWrite, NULL, NULL);
}
