#include "StdAfx.h"

#include "Applewin.h"
#include "SerialComms.h"
#include "Configuration/IPropertySheet.h"
#include "YamlHelper.h"
#include "Video.h"

void VideoLoadSnapshot(class YamlLoadHelper& yamlLoadHelper, UINT version) { }
void KeybLoadSnapshot(YamlLoadHelper&, unsigned int) { }
void SpkrLoadSnapshot(YamlLoadHelper&) { }
void KeybReset() { }
void MB_SaveSnapshot(YamlSaveHelper&, unsigned int) { }
void JoySaveSnapshot(YamlSaveHelper&) { }
void VideoResetState() { }
void JoyLoadSnapshot(YamlLoadHelper&) { }
void MB_LoadSnapshot(YamlLoadHelper&, unsigned int, unsigned int) { }
void SetLoadedSaveStateFlag(bool) { }
void KeybSaveSnapshot(YamlSaveHelper&) { }
void SpkrSaveSnapshot(YamlSaveHelper&) { }
bool CSuperSerialCard::LoadSnapshot(YamlLoadHelper&, unsigned int, unsigned int) { return true; }
void MB_Reset() { }
void MB_InitializeForLoadingSnapshot() { }
void Phasor_LoadSnapshot(YamlLoadHelper&, unsigned int, unsigned int) { }
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

// Copied from Video.cpp as it is too complicated to compile and use Video.cpp

#define SS_YAML_KEY_ALTCHARSET "Alt Char Set"
#define SS_YAML_KEY_VIDEOMODE "Video Mode"
#define SS_YAML_KEY_CYCLESTHISFRAME "Cycles This Frame"

static std::string VideoGetSnapshotStructName(void)
{
	static const std::string name("Video");
	return name;
}

void VideoSaveSnapshot(YamlSaveHelper& yamlSaveHelper)
{
	YamlSaveHelper::Label state(yamlSaveHelper, "%s:\n", VideoGetSnapshotStructName().c_str());
	yamlSaveHelper.SaveBool(SS_YAML_KEY_ALTCHARSET, g_nAltCharSetOffset ? true : false);
	yamlSaveHelper.SaveHexUint32(SS_YAML_KEY_VIDEOMODE, g_uVideoMode);
	yamlSaveHelper.SaveUint(SS_YAML_KEY_CYCLESTHISFRAME, g_dwCyclesThisFrame);
}

void VideoLoadSnapshot(YamlLoadHelper& yamlLoadHelper)
{
	if (!yamlLoadHelper.GetSubMap(VideoGetSnapshotStructName()))
		return;

	g_nAltCharSetOffset = yamlLoadHelper.LoadBool(SS_YAML_KEY_ALTCHARSET) ? 256 : 0;
	g_uVideoMode = yamlLoadHelper.LoadUint(SS_YAML_KEY_VIDEOMODE);
	g_dwCyclesThisFrame = yamlLoadHelper.LoadUint(SS_YAML_KEY_CYCLESTHISFRAME);

	yamlLoadHelper.PopMap();
}
