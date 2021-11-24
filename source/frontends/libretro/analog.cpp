#include "frontends/libretro/analog.h"
#include "frontends/libretro/environment.h"

#include "libretro.h"

#define AXIS_MIN -32768
#define AXIS_MAX 32767

namespace ra2
{

  Analog::Analog(unsigned device)
    : JoypadBase(device)
    , myAxisCodes(2)
  {
    myAxisCodes[0] = std::make_pair(RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X);
    myAxisCodes[1] = std::make_pair(RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y);
  }

  double Analog::getAxis(int i) const
  {
    const auto & code = myAxisCodes[i];
    const int value = input_state_cb(0, myDevice, code.first, code.second);
    const double axis = 2.0 * double(value - AXIS_MIN) / double(AXIS_MAX - AXIS_MIN) - 1.0;
    return axis;
  }

}
