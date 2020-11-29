#include "StdAfx.h"
#include "Common.h"

#include "Log.h"
#include "CPU.h"
#include "Core.h"

void CheckCpu()
{
  const eApple2Type apple2Type = GetApple2Type();
  const eCpuType defaultCpu = ProbeMainCpuDefault(apple2Type);
  const eCpuType mainCpu = GetMainCpu();
  if (mainCpu != defaultCpu)
  {
    LogFileOutput("Detected non standard CPU for Apple2 = %d: default = %d, actual = %d\n", apple2Type, defaultCpu, mainCpu);
  }
}

void alarm_log_too_many_alarms()
{
}
