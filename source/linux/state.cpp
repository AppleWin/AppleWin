#include "StdAfx.h"

#include "Applewin.h"
#include "SerialComms.h"
#include "Configuration/IPropertySheet.h"
#include "YamlHelper.h"

void SpkrLoadSnapshot(YamlLoadHelper&) { }
void KeybReset() { }
void JoySaveSnapshot(YamlSaveHelper&) { }
void JoyLoadSnapshot(YamlLoadHelper&) { }
void SetLoadedSaveStateFlag(bool) { }
void SpkrSaveSnapshot(YamlSaveHelper&) { }
bool CSuperSerialCard::LoadSnapshot(YamlLoadHelper&, unsigned int, unsigned int) { return true; }
void IPropertySheet::ApplyNewConfig(CConfigNeedingRestart const&, CConfigNeedingRestart const&) { }
void FrameUpdateApple2Type() { }
void CSuperSerialCard::SaveSnapshot(YamlSaveHelper&) { }


std::string CSuperSerialCard::GetSnapshotCardName()
{
  return "Super Serial Card";
}
