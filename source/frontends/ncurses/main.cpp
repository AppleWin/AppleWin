#include "StdAfx.h"

#include <chrono>
#include <thread>
#include <iostream>
#include <ncurses.h>

#include "Common.h"
#include "CardManager.h"
#include "Core.h"
#include "Log.h"
#include "CPU.h"
#include "NTSC.h"
#include "SaveState.h"
#include "Utilities.h"

#include "linux/benchmark.h"
#include "linux/paddle.h"
#include "linux/interface.h"
#include "frontends/common2/fileregistry.h"
#include "frontends/common2/programoptions.h"
#include "frontends/common2/utils.h"
#include "frontends/ncurses/world.h"
#include "frontends/ncurses/nframe.h"

namespace
{

  bool ContinueExecution(const EmulatorOptions & options)
  {
    const auto start = std::chrono::steady_clock::now();

    const double fUsecPerSec        = 1.e6;
#if 1
    const UINT nExecutionPeriodUsec = 1000000 / 60;		// 60 FPS
    //	const UINT nExecutionPeriodUsec = 100;		// 0.1ms
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

    cardManager.GetDisk2CardMgr().UpdateDriveState(uActualCyclesExecuted);

    const int key = ProcessKeyboard();

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

    ProcessInput();

    const UINT dwClksPerFrame = NTSC_GetCyclesPerFrame();
    if (g_dwCyclesThisFrame >= dwClksPerFrame)
    {
      g_dwCyclesThisFrame = g_dwCyclesThisFrame % dwClksPerFrame;
      if (!options.headless)
      {
	VideoRedrawScreen();
      }
    }

    if (!options.headless)
    {
      const auto end = std::chrono::steady_clock::now();
      const auto diff = end - start;
      const long us = std::chrono::duration_cast<std::chrono::microseconds>(diff).count();

      const double coeff = exp(-0.000001 * nExecutionPeriodUsec);  // 0.36 after 1 second

      g_relativeSpeed = g_relativeSpeed * coeff + double(us) / double(nExecutionPeriodUsec) * (1.0 - coeff);

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
      return !g_stop;
    }
  }

  void EnterMessageLoop(const EmulatorOptions & options)
  {
    LogFileTimeUntilFirstKeyReadReset();
    while (ContinueExecution(options))
    {
    }
  }

  int run_ncurses(int argc, const char * argv [])
  {
    EmulatorOptions options;
    options.memclear = g_nMemoryClearType;
    const bool run = getEmulatorOptions(argc, argv, "ncurses", options);

    if (!run)
      return 1;

    if (options.log)
    {
      LogInit();
    }

    InitializeFileRegistry(options);

    g_nMemoryClearType = options.memclear;

    initialiseEmulator();
    NVideoInitialize(options.headless);
    applyOptions(options);

    CardManager & cardManager = GetCardMgr();

    FrameRefreshStatus(DRAW_LEDS | DRAW_BUTTON_DRIVES, true);

    if (options.benchmark)
    {
      VideoBenchmark(&VideoRedrawScreen, &VideoRedrawScreen);
    }
    else
    {
      EnterMessageLoop(options);
    }

    Frame::unInitialise();
    uninitialiseEmulator();

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
  catch (const std::string & e)
  {
    std::cerr << e << std::endl;
    return 1;
  }
  catch (int e)
  {
    std::cerr << "Exit process called: " << e << std::endl;
    return e;
  }
}
