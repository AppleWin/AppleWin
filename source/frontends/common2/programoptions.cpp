#include <frontends/common2/programoptions.h>
#include <linux/version.h>
#include <boost/program_options.hpp>

#include "StdAfx.h"
#include "Memory.h"
#include <iostream>

namespace po = boost::program_options;

bool getEmulatorOptions(int argc, const char * argv [], const std::string & edition, EmulatorOptions & options)
{
  const std::string name = "Apple Emulator for " + edition + " (based on AppleWin " + getVersion() + ")";
  po::options_description desc(name);
  desc.add_options()
    ("help,h", "Print this help message")
    ("multi-threaded,m", "Multi threaded")
    ("loose-mutex,l", "Loose mutex")
    ("sdl-driver", po::value<int>()->default_value(options.sdlDriver), "SDL driver")
    ("timer-interval,i", po::value<int>()->default_value(options.timerInterval), "Timer interval in ms");

  po::options_description configDesc("configuration");
  configDesc.add_options()
    ("save-conf", "Save configuration on exit")
    ("config,c", po::value<std::vector<std::string>>(), "Registry options section.path=value")
    ("qt-ini,q", "Use Qt ini file (read only)");
  desc.add(configDesc);

  po::options_description diskDesc("Disk");
  diskDesc.add_options()
    ("d1,1", po::value<std::string>(), "Disk in 1st drive")
    ("d2,2", po::value<std::string>(), "Disk in 2nd drive");
  desc.add(diskDesc);

  po::options_description snapshotDesc("Snapshot");
  snapshotDesc.add_options()
    ("state-filename,f", po::value<std::string>(), "Set snapshot filename")
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
      std::cout << desc << std::endl;
      return false;
    }

    options.saveConfigurationOnExit = vm.count("save-conf");
    options.useQtIni = vm.count("qt-ini");
    options.multiThreaded = vm.count("multi-threaded");
    options.looseMutex = vm.count("loose-mutex");
    options.timerInterval = vm["timer-interval"].as<int>();
    options.sdlDriver = vm["sdl-driver"].as<int>();

    if (vm.count("config"))
    {
      options.registryOptions = vm["config"].as<std::vector<std::string> >();
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
      options.snapshotFilename = vm["load-state"].as<std::string>();
      options.loadSnapshot = true;
    }

    if (vm.count("state-filename"))
    {
      options.snapshotFilename = vm["state-filename"].as<std::string>();
      options.loadSnapshot = false;
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
