/*
AppleWin : An Apple //e emulator for Windows

Copyright (C) 2021, Tom Charlesworth, Michael Pohoreski

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

#include "StdAfx.h"
#include "Card.h"

#include "Tfe/tfe.h"
#include "Mockingboard.h"
#include "ParallelPrinter.h"
#include "z80emu.h"
#include "FourPlay.h"
#include "Joystick.h"
#include "LanguageCard.h"
#include "MouseInterface.h"
#include "SAM.h"
#include "SerialComms.h"
#include "SNESMAX.h"
#include "VidHD.h"

#include <sstream>

void Card::ThrowErrorInvalidSlot()
{
	ThrowErrorInvalidSlot(m_type, m_slot);
}

void Card::ThrowErrorInvalidSlot(SS_CARDTYPE type, UINT slot)
{
	std::ostringstream msg;
	msg << "The card '" << GetCardName(type);
	msg << "' is not allowed in Slot " << slot << ".";

	throw std::runtime_error(msg.str());
}

void Card::ThrowErrorInvalidVersion(UINT version)
{
	ThrowErrorInvalidVersion(m_type, version);
}

void Card::ThrowErrorInvalidVersion(SS_CARDTYPE type, UINT version)
{
	std::ostringstream msg;
	msg << "Version " << version;
	msg << " is not supported for card '" << GetCardName(type) << "'.";

	throw std::runtime_error(msg.str());
}

void DummyCard::InitializeIO(LPBYTE pCxRomPeripheral)
{
	switch (QueryType())
	{
	case CT_GenericPrinter:
		PrintLoadRom(pCxRomPeripheral, m_slot);
		break;
	case CT_Uthernet:
		tfe_InitializeIO(pCxRomPeripheral, m_slot);
		break;
	case CT_GenericClock:
		break; // nothing to do
	case CT_MockingboardC:
	case CT_Phasor:
		// only in slot 4
		if (m_slot == SLOT4)
		{
			MB_InitializeIO(pCxRomPeripheral, SLOT4, SLOT5);
		}
		break;
	case CT_Z80:
		Z80_InitializeIO(pCxRomPeripheral, m_slot);
		break;
	}
}

void DummyCard::Update(const ULONG nExecutedCycles)
{
	switch (QueryType())
	{
	case CT_GenericPrinter:
		PrintUpdate(nExecutedCycles);
		break;
	case CT_MockingboardC:
	case CT_Phasor:
		// only in slot 4
		if (m_slot == SLOT4)
		{
			MB_PeriodicUpdate(nExecutedCycles);
		}
		break;
	}
}

void DummyCard::SaveSnapshot(YamlSaveHelper& yamlSaveHelper)
{
	switch (QueryType())
	{
	case CT_GenericPrinter:
		Printer_SaveSnapshot(yamlSaveHelper, m_slot);
		break;
	case CT_MockingboardC:
		MB_SaveSnapshot(yamlSaveHelper, m_slot);
		break;
	case CT_Phasor:
		Phasor_SaveSnapshot(yamlSaveHelper, m_slot);
		break;
	case CT_Z80:
		Z80_SaveSnapshot(yamlSaveHelper, m_slot);
		break;
	case CT_Uthernet:
		tfe_SaveSnapshot(yamlSaveHelper, m_slot);
		break;
	}
}

bool DummyCard::LoadSnapshot(YamlLoadHelper& yamlLoadHelper, UINT version)
{
	switch (QueryType())
	{
	case CT_GenericPrinter:
		return Printer_LoadSnapshot(yamlLoadHelper, m_slot, version);
	case CT_MockingboardC:
		return MB_LoadSnapshot(yamlLoadHelper, m_slot, version);
	case CT_Phasor:
		return Phasor_LoadSnapshot(yamlLoadHelper, m_slot, version);
	case CT_Z80:
		return Z80_LoadSnapshot(yamlLoadHelper, m_slot, version);
	case CT_Uthernet:
		return tfe_LoadSnapshot(yamlLoadHelper, m_slot, version);
	}
	return false;
}

std::string Card::GetCardName(void)
{
	return GetCardName(m_type);
}

std::string Card::GetCardName(const SS_CARDTYPE cardType)
{
	switch (cardType)
	{
	case CT_Empty:
		return "Empty";
	case CT_LanguageCard:
		return LanguageCardSlot0::GetSnapshotCardName();
	case CT_Saturn128K:
		return Saturn128K::GetSnapshotCardName();
	case CT_Disk2:
		return Disk2InterfaceCard::GetSnapshotCardName();
	case CT_SSC:
		return CSuperSerialCard::GetSnapshotCardName();
	case CT_MockingboardC:
		return MB_GetSnapshotCardName();
	case CT_GenericPrinter:
		return Printer_GetSnapshotCardName();
	case CT_GenericHDD:
		return HarddiskInterfaceCard::GetSnapshotCardName();
	case CT_GenericClock:
		return "Clock";
	case CT_MouseInterface:
		return CMouseInterface::GetSnapshotCardName();
	case CT_Z80:
		return Z80_GetSnapshotCardName();
	case CT_Phasor:
		return Phasor_GetSnapshotCardName();
	case CT_Echo:
		return "Echo";
	case CT_SAM:
		return SAMCard::GetSnapshotCardName();
	case CT_Uthernet:
		return tfe_GetSnapshotCardName();
	case CT_FourPlay:
		return FourPlayCard::GetSnapshotCardName();
	case CT_SNESMAX:
		return SNESMAXCard::GetSnapshotCardName();
	case CT_VidHD:
		return VidHDCard::GetSnapshotCardName();
	default:
		return "Unknown";
	}
}

SS_CARDTYPE Card::GetCardType(const std::string & card)
{
	if (card == Printer_GetSnapshotCardName())
	{
		return CT_GenericPrinter;
	}
	else if (card == CSuperSerialCard::GetSnapshotCardName())
	{
		return CT_SSC;
	}
	else if (card == CMouseInterface::GetSnapshotCardName())
	{
		return CT_MouseInterface;
	}
	else if (card == Z80_GetSnapshotCardName())
	{
		return CT_Z80;
	}
	else if (card == MB_GetSnapshotCardName())
	{
		return CT_MockingboardC;
	}
	else if (card == Phasor_GetSnapshotCardName())
	{
		return CT_Phasor;
	}
	else if (card == SAMCard::GetSnapshotCardName())
	{
		return CT_SAM;
	}
	else if (card == Disk2InterfaceCard::GetSnapshotCardName())
	{
		return CT_Disk2;
	}
	else if (card == HarddiskInterfaceCard::GetSnapshotCardName())
	{
		return CT_GenericHDD;
	}
	else if (card == tfe_GetSnapshotCardName())
	{
		return CT_Uthernet;
	}
	else if (card == LanguageCardSlot0::GetSnapshotCardName())
	{
		return CT_LanguageCard;
	}
	else if (card == Saturn128K::GetSnapshotCardName())
	{
		return CT_Saturn128K;
	}
	else if (card == FourPlayCard::GetSnapshotCardName())
	{
		return CT_FourPlay;
	}
	else if (card == SNESMAXCard::GetSnapshotCardName())
	{
		return CT_SNESMAX;
	}
	else if (card == VidHDCard::GetSnapshotCardName())
	{
		return CT_VidHD;
	}
	else
	{
		throw std::runtime_error("Slots: Unknown card: " + card);	// todo: don't throw - just ignore & continue
	}
}
