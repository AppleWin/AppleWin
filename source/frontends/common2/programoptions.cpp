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
#include "Speaker.h"
#include "Riff.h"
#include "CardManager.h"

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

  template <typename T>
  bool setOption(const po::variables_map & vm, const char * x, std::optional<T> & value)
  {
    if (vm.count(x))
    {
      value = vm[x].as<T>();
      return true;
    }
    else
    {
      return false;
    }
  }

  template <typename T>
  bool setOption(const po::variables_map & vm, const char * x, T & value)
  {
    if (vm.count(x))
    {
      value = vm[x].as<T>();
      return true;
    }
    else
    {
      return false;
    }
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
      ("h1", po::value<std::string>(), "Hard Disk in 1st drive")
      ("h2", po::value<std::string>(), "Hard Disk in 2nd drive")
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
      ("rom", po::value<std::string>(), "Custom 12k/16k ROM")
      ("f8rom", po::value<std::string>(), "Custom 2k ROM")
      ;
    desc.add(memoryDesc);

    po::options_description emulatorDesc("Emulator");
    emulatorDesc.add_options()
      ("log", "Log to AppleWin.log")
      ("fixed-speed", "Fixed (non-adaptive) speed")
      ("benchmark,b", "Benchmark emulator")
      ("no-squaring", "Gamepad range is (already) a square")
      ("nat", po::value<std::vector<std::string>>(), "SLIRP PortFwd")
      ;
    desc.add(emulatorDesc);

    po::options_description audioDesc("Audio");
    audioDesc.add_options()
      ("wav-speaker", po::value<std::string>(), "Speaker wav output")
      ("wav-mockingboard", po::value<std::string>(), "Mockingboard wav output")
      ;
    desc.add(audioDesc);

    po::options_description sdlDesc("SDL");
    sdlDesc.add_options()
      ("sdl-driver", po::value<int>()->default_value(options.sdlDriver), "SDL driver")
      ("gl-swap", po::value<int>()->default_value(options.glSwapInterval), "SDL_GL_SwapInterval")
      ("no-imgui", "Plain SDL2 renderer")
      ("geometry", po::value<std::string>(), "WxH[+X+Y]")
      ("aspect-ratio", "Always preserve correct aspect ratio")
      ("game-controller", po::value<int>(), "SDL_GameControllerOpen")
      ("game-mapping-file", po::value<std::string>(), "SDL_GameControllerAddMappingsFromFile")
      ;
    desc.add(sdlDesc);

    po::options_description applenDesc("applen");
    applenDesc.add_options()
      ("headless", "Headless: disable video (freewheel)")
      ("ntsc,nt", "NTSC: execute NTSC code")
      ("device-name", po::value<std::string>(), "Gamepad device name (for applen)")
      ;
    desc.add(applenDesc);

    po::variables_map vm;
    try
    {
      po::store(po::parse_command_line(argc, argv, desc), vm);

      if (vm.count("help"))
      {
        std::cout << desc << std::endl;
        return false;
      }

      // Configuration
      options.configurationFile = vm["conf"].as<std::string>();
      setOption(vm, "registry", options.registryOptions);
      options.useQtIni = vm.count("qt-ini");

      // Disk
      setOption(vm, "d1", options.disk1);
      setOption(vm, "d2", options.disk2);
      setOption(vm, "h1", options.hardDisk1);
      setOption(vm, "h2", options.hardDisk2);

      // Snapshot
      if (setOption(vm, "load-state", options.snapshotFilename))
      {
        options.loadSnapshot = true;
      }

      if (setOption(vm, "state-filename", options.snapshotFilename))
      {
        options.loadSnapshot = false;
      }

      // Memory
      const int memclear = vm["memclear"].as<int>();
      if (memclear >=0 && memclear < NUM_MIP)
        options.memclear = memclear;
      setOption(vm, "rom", options.customRom);
      setOption(vm, "f8rom", options.customRomF8);

      // Emulator
      options.log = vm.count("log") > 0;
      options.fixedSpeed = vm.count("fixed-speed") > 0;
      options.benchmark = vm.count("benchmark") > 0;
      options.paddleSquaring = vm.count("no-squaring") == 0;
      setOption(vm, "nat", options.natPortFwds);

      // Audio
      setOption(vm, "wav-speaker", options.wavFileSpeaker);
      setOption(vm, "wav-mockingboard", options.wavFileMockingboard);

      // SDL
      options.sdlDriver = vm["sdl-driver"].as<int>();
      options.glSwapInterval = vm["gl-swap"].as<int>();
      options.imgui = vm.count("no-imgui") == 0;

      std::string geometry;
      if (setOption(vm, "geometry", geometry))
      {
        parseGeometry(geometry, options.geometry);
      }

      options.aspectRatio = vm.count("aspect-ratio") > 0;
      setOption(vm, "game-controller", options.gameControllerIndex);
      setOption(vm, "game-mapping-file", options.gameControllerMappingFile);

      // applen
      options.headless = vm.count("headless") > 0;
      options.ntsc = vm.count("ntsc") > 0;
      setOption(vm, "device-name", options.paddleDeviceName);

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

    bool bBoot;

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

    InsertFloppyDisks(SLOT6, szImageName_drive, driveConnected, bBoot);

    LPCSTR szImageName_harddisk[NUM_HARDDISKS] = {nullptr, nullptr};

    if (!options.hardDisk1.empty())
    {
      szImageName_harddisk[DRIVE_1] = options.hardDisk1.c_str();
    }

    if (!options.hardDisk2.empty())
    {
      szImageName_harddisk[DRIVE_2] = options.hardDisk2.c_str();
    }

    InsertHardDisks(SLOT7, szImageName_harddisk, bBoot);

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

    if (!options.wavFileSpeaker.empty())
    {
      if (RiffInitWriteFile(options.wavFileSpeaker.c_str(), SPKR_SAMPLE_RATE, 1))
      {
        Spkr_OutputToRiff();
      }
    }
    else if (!options.wavFileMockingboard.empty())
    {
      if (RiffInitWriteFile(options.wavFileMockingboard.c_str(), 44100, 2))
      {
        GetCardMgr().GetMockingboardCardMgr().OutputToRiff();
      }
    }

    Paddle::setSquaring(options.paddleSquaring);
  }

}
