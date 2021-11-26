#include "StdAfx.h"
#include "SNESMAX.h"

void SNESMAXCard::InitializeIO(LPBYTE pCxRomPeripheral)
{
}

//===========================================================================

static const UINT kUNIT_VERSION = 1;

#define SS_YAML_KEY_BUTTON_INDEX "Button Index"

std::string SNESMAXCard::GetSnapshotCardName(void)
{
	static const std::string name("SNES MAX");
	return name;
}

void SNESMAXCard::SaveSnapshot(YamlSaveHelper& yamlSaveHelper)
{
}

bool SNESMAXCard::LoadSnapshot(YamlLoadHelper& yamlLoadHelper, UINT version)
{
	if (version < 1 || version > kUNIT_VERSION)
		throw std::string("Card: wrong version");

	return true;
}
