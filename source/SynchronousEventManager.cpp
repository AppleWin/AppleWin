/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 1994-1996, Michael O'Brien
Copyright (C) 1999-2001, Oliver Schmidt
Copyright (C) 2002-2005, Tom Charlesworth
Copyright (C) 2006-2020, Tom Charlesworth, Michael Pohoreski, Nick Westgate

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

/* Description: Synchronous Event Manager
 *
 * This manager class maintains a linked-list of ordered timer-based event,
 * where only the head of the list needs updating after every opcode.
 *
 * The Nth event in the list will expire in: event[1] + ... + event[N] cycles time.
 * (So each event has a cycle delta expiry time relative to the previous event.)
 *
 * A synchronous event is used for a deterministic event that will occur in N cycles' time,
 * eg. 6522 timer & Mousecard VBlank. (As opposed to async events, like SSC Rx/Tx interrupts.)
 *
 * Events that are active in the list can be removed before they expire,
 * eg. 6522 timer when the interval changes.
 *
 * Author: Various
 *
 */

#include "StdAfx.h"

#include "SynchronousEventManager.h"

void SynchronousEventManager::Insert(SyncEvent* pNewEvent)
{
	pNewEvent->m_active = true;	// add always succeeds

	if (!m_syncEventHead)
	{
		m_syncEventHead = pNewEvent;
		return;
	}

	// walk list to find where to insert new event

	SyncEvent* pPrevEvent = NULL;
	SyncEvent* pCurrEvent = m_syncEventHead;
	int newEventExtraCycles = pNewEvent->m_cyclesRemaining;

	while (pCurrEvent)
	{
		if (newEventExtraCycles >= pCurrEvent->m_cyclesRemaining)
		{
			newEventExtraCycles -= pCurrEvent->m_cyclesRemaining;

			pPrevEvent = pCurrEvent;
			pCurrEvent = pCurrEvent->m_next;

			if (!pCurrEvent)	// end of list
			{
				pPrevEvent->m_next = pNewEvent;
				pNewEvent->m_cyclesRemaining = newEventExtraCycles;
			}

			continue;
		}

		// insert new event
		if (!pPrevEvent)
			m_syncEventHead = pNewEvent;
		else
			pPrevEvent->m_next = pNewEvent;
		pNewEvent->m_next = pCurrEvent;

		pNewEvent->m_cyclesRemaining = newEventExtraCycles;

		// update cycles for next event
		if (pCurrEvent)
		{
			pCurrEvent->m_cyclesRemaining -= newEventExtraCycles;
			_ASSERT(pCurrEvent->m_cyclesRemaining >= 0);
		}

		return;
	}
}

bool SynchronousEventManager::Remove(int id)
{
	SyncEvent* pPrevEvent = NULL;
	SyncEvent* pCurrEvent = m_syncEventHead;

	while (pCurrEvent)
	{
		if (pCurrEvent->m_id != id)
		{
			pPrevEvent = pCurrEvent;
			pCurrEvent = pCurrEvent->m_next;
			continue;
		}

		// remove event
		if (!pPrevEvent)
			m_syncEventHead = pCurrEvent->m_next;
		else
			pPrevEvent->m_next = pCurrEvent->m_next;

		int oldEventExtraCycles = pCurrEvent->m_cyclesRemaining;
		pPrevEvent = pCurrEvent;
		pCurrEvent = pCurrEvent->m_next;

		pPrevEvent->m_active = false;
		pPrevEvent->m_next = NULL;

		// update cycles for next event
		if (pCurrEvent)
			pCurrEvent->m_cyclesRemaining += oldEventExtraCycles;

		return true;
	}

	_ASSERT(0);
	return false;
}

extern bool g_irqOnLastOpcodeCycle;

void SynchronousEventManager::Update(int cycles, ULONG uExecutedCycles)
{
	SyncEvent* pCurrEvent = m_syncEventHead;

	if (!pCurrEvent)
		return;

	pCurrEvent->m_cyclesRemaining -= cycles;
	if (pCurrEvent->m_cyclesRemaining <= 0)
	{
		if (pCurrEvent->m_cyclesRemaining == 0)
			g_irqOnLastOpcodeCycle = true;		// IRQ occurs on last cycle of opcode

		int cyclesUnderflowed = -pCurrEvent->m_cyclesRemaining;

		pCurrEvent->m_cyclesRemaining = pCurrEvent->m_callback(pCurrEvent->m_id, cycles, uExecutedCycles);
		m_syncEventHead = pCurrEvent->m_next;	// unlink this event

		pCurrEvent->m_active = false;
		pCurrEvent->m_next = NULL;

		// Always Update even if cyclesUnderflowed=0, as next event may have cycleRemaining=0 (ie. the 2 events fire at the same time)
		Update(cyclesUnderflowed, uExecutedCycles);	// update (potential) next event with underflow cycles

		if (pCurrEvent->m_cyclesRemaining)
			Insert(pCurrEvent);	// re-add event
	}
}
