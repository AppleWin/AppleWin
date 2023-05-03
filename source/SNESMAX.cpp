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

  Once data is read, button presses (for each controller) are stored in the following structure:

    bit#                  16 /         11 10 9 8 / 7 6 5 4 3  2  1 0
    m_controllerXButtons: *1   x:x:x:x:Fr:Fl:X:A / R:L:D:U:St:Sl:Y:B
	*1 = Controller plugged in status

  Alex Lukacz  Aug 2021
*/
#include "StdAfx.h"

#include "SNESMAX.h"
#include "Memory.h"
#include "YamlHelper.h"

extern int JOYSTICK_1; // declared in joystick.cpp
extern int JOYSTICK_2;

// Default to Sony DS4 / DualSense:
// b11,..,b0: St,Sl / -,-,R,L,X,A,B,Y
//
// infoEx.dwButtons bit definitions:
const UINT SNESMAXCard::m_mainControllerButtons[NUM_BUTTONS] =
{
	Y,B,A,X,LB,RB,UNUSED,UNUSED, SELECT,START,UNUSED,UNUSED,UNUSED,UNUSED,UNUSED,UNUSED	// bit0 -> Y, bit1 -> B, bit2 -> A, etc
};

// AltControllerType defaults to "8BitDo NES30 PRO" (can be reconfigured via command line + yaml mapping file)
// b11,..,b0: St,Sl,R,L / -,-,-,Y,X,-,B,A
//
// infoEx.dwButtons bit definitions:
UINT SNESMAXCard::m_altControllerButtons[2][NUM_BUTTONS] =
{
	{A,B,UNUSED,X,Y,UNUSED,UNUSED,UNUSED, LB,RB,SELECT,START,UNUSED,UNUSED,UNUSED,UNUSED},	// bit0 -> A, bit1 -> B, bit2 -> unused, etc
	{A,B,UNUSED,X,Y,UNUSED,UNUSED,UNUSED, LB,RB,SELECT,START,UNUSED,UNUSED,UNUSED,UNUSED}
};

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

		if (JOYSTICK_1 >= 0 && joyGetPosEx(JOYSTICK_1, &infoEx) == JOYERR_NOERROR)
			controller1Buttons = pCard->GetControllerButtons(JOYSTICKID1, infoEx, pCard->m_altControllerType[0]);

		controller1Buttons = ~controller1Buttons;

		if (JOYSTICK_2 >= 0 && joyGetPosEx(JOYSTICK_2, &infoEx) == JOYERR_NOERROR)
			controller2Buttons = pCard->GetControllerButtons(JOYSTICKID2, infoEx, pCard->m_altControllerType[1]);

		controller2Buttons = ~controller2Buttons;

		break;
	case 1: // Clock
		if (pCard->m_buttonIndex <= NUM_BUTTONS)	// NB. 17 bits (where last bit is: Controller plugged in status)
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

UINT SNESMAXCard::GetControllerButtons(UINT joyNum, JOYINFOEX& infoEx, bool altControllerType)
{
	UINT xAxis = (infoEx.dwXpos >> 8) & 0xFF;
	UINT yAxis = (infoEx.dwYpos >> 8) & 0xFF;

	UINT controllerButtons = 0;
	controllerButtons |= ((yAxis < 103 || infoEx.dwPOV == 0 || infoEx.dwPOV == 4500 || infoEx.dwPOV == 31500) << 4); // U Button
	controllerButtons |= ((yAxis > 153 || (infoEx.dwPOV >= 13500 && infoEx.dwPOV <= 22500)) << 5); // D Button
	controllerButtons |= ((xAxis < 103 || (infoEx.dwPOV >= 22500 && infoEx.dwPOV <= 31500)) << 6); // L Button
	controllerButtons |= ((xAxis > 153 || (infoEx.dwPOV >= 4500 && infoEx.dwPOV <= 13500)) << 7); // R Button

	UINT* controllerButtonMappings = !altControllerType ? (UINT*)m_mainControllerButtons : (UINT*)(&m_altControllerButtons[joyNum][0]);

	for (UINT i = 0; i < NUM_BUTTONS; i++)
	{
		if (infoEx.dwButtons & (1 << i))
		{
			if (controllerButtonMappings[i] != UNUSED)
				controllerButtons |= (1 << controllerButtonMappings[i]);
		}
	}

	return controllerButtons | 0x10000; // Controller plugged in status.
}

void SNESMAXCard::InitializeIO(LPBYTE pCxRomPeripheral)
{
	RegisterIoHandler(m_slot, &SNESMAXCard::IORead, &SNESMAXCard::IOWrite, IO_Null, IO_Null, this, NULL);
}

bool SNESMAXCard::ParseControllerMappingFile(UINT joyNum, const char* pathname, std::string& errorMsg)
{
	bool res = true;
	YamlHelper yamlHelper;

	try
	{
		if (!yamlHelper.InitParser(pathname))
			throw std::runtime_error("Controller mapping file: Failed to initialize parser or open file");

		if (yamlHelper.ParseFileHdr("AppleWin Controller Button Remapping") != 1)
			throw std::runtime_error("Controller mapping file: Version mismatch");

		std::string scalar;
		while (yamlHelper.GetScalar(scalar))
		{
			if (scalar == SS_YAML_KEY_UNIT)
			{
				yamlHelper.GetMapStartEvent();
				YamlLoadHelper yamlLoadHelper(yamlHelper);
				std::string hid = yamlLoadHelper.LoadString("HID");
				std::string desc = yamlLoadHelper.LoadString("Description");
				for (UINT i = 0; i < SNESMAXCard::NUM_BUTTONS; i++)
				{
					char szButtonNum[3] = "00";
					sprintf_s(szButtonNum, "%d", i + 1);	// +1 as 1-based
					std::string buttonNum = szButtonNum;
					bool found = false;
					std::string buttonStr = yamlLoadHelper.LoadString_NoThrow(buttonNum, found);
					SNESMAXCard::Button button = SNESMAXCard::UNUSED;
					if (found)
					{
						if (buttonStr == "A") button = SNESMAXCard::A;
						else if (buttonStr == "B") button = SNESMAXCard::B;
						else if (buttonStr == "X") button = SNESMAXCard::X;
						else if (buttonStr == "Y") button = SNESMAXCard::Y;
						else if (buttonStr == "LB") button = SNESMAXCard::LB;
						else if (buttonStr == "RB") button = SNESMAXCard::RB;
						else if (buttonStr == "SELECT") button = SNESMAXCard::SELECT;
						else if (buttonStr == "START") button = SNESMAXCard::START;
						else if (buttonStr == "") button = SNESMAXCard::UNUSED;
						else throw std::runtime_error("Controller mapping file: Unknown button: " + buttonStr);
					}
					m_altControllerButtons[joyNum][i] = button;
				}
			}
			else
			{
				throw std::runtime_error("Unknown top-level scalar: " + scalar);
			}

			break;	// TODO: extend to support multiple controllers
		}
	}
	catch (const std::exception& szMessage)
	{
		errorMsg = "Error with yaml file: ";
		errorMsg += pathname;
		errorMsg += "\n";
		errorMsg += szMessage.what();
		res = false;
	}

	yamlHelper.FinaliseParser();

	if (res)
		g_cmdLine.snesMaxAltControllerType[joyNum] = true;	// Enable the alt controller

	return res;
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
	m_controller1Buttons = 0x1ffff;
	m_controller2Buttons = 0x1ffff;

	return true;
}
