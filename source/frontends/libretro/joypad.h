#pragma once

#include "frontends/libretro/joypadbase.h"

#include <vector>
#include <map>

namespace ra2
{

  class Joypad : public JoypadBase
  {
  public:
    Joypad();

    double getAxis(int i) const override;

  private:
    std::vector<std::vector<std::pair<InputDescriptor, double> > > myAxisCodes;
  };

}
