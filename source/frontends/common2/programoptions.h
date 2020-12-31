#pragma once

#include <string>
#include <vector>


struct EmulatorOptions
{
  std::string disk1;
  std::string disk2;

  std::string snapshotFilename;
  bool loadSnapshot = false;

  int memclear = 0;

  bool log = false;

  bool benchmark = false;
  bool headless = false;
  bool ntsc = false;  // only for applen

  bool paddleSquaring = true;  // turn the x/y range to a square
  // on my PC it is something like
  // "/dev/input/by-id/usb-©Microsoft_Corporation_Controller_1BBE3DB-event-joystick"
  std::string paddleDeviceName;

  bool saveConfigurationOnExit = false;
  bool useQtIni = false;  // use Qt .ini file (read only)

  bool run = true;  // false if options include "-h"

  bool multiThreaded = false;
  bool looseMutex = false;   // whether SDL_UpdateTexture is mutex protected (from CPU)
  int timerInterval = 16; // only when multithreaded
  bool fixedSpeed = false; // default adaptive

  int sdlDriver = -1; // default = -1 to let SDL choose

  std::vector<std::string> registryOptions;
};

bool getEmulatorOptions(int argc, const char * argv [], const std::string & edition, EmulatorOptions & options);

void applyOptions(const EmulatorOptions & options);
