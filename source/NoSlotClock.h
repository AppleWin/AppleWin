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

/* Description: No Slot Clock (Dallas SmartWatch) emulation
 *
 * Author: Nick Westgate
 */

#pragma once

class CNoSlotClock
{
	class RingRegister
	{
	public:
		RingRegister(int bitCount);
		RingRegister(BYTE* bytes, int byteCount);
		~RingRegister();

		void Reset();
		void WriteByte(int data);
		void WriteNibble(int data);
		void WriteBits(int data, int count);
		bool WriteBit(int data);
		bool ReadBit(int& data);
		bool CompareBitNoIncrement(int data);
		bool IncrementPointer();

	private:
		RingRegister() {};

		int m_Pointer;
		int m_PointerWrap;
		int* m_pRegister;
	};

public:
	CNoSlotClock();

	void Reset();
	bool ReadAccess(int address, int& data);
	void WriteAccess(int address);
	bool ClockRead(int& d0);
	void ClockWrite(int address);

private:
	void PopulateClockRegister();

	bool m_bClockRegisterEnabled;
	bool m_bWriteEnabled;
	RingRegister m_ClockRegister;
	RingRegister m_ComparisonRegister;
};
