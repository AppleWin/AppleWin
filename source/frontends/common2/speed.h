#pragma once

#include <chrono>

namespace common2
{

  class Speed
  {
  public:
    struct Stats
    {
      double nominal;       // nominal speed
      double audio;         // audio adjust speed (multiple of SPKR_SAMPLE_RATE)
      double actual;        // real speed since last reset

      double netFeedback;   // Hz net feedback adjustment
    };

    Speed(const bool fixedSpeed);

    void reset();

    // calculate the number of cycles to execute in the current period
    // assuming the next call will happen in x microseconds
    uint32_t getCyclesTillNext(const uint64_t microseconds);
    uint32_t getCyclesAtFixedSpeed(const uint64_t microseconds) const;

    Stats getSpeedStats() const;

  private:

    const bool myFixedSpeed;

    std::chrono::time_point<std::chrono::steady_clock> myStartTime;
    int64_t myStartCycles;
    int64_t myOrgStartCycles;
    double myAudioSpeed;

    int64_t myTotalFeedbackCycles;
  };

}
