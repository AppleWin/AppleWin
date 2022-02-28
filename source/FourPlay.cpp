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
  Bit 4 = Trigger 2, Active High
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
#include "Memory.h"
#include "YamlHelper.h"

BYTE __stdcall FourPlayCard::IORead(WORD pc, WORD addr, BYTE bWrite, BYTE value, ULONG nExecutedCycles)
{
	BYTE nOutput = MemReadFloatingBus(nExecutedCycles);
	BOOL up = 0;
	BOOL down = 0;
	BOOL left = 0;
	BOOL right = 0;
	BOOL trigger1 = 0;
	BOOL trigger2 = 0;
	BOOL trigger3 = 0;
	BOOL alwaysHigh = 1;
	UINT xAxis = 0;
	UINT yAxis = 0;

	JOYINFOEX infoEx;
	MMRESULT result = 0;
	infoEx.dwSize = sizeof(infoEx);
	infoEx.dwFlags = JOY_RETURNPOV | JOY_RETURNBUTTONS;

	switch (addr & 0xF)
	{
	case 0: // Joystick 1
		result = joyGetPosEx(JOYSTICKID1, &infoEx);
		if (result == JOYERR_NOERROR)
		{
			xAxis = (infoEx.dwXpos >> 8) & 0xFF;
			yAxis = (infoEx.dwYpos >> 8) & 0xFF;
			trigger1 = infoEx.dwButtons & 0x01;
			trigger2 = (infoEx.dwButtons & 0x02) >> 1;
			up = yAxis < 103 || infoEx.dwPOV == 0 || infoEx.dwPOV == 4500 || infoEx.dwPOV == 31500;
			down = yAxis > 153 || (infoEx.dwPOV >= 13500 && infoEx.dwPOV <= 22500);
			left = xAxis < 103 || (infoEx.dwPOV >= 22500 && infoEx.dwPOV <= 31500);
			right = xAxis > 153 || (infoEx.dwPOV >= 4500 && infoEx.dwPOV <= 13500);
		}
		nOutput = up | (down << 1) | (left << 2) | (right << 3) | (alwaysHigh << 5) | (trigger2 << 6) | (trigger1 << 7);
		break;
	case 1: // Joystick 2
		result = joyGetPosEx(JOYSTICKID2, &infoEx);
		if (result == JOYERR_NOERROR)
		{
			xAxis = (infoEx.dwXpos >> 8) & 0xFF;
			yAxis = (infoEx.dwYpos >> 8) & 0xFF;
			trigger1 = infoEx.dwButtons & 0x01;
			trigger2 = (infoEx.dwButtons & 0x02) >> 1;
			up = yAxis < 103 || infoEx.dwPOV == 0 || infoEx.dwPOV == 4500 || infoEx.dwPOV == 31500;
			down = yAxis > 153 || (infoEx.dwPOV >= 13500 && infoEx.dwPOV <= 22500);
			left = xAxis < 103 || (infoEx.dwPOV >= 22500 && infoEx.dwPOV <= 31500);
			right = xAxis > 153 || (infoEx.dwPOV >= 4500 && infoEx.dwPOV <= 13500);
		}
		nOutput = up | (down << 1) | (left << 2) | (right << 3) | (alwaysHigh << 5) | (trigger2 << 6) | (trigger1 << 7);
		break;
	case 2: // Joystick 3
		nOutput = FourPlayCard::JOYSTICKSTATIONARY; // esdf - direction buttons, zx - trigger buttons
		nOutput = nOutput | (MyGetAsyncKeyState('E') | (MyGetAsyncKeyState('D') << 1) | (MyGetAsyncKeyState('S') << 2) | (MyGetAsyncKeyState('F') << 3) | (MyGetAsyncKeyState('X') << 6) | (MyGetAsyncKeyState('Z') << 7));
		break;
	case 3: // Joystick 4
		nOutput = FourPlayCard::JOYSTICKSTATIONARY; // ijkl - direction buttons, nm - trigger buttons
		nOutput = nOutput | (MyGetAsyncKeyState('I') | (MyGetAsyncKeyState('K') << 1) | (MyGetAsyncKeyState('J') << 2) | (MyGetAsyncKeyState('L') << 3) | (MyGetAsyncKeyState('M') << 6) | (MyGetAsyncKeyState('N') << 7));
		break;
	default:
		break;
	}

	return nOutput;
}

BYTE FourPlayCard::MyGetAsyncKeyState(int vKey)
{
	return GetAsyncKeyState(vKey) < 0 ? 1 : 0;
}

void FourPlayCard::InitializeIO(LPBYTE pCxRomPeripheral)
{
	RegisterIoHandler(m_slot, &FourPlayCard::IORead, IO_Null, IO_Null, IO_Null, this, NULL);
}

//===========================================================================

static const UINT kUNIT_VERSION = 1;

const std::string& FourPlayCard::GetSnapshotCardName(void)
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
