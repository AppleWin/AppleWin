#include "frontends/common2/speed.h"

#include "StdAfx.h"
#include "CPU.h"
#include "Core.h"

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
  }

  uint64_t Speed::getCyclesAtFixedSpeed(const size_t microseconds) const
  {
    const uint64_t cycles = static_cast<uint64_t>(microseconds * g_fCurrentCLK6502 * 1.0e-6);
    return cycles;
  }

  uint64_t Speed::getCyclesTillNext(const size_t microseconds) const
  {
    if (myFixedSpeed || g_bFullSpeed)
    {
      return getCyclesAtFixedSpeed(microseconds);
    }
    else
    {
      const uint64_t currentCycles = g_nCumulativeCycles;
      const auto currentTime = std::chrono::steady_clock::now();

      const auto currentDelta = std::chrono::duration_cast<std::chrono::microseconds>(currentTime - myStartTime).count();
      // target the next time we will be called
      const auto targetDeltaInMicros = currentDelta + microseconds;

      const uint64_t targetCycles = static_cast<uint64_t>(targetDeltaInMicros * g_fCurrentCLK6502 * 1.0e-6) + myStartCycles;
      if (targetCycles > currentCycles)
      {
        // number of cycles to fill this period
        const uint64_t cyclesToExecute = targetCycles - currentCycles;
        return cyclesToExecute;
      }
      else
      {
        // we got ahead, nothing to do this time
        // CpuExecute will still execute 1 instruction, which does not cause any issues
        return 0;
      }
    }
  }

}
