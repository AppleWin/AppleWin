/*
  AppleWin : An Apple //e emulator for Windows

  Copyright (C) 1994-1996, Michael O'Brien
  Copyright (C) 1999-2001, Oliver Schmidt
  Copyright (C) 2002-2005, Tom Charlesworth
  Copyright (C) 2006-2025, Tom Charlesworth, Michael Pohoreski

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
/*
  Emulate an Echo II card (Street Electronics)

  Info: ...
*/

#include "StdAfx.h"

#include "SoundCore.h"
#include "CPU.h"
#include "EchoII.h"
#include "Memory.h"
#include "YamlHelper.h"

void EchoII::Reset(const bool powerCycle)
{
	m_tms5220.Reset(powerCycle);
}

void EchoII::InitializeIO(LPBYTE pCxRomPeripheral)
{
	RegisterIoHandler(m_slot, &EchoII::IORead, &EchoII::IOWrite, IO_Null, IO_Null, this, NULL);
}

//-----------------------------------------------------------------------------

void EchoII::MuteControl(bool mute)
{
	if (mute)
		m_tms5220.Mute();
	else
		m_tms5220.Unmute();
}

//-----------------------------------------------------------------------------

#ifdef _DEBUG
void EchoII::CheckCumulativeCycles()
{
	_ASSERT(m_lastCumulativeCycle == g_nCumulativeCycles);
	m_lastCumulativeCycle = g_nCumulativeCycles;
}
#endif

// Called by: ResetState() and Snapshot_LoadState_v2()
void EchoII::SetCumulativeCycles()
{
	m_lastCumulativeCycle = g_nCumulativeCycles;
}

// Called by ContinueExecution() at the end of every execution period (~1000 cycles or ~3 cycles when MODE_STEPPING)
void EchoII::Update(const ULONG executedCycles)
{
	m_tms5220.PeriodicUpdate(executedCycles);
}

//-----------------------------------------------------------------------------

#if 0
// Called by:
// . TODO: Do this? CpuExecute() every ~1000 cycles @ 1MHz (or ~3 cycles when MODE_STEPPING)
// . TODO: Do this? IORead() / IOWrite() (for both normal & full-speed)
void EchoII::UpdateCycles(ULONG executedCycles)
{
	CpuCalcCycles(executedCycles);
	UINT64 uCycles = g_nCumulativeCycles - m_lastCumulativeCycle;
	_ASSERT(uCycles >= 0);
	if (uCycles == 0)
		return;

	m_lastCumulativeCycle = g_nCumulativeCycles;
}
#endif

//-----------------------------------------------------------------------------

BYTE __stdcall EchoII::IORead(WORD pc, WORD addr, BYTE bWrite, BYTE value, ULONG nExecutedCycles)
{
	const UINT slot = ((addr & 0xff) >> 4) - 8;
	EchoII* pCard = (EchoII*)MemGetSlotParameters(slot);

	return pCard->m_tms5220.GetStatus();
}

BYTE __stdcall EchoII::IOWrite(WORD pc, WORD addr, BYTE bWrite, BYTE value, ULONG nExecutedCycles)
{
	const UINT slot = ((addr & 0xff) >> 4) - 8;
	EchoII* pCard = (EchoII*)MemGetSlotParameters(slot);

	pCard->m_tms5220.Write(value);
	return 0;
}

// Called from class TMS5220
UINT64 EchoII::GetLastCumulativeCycles()
{
	return m_lastCumulativeCycle;
}

//===========================================================================

static const UINT kUNIT_VERSION = 1;

const std::string& EchoII::GetSnapshotCardName()
{
	static const std::string name("Echo II");
	return name;
}

void EchoII::SaveSnapshot(YamlSaveHelper& yamlSaveHelper)
{

}

bool EchoII::LoadSnapshot(YamlLoadHelper& yamlLoadHelper, UINT version)
{
	return false;
}
