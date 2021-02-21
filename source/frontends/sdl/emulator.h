#pragma once

#include <SDL.h>
#include <memory>

#include "frontends/common2/speed.h"

class SDLFrame;

class Emulator
{
public:
  Emulator(
    const std::shared_ptr<SDLFrame> & frame,
    const bool fixedSpeed
    );

  void execute(const size_t milliseconds);

  void processEvents(bool & quit);

private:
  void processKeyDown(const SDL_KeyboardEvent & key, bool & quit);
  void processKeyUp(const SDL_KeyboardEvent & key);
  void processText(const SDL_TextInputEvent & text);

  const std::shared_ptr<SDLFrame> myFrame;

  bool myForceCapsLock;
  int myMultiplier;
  bool myFullscreen;

  Speed mySpeed;
};
