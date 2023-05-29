#pragma once

#include <string>
#include <vector>
#include <optional>

namespace common2
{

  struct Geometry
  {
    int width;
    int height;
    int x;
    int y;
  };


  struct EmulatorOptions
  {
    EmulatorOptions();

    std::string disk1;
    std::string disk2;

    std::string hardDisk1;
    std::string hardDisk2;

    std::string snapshotFilename;
    bool loadSnapshot = false;

    int memclear;

    bool log = false;

    bool benchmark = false;
    bool headless = false;
    bool ntsc = false;  // only for applen

    bool paddleSquaring = true;  // turn the x/y range to a square
    // on my PC it is something like
    // "/dev/input/by-id/usb-©Microsoft_Corporation_Controller_1BBE3DB-event-joystick"
    std::string paddleDeviceName;

    std::string configurationFile;
    bool useQtIni = false;  // use Qt .ini file (read only)

    bool run = true;  // false if options include "-h"

    bool fixedSpeed = false; // default adaptive
    size_t audioBuffer = 46; // in ms -> corresponds to 2048 samples (keep below 90ms)

    int sdlDriver = -1; // default = -1 to let SDL choose
    bool imgui = true; // use imgui renderer
    std::optional<Geometry> geometry; // must be initialised with defaults
    bool aspectRatio = false; // preserve aspect ratio
    int glSwapInterval = 1; // SDL_GL_SetSwapInterval
    std::optional<int> gameControllerIndex;
    std::string gameControllerMappingFile;

    std::string customRomF8;
    std::string customRom;

    std::string wavFileSpeaker;
    std::string wavFileMockingboard;

    std::vector<std::string> registryOptions;

    std::vector<std::string> natPortFwds;
  };

  bool getEmulatorOptions(int argc, const char * argv [], const std::string & edition, EmulatorOptions & options);

  void applyOptions(const EmulatorOptions & options);

}
