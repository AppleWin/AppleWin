#include "StdAfx.h"
#include "FourPlay.h"

void FourPlayCard::InitializeIO(LPBYTE pCxRomPeripheral, UINT slot)
{
}

//===========================================================================

static const UINT kUNIT_VERSION = 1;

std::string FourPlayCard::GetSnapshotCardName(void)
{
	static const std::string name("4Play");
	return name;
}

void FourPlayCard::SaveSnapshot(YamlSaveHelper& yamlSaveHelper)
{
}

bool FourPlayCard::LoadSnapshot(YamlLoadHelper& yamlLoadHelper, UINT slot, UINT version)
{
	if (version < 1 || version > kUNIT_VERSION)
		throw std::string("Card: wrong version");

	return true;
}
