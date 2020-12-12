#pragma once

#include "frontends/common2/speed.h"

class Game
{
public:
  Game();
  ~Game();

  bool loadGame(const char * path);

  void executeOneFrame();
  void processInputEvents();

  void drawVideoBuffer();

  static void keyboardCallback(bool down, unsigned keycode, uint32_t character, uint16_t key_modifiers);

 private:
  Speed mySpeed;  // fixed speed
};
