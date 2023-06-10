#pragma once

#include "frontends/common2/speed.h"
#include "frontends/libretro/environment.h"
#include "frontends/libretro/diskcontrol.h"

#include <chrono>
#include <string>
#include <vector>

class LoggerContext;
class RegistryContext;

namespace common2
{
  class PTreeRegistry;
}

namespace ra2
{

  class RetroFrame;

  class Game
  {
  public:
    Game();
    ~Game();

    bool loadSnapshot(const std::string & path);

    void reset();

    void updateVariables();
    void executeOneFrame();
    void processInputEvents();
    void writeAudio(const size_t fps);

    void drawVideoBuffer();

    double getMousePosition(int i) const;

    DiskControl & getDiskControl();

    static void keyboardCallback(bool down, unsigned keycode, uint32_t character, uint16_t key_modifiers);

    static void frameTimeCallback(retro_usec_t usec);
    static constexpr size_t FPS = 60;
    static unsigned ourInputDevices[MAX_PADS];
    static retro_usec_t ourFrameTime;

  private:
    // keep them in this order!
    std::shared_ptr<LoggerContext> myLoggerContext;
    std::shared_ptr<common2::PTreeRegistry> myRegistry;
    std::shared_ptr<RegistryContext> myRegistryContext;
    std::shared_ptr<RetroFrame> myFrame;

    common2::Speed mySpeed;  // fixed speed

    std::chrono::steady_clock::time_point myFirstBtnPress;

    std::vector<int> myButtonStates;

    size_t myAudioChannelsSelected;

    struct MousePosition_t
    {
      double position; // -1 to 1
      double multiplier;
      unsigned id;
    };

    MousePosition_t myMouse[2];

    DiskControl myDiskControl;

    bool checkButtonPressed(unsigned id);
    void keyboardEmulation();
    void mouseEmulation();

    static void processKeyDown(unsigned keycode, uint32_t character, uint16_t key_modifiers);
    static void processKeyUp(unsigned keycode, uint32_t character, uint16_t key_modifiers);
  };

}
