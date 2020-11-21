#include <frontends/common2/programoptions.h>
#include <boost/program_options.hpp>

#include "StdAfx.h"
#include "Memory.h"
#include <iostream>

namespace po = boost::program_options;

bool getEmulatorOptions(int argc, const char * argv [], const std::string & version, EmulatorOptions & options)
{
  po::options_description desc("AppleWin " + version);
  desc.add_options()
    ("help,h", "Print this help message")
    ("conf", "Save configuration on exit")
    ("multi-threaded,m", "Multi threaded")
    ("loose-mutex,l", "Loose mutex")
    ("sdl-driver", po::value<int>()->default_value(options.sdlDriver), "SDL driver")
    ("timer-interval,i", po::value<int>()->default_value(options.timerInterval), "Timer interval in ms")
    ("qt-ini,q", "Use Qt ini file (read only)");

  po::options_description diskDesc("Disk");
  diskDesc.add_options()
    ("d1,1", po::value<std::string>(), "Disk in 1st drive")
    ("d2,2", po::value<std::string>(), "Disk in 2nd drive")
    ("create,c", "Create missing disks");
  desc.add(diskDesc);

  po::options_description snapshotDesc("Snapshot");
  snapshotDesc.add_options()
    ("load-state,s", po::value<std::string>(), "Load snapshot from file");
  desc.add(snapshotDesc);

  po::options_description memoryDesc("Memory");
  memoryDesc.add_options()
    ("memclear", po::value<int>()->default_value(options.memclear), "Memory initialization pattern [0..7]");
  desc.add(memoryDesc);

  po::options_description emulatorDesc("Emulator");
  emulatorDesc.add_options()
    ("log", "Log to AppleWin.log")
    ("headless", "Headless: disable video (freewheel)")
    ("fixed-speed", "Fixed (non-adaptive) speed")
    ("ntsc,nt", "NTSC: execute NTSC code")
    ("benchmark,b", "Benchmark emulator")
    ("no-squaring", "Gamepad range is (already) a square");
  desc.add(emulatorDesc);

  po::variables_map vm;
  try
  {
    po::store(po::parse_command_line(argc, argv, desc), vm);

    if (vm.count("help"))
    {
      std::cout << "AppleWin " << version << " edition." << std::endl << std::endl << desc << std::endl;
      return false;
    }

    options.saveConfigurationOnExit = vm.count("conf");
    options.useQtIni = vm.count("qt-ini");
    options.multiThreaded = vm.count("multi-threaded");
    options.looseMutex = vm.count("loose-mutex");
    options.timerInterval = vm["timer-interval"].as<int>();
    options.sdlDriver = vm["sdl-driver"].as<int>();

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

    const int memclear = vm["memclear"].as<int>();
    if (memclear >=0 && memclear < NUM_MIP)
      options.memclear = memclear;

    options.benchmark = vm.count("benchmark") > 0;
    options.headless = vm.count("headless") > 0;
    options.log = vm.count("log") > 0;
    options.ntsc = vm.count("ntsc") > 0;
    options.squaring = vm.count("no-squaring") == 0;
    options.fixedSpeed = vm.count("fixed-speed") > 0;

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
