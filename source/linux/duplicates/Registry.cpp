#include "StdAfx.h"
#include "Registry.h"
#include "CmdLine.h"



//===========================================================================
static inline std::string RegGetSlotSection(UINT slot)
{
	return (slot == SLOT_AUX)
		? std::string(REG_CONFIG_SLOT_AUX)
		: (std::string(REG_CONFIG_SLOT) + (char)('0' + slot));
}

std::string RegGetConfigSlotSection(UINT slot)
{
	return std::string(REG_CONFIG "\\") + RegGetSlotSection(slot);
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
