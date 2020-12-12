#pragma once

#include "frontends/common2/speed.h"

#include <unordered_map>

class Game
{
public:
  Game();
  ~Game();

  bool loadGame(const char * path);

  void executeOneFrame();
  void processInputEvents();

  void drawVideoBuffer();

 private:
  const std::unordered_map<unsigned, BYTE> myKeymap;

  Speed mySpeed;  // fixed speed
  std::unordered_map<unsigned, bool> myKeystate;

  void processKeyboardEvents();
};
