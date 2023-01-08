#include "StdAfx.h"

#include <chrono>
#include <thread>
#include <iostream>
#include <ncurses.h>

#include "CardManager.h"
#include "Core.h"
#include "CPU.h"
#include "NTSC.h"
#include "SaveState.h"
#include "Utilities.h"

#include "linux/benchmark.h"
#include "linux/paddle.h"
#include "linux/context.h"
#include "frontends/common2/fileregistry.h"
#include "frontends/common2/programoptions.h"
#include "frontends/common2/utils.h"
#include "frontends/ncurses/world.h"
#include "frontends/ncurses/nframe.h"
#include "frontends/ncurses/evdevpaddle.h"

namespace
{
  bool ContinueExecution(const common2::EmulatorOptions & options, const std::shared_ptr<na2::NFrame> & frame)
  {
    const auto start = std::chrono::steady_clock::now();

    const double fUsecPerSec        = 1.e6;
#if 1
    const UINT nExecutionPeriodUsec = 1000000 / 60;             // 60 FPS
    //  const UINT nExecutionPeriodUsec = 100;          // 0.1ms
    const double fExecutionPeriodClks = g_fCurrentCLK6502 * ((double)nExecutionPeriodUsec / fUsecPerSec);
#else
    const double fExecutionPeriodClks = 1800.0;
    const UINT nExecutionPeriodUsec = (UINT) (fUsecPerSec * (fExecutionPeriodClks / g_fCurrentCLK6502));
#endif

    const DWORD uCyclesToExecute = fExecutionPeriodClks;

    const bool bVideoUpdate = options.ntsc;
    g_bFullSpeed = !bVideoUpdate;

    const DWORD uActualCyclesExecuted = CpuExecute(uCyclesToExecute, bVideoUpdate);
    g_dwCyclesThisFrame += uActualCyclesExecuted;

    CardManager & cardManager = GetCardMgr();

    cardManager.Update(uActualCyclesExecuted);

    const int key = ProcessKeyboard(frame);

    switch (key)
    {
    case KEY_F(2):
      {
        ResetMachineState();
        break;
      }
    case 278: // Shift-F2
      {
        CtrlReset();
        break;
      }
    case KEY_F(3):
      {
        return false;
      }
    case KEY_F(5):
      {
        CardManager & cardManager = GetCardMgr();
        if (cardManager.QuerySlot(SLOT6) == CT_Disk2)
        {
          dynamic_cast<Disk2InterfaceCard*>(cardManager.GetObj(SLOT6))->DriveSwap();
        }
        break;
      }
    case KEY_F(11):
      {
        Snapshot_SaveState();
        break;
      }
    case KEY_F(12):
      {
        Snapshot_LoadState();
        break;
      }
    }

    frame->ProcessEvDev();

    const UINT dwClksPerFrame = NTSC_GetCyclesPerFrame();
    if (g_dwCyclesThisFrame >= dwClksPerFrame)
    {
      g_dwCyclesThisFrame = g_dwCyclesThisFrame % dwClksPerFrame;
      if (!options.headless)
      {
        frame->VideoPresentScreen();
      }
    }

    if (!options.headless)
    {
      const auto end = std::chrono::steady_clock::now();
      const auto diff = end - start;
      const long us = std::chrono::duration_cast<std::chrono::microseconds>(diff).count();

      const double coeff = exp(-0.000001 * nExecutionPeriodUsec);  // 0.36 after 1 second

      na2::g_relativeSpeed = na2::g_relativeSpeed * coeff + double(us) / double(nExecutionPeriodUsec) * (1.0 - coeff);

      if (!cardManager.GetDisk2CardMgr().IsConditionForFullSpeed())
      {
        if (us < nExecutionPeriodUsec)
        {
          const auto duration = std::chrono::microseconds(nExecutionPeriodUsec - us);
          std::this_thread::sleep_for(duration);
        }
      }
      return true;
    }
    else
    {
      return !na2::g_stop;
    }
  }

  void EnterMessageLoop(const common2::EmulatorOptions & options, const std::shared_ptr<na2::NFrame> & frame)
  {
    while (ContinueExecution(options, frame))
    {
    }
  }

  int run_ncurses(int argc, const char * argv [])
  {
    common2::EmulatorOptions options;
    const bool run = getEmulatorOptions(argc, argv, "ncurses", options);

    if (!run)
      return 1;

    const LoggerContext loggerContext(options.log);
    const RegistryContext registryContet(CreateFileRegistry(options));
    const std::shared_ptr<na2::EvDevPaddle> paddle(new na2::EvDevPaddle(options.paddleDeviceName));
    const std::shared_ptr<na2::NFrame> frame(new na2::NFrame(paddle));

    const Initialisation init(frame, paddle);
    common2::applyOptions(options);
    frame->Begin();

    common2::setSnapshotFilename(options.snapshotFilename);
    if (options.loadSnapshot)
    {
      frame->LoadSnapshot();
    }

    na2::SetCtrlCHandler(options.headless);

    if (options.benchmark)
    {
      const auto redraw = [&frame]() { frame->VideoRedrawScreen(); };
      VideoBenchmark(redraw, redraw);
    }
    else
    {
      EnterMessageLoop(options, frame);
    }
    frame->End();

    return 0;
  }

}

int main(int argc, const char * argv [])
{
  try
  {
    return run_ncurses(argc, argv);
  }
  catch (const std::exception & e)
  {
    std::cerr << e.what() << std::endl;
    return 1;
  }
}
