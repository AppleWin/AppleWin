#pragma once

#include "linux/paddle.h"

#include <string>
#include <vector>

struct libevdev;
struct input_event;

class EvDevPaddle : public Paddle
{
public:
  EvDevPaddle(const std::string & device);
  ~EvDevPaddle();

  int poll();

  const std::string & getName() const;
  virtual bool getButton(int i) const;
  virtual double getAxis(int i) const;

private:
  int myFD;
  std::shared_ptr<libevdev> myDev;

  void process(const input_event & ev);

  std::string myName;

  std::vector<unsigned int> myButtonCodes;
  std::vector<unsigned int> myAxisCodes;
  std::vector<int> myAxisMins;
  std::vector<int> myAxisMaxs;
};
