#pragma once

#include "frontends/libretro/joypadbase.h"

#include <memory>

namespace ra2
{

  class Game;

  class Mouse : public ControllerBase
  {
  public:
    Mouse(unsigned device, const std::unique_ptr<Game> * game);

    double getAxis(int i) const override;

  private:
    const std::unique_ptr<Game> * myGame;
  };

}
