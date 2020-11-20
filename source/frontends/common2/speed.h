#pragma once

#include <chrono>

class Speed
{
public:
  Speed();

  // calculate the number of cycles to execute in the current period
  // assuming the next call will happen in x milliseconds
  size_t getCyclesTillNext(const size_t milliseconds) const;

private:

  std::chrono::time_point<std::chrono::steady_clock> myStartTime;
  uint64_t myStartCycles;
};
