#pragma once

#include "frontends/common2/speed.h"
#include "frontends/retro/environment.h"

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

  static unsigned input_devices[MAX_PADS];

private:
  Speed mySpeed;  // fixed speed
  std::vector<uint8_t> myVideoBuffer;

  size_t myPitch;
  size_t myOffset;
  size_t myHeight;
  size_t myBorderlessWidth;
  size_t myBorderlessHeight;

  std::vector<int> myButtonStates;

  bool checkButtonPressed(unsigned id);
  void keyboardEmulation();

  static void processKeyDown(unsigned keycode, uint32_t character, uint16_t key_modifiers);
  static void processKeyUp(unsigned keycode, uint32_t character, uint16_t key_modifiers);
};
