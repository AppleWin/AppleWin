#pragma once

#include "linux/paddle.h"

#include <vector>
#include <map>

namespace ra2
{

  class ControllerBase : public Paddle
  {
  public:
    ControllerBase(unsigned device, const std::vector<unsigned> & buttonCodes);

    bool getButton(int i) const override;

  protected:
    const unsigned myDevice;

  private:
    const std::vector<unsigned> myButtonCodes;
  };

  class JoypadBase : public ControllerBase
  {
  public:
    JoypadBase(unsigned device);
  };


}
