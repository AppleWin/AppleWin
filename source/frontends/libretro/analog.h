#pragma once

#include "frontends/libretro/joypadbase.h"

#include <vector>


class Analog : public JoypadBase
{
public:
  Analog();

  virtual double getAxis(int i) const;

private:
  std::vector<std::pair<unsigned, unsigned> > myAxisCodes;
};
