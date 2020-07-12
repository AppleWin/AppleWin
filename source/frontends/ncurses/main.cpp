#include "StdAfx.h"

#include <chrono>
#include <thread>
#include <iostream>
#include <ncurses.h>
#include <libgen.h>

#include <boost/program_options.hpp>

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
#include "frontends/ncurses/configuration.h"
#include "frontends/ncurses/world.h"

namespace
{
  namespace po = boost::program_options;

  struct EmulatorOptions
  {
    std::string disk1;
    std::string disk2;
    bool createMissingDisks;
    std::string snapshot;
    int memclear;
    bool log;
    bool benchmark;
    bool headless;
    bool ntsc;
    bool saveConfigurationOnExit;

    bool run;  // false if options include "-h"
  };

  bool getEmulatorOptions(int argc, const char * argv [], EmulatorOptions & options)
  {
    po::options_description desc("AppleWin ncurses");
    desc.add_options()
      ("help,h", "Print this help message")
      ("conf", "Save configuration on exit");

    po::options_description diskDesc("Disk");
    diskDesc.add_options()
      ("d1,1", po::value<std::string>(), "Mount disk image in first drive")
      ("d2,2", po::value<std::string>(), "Mount disk image in second drive")
      ("create,c", "Create missing disks");
    desc.add(diskDesc);

    po::options_description snapshotDesc("Snapshot");
    snapshotDesc.add_options()
      ("load-state,ls", po::value<std::string>(), "Load snapshot from file");
    desc.add(snapshotDesc);

    po::options_description memoryDesc("Memory");
    memoryDesc.add_options()
      ("memclear,m", po::value<int>(), "Memory initialization pattern [0..7]");
    desc.add(memoryDesc);

    po::options_description emulatorDesc("Emulator");
    emulatorDesc.add_options()
      ("log", "Log to AppleWin.log")
      ("headless,hl", "Headless: disable video")
      ("ntsc,nt", "NTSC: execute NTSC code")
      ("benchmark,b", "Benchmark emulator");
    desc.add(emulatorDesc);

    po::variables_map vm;
    try
    {
      po::store(po::parse_command_line(argc, argv, desc), vm);

      if (vm.count("help"))
      {
	std::cout << "AppleWin ncurses edition" << std::endl << std::endl << desc << std::endl;
	return false;
      }

      options.saveConfigurationOnExit = vm.count("conf");

      if (vm.count("d1"))
      {
	options.disk1 = vm["d1"].as<std::string>();
      }

      if (vm.count("d2"))
      {
	options.disk2 = vm["d2"].as<std::string>();
      }

      options.createMissingDisks = vm.count("create") > 0;

      if (vm.count("load-state"))
      {
	options.snapshot = vm["load-state"].as<std::string>();
      }

      if (vm.count("memclear"))
      {
	const int memclear = vm["memclear"].as<int>();
	if (memclear >=0 && memclear < NUM_MIP)
	  options.memclear = memclear;
      }

      options.benchmark = vm.count("benchmark") > 0;
      options.headless = vm.count("headless") > 0;
      options.log = vm.count("log") > 0;
      options.ntsc = vm.count("ntsc") > 0;

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

    g_CardMgr.GetDisk2CardMgr().UpdateDriveState(uActualCyclesExecuted);

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

      if (!g_CardMgr.GetDisk2CardMgr().IsConditionForFullSpeed())
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

  bool DoDiskInsert(const UINT slot, const int nDrive, const std::string & fileName, const bool createMissingDisk)
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

    Disk2InterfaceCard* pDisk2Card = dynamic_cast<Disk2InterfaceCard*> (g_CardMgr.GetObj(slot));
    ImageError_e Error = pDisk2Card->InsertDisk(nDrive, strPathName.c_str(), IMAGE_USE_FILES_WRITE_PROTECT_STATUS, createMissingDisk);
    return Error == eIMAGE_ERROR_NONE;
  }

  void setSnapshotFilename(const std::string & filename)
  {
    // same logic as qapple
    // setting chdir allows to load relative disks from the snapshot file (tests?)
    // but if the snapshot file itself is relative, it wont work after a chdir
    // so we convert to absolute first
    char * absPath = realpath(filename.c_str(), nullptr);
    if (absPath)
    {
      char * temp = strdup(absPath);
      const char * dir = dirname(temp);
      // dir points inside temp!
      if (dir)
      {
	  chdir(dir);
      }
      Snapshot_SetFilename(absPath);

      free(temp);
      free(absPath);
      Snapshot_LoadState();
    }
  }

  int foo(int argc, const char * argv [])
  {
    EmulatorOptions options;
    options.memclear = g_nMemoryClearType;
    const bool run = getEmulatorOptions(argc, argv, options);

    if (!run)
      return 1;

    if (options.log)
    {
      LogInit();
    }

    InitializeRegistry("applen.conf");

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
	g_CardMgr.GetDisk2CardMgr().Reset();
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

	CMouseInterface* pMouseCard = g_CardMgr.GetMouseCard();
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

    g_CardMgr.GetDisk2CardMgr().Destroy();
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
