#include "StdAfx.h"

#include "Applewin.h"
#include "SerialComms.h"
#include "Configuration/IPropertySheet.h"
#include "YamlHelper.h"

void SpkrLoadSnapshot(YamlLoadHelper&) { }
void KeybReset() { }
void MB_SaveSnapshot(YamlSaveHelper&, unsigned int) { }
void JoySaveSnapshot(YamlSaveHelper&) { }
void JoyLoadSnapshot(YamlLoadHelper&) { }
bool MB_LoadSnapshot(class YamlLoadHelper&, UINT, UINT) { return true; }
void SetLoadedSaveStateFlag(bool) { }
void SpkrSaveSnapshot(YamlSaveHelper&) { }
bool CSuperSerialCard::LoadSnapshot(YamlLoadHelper&, unsigned int, unsigned int) { return true; }
void MB_Reset() { }
void MB_InitializeForLoadingSnapshot() { }
bool Phasor_LoadSnapshot(class YamlLoadHelper&, UINT, UINT) { return true; }
void Phasor_SaveSnapshot(YamlSaveHelper&, unsigned int) { }
void IPropertySheet::ApplyNewConfig(CConfigNeedingRestart const&, CConfigNeedingRestart const&) { }
void FrameUpdateApple2Type() { }
void CSuperSerialCard::SaveSnapshot(YamlSaveHelper&) { }

std::string MB_GetSnapshotCardName()
{
  return "Mockingboard C";
}

std::string CSuperSerialCard::GetSnapshotCardName()
{
  return "Super Serial Card";
}

std::string Phasor_GetSnapshotCardName()
{
  return "Phasor";
}
