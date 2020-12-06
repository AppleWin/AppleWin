#include "frontends/common2/speed.h"

#include "StdAfx.h"
#include "CPU.h"
#include "Core.h"

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

size_t Speed::getCyclesTillNext(const size_t milliseconds) const
{
  if (myFixedSpeed)
  {
    const size_t cycles = static_cast<uint64_t>(milliseconds * g_fCurrentCLK6502 * 1.0e-3);
    return cycles;
  }
  else
  {
    const uint64_t currentCycles = g_nCumulativeCycles;
    const auto currentTime = std::chrono::steady_clock::now();

    const auto currentDelta = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - myStartTime).count();
    // target the next time we will be called
    const auto targetDeltaInMillis = currentDelta + milliseconds;

    const uint64_t targetCycles = static_cast<uint64_t>(targetDeltaInMillis * g_fCurrentCLK6502 * 1.0e-3) + myStartCycles;
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
