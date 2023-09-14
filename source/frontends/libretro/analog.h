#pragma once

#include "frontends/libretro/joypadbase.h"

#include <vector>

namespace ra2
{

  class Analog : public JoypadBase
  {
  public:
    Analog();

    double getAxis(int i) const override;

  private:
    std::vector<InputDescriptor> myAxisCodes;
  };

}
