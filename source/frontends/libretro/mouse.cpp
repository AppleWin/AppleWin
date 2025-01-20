#include "StdAfx.h"
#include "frontends/libretro/mouse.h"
#include "frontends/libretro/game.h"

#include "libretro.h"

namespace ra2
{

    Mouse::Mouse(const std::unique_ptr<Game> *game)
        : ControllerBase({
              {RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_LEFT},
              {RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_RIGHT},
          })
        , myGame(game)
    {
    }

    double Mouse::getAxis(int i) const
    {
        return *myGame ? (*myGame)->getMousePosition(i) : 0.0;
    }

} // namespace ra2
