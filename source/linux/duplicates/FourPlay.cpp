#include "StdAfx.h"
#include "FourPlay.h"
#include "YamlHelper.h"

void FourPlayCard::InitializeIO(LPBYTE pCxRomPeripheral)
{
}

//===========================================================================

static const UINT kUNIT_VERSION = 1;

const std::string & FourPlayCard::GetSnapshotCardName(void)
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
