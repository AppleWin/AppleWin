/*
  AppleWin : An Apple //e emulator for Windows

  Copyright (C) 1994-1996, Michael O'Brien
  Copyright (C) 1999-2001, Oliver Schmidt
  Copyright (C) 2002-2005, Tom Charlesworth
  Copyright (C) 2006-2021, Tom Charlesworth, Michael Pohoreski

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

#include "StdAfx.h"

#include "Memory.h"
#include "VidHD.h"
#include "YamlHelper.h"

void VidHDCard::InitializeIO(LPBYTE pCxRomPeripheral)
{
	RegisterIoHandler(m_slot, IO_Null, IO_Null, &VidHDCard::IORead, IO_Null, this, NULL);
}

BYTE __stdcall VidHDCard::IORead(WORD pc, WORD addr, BYTE bWrite, BYTE value, ULONG nExecutedCycles)
{
	// Return magic bytes for VidHD detection
	switch (addr & 0xff)
	{
	case 0: return 0x24;
	case 1: return 0xEA;
	case 2: return 0x4C;
	}
	return IO_Null(pc, addr, bWrite, value, nExecutedCycles);
}

//===========================================================================

static const UINT kUNIT_VERSION = 1;

std::string VidHDCard::GetSnapshotCardName(void)
{
	static const std::string name("VidHD");
	return name;
}

void VidHDCard::SaveSnapshot(class YamlSaveHelper& yamlSaveHelper)
{

}

bool VidHDCard::LoadSnapshot(class YamlLoadHelper& yamlLoadHelper, UINT slot, UINT version)
{
	return false;
}
