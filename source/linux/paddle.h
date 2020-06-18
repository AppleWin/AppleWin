#pragma once

#include <memory>
#include <set>

class Paddle
{
public:
  Paddle();

  virtual ~Paddle();

  virtual bool getButton(int i) const;
  virtual int getAxis(int i) const;

  static const int ourOpenApple;
  static const int ourClosedApple;

  static void setButtonPressed(int i);
  static void setButtonReleased(int i);
  static std::set<int> ourButtons;

  static std::shared_ptr<const Paddle> & instance();
};
