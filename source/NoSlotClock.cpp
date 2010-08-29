/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 1994-1996, Michael O'Brien
Copyright (C) 1999-2001, Oliver Schmidt
Copyright (C) 2002-2005, Tom Charlesworth
Copyright (C) 2006-2010, Tom Charlesworth, Michael Pohoreski, Nick Westgate

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

/* Description: No Slot Clock/Phantom Clock (Dallas SmartWatch DS1216) emulation
 *
 * Author: Nick Westgate
 */

#include "StdAfx.h"
#include "NoSlotClock.h"

CNoSlotClock::CNoSlotClock()
:
	m_ClockRegister(),
	m_ComparisonRegister(kClockInitSequence)
{
	Reset();
}

void CNoSlotClock::Reset()
{
	// SmartWatch reset - whether tied to system reset is component specific
	m_ComparisonRegister.Reset();
	m_bClockRegisterEnabled = false;
	m_bWriteEnabled = true;
}

bool CNoSlotClock::Read(int address, int& data)
{
	// this may read or write the clock (returns true if data is changed)
	if (address & 0x04)
		return ClockRead(data);
	else
	{
		ClockWrite(address);
		return false;
	}
}

void CNoSlotClock::Write(int address)
{
	// this may read or write the clock
	int dummy = 0;
	if (address & 0x04)
		ClockRead(dummy);
	else
		ClockWrite(address);
}

bool CNoSlotClock::ClockRead(int& data)
{
	// for a ROM, A2 high = read, and data out (if any) is on D0
	if (!m_bClockRegisterEnabled)
	{
		m_ComparisonRegister.Reset();
		m_bWriteEnabled = true;
		return false;
	}
	else
	{
		m_ClockRegister.ReadBit(data);
		if (m_ClockRegister.NextBit())
			m_bClockRegisterEnabled = false;
		return true;
	}
}

void CNoSlotClock::ClockWrite(int address)
{
	// for a ROM, A2 low = write, and data in is on A0
	if (!m_bWriteEnabled)
		return;

	if (!m_bClockRegisterEnabled)
	{
		if ((m_ComparisonRegister.CompareBit(address & 0x1)))
		{
			if (m_ComparisonRegister.NextBit())
			{
				m_bClockRegisterEnabled = true;
				PopulateClockRegister();
			}
		}
		else
		{
			// mismatch ignores further writes
			m_bWriteEnabled = false;
		}
	}
	else if (m_ClockRegister.NextBit())
	{
		// simulate writes, but our clock register is read-only
		m_bClockRegisterEnabled = false;
	}
}

void CNoSlotClock::PopulateClockRegister()
{
	// all values are in packed BCD format (4 bits per decimal digit)
	SYSTEMTIME now;
	GetLocalTime(&now);

	int centisecond = now.wMilliseconds / 10; // 00-99
	m_ClockRegister.WriteNibble(centisecond % 10);
	m_ClockRegister.WriteNibble(centisecond / 10);

	int second = now.wSecond; // 00-59
	m_ClockRegister.WriteNibble(second % 10);
	m_ClockRegister.WriteNibble(second / 10);

	int minute = now.wMinute; // 00-59
	m_ClockRegister.WriteNibble(minute % 10);
	m_ClockRegister.WriteNibble(minute / 10);

	int hour = now.wHour; // 01-23
	m_ClockRegister.WriteNibble(hour % 10);
	m_ClockRegister.WriteNibble(hour / 10);

	int day = now.wDayOfWeek + 1; // 01-07 (1 = Sunday)
	m_ClockRegister.WriteNibble(day % 10);
	m_ClockRegister.WriteNibble(day / 10);

	int date = now.wDay; // 01-31
	m_ClockRegister.WriteNibble(date % 10);
	m_ClockRegister.WriteNibble(date / 10);

	int month = now.wMonth; // 01-12
	m_ClockRegister.WriteNibble(month % 10);
	m_ClockRegister.WriteNibble(month / 10);

	int year = now.wYear % 100; // 00-99
	m_ClockRegister.WriteNibble(year % 10);
	m_ClockRegister.WriteNibble(year / 10);
}

CNoSlotClock::RingRegister64::RingRegister64()
{
	Reset();
	m_Register = 0;
}

CNoSlotClock::RingRegister64::RingRegister64(UINT64 data)
{
	Reset();
	m_Register = data;
}

void CNoSlotClock::RingRegister64::Reset()
{
	m_Mask = 1;
}

void CNoSlotClock::RingRegister64::WriteNibble(int data)
{
	WriteBits(data, 4);
}

void CNoSlotClock::RingRegister64::WriteBits(int data, int count)
{
	for (int i = 1; i <= count; i++)
	{
		WriteBit(data);
		NextBit();
		data >>= 1;
	}
}

void CNoSlotClock::RingRegister64::WriteBit(int data)
{
	m_Register = (data & 0x1) ? (m_Register | m_Mask) : (m_Register & ~m_Mask);
}

void CNoSlotClock::RingRegister64::ReadBit(int& data)
{
	data = (m_Register & m_Mask) ? data | 1 : data & ~1;
}

bool CNoSlotClock::RingRegister64::CompareBit(int data)
{
	return ((m_Register & m_Mask) != 0) == ((data & 1) != 0);
}

bool CNoSlotClock::RingRegister64::NextBit()
{
	if ((m_Mask <<= 1) == 0)
	{
		m_Mask = 1;
		return true; // wrap
	}
	return false;
}
