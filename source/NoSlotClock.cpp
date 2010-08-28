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

/* Description: No Slot Clock (Dallas SmartWatch DS1216) emulation
 *
 * Author: Nick Westgate
 */

#include "StdAfx.h"
#include "NoSlotClock.h"

static byte ClockInitSequence[] = { 0xC5, 0x3A, 0xA3, 0x5C, 0xC5, 0x3A, 0xA3, 0x5C };

CNoSlotClock::CNoSlotClock()
:
	m_ClockRegister(64),
	m_ComparisonRegister(ClockInitSequence, sizeof(ClockInitSequence))
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

bool CNoSlotClock::ReadAccess(int address, int& data)
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

void CNoSlotClock::WriteAccess(int address)
{
	// this may read or write the clock
	int dummy = 0;
	if (address & 0x04)
		ClockRead(dummy);
	else
		ClockWrite(address);
}

bool CNoSlotClock::ClockRead(int& d0)
{
	// for a ROM, A2 high = read, and data out (if any) is on D0
	if (!m_bClockRegisterEnabled)
	{
		m_ComparisonRegister.Reset();
		m_bWriteEnabled = true;
		return false;
	}
	else if (m_ClockRegister.ReadBit(d0))
	{
		m_bClockRegisterEnabled = false;
	}
	return true;
}

void CNoSlotClock::ClockWrite(int address)
{
	// for a ROM, A2 low = write, and data in is on A0
	if (!m_bWriteEnabled)
		return;

	if (!m_bClockRegisterEnabled)
	{
		if ((m_ComparisonRegister.CompareBitNoIncrement(address & 0x1)))
		{
			if (m_ComparisonRegister.IncrementPointer())
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
	else if (m_ClockRegister.IncrementPointer())
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

	m_ClockRegister.Reset();

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

CNoSlotClock::RingRegister::~RingRegister()
{
	delete[] m_pRegister;
}

CNoSlotClock::RingRegister::RingRegister(int bitCount)
{
	Reset();

	m_pRegister = new int[bitCount];
	m_PointerWrap = bitCount;
}

CNoSlotClock::RingRegister::RingRegister(byte* bytes, int byteCount)
{
	Reset();

	m_PointerWrap = byteCount * 8;
	m_pRegister = new int[m_PointerWrap];

	for (int i = 0; i < byteCount; i++)
		WriteByte(bytes[i]);
}

void CNoSlotClock::RingRegister::Reset()
{
	m_Pointer = 0;
}

void CNoSlotClock::RingRegister::WriteByte(int data)
{
	WriteBits(data, 8);
}

void CNoSlotClock::RingRegister::WriteNibble(int data)
{
	WriteBits(data, 4);
}

void CNoSlotClock::RingRegister::WriteBits(int data, int count)
{
	for (int i = 1; i <= count; i++)
	{
		WriteBit(data);
		data >>= 1;
	}
}

bool CNoSlotClock::RingRegister::WriteBit(int data)
{
	m_pRegister[m_Pointer] = data & 1;
	return IncrementPointer();
}

bool CNoSlotClock::RingRegister::ReadBit(int& data)
{
	data = m_pRegister[m_Pointer];
	return IncrementPointer();
}

bool CNoSlotClock::RingRegister::CompareBitNoIncrement(int data)
{
	return m_pRegister[m_Pointer] == data;
}

bool CNoSlotClock::RingRegister::IncrementPointer()
{
	if (++m_Pointer == m_PointerWrap)
	{
		m_Pointer = 0;
		return true; // wrap
	}
	return false;
}
