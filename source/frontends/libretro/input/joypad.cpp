#include "frontends/libretro/input/joypad.h"

#include "libretro.h"

namespace ra2
{

    Joypad::Joypad(const std::unique_ptr<Game> &game, unsigned port)
        : JoypadBase(game, port)
        , myAxisCodes(2)
    {
        myAxisCodes[0] = {
            {{RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT}, -1.0},
            {{RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT}, 1.0},
        };
        myAxisCodes[1] = {
            {{RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP}, -1.0},
            {{RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN}, 1.0},
        };
    }

    double Joypad::getAxis(int i) const
    {
        for (const auto &axis : myAxisCodes[i])
        {
            const int value = inputState(axis.first);
            if (value)
            {
                return axis.second;
            }
        }

        return 0.0;
    }

} // namespace ra2
