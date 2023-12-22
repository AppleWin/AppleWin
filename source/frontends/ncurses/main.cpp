#include "StdAfx.h"

#include <chrono>
#include <thread>
#include <iostream>
#include <ncurses.h>

#include "CardManager.h"
#include "Core.h"
#include "SaveState.h"
#include "Utilities.h"

#include "linux/benchmark.h"
#include "linux/context.h"
#include "frontends/common2/fileregistry.h"
#include "frontends/common2/programoptions.h"
#include "frontends/common2/commoncontext.h"
#include "frontends/ncurses/world.h"
#include "frontends/ncurses/nframe.h"
#include "frontends/ncurses/evdevpaddle.h"

namespace
{

  void ProcessKeys(const std::shared_ptr<na2::NFrame> & frame, bool &quit)
  {
    const int key = GetKeyPressed(frame);

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
        quit = true;
        break;
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
        frame->LoadSnapshot();
        break;
      }
    }
  }

  void ContinueExecution(const common2::EmulatorOptions & options, const std::shared_ptr<na2::NFrame> & frame, bool &quit)
  {
    const auto start = std::chrono::steady_clock::now();

    constexpr const int64_t nExecutionPeriodUsec = 1000000 / 60;             // 60 FPS
    frame->ExecuteOneFrame(nExecutionPeriodUsec);

    ProcessKeys(frame, quit);
    frame->ProcessEvDev();

    if (!options.headless)
    {
      frame->VideoPresentScreen();
      if (!g_bFullSpeed)
      {
        const auto end = std::chrono::steady_clock::now();
        const auto diff = end - start;
        const int64_t us = std::chrono::duration_cast<std::chrono::microseconds>(diff).count();
        if (us < nExecutionPeriodUsec)
        {
          const auto duration = std::chrono::microseconds(nExecutionPeriodUsec - us);
          std::this_thread::sleep_for(duration);
        }
      }
    }
  }

  void EnterMessageLoop(const common2::EmulatorOptions & options, const std::shared_ptr<na2::NFrame> & frame)
  {
    bool quit = false;

    do
    {
      ContinueExecution(options, frame, quit);
    } while (!quit && !na2::g_stop);
  }

  int run_ncurses(int argc, const char * argv [])
  {
    common2::EmulatorOptions options;
    const bool run = getEmulatorOptions(argc, argv, "ncurses", options);
    options.fixedSpeed = true;  // TODO: remove, some testing required

    if (!run)
      return 1;

    const LoggerContext loggerContext(options.log);
    const RegistryContext registryContet(CreateFileRegistry(options));
    const std::shared_ptr<na2::EvDevPaddle> paddle = std::make_shared<na2::EvDevPaddle>(options.paddleDeviceName);
    const std::shared_ptr<na2::NFrame> frame = std::make_shared<na2::NFrame>(options, paddle);

    const common2::CommonInitialisation init(frame, paddle, options);

    // no audio in the ncurses frontend
    g_bDisableDirectSound = true;
    g_bDisableDirectSoundMockingboard = true;
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
