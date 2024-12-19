#include "StdAfx.h"
#include "frontends/common2/commonframe.h"
#include "frontends/common2/programoptions.h"
#include "linux/resources.h"

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <thread>

#include "CardManager.h"
#include "Core.h"
#include "CPU.h"
#include "Debugger/Debug.h"
#include "Interface.h"
#include "Log.h"
#include "NTSC.h"
#include "Speaker.h"

namespace common2
{

  CommonFrame::CommonFrame(const EmulatorOptions & options)
    : LinuxFrame(options.autoBoot)
    , mySpeed(options.fixedSpeed)
    , mySynchroniseWithTimer(options.syncWithTimer)
    , myAllowVideoUpdate(!options.noVideoUpdate)
  {
    myLastSync = std::chrono::steady_clock::now();
  }

  void CommonFrame::Begin()
  {
    LinuxFrame::Begin();
    ResetSpeed();
    ResetHardware();
  }

  BYTE* CommonFrame::GetResource(WORD id, LPCSTR lpType, uint32_t expectedSize)
  {
    myResource.clear();

    const std::string & filename = getResourceName(id);
    const std::string path = getResourcePath(filename);

    const int fd = open(path.c_str(), O_RDONLY);

    if (fd != -1)
    {
      struct stat stdbuf;
      if ((fstat(fd, &stdbuf) == 0) && S_ISREG(stdbuf.st_mode))
      {
        const off_t size = stdbuf.st_size;
        std::vector<BYTE> data(size);
        const ssize_t rd = read(fd, data.data(), size);
        if (rd == expectedSize)
        {
          std::swap(myResource, data);
        }
      }
      close(fd);
    }

    if (myResource.empty())
    {
      LogFileOutput("FindResource: could not load resource %s\n", filename.c_str());
    }

    return myResource.data();
  }

  std::string CommonFrame::getBitmapFilename(const std::string & resource)
  {
    if (resource == "CHARSET82") return "CHARSET82.bmp";
    if (resource == "CHARSET8M") return "CHARSET8M.bmp";
    if (resource == "CHARSET8C") return "CHARSET8C.bmp";

    return resource;
  }

  void CommonFrame::ResetSpeed()
  {
    mySpeed.reset();
  }

  void CommonFrame::SetFullSpeed(const bool value)
  {
    if (g_bFullSpeed != value)
    {
      if (value)
      {
        // entering full speed
        GetCardMgr().GetMockingboardCardMgr().MuteControl(true);
        VideoRedrawScreenDuringFullSpeed(0, true);
      }
      else
      {
        // leaving full speed
        GetCardMgr().GetMockingboardCardMgr().MuteControl(false);
        ResetSpeed();
      }
      g_bFullSpeed = value;
      g_nCpuCyclesFeedback = 0;
    }
  }

  bool CommonFrame::CanDoFullSpeed()
  {
    return (g_dwSpeed == SPEED_MAX) ||
           (GetCardMgr().GetDisk2CardMgr().IsConditionForFullSpeed() && 
             !Spkr_IsActive() && 
             !GetCardMgr().GetMockingboardCardMgr().IsActiveToPreventFullSpeed()
           ) ||
           IsDebugSteppingAtFullSpeed();
  }

  void CommonFrame::ExecuteOneFrame(const int64_t microseconds)
  {
    // when running in adaptive speed
    // the value msNextFrame is only a hint for when the next frame will arrive
    switch (g_nAppMode)
    {
      case MODE_RUNNING:
        {
          ExecuteInRunningMode(microseconds);
          break;
        }
      case MODE_STEPPING:
        {
          ExecuteInDebugMode(microseconds);
          break;
        }
      default:
        break;
    };
  }

  void CommonFrame::ExecuteInRunningMode(const int64_t microseconds)
  {
    SetFullSpeed(CanDoFullSpeed());
    const uint32_t cyclesToExecute = mySpeed.getCyclesTillNext(microseconds);  // this checks g_bFullSpeed
    Execute(cyclesToExecute);
  }

  void CommonFrame::ExecuteInDebugMode(const int64_t microseconds)
  {
    // In AppleWin this is called without a timer for just one iteration
    // because we run a "frame" at a time, we need a bit of ingenuity
    const uint32_t cyclesToExecute = mySpeed.getCyclesAtFixedSpeed(microseconds);
    const int64_t target = g_nCumulativeCycles + cyclesToExecute;

    while (g_nAppMode == MODE_STEPPING && g_nCumulativeCycles < target)
    {
      DebugContinueStepping();
    }
  }

  void CommonFrame::Execute(const uint32_t cyclesToExecute)
  {
    const bool bVideoUpdate = myAllowVideoUpdate && !g_bFullSpeed;
    const UINT dwClksPerFrame = NTSC_GetCyclesPerFrame();

    // do it in the same batches as AppleWin (1 ms)
    const uint32_t fExecutionPeriodClks = g_fCurrentCLK6502 * (1.0 / 1000.0);  // 1 ms

    uint32_t totalCyclesExecuted = 0;
    // check at the end because we want to always execute at least 1 cycle even for "0"
    do
    {
      _ASSERT(cyclesToExecute >= totalCyclesExecuted);
      const uint32_t thisCyclesToExecute = std::min(fExecutionPeriodClks, cyclesToExecute - totalCyclesExecuted);
      const uint32_t executedCycles = CpuExecute(thisCyclesToExecute, bVideoUpdate);
      totalCyclesExecuted += executedCycles;

      GetCardMgr().Update(executedCycles);
      SpkrUpdate(executedCycles);

      g_dwCyclesThisFrame = (g_dwCyclesThisFrame + executedCycles) % dwClksPerFrame;

    } while (totalCyclesExecuted < cyclesToExecute);
  }

  void CommonFrame::ChangeMode(const AppMode_e mode)
  {
    if (mode != g_nAppMode)
    {
      switch (mode)
      {
      case MODE_RUNNING:
        DebugExitDebugger();
        SoundCore_SetFade(FADE_IN);
        break;
      case MODE_DEBUG:
        DebugBegin();
        CmdWindowViewConsole(0);
        break;
      default:
        g_nAppMode = mode;
        SoundCore_SetFade(FADE_OUT);
        break;
      }
      FrameRefreshStatus(DRAW_TITLE);
      ResetSpeed();
    }
  }

  void CommonFrame::TogglePaused()
  {
    const AppMode_e newMode = (g_nAppMode == MODE_RUNNING) ? MODE_PAUSED : MODE_RUNNING;
    ChangeMode(newMode);
  }

  void CommonFrame::SingleStep()
  {
    SetFullSpeed(CanDoFullSpeed());
    Execute(0);
  }

  void CommonFrame::ResetHardware()
  {
    myHardwareConfig.Reload();
  }

  bool CommonFrame::HardwareChanged() const
  {
    const CConfigNeedingRestart currentConfig = CConfigNeedingRestart::Create();
    return myHardwareConfig != currentConfig;
  }

  void CommonFrame::LoadSnapshot()
  {
    LinuxFrame::LoadSnapshot();
    ResetSpeed();
    ResetHardware();
  }

  void CommonFrame::SyncVideoPresentScreen(const int64_t microseconds)
  {
    if (mySynchroniseWithTimer)
    {
      const auto next = myLastSync + std::chrono::microseconds(microseconds);
      // no need to check here if "next" is in the past
      std::this_thread::sleep_until(next);
      myLastSync = std::chrono::steady_clock::now();
    }
    VideoPresentScreen();
  }

}

void SingleStep(bool /* bReinit */)
{
  dynamic_cast<common2::CommonFrame &>(GetFrame()).SingleStep();
}
