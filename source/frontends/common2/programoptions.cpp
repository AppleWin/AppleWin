#include "frontends/common2/programoptions.h"
#include "frontends/common2/fileregistry.h"
#include "linux/version.h"
#include "linux/paddle.h"

#include <boost/program_options.hpp>

#include "StdAfx.h"
#include "Memory.h"
#include "Log.h"
#include "Disk.h"
#include "Utilities.h"
#include "Core.h"

#include <iostream>
#include <regex>

namespace po = boost::program_options;

namespace
{

  void parseGeometry(const std::string & s, std::optional<common2::Geometry> & geometry)
  {
    std::smatch m;
    if (std::regex_match(s, m, std::regex("^(\\d+)x(\\d+)(\\+(\\d+)\\+(\\d+))?$")))
    {
      const size_t groups = m.size();
      if (groups == 6)
      {
        geometry = common2::Geometry();
        geometry->width = std::stoi(m.str(1));
        geometry->height = std::stoi(m.str(2));
        if (!m.str(3).empty())
        {
          geometry->x = std::stoi(m.str(4));
          geometry->y = std::stoi(m.str(5));
        }
        return;
      }
    }
    throw std::runtime_error("Invalid sizes: " + s);
  }

}

namespace common2
{

  EmulatorOptions::EmulatorOptions()
  {
    memclear = g_nMemoryClearType;
    configurationFile = GetConfigFile("applewin.conf");
  }

  bool getEmulatorOptions(int argc, const char * argv [], const std::string & edition, EmulatorOptions & options)
  {
    const std::string name = "Apple Emulator for " + edition + " (based on AppleWin " + getVersion() + ")";
    po::options_description desc(name);
    desc.add_options()
      ("help,h", "Print this help message")
      ;

    po::options_description configDesc("Configuration");
    configDesc.add_options()
      ("conf", po::value<std::string>()->default_value(options.configurationFile), "Select configuration file")
      ("registry,r", po::value<std::vector<std::string>>(), "Registry options section.path=value")
      ("qt-ini,q", "Use Qt ini file (read only)")
      ;
    desc.add(configDesc);

    po::options_description diskDesc("Disk");
    diskDesc.add_options()
      ("d1,1", po::value<std::string>(), "Disk in 1st drive")
      ("d2,2", po::value<std::string>(), "Disk in 2nd drive")
      ;
    desc.add(diskDesc);

    po::options_description snapshotDesc("Snapshot");
    snapshotDesc.add_options()
      ("state-filename,f", po::value<std::string>(), "Set snapshot filename")
      ("load-state,s", po::value<std::string>(), "Load snapshot from file")
      ;
    desc.add(snapshotDesc);

    po::options_description memoryDesc("Memory");
    memoryDesc.add_options()
      ("memclear", po::value<int>()->default_value(options.memclear), "Memory initialization pattern [0..7]")
      ;
    desc.add(memoryDesc);

    po::options_description emulatorDesc("Emulator");
    emulatorDesc.add_options()
      ("log", "Log to AppleWin.log")
      ("headless", "Headless: disable video (freewheel)")
      ("fixed-speed", "Fixed (non-adaptive) speed")
      ("ntsc,nt", "NTSC: execute NTSC code")
      ("benchmark,b", "Benchmark emulator")
      ("rom", po::value<std::string>(), "Custom 12k/16k ROM")
      ("f8rom", po::value<std::string>(), "Custom 2k ROM")
      ;
    desc.add(emulatorDesc);

    po::options_description sdlDesc("SDL");
    sdlDesc.add_options()
      ("sdl-driver", po::value<int>()->default_value(options.sdlDriver), "SDL driver")
      ("gl-swap", po::value<int>()->default_value(options.glSwapInterval), "SDL_GL_SwapInterval")
      ("no-imgui", "Plain SDL2 renderer")
      ("geometry", po::value<std::string>(), "WxH[+X+Y]")
      ;
    desc.add(sdlDesc);

    po::options_description paddleDesc("Paddle");
    paddleDesc.add_options()
      ("no-squaring", "Gamepad range is (already) a square")
      ("device-name", po::value<std::string>(), "Gamepad device name")
      ;
    desc.add(paddleDesc);

    po::variables_map vm;
    try
    {
      po::store(po::parse_command_line(argc, argv, desc), vm);

      if (vm.count("help"))
      {
        std::cout << desc << std::endl;
        return false;
      }

      options.configurationFile = vm["conf"].as<std::string>();
      options.useQtIni = vm.count("qt-ini");
      options.sdlDriver = vm["sdl-driver"].as<int>();
      options.glSwapInterval = vm["gl-swap"].as<int>();
      options.imgui = vm.count("no-imgui") == 0;

      if (vm.count("registry"))
      {
        options.registryOptions = vm["registry"].as<std::vector<std::string> >();
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

      if (vm.count("rom"))
      {
        options.customRom = vm["rom"].as<std::string>();
      }

      if (vm.count("f8rom"))
      {
        options.customRomF8 = vm["f8rom"].as<std::string>();
      }

      const int memclear = vm["memclear"].as<int>();
      if (memclear >=0 && memclear < NUM_MIP)
        options.memclear = memclear;

      options.benchmark = vm.count("benchmark") > 0;
      options.headless = vm.count("headless") > 0;
      options.log = vm.count("log") > 0;
      options.ntsc = vm.count("ntsc") > 0;
      options.fixedSpeed = vm.count("fixed-speed") > 0;

      options.paddleSquaring = vm.count("no-squaring") == 0;
      if (vm.count("device-name"))
      {
        options.paddleDeviceName = vm["device-name"].as<std::string>();
      }

      if (vm.count("geometry"))
      {
        parseGeometry(vm["geometry"].as<std::string>(), options.geometry);
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

  void applyOptions(const EmulatorOptions & options)
  {
    g_nMemoryClearType = options.memclear;

    LPCSTR szImageName_drive[NUM_DRIVES] = {nullptr, nullptr};
	  bool driveConnected[NUM_DRIVES] = {true, true};

    if (!options.disk1.empty())
    {
      szImageName_drive[DRIVE_1] = options.disk1.c_str();
    }

    if (!options.disk2.empty())
    {
      szImageName_drive[DRIVE_2] = options.disk2.c_str();
    }

    bool bBoot;
    InsertFloppyDisks(SLOT6, szImageName_drive, driveConnected, bBoot);

    if (!options.customRom.empty())
    {
      CloseHandle(g_hCustomRom);
      g_hCustomRom = CreateFile(options.customRom.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_READONLY, nullptr);
      if (g_hCustomRom == INVALID_HANDLE_VALUE)
      {
        LogFileOutput("Init: Failed to load Custom ROM: %s\n", options.customRom.c_str());
      }
    }

    if (!options.customRomF8.empty())
    {
      CloseHandle(g_hCustomRomF8);
      g_hCustomRomF8 = CreateFile(options.customRomF8.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_READONLY, nullptr);
      if (g_hCustomRomF8 == INVALID_HANDLE_VALUE)
      {
        LogFileOutput("Init: Failed to load custom F8 ROM: %s\n", options.customRomF8.c_str());
      }
    }

    Paddle::setSquaring(options.paddleSquaring);
  }

}
