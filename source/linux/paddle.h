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

  static constexpr int ourOpenApple = 0x61;
  static constexpr int ourClosedApple = 0x62;
  static constexpr int ourThirdApple = 0x63;

  static void setButtonPressed(int i);
  static void setButtonReleased(int i);
  static std::set<int> ourButtons;

  static std::shared_ptr<const Paddle> & instance();
};
