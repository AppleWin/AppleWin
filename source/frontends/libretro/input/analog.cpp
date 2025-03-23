#include "frontends/libretro/input/analog.h"

#include "libretro.h"

#define AXIS_MIN -32768
#define AXIS_MAX 32767

namespace ra2
{

    Analog::Analog(const std::unique_ptr<Game> &game, unsigned port)
        : JoypadBase(game, port)
        , myAxisCodes(2)
    {
        myAxisCodes[0] = {RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X};
        myAxisCodes[1] = {RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y};
    }

    double Analog::getAxis(int i) const
    {
        const auto &axis = myAxisCodes[i];
        const int value = inputState(axis);
        const double res = 2.0 * double(value - AXIS_MIN) / double(AXIS_MAX - AXIS_MIN) - 1.0;
        return res;
    }

} // namespace ra2
