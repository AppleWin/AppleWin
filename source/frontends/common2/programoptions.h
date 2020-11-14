#pragma once

#include <string>


struct EmulatorOptions
{
  std::string disk1;
  std::string disk2;
  bool createMissingDisks = false;

  std::string snapshot;

  int memclear = 0;

  bool log = false;

  bool benchmark = false;
  bool headless = false;
  bool ntsc = false;  // only for applen

  bool squaring = true;  // turn the x/y range to a square

  bool saveConfigurationOnExit = false;
  bool useQtIni = false;  // use Qt .ini file (read only)

  bool run = true;  // false if options include "-h"

  bool multiThreaded = false;
  bool looseMutex = false;   // whether SDL_UpdateTexture is mutex protected (from CPU)
  int timerInterval = 16; // only when multithreaded

  int sdlDriver = -1; // default = -1 to let SDL choose
};

bool getEmulatorOptions(int argc, const char * argv [], const std::string & version, EmulatorOptions & options);
