#include "StdAfx.h"

#include <chrono>
#include <thread>
#include <iostream>
#include <ncurses.h>

#include "Common.h"
#include "CardManager.h"
#include "Applewin.h"
#include "Disk.h"
#include "Harddisk.h"
#include "Log.h"
#include "CPU.h"
#include "Frame.h"
#include "Memory.h"
#include "Video.h"
#include "NTSC.h"
#include "ParallelPrinter.h"
#include "SaveState.h"
#include "MouseInterface.h"
#include "Mockingboard.h"
#include "SoundCore.h"

#include "linux/data.h"
#include "linux/benchmark.h"
#include "frontends/common2/configuration.h"
#include "frontends/common2/programoptions.h"
#include "frontends/common2/utils.h"
#include "frontends/ncurses/world.h"

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
	g_bRestart = false;
	return false;
      }
    case KEY_F(3):
      {
	g_bRestart = true;
	return false;
      }
    case KEY_F(12):
      {
	// F12: as F11 is already Fullscreen in the terminal
	// To load an image, use command line options
	Snapshot_SetFilename("");
	Snapshot_SaveState();
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
    }

    return true;
  }

  void EnterMessageLoop(const EmulatorOptions & options)
  {
    LogFileTimeUntilFirstKeyReadReset();
    while (ContinueExecution(options))
    {
    }
  }

  int foo(int argc, const char * argv [])
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

    InitializeRegistry(options);

    g_nMemoryClearType = options.memclear;

    LogFileOutput("Initialisation\n");

    ImageInitialize();

    bool disksOk = true;
    if (!options.disk1.empty())
    {
      const bool ok = DoDiskInsert(SLOT6, DRIVE_1, options.disk1, options.createMissingDisks);
      disksOk = disksOk && ok;
      LogFileOutput("Init: DoDiskInsert(D1), res=%d\n", ok);
    }

    if (!options.disk2.empty())
    {
      const bool ok = DoDiskInsert(SLOT6, DRIVE_2, options.disk2, options.createMissingDisks);
      disksOk = disksOk && ok;
      LogFileOutput("Init: DoDiskInsert(D2), res=%d\n", ok);
    }

    CardManager & cardManager = GetCardMgr();

    if (disksOk)
    {
      // AFTER a restart
      do
      {
	LoadConfiguration();

	CheckCpu();

	FrameRefreshStatus(DRAW_LEDS | DRAW_BUTTON_DRIVES);

	DSInit();
	MB_Initialize();
	MemInitialize();
	NVideoInitialize();
	cardManager.GetDisk2CardMgr().Reset();
	HD_Reset();

	if (!options.snapshot.empty())
	{
	  setSnapshotFilename(options.snapshot);
	}

	if (options.benchmark)
	{
	  VideoBenchmark(&VideoRedrawScreen, &VideoRedrawScreen);
	}
	else
	{
	  EnterMessageLoop(options);
	}

	CMouseInterface* pMouseCard = cardManager.GetMouseCard();
	if (pMouseCard)
	{
	  pMouseCard->Reset();
	}
	MemDestroy();
	MB_Destroy();
	DSUninit();
      }
      while (g_bRestart);

      VideoUninitialize();
    }
    else
    {
      LogOutput("Disk error.");
    }

    HD_Destroy();
    PrintDestroy();
    CpuDestroy();

    cardManager.GetDisk2CardMgr().Destroy();
    ImageDestroy();

    LogDone();

    return 0;
  }

}

int main(int argc, const char * argv [])
{
  try
  {
    return foo(argc, argv);
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
