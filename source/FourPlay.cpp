/*
  AppleWin : An Apple //e emulator for Windows

  Copyright (C) 1994-1996, Michael O'Brien
  Copyright (C) 1999-2001, Oliver Schmidt
  Copyright (C) 2002-2005, Tom Charlesworth
  Copyright (C) 2006-2007, Tom Charlesworth, Michael Pohoreski

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
  FourPlay.CPP

  Emulate a 4Play Joystick card

  4 ports (one for each of the four possible joysticks)

  Each port 
  Bit 0 = Up, Active High
  Bit 1 = Down, Active High
  Bit 2 = Left, Active High
  Bit 3 = Right, Active High
  Bit 4 = Not Used, Always Low
  Bit 5 = Not Used, Always High
  Bit 6 = Trigger 2, Active High
  Bit 7 = Trigger 1, Active High

  Address = C0nx
  n = 8 + slot number
  x = 0 - Joystick 1
      1 - Joystick 2
      2 - Joystick 3
      3 - Joystick 4

  Alex Lukacz  Aug 2021
*/
#include "StdAfx.h"

#include "FourPlay.h"
#include "Joystick.h"
#include "Memory.h"
#include "YamlHelper.h"

inline static int bool_to_bit(bool b, int shift)
{
	return b ? (1 << shift) : 0;
}

BYTE FourPlayCard::MakeByte(bool up, bool down, bool left, bool right, bool trigger1, bool trigger2)
{
	const int byte = bool_to_bit(up,       0)
				   | bool_to_bit(down,     1)
				   | bool_to_bit(left,     2)
				   | bool_to_bit(right,    3)
				   | bool_to_bit(false,    4)
				   | bool_to_bit(true,     5)
				   | bool_to_bit(trigger2, 6)
				   | bool_to_bit(trigger1, 7);
	return byte;
}

BYTE __stdcall FourPlayCard::IORead(WORD pc, WORD addr, BYTE bWrite, BYTE value, ULONG nExecutedCycles)
{
	bool up = false;
	bool down = false;
	bool left = false;
	bool right = false;
	bool trigger1 = false;
	bool trigger2 = false;

	JOYINFOEX infoEx;
	infoEx.dwSize = sizeof(infoEx);
	infoEx.dwFlags = JOY_RETURNPOV | JOY_RETURNBUTTONS;

	switch (addr & 0xF)
	{
	case 0: // Joystick 1
		if (GetJoystick1() >= 0 && joyGetPosEx(GetJoystick1(), &infoEx) == JOYERR_NOERROR)
		{
			UINT xAxis = (infoEx.dwXpos >> 8) & 0xFF;
			UINT yAxis = (infoEx.dwYpos >> 8) & 0xFF;
			trigger1 = (infoEx.dwButtons & 0x01);
			trigger2 = (infoEx.dwButtons & 0x02);
			up = yAxis < 103 || infoEx.dwPOV == 0 || infoEx.dwPOV == 4500 || infoEx.dwPOV == 31500;
			down = yAxis > 153 || (infoEx.dwPOV >= 13500 && infoEx.dwPOV <= 22500);
			left = xAxis < 103 || (infoEx.dwPOV >= 22500 && infoEx.dwPOV <= 31500);
			right = xAxis > 153 || (infoEx.dwPOV >= 4500 && infoEx.dwPOV <= 13500);
		}
		break;
	case 1: // Joystick 2
		if (GetJoystick2() >= 0 && joyGetPosEx(GetJoystick2(), &infoEx) == JOYERR_NOERROR)
		{
			UINT xAxis = (infoEx.dwXpos >> 8) & 0xFF;
			UINT yAxis = (infoEx.dwYpos >> 8) & 0xFF;
			trigger1 = (infoEx.dwButtons & 0x01);
			trigger2 = (infoEx.dwButtons & 0x02);
			up = yAxis < 103 || infoEx.dwPOV == 0 || infoEx.dwPOV == 4500 || infoEx.dwPOV == 31500;
			down = yAxis > 153 || (infoEx.dwPOV >= 13500 && infoEx.dwPOV <= 22500);
			left = xAxis < 103 || (infoEx.dwPOV >= 22500 && infoEx.dwPOV <= 31500);
			right = xAxis > 153 || (infoEx.dwPOV >= 4500 && infoEx.dwPOV <= 13500);
		}
		break;
	case 2: // Joystick 3
        // esdf - direction buttons, zx - trigger buttons
        trigger1 = MyGetAsyncKeyState('Z');
        trigger2 = MyGetAsyncKeyState('X');
        up = MyGetAsyncKeyState('E');
        down = MyGetAsyncKeyState('D');
        left = MyGetAsyncKeyState('S');
        right = MyGetAsyncKeyState('F');
		break;
	case 3: // Joystick 4
        // ijkl - direction buttons, nm - trigger buttons
        trigger1 = MyGetAsyncKeyState('N');
        trigger2 = MyGetAsyncKeyState('M');
        up = MyGetAsyncKeyState('I');
        down = MyGetAsyncKeyState('K');
        left = MyGetAsyncKeyState('J');
        right = MyGetAsyncKeyState('L');
		break;
	default:
        return MemReadFloatingBus(nExecutedCycles);
	}

    return MakeByte(up, down, left, right, trigger1, trigger2);
}

bool FourPlayCard::MyGetAsyncKeyState(int vKey)
{
	return (GetAsyncKeyState(vKey) < 0);
}

void FourPlayCard::InitializeIO(LPBYTE pCxRomPeripheral)
{
	RegisterIoHandler(m_slot, &FourPlayCard::IORead, IO_Null, IO_Null, IO_Null, this, NULL);
}

//===========================================================================

static const UINT kUNIT_VERSION = 1;

const std::string& FourPlayCard::GetSnapshotCardName()
{
	static const std::string name("4Play");
	return name;
}

void FourPlayCard::SaveSnapshot(YamlSaveHelper& yamlSaveHelper)
{
	YamlSaveHelper::Slot slot(yamlSaveHelper, GetSnapshotCardName(), m_slot, kUNIT_VERSION);

	YamlSaveHelper::Label unit(yamlSaveHelper, "%s: null\n", SS_YAML_KEY_STATE);
	// NB. No state for this card
}

bool FourPlayCard::LoadSnapshot(YamlLoadHelper& yamlLoadHelper, UINT version)
{
	if (version < 1 || version > kUNIT_VERSION)
		ThrowErrorInvalidVersion(version);

	return true;
}
