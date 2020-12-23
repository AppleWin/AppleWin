#pragma once

#include "linux/paddle.h"

#include <vector>
#include <map>


class JoypadBase : public Paddle
{
public:
  JoypadBase();

  virtual bool getButton(int i) const;

private:
  std::vector<unsigned> myButtonCodes;
};
