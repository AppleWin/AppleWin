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

void JoySetHookAltKeys(bool hook)
{
}

void JoySetButtonVirtualKey(UINT button, UINT virtKey)
{
}

#define SS_YAML_KEY_COUNTERRESETCYCLE "Counter Reset Cycle"
#define SS_YAML_KEY_JOY0TRIMX "Joystick0 TrimX"
#define SS_YAML_KEY_JOY0TRIMY "Joystick0 TrimY"
#define SS_YAML_KEY_JOY1TRIMX "Joystick1 TrimX"
#define SS_YAML_KEY_JOY1TRIMY "Joystick1 TrimY"
#define SS_YAML_KEY_PDL_INACTIVE_CYCLE "Paddle%1d Inactive Cycle"

static std::string JoyGetSnapshotStructName(void)
{
  static const std::string name("Joystick");
  return name;
}

void JoyLoadSnapshot(YamlLoadHelper& yamlLoadHelper, UINT version)
{
  if (!yamlLoadHelper.GetSubMap(JoyGetSnapshotStructName()))
    return;

  yamlLoadHelper.LoadInt(SS_YAML_KEY_JOY0TRIMX);
  yamlLoadHelper.LoadInt(SS_YAML_KEY_JOY0TRIMY);
  yamlLoadHelper.LoadInt(SS_YAML_KEY_JOY1TRIMX);
  yamlLoadHelper.LoadInt(SS_YAML_KEY_JOY1TRIMY);

  if (version >= 7)
  {
    for (UINT n = 0; n < 4; n++)
    {
      const std::string str = StrFormat(SS_YAML_KEY_PDL_INACTIVE_CYCLE, n);
      yamlLoadHelper.LoadUint64(str);
    }
  }
  else
  {
    yamlLoadHelper.LoadUint64(SS_YAML_KEY_COUNTERRESETCYCLE);
  }

  yamlLoadHelper.PopMap();
}
