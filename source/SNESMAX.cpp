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
  SNESMAX.CPP

  Emulate a SNES MAX controller serial interface card

  2 ports (one for each of the two possible joysticks)

  Latch = write any data to C0n0
  Clock = write any data to C0n1
  Data = read from C0n0

  n = 8 + slot number

  Data 
  Bit 0 = Not Used
  Bit 1 = Not Used
  Bit 2 = Not Used
  Bit 3 = Not Used
  Bit 4 = Not Used
  Bit 5 = Not Used
  Bit 6 = Controller 2, Active Low
  Bit 7 = Controller 1, Active Low

  Once data is read, button presses (for each controller) should be stored in the following structure
  Byte 0: B:Y:Sl:St:U:D:L:R
  Byte 1: A:X:Fl:Fr:x:x:x:x

  The variable controllerXButtons will stored in the reverse order.

  Alex Lukacz  Aug 2021
*/
#include "StdAfx.h"

#include "SNESMAX.h"
#include "Memory.h"
#include "YamlHelper.h"

BYTE __stdcall SNESMAXCard::IORead(WORD pc, WORD addr, BYTE bWrite, BYTE value, ULONG nExecutedCycles)
{
	const UINT slot = ((addr & 0xff) >> 4) - 8;
	SNESMAXCard* pCard = (SNESMAXCard*)MemGetSlotParameters(slot);

	BYTE output = MemReadFloatingBus(nExecutedCycles);

	switch (addr & 0xF)
	{
	case 0:
		output = (output & 0x3F) | ((pCard->m_controller2Buttons & 0x1) << 6) | ((pCard->m_controller1Buttons & 0x1) << 7);
		break;
	default:
		break;
	}
	return output;
}

