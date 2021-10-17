#include "StdAfx.h"
#include "Registry.h"
#include "CmdLine.h"



//===========================================================================
static std::string& RegGetSlotSection(UINT slot)
{
	static std::string section;
	if (slot == SLOT_AUX)
	{
		section = REG_CONFIG_SLOT_AUX;
	}
	else
	{
		section = REG_CONFIG_SLOT;
		section += (char)('0' + slot);
	}
	return section;
}

std::string& RegGetConfigSlotSection(UINT slot)
{
	static std::string section;
	section = REG_CONFIG "\\";
	section += RegGetSlotSection(slot);
	return section;
}

void RegDeleteConfigSlotSection(UINT slot)
{
}

void RegSetConfigSlotNewCardType(UINT slot, SS_CARDTYPE type)
{
	RegDeleteConfigSlotSection(slot);

	std::string regSection;
	regSection = RegGetConfigSlotSection(slot);

	RegSaveValue(regSection.c_str(), REGVALUE_CARD_TYPE, TRUE, type);
}
