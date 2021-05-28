#pragma once

#include <chrono>

namespace common2
{

  class Speed
  {
  public:
    Speed(const bool fixedSpeed);

    void reset();

    // calculate the number of cycles to execute in the current period
    // assuming the next call will happen in x microseconds
    uint64_t getCyclesTillNext(const size_t microseconds) const;
    uint64_t getCyclesAtFixedSpeed(const size_t microseconds) const;

  private:

    const bool myFixedSpeed;
    std::chrono::time_point<std::chrono::steady_clock> myStartTime;
    uint64_t myStartCycles;
  };

}
