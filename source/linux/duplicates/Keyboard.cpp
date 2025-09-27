#include "StdAfx.h"
#include "Keyboard.h"

#include "Core.h"
#include "YamlHelper.h"

#include <queue>

namespace
{
    std::queue<BYTE> keys;
    bool g_bCapsLock = true; // Caps lock key for Apple2 and Lat/Cyr lock for Pravets8
    BYTE keycode = 0;

    void setKeyCode()
    {
        if (!keys.empty())
        {
            keycode = keys.front();
        }
    }
} // namespace

void addKeyToBuffer(BYTE key)
{
    keys.push(key);
}

void addTextToBuffer(const char *text)
{
    while (*text)
    {
        if (*text == '\n')
        {
            addKeyToBuffer(0x0d);
        }
        else if (*text >= 0x20 && *text <= 0x7e)
        {
            addKeyToBuffer(*text);
        }
        // skip non ASCII characters
        ++text;
    }
}

void KeybSetAltGrSendsWM_CHAR(bool state)
{
}

void KeybSetCapsLock(bool state)
{
}

bool KeybGetCapsStatus()
{
    return g_bCapsLock;
}

BYTE KeybGetKeycode()
{
    return keycode;
}

BYTE KeybReadData()
{
    LogFileTimeUntilFirstKeyRead();

    setKeyCode();

    return keycode | (keys.empty() ? 0 : 0x80);
}

BYTE KeybReadFlag()
{
    _ASSERT(!IS_APPLE2); // And also not Pravets machines?

    BYTE res = KeybClearStrobe();
    if (res)
        return res;

    // AKD
    return keycode | (keys.empty() ? 0 : 0x80);
}

#define SS_YAML_KEY_LASTKEY "Last Key"
#define SS_YAML_KEY_KEYWAITING "Key Waiting"

static std::string KeybGetSnapshotStructName(void)
{
    static const std::string name("Keyboard");
    return name;
}

void KeybSaveSnapshot(YamlSaveHelper &yamlSaveHelper)
{
    YamlSaveHelper::Label state(yamlSaveHelper, "%s:\n", KeybGetSnapshotStructName().c_str());
    yamlSaveHelper.SaveHexUint8(SS_YAML_KEY_LASTKEY, keycode);
    yamlSaveHelper.SaveBool(SS_YAML_KEY_KEYWAITING, keys.empty() ? false : false);
}

void KeybLoadSnapshot(YamlLoadHelper &yamlLoadHelper, UINT version)
{
    if (!yamlLoadHelper.GetSubMap(KeybGetSnapshotStructName()))
        return;

    keycode = (BYTE)yamlLoadHelper.LoadUint(SS_YAML_KEY_LASTKEY);

    bool keywaiting = false;
    if (version >= 2)
        keywaiting = yamlLoadHelper.LoadBool(SS_YAML_KEY_KEYWAITING);

    keys = std::queue<BYTE>();
    addKeyToBuffer(keycode);

    yamlLoadHelper.PopMap();
}

void KeybReset()
{
    keycode = 0;
    std::queue<BYTE>().swap(keys);
}

bool KeybGetShiftStatus()
{
    return false;
}

bool KeybGetAltStatus()
{
    return false;
}

bool KeybGetCtrlStatus()
{
    return false;
}

BYTE KeybClearStrobe(void)
{
    if (keys.empty())
    {
        return 0;
    }
    else
    {
        const BYTE result = keys.front();
        keys.pop();
        return result | 0x80;
    }
}
