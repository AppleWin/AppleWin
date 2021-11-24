#include "frontends/libretro/joypadbase.h"
#include "frontends/libretro/environment.h"

#include "libretro.h"

namespace ra2
{

  ControllerBase::ControllerBase(unsigned device, const std::vector<unsigned> & buttonCodes)
    : myDevice(device), myButtonCodes(buttonCodes)
  {
  }

  bool ControllerBase::getButton(int i) const
  {
    const int value = input_state_cb(0, myDevice, 0, myButtonCodes[i]);
    return value != 0;
  }

  JoypadBase::JoypadBase(unsigned device) : ControllerBase(device, {RETRO_DEVICE_ID_JOYPAD_A, RETRO_DEVICE_ID_JOYPAD_B})
  {

  }

}
