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
#include "Z80VICE/z80.h"


BYTE __stdcall Z80Card::IOWrite(WORD pc, WORD addr, BYTE bWrite, BYTE value, ULONG nExecutedCycles)
{
	const UINT slot = (addr >> 8) & 0x7;

	if ((addr & 0xFF00) == (0xC000 + (slot << 8)))
		SetActiveCpu( GetActiveCpu() == CPU_Z80 ? GetMainCpu() : CPU_Z80 );

	return IO_Null(pc, addr, bWrite, value, nExecutedCycles);
}

void Z80Card::InitializeIO(LPBYTE pCxRomPeripheral)
{	
	memset(pCxRomPeripheral + (m_slot << 8), 0xFF, APPLE_SLOT_SIZE);
	
	RegisterIoHandler(m_slot, IO_Null, IO_Null, IO_Null, &Z80Card::IOWrite, this, NULL);
}

//===========================================================================

const std::string& Z80Card::GetSnapshotCardName(void)
{
	return Z80_GetSnapshotCardName();
}

void Z80Card::SaveSnapshot(YamlSaveHelper& yamlSaveHelper)
{
	return Z80_SaveSnapshot(yamlSaveHelper, m_slot);
}

bool Z80Card::LoadSnapshot(YamlLoadHelper& yamlLoadHelper, UINT version)
{
	return Z80_LoadSnapshot(yamlLoadHelper, m_slot, version);
}
