#pragma once

#include "linux/paddle.h"
#include <SDL.h>

#include <vector>
#include <memory>

namespace sa2
{

  class Gamepad : public Paddle
  {
  public:
    Gamepad(const int index);

    bool getButton(int i) const override;
    double getAxis(int i) const override;

  private:
    std::shared_ptr<SDL_GameController> myController;

    std::vector<SDL_GameControllerButton> myButtonCodes;
    std::vector<SDL_GameControllerAxis> myAxisCodes;
  };

}
