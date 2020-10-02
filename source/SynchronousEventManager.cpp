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
 * Author: Various
 *
 */

#include "StdAfx.h"

#include "AppleWin.h"
#include "SynchronousEventManager.h"

void SynchronousEventManager::Add(SyncEvent* pNewEvent)
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

	do
	{
		if (newEventExtraCycles >= pCurrEvent->m_cyclesRemaining)
		{
			newEventExtraCycles -= pCurrEvent->m_cyclesRemaining;
			_ASSERT(newEventExtraCycles >= 0);
		}
		else
		{
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

		pPrevEvent = pCurrEvent;
		pCurrEvent = pCurrEvent->m_next;

		if (!pCurrEvent)	// end of list
		{
			pPrevEvent->m_next = pNewEvent;
			pNewEvent->m_cyclesRemaining = newEventExtraCycles;
		}
	}
	while (pCurrEvent);
}

bool SynchronousEventManager::Remove(int id)
{
	SyncEvent* pPrevEvent = NULL;
	SyncEvent* pCurrEvent = m_syncEventHead;

	while (pCurrEvent)
	{
		if (pCurrEvent->m_id == id)
		{
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

		pPrevEvent = pCurrEvent;
		pCurrEvent = pCurrEvent->m_next;
	}

	_ASSERT(0);
	return false;
}

void SynchronousEventManager::Update(int cycles)
{
	SyncEvent* pCurrEvent = m_syncEventHead;

	if (!pCurrEvent)
		return;

	pCurrEvent->m_cyclesRemaining -= cycles;
	if (pCurrEvent->m_cyclesRemaining < 0)
	{
		pCurrEvent->m_cyclesRemaining = pCurrEvent->m_callback(pCurrEvent->m_id);
		m_syncEventHead = pCurrEvent->m_next;	// unlink this event
		pCurrEvent->m_next = NULL;

		if (pCurrEvent->m_cyclesRemaining)
			Add(pCurrEvent);	// re-add event
	}
}
