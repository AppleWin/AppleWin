#include "StdAfx.h"

#include "frontends/libretro/input/joypadbase.h"
#include "frontends/libretro/game.h"

#include "libretro.h"

namespace ra2
{

    ControllerBase::ControllerBase(
        const std::unique_ptr<Game> &game, const unsigned port, const std::vector<InputDescriptor> &buttonCodes)
        : myGame(game)
        , myPort(port)
        , myButtonCodes(buttonCodes)
    {
    }

    bool ControllerBase::getButton(int i) const
    {
        const auto &button = myButtonCodes[i];
        const int value = myGame->getInputRemapper().inputState(myPort, button);
        return value != 0;
    }

    int16_t ControllerBase::inputState(const InputDescriptor &descriptor) const
    {
        const int16_t value = myGame->getInputRemapper().inputState(myPort, descriptor);

        return value;
    }

    JoypadBase::JoypadBase(const std::unique_ptr<Game> &game, unsigned port)
        : ControllerBase(
              game, port,
              {
                  {RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A},
                  {RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B},
              })
    {
    }

} // namespace ra2