BYTE __stdcall SNESMAXCard::IOWrite(WORD pc, WORD addr, BYTE bWrite, BYTE value, ULONG nExecutedCycles)
{
	const UINT slot = ((addr & 0xff) >> 4) - 8;
	SNESMAXCard* pCard = (SNESMAXCard*)MemGetSlotParameters(slot);

	UINT xAxis = 0;
	UINT yAxis = 0;

	JOYINFOEX infoEx;
	MMRESULT result = 0;
	infoEx.dwSize = sizeof(infoEx);
	infoEx.dwFlags = JOY_RETURNPOV | JOY_RETURNBUTTONS;

	UINT& controller1Buttons = pCard->m_controller1Buttons;	// ref to object's controller1Buttons
	UINT& controller2Buttons = pCard->m_controller2Buttons;	// ref to object's controller2Buttons

	switch (addr & 0xF)
	{
	case 0: // Latch
		pCard->m_buttonIndex = 0;
		controller1Buttons = 0;
		controller2Buttons = 0;

		result = joyGetPosEx(JOYSTICKID1, &infoEx);
		if (result == JOYERR_NOERROR)
		{
			xAxis = (infoEx.dwXpos >> 8) & 0xFF;
			yAxis = (infoEx.dwYpos >> 8) & 0xFF;
			controller1Buttons = controller1Buttons | ((yAxis < 103 || infoEx.dwPOV == 0 || infoEx.dwPOV == 4500 || infoEx.dwPOV == 31500) << 4); // U Button
			controller1Buttons = controller1Buttons | ((yAxis > 153 || (infoEx.dwPOV >= 13500 && infoEx.dwPOV <= 22500)) << 5); // D Button
			controller1Buttons = controller1Buttons | ((xAxis < 103 || (infoEx.dwPOV >= 22500 && infoEx.dwPOV <= 31500)) << 6); // L Button
			controller1Buttons = controller1Buttons | ((xAxis > 153 || (infoEx.dwPOV >= 4500 && infoEx.dwPOV <= 13500)) << 7); // R Button
//			controller1Buttons = controller1Buttons | 0 * 0x1000; // spare Button
//			controller1Buttons = controller1Buttons | 0 * 0x2000; // spare Button
//			controller1Buttons = controller1Buttons | 0 * 0x4000; // spare Button
//			controller1Buttons = controller1Buttons | 0 * 0x8000; // spare Button

			if (pCard->m_altControllerType[0])
			{
				// 8BitDo NES30 PRO
				controller1Buttons = controller1Buttons | ((infoEx.dwButtons & 0x0002) >> 1); // B Button
				controller1Buttons = controller1Buttons | ((infoEx.dwButtons & 0x0010) >> 3); // Y Button
				controller1Buttons = controller1Buttons | ((infoEx.dwButtons & 0x0400) >> 8); // Sl Button
				controller1Buttons = controller1Buttons | ((infoEx.dwButtons & 0x0800) >> 8); // St Button

				controller1Buttons = controller1Buttons | ((infoEx.dwButtons & 0x0001) << 8); // A Button
				controller1Buttons = controller1Buttons | ((infoEx.dwButtons & 0x0008) << 6); // X Button
				controller1Buttons = controller1Buttons | ((infoEx.dwButtons & 0x0100) << 2) | ((infoEx.dwButtons & 0x0040) << 4); // Fl Button
				controller1Buttons = controller1Buttons | ((infoEx.dwButtons & 0x0200) << 2) | ((infoEx.dwButtons & 0x0080) << 4); // Fr Button
			}
			else
			{
				// Logitech F310, Dualshock 4
				controller1Buttons = controller1Buttons | ((infoEx.dwButtons & 0x0002) >> 1); // B Button
				controller1Buttons = controller1Buttons | ((infoEx.dwButtons & 0x0001) << 1); // Y Button
				controller1Buttons = controller1Buttons | ((infoEx.dwButtons & 0x0100) >> 6); // Sl Button
				controller1Buttons = controller1Buttons | ((infoEx.dwButtons & 0x0200) >> 6); // St Button

				controller1Buttons = controller1Buttons | ((infoEx.dwButtons & 0x0004) << 6); // A Button
				controller1Buttons = controller1Buttons | ((infoEx.dwButtons & 0x0008) << 6); // X Button
				controller1Buttons = controller1Buttons | ((infoEx.dwButtons & 0x0010) << 6) | ((infoEx.dwButtons & 0x0040) << 4); // Fl Button
				controller1Buttons = controller1Buttons | ((infoEx.dwButtons & 0x0020) << 6) | ((infoEx.dwButtons & 0x0080) << 4); // Fr Button
			}
			controller1Buttons = controller1Buttons | 0x10000; // Controller plugged in status.
		}
		controller1Buttons = ~controller1Buttons;

		result = joyGetPosEx(JOYSTICKID2, &infoEx);
		if (result == JOYERR_NOERROR)
		{
			xAxis = (infoEx.dwXpos >> 8) & 0xFF;
			yAxis = (infoEx.dwYpos >> 8) & 0xFF;
			controller2Buttons = controller2Buttons | ((yAxis < 103 || infoEx.dwPOV == 0 || infoEx.dwPOV == 4500 || infoEx.dwPOV == 31500) << 4); // U Button
			controller2Buttons = controller2Buttons | ((yAxis > 153 || (infoEx.dwPOV >= 13500 && infoEx.dwPOV <= 22500)) << 5); // D Button
			controller2Buttons = controller2Buttons | ((xAxis < 103 || (infoEx.dwPOV >= 22500 && infoEx.dwPOV <= 31500)) << 6); // L Button
			controller2Buttons = controller2Buttons | ((xAxis > 153 || (infoEx.dwPOV >= 4500 && infoEx.dwPOV <= 13500)) << 7); // R Button
//			controller2Buttons = controller2Buttons | 0 * 0x1000; // spare Button
//			controller2Buttons = controller2Buttons | 0 * 0x2000; // spare Button
//			controller2Buttons = controller2Buttons | 0 * 0x4000; // spare Button
//			controller2Buttons = controller2Buttons | 0 * 0x8000; // spare Button

			if (pCard->m_altControllerType[1])
			{
				// 8BitDo NES30 PRO
				controller2Buttons = controller2Buttons | ((infoEx.dwButtons & 0x0002) >> 1); // B Button
				controller2Buttons = controller2Buttons | ((infoEx.dwButtons & 0x0010) >> 3); // Y Button
				controller2Buttons = controller2Buttons | ((infoEx.dwButtons & 0x0400) >> 8); // Sl Button
				controller2Buttons = controller2Buttons | ((infoEx.dwButtons & 0x0800) >> 8); // St Button

				controller2Buttons = controller2Buttons | ((infoEx.dwButtons & 0x0001) << 8); // A Button
				controller2Buttons = controller2Buttons | ((infoEx.dwButtons & 0x0008) << 6); // X Button
				controller2Buttons = controller2Buttons | ((infoEx.dwButtons & 0x0100) << 2) | ((infoEx.dwButtons & 0x0040) << 4); // Fl Button
				controller2Buttons = controller2Buttons | ((infoEx.dwButtons & 0x0200) << 2) | ((infoEx.dwButtons & 0x0080) << 4); // Fr Button
			}
			else
			{
				// Logitech F310, Dualshock 4
				controller2Buttons = controller2Buttons | ((infoEx.dwButtons & 0x0002) >> 1); // B Button
				controller2Buttons = controller2Buttons | ((infoEx.dwButtons & 0x0001) << 1); // Y Button
				controller2Buttons = controller2Buttons | ((infoEx.dwButtons & 0x0100) >> 6); // Sl Button
				controller2Buttons = controller2Buttons | ((infoEx.dwButtons & 0x0200) >> 6); // St Button

				controller2Buttons = controller2Buttons | ((infoEx.dwButtons & 0x0004) << 6); // A Button
				controller2Buttons = controller2Buttons | ((infoEx.dwButtons & 0x0008) << 6); // X Button
				controller2Buttons = controller2Buttons | ((infoEx.dwButtons & 0x0010) << 6) | ((infoEx.dwButtons & 0x0040) << 4); // Fl Button
				controller2Buttons = controller2Buttons | ((infoEx.dwButtons & 0x0020) << 6) | ((infoEx.dwButtons & 0x0080) << 4); // Fr Button
			}
			controller2Buttons = controller2Buttons | 0x10000; // Controller plugged in status.
		}
		controller2Buttons = ~controller2Buttons;

		break;
	case 1: // Clock
		if (pCard->m_buttonIndex <= 16)
		{
			pCard->m_buttonIndex++;
			controller1Buttons = controller1Buttons >> 1;
			controller2Buttons = controller2Buttons >> 1;
		}
		break;
	default:
		break;
	}

	return 0;
}

