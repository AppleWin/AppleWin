#pragma once

#include "frontends/common2/speed.h"
#include "frontends/libretro/environment.h"

#include "linux/context.h"

#include <string>
#include <vector>

namespace ra2
{

  class RetroFrame;

  class Game
  {
  public:
    Game();

    bool loadGame(const std::string & path);
    bool loadSnapshot(const std::string & path);

    void executeOneFrame();
    void processInputEvents();

    void drawVideoBuffer();

    static void keyboardCallback(bool down, unsigned keycode, uint32_t character, uint16_t key_modifiers);

    static void frameTimeCallback(retro_usec_t usec);
    static constexpr size_t FPS = 60;
    static unsigned ourInputDevices[MAX_PADS];
    static retro_usec_t ourFrameTime;

  private:
    // keep them in this order!
    std::shared_ptr<LoggerContext> myLoggerContext;
    std::shared_ptr<RegistryContext> myRegistryContext;
    std::shared_ptr<RetroFrame> myFrame;
    std::shared_ptr<Initialisation> myInitialisation;

    common2::Speed mySpeed;  // fixed speed

    std::vector<int> myButtonStates;

    bool checkButtonPressed(unsigned id);
    void keyboardEmulation();

    static void processKeyDown(unsigned keycode, uint32_t character, uint16_t key_modifiers);
    static void processKeyUp(unsigned keycode, uint32_t character, uint16_t key_modifiers);
  };

}
