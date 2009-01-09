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

// Variaveis
static int CPMZ80Slot	= 0;
int Z80_IRQ				= 0;	// Used by Z80Em

BYTE __stdcall CPMZ80_IONull(WORD PC, WORD uAddr, BYTE bWrite, BYTE uValue, ULONG nCyclesLeft)
{
	return IO_Null(PC, uAddr, bWrite, uValue, nCyclesLeft);
}

BYTE __stdcall CPMZ80_IOWrite(WORD PC, WORD uAddr, BYTE bWrite, BYTE uValue, ULONG nCyclesLeft)
{
	if ((uAddr & 0xFF00) == (0xC000 + (CPMZ80Slot << 8)))
	{
		g_ActiveCPU = (g_ActiveCPU == CPU_6502) ? CPU_Z80 : CPU_6502;
	}
	return IO_Null(PC, uAddr, bWrite, uValue, nCyclesLeft);
}

//===========================================================================
void ConfigureSoftcard(LPBYTE pCxRomPeripheral, int Slot, UINT addOrRemove)
{	
	memset(pCxRomPeripheral + (Slot << 8), 0xFF, APPLE_SLOT_SIZE);
	
	CPMZ80Slot = Slot;

	RegisterIoHandler(Slot, CPMZ80_IONull, CPMZ80_IONull, CPMZ80_IONull, addOrRemove ? CPMZ80_IOWrite : NULL, NULL, NULL);
}

// EOF