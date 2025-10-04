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
/*
  Breakpoint card
*/

#include "StdAfx.h"

#include "BreakpointCard.h"
#include "Memory.h"

void BreakpointCard::Reset(const bool powerCycle)
{
	m_interceptBPByCard = false;
	ResetState();
}

void BreakpointCard::InitializeIO(LPBYTE pCxRomPeripheral)
{
	RegisterIoHandler(m_slot, &BreakpointCard::IORead, &BreakpointCard::IOWrite, IO_Null, IO_Null, this, NULL);
}

BYTE __stdcall BreakpointCard::IORead(WORD pc, WORD addr, BYTE bWrite, BYTE value, ULONG nExecutedCycles)
{
	const UINT slot = ((addr & 0xff) >> 4) - 8;
	BreakpointCard* pCard = (BreakpointCard*)MemGetSlotParameters(slot);

	return pCard->m_status;
}

BYTE __stdcall BreakpointCard::IOWrite(WORD pc, WORD addr, BYTE bWrite, BYTE value, ULONG nExecutedCycles)
{
	const UINT slot = ((addr & 0xff) >> 4) - 8;
	BreakpointCard* pCard = (BreakpointCard*)MemGetSlotParameters(slot);

	const BYTE reg = addr & 0xf;

	if (reg == 0)	// Command
	{
		switch (value)
		{
		case 0: // reset
			pCard->ResetState();
			break;
		case 1: // intercept BP by card
			pCard->m_interceptBPByCard = true;
			break;
		case 2: // intercept BP by debugger
			pCard->m_interceptBPByCard = false;
			break;
		default:
			break;
		}
	}
	else if (reg == 1)	// FIFO
	{
		pCard->m_BPSet[pCard->m_BPSetIdx++] = value;

		if (pCard->m_BPSetIdx == kNumParams)
		{
			pCard->m_BPSetIdx = 0;

			if (!(pCard->m_status & kFull))
			{
				BPSet bpSet;
				bpSet.slot = pCard->m_BPSet[0];
				bpSet.bank = (pCard->m_BPSet[2] << 8) | pCard->m_BPSet[1];
				bpSet.langCard = pCard->m_BPSet[3];
				bpSet.addr = (pCard->m_BPSet[5] << 8) | pCard->m_BPSet[4];
				bpSet.rw = pCard->m_BPSet[6];

				pCard->m_BP_FIFO.push_back(bpSet);
			}

			if (pCard->m_BP_FIFO.size() == kFIFO_SIZE)
				pCard->m_status |= kFull;
		}
	}

	return 0;
}
