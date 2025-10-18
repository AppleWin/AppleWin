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
  Breakpoint card
  ---------------

  For debugger breakpoint regression tests.

  C0n0 (R) : Status
				b7: BP match
				b6: BP mismatch
				b1: FIFO full
				b0: FIFO empty
  C0n1 (R) : Cmd: Reset
				Flush FIFO
				NB. don't change intercept mode
  C0n2 (R) : Cmd: Intercept BP by card
  C0n3 (R) : Cmd: Intercept BP by debugger
  C0nX (R) : ID byte $0X (except when X=0)

  C0nX (W) : FIFO
				FIFO is 32 x 6 bytes deep
				where: 6-byte BP-set = {type.b, addrStart.w, addrEnd.w, access.b}

  Any r/w clears the IRQ
  RESET clears the IRQ, flushes FIFO and sets: Intercept BP by card

  Operation:
  . Cmd:Reset flushes FIFO & sets Status.empty=1 (all other bits are zero)
  . Cmd:Intercept BP by card
  . Write FIFO with multiple BP-sets for test code
    - these are the expected BP hit results
  . Status.full=1 after writing 32 BP-sets
  . Execute test code
  . AppleWin debugger:
    - Cmd:Intercept BP by card: when active, then debugger doesn't break on BP match
    - Instead debugger hands off BP to Breakpoint card (only 'bp' & 'bpm[r|w]')
  . Remove BP-set from front of FIFO
    - Check if it matches/mismatches and set status accordingly
  . Delay 1 cycle & assert IRQ
  . CPU vectors to IRQ handler
  . Reading Status clears IRQ
*/

#include "StdAfx.h"

#include "CardManager.h"
#include "Core.h"
#include "CPU.h"
#include "Debug.h"
#include "BreakpointCard.h"
#include "Memory.h"

void BreakpointCard::Reset(const bool powerCycle)
{
	m_interceptBPByCard = false;
	InterceptBreakpoints(m_slot, nullptr);
	ResetState();
	if (!powerCycle)	// if called by ctor: CriticalSection not created yet
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

	const BYTE reg = addr & 0xf;
	if (reg == 0)
	{
		uint8_t otherStatus = pCard->m_BP_FIFO.empty() ? kEmpty : 0;
		return pCard->m_status | otherStatus;
	}
	else if (reg == 1)	// Cmd: Reset
	{
		pCard->ResetState();
	}
	else if (reg == 2)	// Cmd: Intercept BP by card
	{
		pCard->m_interceptBPByCard = true;
		InterceptBreakpoints(slot, BreakpointCard::CbFunction);
	}
	else if (reg == 3)	// Cmd: Intercept BP by debugger
	{
		pCard->m_interceptBPByCard = false;
		InterceptBreakpoints(slot, nullptr);
	}

	return addr & 0x0f;	// ID bytes [$01-$0F]
}

BYTE __stdcall BreakpointCard::IOWrite(WORD pc, WORD addr, BYTE bWrite, BYTE value, ULONG nExecutedCycles)
{
	const UINT slot = ((addr & 0xff) >> 4) - 8;
	BreakpointCard* pCard = (BreakpointCard*)MemGetSlotParameters(slot);

	CpuIrqDeassert(IS_BREAKPOINTCARD);

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

	return 0;
}

void BreakpointCard::CbFunction(uint8_t slot, INTERCEPTBREAKPOINT interceptBreakpoint)
{
	BreakpointCard* pCard = (BreakpointCard*)MemGetSlotParameters(slot);

	pCard->m_deferred.type = interceptBreakpoint.m_type;
	pCard->m_deferred.addrStart = interceptBreakpoint.m_addrStart;
	pCard->m_deferred.addrEnd= interceptBreakpoint.m_addrEnd;
	pCard->m_deferred.access = interceptBreakpoint.m_access;

	// Defer processing the breakpoint by 1 opcode (ie. 1 cycle), otherwise this happens:
	// . BP occurs at <addr>, but opcode hasn't executed yet
	// . Breakpoint card aserts IRQ, and IRQ is taken by 6502
	// . 6502 executes ISR, and returns to <addr>
	// . BP occurs at <addr>...

	pCard->m_syncEvent.m_cyclesRemaining = 1;	// Next opcode
	g_SynchronousEventMgr.Insert(&pCard->m_syncEvent);
}

int BreakpointCard::SyncEventCallback(int id, int cycles, ULONG uExecutedCycles)
{
	BreakpointCard& card = dynamic_cast<BreakpointCard&>(GetCardMgr().GetRef(id));
	card.Deferred(card.m_deferred.type, card.m_deferred.addrStart, card.m_deferred.addrEnd, card.m_deferred.access);
	return 0;	// Don't repeat event
}

void BreakpointCard::Deferred(uint8_t type, uint16_t addrStart, uint16_t addrEnd, uint8_t access)
{
	m_status &= ~(kMatch | kMismatch);

	CpuIrqAssert(IS_BREAKPOINTCARD);

	if (m_BP_FIFO.empty())
	{
		m_status |= kMismatch;
		return;
	}

	const BPSet bpSet = m_BP_FIFO.front();
	m_BP_FIFO.pop();
	m_status &= ~kFull;

	if (bpSet.type != type)
	{
		m_status |= kMismatch;
		return;
	}

	switch (type)
	{
	case BPTYPE_PC:
	case BPTYPE_MEM:
		if ((bpSet.addrStart != addrStart) || (bpSet.access != access))
		{
			m_status |= kMismatch;
			break;
		}
		m_status |= kMatch;
		break;
	case BPTYPE_DMA:
		{
			// S=start of BP range; E=end of BP range
			//     S dma E dma	; DMA starts within BP range [1]
			// dma S dma E		; DMA ends within BP range [2]
			// dma S dma E dma	; DMA start before & ends after BP range [3]
			//     S dma E		; DMA starts & ends within BP range [1] + [2]
			bool validAddr = ((bpSet.addrStart <= addrStart && addrStart <= bpSet.addrEnd) ||	// Does DMA *start* within BP range? [1]
							  (bpSet.addrStart <= addrEnd   && addrEnd   <= bpSet.addrEnd) ||	// Or does DMA *end* within BP range? [2]
							  (addrStart <= bpSet.addrStart && bpSet.addrEnd <= addrEnd));		// Or does DMA *start* before BP range & *end* after BP range? [3]
			if (!validAddr || (bpSet.access != access))
			{
				m_status |= kMismatch;
				break;
			}
			m_status |= kMatch;
		}
		break;
	case BPTYPE_UNKNOWN:
	default:
		m_status |= kMismatch;
		break;
	}

}
