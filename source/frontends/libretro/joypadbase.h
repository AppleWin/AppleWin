#pragma once

#include "linux/paddle.h"

#include <vector>
#include <map>

namespace ra2
{

  class JoypadBase : public Paddle
  {
  public:
    JoypadBase();

    bool getButton(int i) const override;

  private:
    std::vector<unsigned> myButtonCodes;
  };

}
