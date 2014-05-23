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

#pragma once

class CNoSlotClock
{
	class RingRegister64
	{
	public:
		RingRegister64();
		RingRegister64(UINT64 data);

		void Reset();
		void WriteNibble(int data);
		void WriteBits(int data, int count);
		void WriteBit(int data);
		void ReadBit(int& data);
		bool CompareBit(int data);
		bool NextBit();

		UINT64 m_Mask;
		UINT64 m_Register;
	};

public:
	CNoSlotClock();

	void Reset();
	bool Read(int address, int& data);
	void Write(int address);
	bool ClockRead(int& data);
	void ClockWrite(int address);

	bool m_bClockRegisterEnabled;
	bool m_bWriteEnabled;
	RingRegister64 m_ClockRegister;
	RingRegister64 m_ComparisonRegister;

private:
	void PopulateClockRegister();

	static const UINT64 kClockInitSequence = 0x5CA33AC55CA33AC5;
};
