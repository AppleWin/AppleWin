#include "StdAfx.h"
#include "Registry.h"



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

void RegSetConfigGameIOConnectorNewDongleType(UINT slot, DONGLETYPE type)
{
	_ASSERT(slot == GAME_IO_CONNECTOR);
	if (slot != GAME_IO_CONNECTOR)
		return;

	RegDeleteConfigSlotSection(slot);

	std::string regSection;
	regSection = RegGetConfigSlotSection(slot);

	RegSaveValue(regSection.c_str(), REGVALUE_GAME_IO_TYPE, TRUE, type);
}
