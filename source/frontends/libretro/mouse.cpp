#include "StdAfx.h"
#include "frontends/libretro/mouse.h"
#include "frontends/libretro/environment.h"
#include "frontends/libretro/game.h"

#include "libretro.h"

namespace ra2
{

  Mouse::Mouse(unsigned device, const std::unique_ptr<Game> * game)
    : ControllerBase(device, {RETRO_DEVICE_ID_MOUSE_LEFT, RETRO_DEVICE_ID_MOUSE_RIGHT})
    , myGame(game)
  {
  }

  double Mouse::getAxis(int i) const
  {
    return *myGame ? (*myGame)->getMousePosition(i) : 0.0;
  }

}
