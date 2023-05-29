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
      double absFeedback;   // Hz sum(abc) feedback adjustment
    };

    Speed(const bool fixedSpeed);

    void reset();

    // calculate the number of cycles to execute in the current period
    // assuming the next call will happen in x microseconds
    uint64_t getCyclesTillNext(const uint64_t microseconds);
    uint64_t getCyclesAtFixedSpeed(const uint64_t microseconds) const;

  private:

    const bool myFixedSpeed;

    std::chrono::time_point<std::chrono::steady_clock> myStartTime;
    uint64_t myStartCycles;
    uint64_t myOrgStartCycles;
    double myAudioSpeed;

    int64_t myTotalFeedbackCycles;
    int64_t myAbsFeedbackCycles;
  };

}
