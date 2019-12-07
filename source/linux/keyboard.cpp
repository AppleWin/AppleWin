#include "linux/keyboard.h"

#include "StdAfx.h"
#include "Applewin.h"

#include <queue>

namespace
{
  std::queue<BYTE> keys;
  BYTE keycode = 0;
}

void addKeyToBuffer(BYTE key)
{
  keys.push(key);
}

BYTE KeybGetKeycode()
{
  return keycode;
}

BYTE KeybReadData()
{
  LogFileTimeUntilFirstKeyRead();

  if (keys.empty())
  {
    return keycode;
  }
  else
  {
    keycode = keys.front();
    const BYTE result = keycode | 0x80;
    return result;
  }
}

BYTE KeybReadFlag()
{
  if (!keys.empty())
  {
    keys.pop();
  }

  return KeybReadData();
}
