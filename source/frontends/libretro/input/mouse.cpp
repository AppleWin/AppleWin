#include "StdAfx.h"
#include "frontends/libretro/input/mouse.h"
#include "frontends/libretro/game.h"

#include "libretro.h"

namespace ra2
{

    Mouse::Mouse(const std::unique_ptr<Game> &game, unsigned port)
        : ControllerBase(
              game, port,
              {
                  {RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_LEFT},
                  {RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_RIGHT},
              })
    {
    }

    double Mouse::getAxis(int i) const
    {
        return myGame->getInputRemapper().getMousePosition(myPort, i);
    }

} // namespace ra2
