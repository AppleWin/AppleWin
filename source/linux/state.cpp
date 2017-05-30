#include "StdAfx.h"

#include "SerialComms.h"
#include "MouseInterface.h"
#include "Configuration/IPropertySheet.h"

void VideoReinitialize() { }
void KeybLoadSnapshot(YamlLoadHelper&) { }
std::string GetSnapshotCardName() { }
bool CMouseInterface::LoadSnapshot(YamlLoadHelper&, unsigned int, unsigned int) { return true; }
void SpkrLoadSnapshot(YamlLoadHelper&) { }
void KeybReset() { }
void MB_SaveSnapshot(YamlSaveHelper&, unsigned int) { }
void JoySaveSnapshot(YamlSaveHelper&) { }
void VideoResetState() { }
void JoyLoadSnapshot(YamlLoadHelper&) { }
void MB_LoadSnapshot(YamlLoadHelper&, unsigned int, unsigned int) { }
std::string CMouseInterface::GetSnapshotCardName() { }
void SetLoadedSaveStateFlag(bool) { }
void KeybSaveSnapshot(YamlSaveHelper&) { }
void CMouseInterface::Reset() { }
void SpkrSaveSnapshot(YamlSaveHelper&) { }
bool CSuperSerialCard::LoadSnapshot(YamlLoadHelper&, unsigned int, unsigned int) { return true; }
std::string CSuperSerialCard::GetSnapshotCardName() { return ""; }
std::string MB_GetSnapshotCardName() { return ""; }
void MB_Reset() { }
void Phasor_LoadSnapshot(YamlLoadHelper&, unsigned int, unsigned int) { }
void Phasor_SaveSnapshot(YamlSaveHelper&, unsigned int) { }
std::string Phasor_GetSnapshotCardName() { return ""; }
void CMouseInterface::SaveSnapshot(YamlSaveHelper&) { }
void IPropertySheet::ApplyNewConfig(CConfigNeedingRestart const&, CConfigNeedingRestart const&) { }
void FrameUpdateApple2Type() { }
void VideoLoadSnapshot(YamlLoadHelper&) { }
void SetCurrentImageDir(char const*) { }
void CMouseInterface::Uninitialize() { }
void CSuperSerialCard::SaveSnapshot(YamlSaveHelper&) { }
void VideoSaveSnapshot(YamlSaveHelper&) { }
