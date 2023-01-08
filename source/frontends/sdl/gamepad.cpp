#include "StdAfx.h"
#include "frontends/sdl/gamepad.h"

#define AXIS_MIN -32768
#define AXIS_MAX 32767


namespace sa2
{

  Gamepad::Gamepad(const int index)
    : myButtonCodes(2), myAxisCodes(2)
  {
    myController.reset(SDL_GameControllerOpen(index), SDL_GameControllerClose);
    myButtonCodes[0] = SDL_CONTROLLER_BUTTON_A;
    myButtonCodes[1] = SDL_CONTROLLER_BUTTON_B;
    myAxisCodes[0] = SDL_CONTROLLER_AXIS_LEFTX;
    myAxisCodes[1] = SDL_CONTROLLER_AXIS_LEFTY;
  }

  bool Gamepad::getButton(int i) const
  {
    int value = 0;
    if (myController)
    {
      value = SDL_GameControllerGetButton(myController.get(), myButtonCodes[i]);
    }
    return value != 0;
  }

  double Gamepad::getAxis(int i) const
  {
    if (myController)
    {
      const int value = SDL_GameControllerGetAxis(myController.get(), myAxisCodes[i]);
      const double axis = 2.0 * double(value - AXIS_MIN) / double(AXIS_MAX - AXIS_MIN) - 1.0;
      return axis;
    }
    else
    {
      return 0.0;
    }
  }

}
