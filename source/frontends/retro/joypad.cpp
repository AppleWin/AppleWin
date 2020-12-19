#include "frontends/retro/joypad.h"
#include "frontends/retro/environment.h"

#include "libretro.h"


Joypad::Joypad()
  : myButtonCodes(2), myAxisCodes(2)
{
  myButtonCodes[0] = RETRO_DEVICE_ID_JOYPAD_A;
  myButtonCodes[1] = RETRO_DEVICE_ID_JOYPAD_B;
  myAxisCodes[0][RETRO_DEVICE_ID_JOYPAD_LEFT] = -1.0;
  myAxisCodes[0][RETRO_DEVICE_ID_JOYPAD_RIGHT] = 1.0;
  myAxisCodes[1][RETRO_DEVICE_ID_JOYPAD_UP] = -1.0;
  myAxisCodes[1][RETRO_DEVICE_ID_JOYPAD_DOWN] = 1.0;
}

bool Joypad::getButton(int i) const
{
  const int value = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, myButtonCodes[i]);
  //  if (value)
  //    log_cb(RETRO_LOG_INFO, "Joypad button: %d.\n", value);
  return value != 0;
}

double Joypad::getAxis(int i) const
{
  for (const auto & axis : myAxisCodes[i])
  {
    const int value = input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, axis.first);
    if (value)
    {
      //      log_cb(RETRO_LOG_INFO, "Joypad axis: %d.\n", value);
      return axis.second;
    }
  }

  return 0.0;
}
