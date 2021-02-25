#include "frontends/libretro/joypadbase.h"
#include "frontends/libretro/environment.h"

#include "libretro.h"

namespace ra2
{

  JoypadBase::JoypadBase()
    : myButtonCodes(2)
  {
    myButtonCodes[0] = RETRO_DEVICE_ID_JOYPAD_A;
    myButtonCodes[1] = RETRO_DEVICE_ID_JOYPAD_B;
  }

  bool JoypadBase::getButton(int i) const
  {
    const int value = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, myButtonCodes[i]);
    return value != 0;
  }

}
