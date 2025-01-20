#include "frontends/libretro/joypadbase.h"
#include "frontends/libretro/environment.h"

#include "libretro.h"

#include <iostream>

namespace ra2
{

    ControllerBase::ControllerBase(const std::vector<InputDescriptor> &buttonCodes)
        : myButtonCodes(buttonCodes)
    {
    }

    bool ControllerBase::getButton(int i) const
    {
        const auto &button = myButtonCodes[i];
        const int value = input_state_cb(0, button.device, button.index, button.id);
        return value != 0;
    }

    JoypadBase::JoypadBase()
        : ControllerBase({
              {RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A},
              {RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B},
          })
    {
    }

} // namespace ra2
