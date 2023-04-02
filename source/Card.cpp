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

#include "Uthernet1.h"
#include "Uthernet2.h"
#include "Mockingboard.h"
#include "ParallelPrinter.h"
#include "z80emu.h"
#include "FourPlay.h"
#include "LanguageCard.h"
#include "MouseInterface.h"
#include "SAM.h"
#include "SerialComms.h"
#include "SNESMAX.h"
#include "VidHD.h"
#include "z80emu.h"

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
	case CT_GenericClock:
	case CT_Echo:
	default:
		_ASSERT(0);
	}
}

void DummyCard::Update(const ULONG nExecutedCycles)
{
	switch (QueryType())
	{
	case CT_GenericClock:
	case CT_Echo:
	default:
		_ASSERT(0);
		break;
	}
}

void DummyCard::SaveSnapshot(YamlSaveHelper& yamlSaveHelper)
{
	switch (QueryType())
	{
	case CT_GenericClock:
	case CT_Echo:
	default:
		_ASSERT(0);
		break;
	}
}

bool DummyCard::LoadSnapshot(YamlLoadHelper& yamlLoadHelper, UINT version)
{
	switch (QueryType())
	{
	case CT_GenericClock:
	case CT_Echo:
	default:
		_ASSERT(0);
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
		return MockingboardCard::GetSnapshotCardName();
	case CT_GenericPrinter:
		return ParallelPrinterCard::GetSnapshotCardName();
	case CT_GenericHDD:
		return HarddiskInterfaceCard::GetSnapshotCardName();
	case CT_GenericClock:
		return "Clock";
	case CT_MouseInterface:
		return CMouseInterface::GetSnapshotCardName();
	case CT_Z80:
		return Z80Card::GetSnapshotCardName();
	case CT_Phasor:
		return MockingboardCard::GetSnapshotCardNamePhasor();
	case CT_Echo:
		return "Echo";
	case CT_SAM:
		return SAMCard::GetSnapshotCardName();
	case CT_Uthernet:
		return Uthernet1::GetSnapshotCardName();
	case CT_FourPlay:
		return FourPlayCard::GetSnapshotCardName();
	case CT_SNESMAX:
		return SNESMAXCard::GetSnapshotCardName();
	case CT_VidHD:
		return VidHDCard::GetSnapshotCardName();
	case CT_Uthernet2:
		return Uthernet2::GetSnapshotCardName();
	case CT_MegaAudio:
		return MockingboardCard::GetSnapshotCardNameMegaAudio();
	default:
		return "Unknown";
	}
}

SS_CARDTYPE Card::GetCardType(const std::string & card)
{
	if (card == ParallelPrinterCard::GetSnapshotCardName())
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
	else if (card == Z80Card::GetSnapshotCardName())
	{
		return CT_Z80;
	}
	else if (card == MockingboardCard::GetSnapshotCardName())
	{
		return CT_MockingboardC;
	}
	else if (card == MockingboardCard::GetSnapshotCardNamePhasor())
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
	else if (card == Uthernet1::GetSnapshotCardName())
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
	else if (card == Uthernet2::GetSnapshotCardName())
	{
		return CT_Uthernet2;
	}
	else if (card == MockingboardCard::GetSnapshotCardNameMegaAudio())
	{
		return CT_MegaAudio;
	}
	else
	{
		throw std::runtime_error("Slots: Unknown card: " + card);	// todo: don't throw - just ignore & continue
	}
}
