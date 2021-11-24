#pragma once

#include "frontends/libretro/joypadbase.h"

#include <vector>

namespace ra2
{

  class Analog : public JoypadBase
  {
  public:
    Analog(unsigned device);

    double getAxis(int i) const override;

  private:
    std::vector<std::pair<unsigned, unsigned> > myAxisCodes;
  };

}
