#include "StdAfx.h"
#include "SNESMAX.h"
#include "YamlHelper.h"

void SNESMAXCard::InitializeIO(LPBYTE pCxRomPeripheral)
{
}

//===========================================================================

static const UINT kUNIT_VERSION = 1;

#define SS_YAML_KEY_BUTTON_INDEX "Button Index"

const std::string & SNESMAXCard::GetSnapshotCardName(void)
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

  yamlLoadHelper.LoadUint(SS_YAML_KEY_BUTTON_INDEX);

  return true;
}
