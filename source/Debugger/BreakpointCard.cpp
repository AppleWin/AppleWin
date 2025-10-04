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

#include "CPU.h"
#include "Debug.h"
#include "BreakpointCard.h"
#include "Memory.h"

void BreakpointCard::Reset(const bool powerCycle)
{
	m_interceptBPByCard = false;
	InterceptBreakpoints(m_slot, nullptr);
	ResetState();
	CpuIrqDeassert(IS_BREAKPOINTCARD);
}

void BreakpointCard::InitializeIO(LPBYTE pCxRomPeripheral)
{
	RegisterIoHandler(m_slot, &BreakpointCard::IORead, &BreakpointCard::IOWrite, IO_Null, IO_Null, this, NULL);
}

BYTE __stdcall BreakpointCard::IORead(WORD pc, WORD addr, BYTE bWrite, BYTE value, ULONG nExecutedCycles)
{
	const UINT slot = ((addr & 0xff) >> 4) - 8;
	BreakpointCard* pCard = (BreakpointCard*)MemGetSlotParameters(slot);

	CpuIrqDeassert(IS_BREAKPOINTCARD);

	uint8_t otherStatus = pCard->m_BP_FIFO.empty() ? kEmpty : 0;

	return pCard->m_status | otherStatus;
}

BYTE __stdcall BreakpointCard::IOWrite(WORD pc, WORD addr, BYTE bWrite, BYTE value, ULONG nExecutedCycles)
{
	const UINT slot = ((addr & 0xff) >> 4) - 8;
	BreakpointCard* pCard = (BreakpointCard*)MemGetSlotParameters(slot);

	CpuIrqDeassert(IS_BREAKPOINTCARD);

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
			InterceptBreakpoints(slot, BreakpointCard::CbFunction);
			break;
		case 2: // intercept BP by debugger
			pCard->m_interceptBPByCard = false;
			InterceptBreakpoints(slot, nullptr);
			break;
		default:
			break;
		}
	}
	else // FIFO (reg 1-F)
	{
		pCard->m_BPSet[pCard->m_BPSetIdx++] = value;

		if (pCard->m_BPSetIdx == kNumParams)
		{
			pCard->m_BPSetIdx = 0;

			if (!(pCard->m_status & kFull))
			{
				BPSet bpSet;
				bpSet.type = pCard->m_BPSet[0];
				bpSet.addrStart = (pCard->m_BPSet[2] << 8) | pCard->m_BPSet[1];
				bpSet.addrEnd   = (pCard->m_BPSet[4] << 8) | pCard->m_BPSet[3];
				bpSet.access = pCard->m_BPSet[5];

				pCard->m_BP_FIFO.push(bpSet);
			}

			if (pCard->m_BP_FIFO.size() == kFIFO_SIZE)
				pCard->m_status |= kFull;
		}
	}

	return 0;
}

void BreakpointCard::CbFunction(uint8_t slot, uint8_t type, uint16_t addrStart, uint16_t addrEnd, uint8_t access)
{
	BreakpointCard* pCard = (BreakpointCard*)MemGetSlotParameters(slot);

	pCard->m_status &= ~(kMatch | kMismatch);

	CpuIrqAssert(IS_BREAKPOINTCARD);

	if (pCard->m_BP_FIFO.empty())
	{
		pCard->m_status |= kMismatch;
		return;
	}

	const BPSet bpSet = pCard->m_BP_FIFO.front();
	pCard->m_BP_FIFO.pop();
	pCard->m_status &= ~kFull;

	if (bpSet.type != type)
	{
		pCard->m_status |= kMismatch;
		return;
	}

	switch (type)
	{
	case BPTYPE_PC:
	case BPTYPE_MEM:
		if ((bpSet.addrStart != addrStart) ||
			(bpSet.access != access))
		{
			pCard->m_status |= kMismatch;
			break;
		}
		pCard->m_status |= kMatch;
		break;
	case BPTYPE_DMA:
		// dma S dma E
		//     S dma E
		//     S dma E dma
		if ((bpSet.addrStart >= addrStart && addrStart <= bpSet.addrEnd) ||	// Does DMA start within BP range?
			(bpSet.addrStart >= addrEnd   && addrEnd   <= bpSet.addrEnd) ||	// Does DMA end within BP range?
			(bpSet.access != access))
		{
			pCard->m_status |= kMismatch;
			break;
		}
		pCard->m_status |= kMatch;
		break;
	case BPTYPE_UNKNOWN:
	default:
		pCard->m_status |= kMismatch;
		break;
	}

}
