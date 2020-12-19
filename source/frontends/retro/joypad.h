#pragma once

#include "linux/paddle.h"

#include <vector>
#include <map>


class Joypad : public Paddle
{
public:
  Joypad();

  virtual bool getButton(int i) const;
  virtual double getAxis(int i) const;

private:
  std::vector<unsigned> myButtonCodes;
  std::vector<std::map<unsigned, double> > myAxisCodes;
};
