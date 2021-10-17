#include "StdAfx.h"

#include "YamlHelper.h"
#include "Common.h"

void JoyportControl(const UINT uControl)
{
}

void JoySaveSnapshot(YamlSaveHelper&)
{
}

void JoySetTrim(short nValue, bool bAxisX)
{
}

void JoySetJoyType(UINT num, DWORD type)
{
}

void JoyReset()
{
}

#define SS_YAML_KEY_COUNTERRESETCYCLE "Counter Reset Cycle"
#define SS_YAML_KEY_JOY0TRIMX "Joystick0 TrimX"
#define SS_YAML_KEY_JOY0TRIMY "Joystick0 TrimY"
#define SS_YAML_KEY_JOY1TRIMX "Joystick1 TrimX"
#define SS_YAML_KEY_JOY1TRIMY "Joystick1 TrimY"

static std::string JoyGetSnapshotStructName(void)
{
  static const std::string name("Joystick");
  return name;
}

void JoyLoadSnapshot(YamlLoadHelper& yamlLoadHelper, unsigned int)
{
  if (!yamlLoadHelper.GetSubMap(JoyGetSnapshotStructName()))
    return;

  yamlLoadHelper.LoadUint64(SS_YAML_KEY_COUNTERRESETCYCLE);
  yamlLoadHelper.LoadInt(SS_YAML_KEY_JOY0TRIMX);
  yamlLoadHelper.LoadInt(SS_YAML_KEY_JOY0TRIMY);
  yamlLoadHelper.LoadInt(SS_YAML_KEY_JOY1TRIMX);	// dump value
  yamlLoadHelper.LoadInt(SS_YAML_KEY_JOY1TRIMY);	// dump value

  yamlLoadHelper.PopMap();
}
