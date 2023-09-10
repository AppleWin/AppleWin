#pragma once

#include <chrono>

namespace common2
{

  class ControllerQuit
  {
  public:
    bool pressButton();  // if it returns true, app should end
  private:
    // too short and it becomes hard to trigger with SDL.
    static constexpr long myThreshold = 1000;
    std::chrono::steady_clock::time_point myFirstBtnPress;    
  };

}