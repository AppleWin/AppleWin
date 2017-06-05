#include "StdAfx.h"

#include <chrono>
#include <iostream>
#include <unistd.h>

#include <boost/program_options.hpp>

#include "Common.h"
#include "Applewin.h"
#include "Disk.h"
#include "Log.h"
#include "CPU.h"
#include "Frame.h"
#include "Memory.h"
#include "Video.h"
#include "SaveState.h"

#include "frontends/ncurses/world.h"
#include "ncurses.h"

namespace
{
  namespace po = boost::program_options;

  struct EmulatorOptions
  {
    std::string disk1;
    std::string disk2;
    std::string snapshot;
    bool run;
  };

  bool getEmulatorOptions(int argc, const char * argv [], EmulatorOptions & options)
  {
    po::options_description desc("AppleWin ncurses");
    desc.add_options()
      ("help,h", "Print this help message");

    po::options_description diskDesc("Disk");
    diskDesc.add_options()
      ("d1,1", po::value<std::string>(), "Mount disk image in first drive")
      ("d2,2", po::value<std::string>(), "Mount disk image in second drive");
    desc.add(diskDesc);

    po::options_description snapshotDesc("Snapshot");
    snapshotDesc.add_options()
      ("load-state,ls", po::value<std::string>(), "Load snapshot from file");
    desc.add(snapshotDesc);

    po::variables_map vm;
    try
    {
      po::store(po::parse_command_line(argc, argv, desc), vm);

      if (vm.count("help"))
      {
	std::cout << "AppleWin ncurses edition" << std::endl << std::endl << desc << std::endl;
	return false;
      }

      if (vm.count("d1"))
      {
	options.disk1 = vm["d1"].as<std::string>();
      }

      if (vm.count("d2"))
      {
	options.disk2 = vm["d2"].as<std::string>();
      }

      if (vm.count("load-state"))
      {
	options.snapshot = vm["load-state"].as<std::string>();
      }

      return true;
    }
    catch (const po::error& e)
    {
      std::cerr << "ERROR: " << e.what() << std::endl << desc << std::endl;
      return false;
    }
    catch (const std::exception & e)
    {
      std::cerr << "ERROR: " << e.what() << std::endl;
      return false;
    }
  }

  bool ContinueExecution()
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

    const bool bVideoUpdate = false;
    const DWORD uActualCyclesExecuted = CpuExecute(uCyclesToExecute, bVideoUpdate);
    g_dwCyclesThisFrame += uActualCyclesExecuted;

    DiskUpdatePosition(uActualCyclesExecuted);

    const int key = ProcessKeyboard();

    switch (key)
    {
    case KEY_F(2):
      {
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

    if (g_dwCyclesThisFrame >= dwClksPerFrame)
    {
      g_dwCyclesThisFrame -= dwClksPerFrame;
      VideoRedrawScreen();
    }

    if (!DiskIsSpinning())
    {
      const auto end = std::chrono::steady_clock::now();
      const auto diff = end - start;
      const long ms = std::chrono::duration_cast<std::chrono::microseconds>(diff).count();

      if (ms < nExecutionPeriodUsec)
      {
	usleep(nExecutionPeriodUsec - ms);
      }
    }

    return true;
  }

  void EnterMessageLoop()
  {
    while (ContinueExecution())
    {
    }
  }

  bool DoDiskInsert(const int nDrive, const std::string & fileName)
  {
    std::string strPathName;

    if (fileName.empty())
    {
      return false;
    }

    if (fileName[0] == '/')
    {
      // Abs pathname
      strPathName = fileName;
    }
    else
    {
      // Rel pathname
      char szCWD[MAX_PATH] = {0};
      if (!GetCurrentDirectory(sizeof(szCWD), szCWD))
	return false;

      strPathName = szCWD;
      strPathName.append("/");
      strPathName.append(fileName);
    }

    ImageError_e Error = DiskInsert(nDrive, strPathName.c_str(), IMAGE_USE_FILES_WRITE_PROTECT_STATUS, IMAGE_DONT_CREATE);
    return Error == eIMAGE_ERROR_NONE;
  }

  int foo(int argc, const char * argv [])
  {
    EmulatorOptions options;
    const bool run = getEmulatorOptions(argc, argv, options);

    if (!run)
      return 1;

    g_fh = fopen("/tmp/applewin.txt", "w");
    setbuf(g_fh, NULL);

    LogFileOutput("Initialisation\n");

    ImageInitialize();
    DiskInitialize();

    bool boot = false;

    if (!options.disk1.empty())
    {
      const bool ok = DoDiskInsert(DRIVE_1, options.disk1);
      LogOutput("Init: DoDiskInsert(D1), res=%d\n", ok);
      boot = ok;
    }

    if (!options.disk2.empty())
    {
      const bool ok = DoDiskInsert(DRIVE_2, options.disk2);
      LogOutput("Init: DoDiskInsert(D2), res=%d\n", ok);
      if (!ok)
      {
	boot = false;
      }
    }

    if (!options.snapshot.empty())
    {
      boot = true;
    }

    if (boot)
    {
      FrameRefreshStatus(DRAW_LEDS | DRAW_BUTTON_DRIVES);

      do
      {
	MemInitialize();
	VideoInitialize();
	DiskReset();

	if (!options.snapshot.empty())
	{
	  Snapshot_SetFilename(options.snapshot.c_str());
	  Snapshot_LoadState();
	}

	EnterMessageLoop();
      }
      while (g_bRestart);

      VideoUninitialize();
    }
  }

}

int main(int argc, const char * argv [])
{
  return foo(argc, argv);
}
