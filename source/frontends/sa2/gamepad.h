#pragma once

#include "linux/paddle.h"
#include <SDL.h>

#include <vector>
#include <memory>


class Gamepad : public Paddle
{
public:
  Gamepad(const int index);

  virtual bool getButton(int i) const;
  virtual double getAxis(int i) const;

private:
  std::shared_ptr<SDL_GameController> myController;

  std::vector<SDL_GameControllerButton> myButtonCodes;
  std::vector<SDL_GameControllerAxis> myAxisCodes;
};
