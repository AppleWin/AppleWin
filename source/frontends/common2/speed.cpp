#include "frontends/common2/speed.h"

#include "StdAfx.h"
#include "CPU.h"
#include "Core.h"
#include "Speaker.h"

namespace
{
  double getAudioAdjustedSpeed()
  {
    return g_fClksPerSpkrSample * SPKR_SAMPLE_RATE;
    // return g_fCurrentCLK6502;
  }

  uint32_t checkAndReturn(const int64_t cycles)
  {
    // AppleWin uses uint32_t, so we check the value is sound
    const int64_t cyclesToExecute = std::max<int64_t>(0, cycles);
    _ASSERT(cyclesToExecute <= std::numeric_limits<uint32_t>::max());

    return static_cast<uint32_t>(cyclesToExecute);
  }
}

namespace common2
{

  Speed::Speed(const bool fixedSpeed)
    : myFixedSpeed(fixedSpeed)
  {
    reset();
  }

  void Speed::reset()
  {
    myStartTime = std::chrono::steady_clock::now();
    myStartCycles = g_nCumulativeCycles;
    myOrgStartCycles = myStartCycles;
    myTotalFeedbackCycles = 0;
    myAudioSpeed = getAudioAdjustedSpeed();
  }

  uint32_t Speed::getCyclesAtFixedSpeed(const int64_t microseconds) const
  {
    const int64_t cycles = static_cast<int64_t>(microseconds * myAudioSpeed * 1.0e-6) + g_nCpuCyclesFeedback;
    return checkAndReturn(cycles);
  }

  uint32_t Speed::getCyclesTillNext(const int64_t microseconds)
  {
    myTotalFeedbackCycles += g_nCpuCyclesFeedback;

    if (myFixedSpeed || g_bFullSpeed)
    {
      return getCyclesAtFixedSpeed(microseconds);
    }
    else
    {
      const int64_t currentCycles = g_nCumulativeCycles;
      const auto currentTime = std::chrono::steady_clock::now();

      const auto currentDelta = std::chrono::duration_cast<std::chrono::microseconds>(currentTime - myStartTime).count();
      // target the next time we will be called
      const auto targetDeltaInMicros = currentDelta + microseconds;

      // permanently apply the correction
      myStartCycles += g_nCpuCyclesFeedback;

      const int64_t targetCycles = static_cast<int64_t>(targetDeltaInMicros * myAudioSpeed * 1.0e-6) + myStartCycles;

      // if we got ahead, nothing to do this time
      // CpuExecute will still execute 1 instruction, which does not cause any issues
      return checkAndReturn(targetCycles - currentCycles);
    }
  }

  Speed::Stats Speed::getSpeedStats() const
  {
    const auto currentTime = std::chrono::steady_clock::now();
    const auto currentDelta = std::chrono::duration_cast<std::chrono::microseconds>(currentTime - myStartTime).count();
    const double multiplier = 1000000.0 / currentDelta;

    Stats stats{};

    stats.nominal = g_fCurrentCLK6502;
    stats.audio = myAudioSpeed;

    if (g_nAppMode == MODE_RUNNING)
    {
      stats.netFeedback = myTotalFeedbackCycles * multiplier;

      const int64_t totalCycles = g_nCumulativeCycles - myOrgStartCycles;
      stats.actual = totalCycles * multiplier;
    }

    return stats;
  }

}
