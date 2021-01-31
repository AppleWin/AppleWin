#pragma once

#include "linux/paddle.h"

#include <vector>
#include <map>


class JoypadBase : public Paddle
{
public:
  JoypadBase();

  bool getButton(int i) const override;

private:
  std::vector<unsigned> myButtonCodes;
};
