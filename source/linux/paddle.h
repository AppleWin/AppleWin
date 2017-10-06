#pragma once

#include <memory>

class Paddle
{
public:
  Paddle();

  virtual ~Paddle();

  virtual bool getButton(int i) const;
  virtual int getAxis(int i) const;

  static std::shared_ptr<const Paddle> & instance();
};