void SNESMAXCard::InitializeIO(LPBYTE pCxRomPeripheral)
{
	RegisterIoHandler(m_slot, &SNESMAXCard::IORead, &SNESMAXCard::IOWrite, IO_Null, IO_Null, this, NULL);
}

//===========================================================================

static const UINT kUNIT_VERSION = 1;

#define SS_YAML_KEY_BUTTON_INDEX "Button Index"

const std::string& SNESMAXCard::GetSnapshotCardName(void)
{
	static const std::string name("SNES MAX");
	return name;
}

void SNESMAXCard::SaveSnapshot(YamlSaveHelper& yamlSaveHelper)
{
	YamlSaveHelper::Slot slot(yamlSaveHelper, GetSnapshotCardName(), m_slot, kUNIT_VERSION);

	YamlSaveHelper::Label unit(yamlSaveHelper, "%s:\n", SS_YAML_KEY_STATE);
	yamlSaveHelper.SaveUint(SS_YAML_KEY_BUTTON_INDEX, m_buttonIndex);
}

bool SNESMAXCard::LoadSnapshot(YamlLoadHelper& yamlLoadHelper, UINT version)
{
	if (version < 1 || version > kUNIT_VERSION)
		ThrowErrorInvalidVersion(version);

	m_buttonIndex = yamlLoadHelper.LoadUint(SS_YAML_KEY_BUTTON_INDEX);

	// Initialise with no buttons pressed (loaded state maybe part way through reading the buttons)
	m_controller1Buttons = 0xff;
	m_controller2Buttons = 0xff;

	return true;
}
