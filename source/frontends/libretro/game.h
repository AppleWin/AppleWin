#pragma once

#include "frontends/common2/controllerquit.h"
#include "frontends/libretro/environment.h"
#include "frontends/libretro/diskcontrol.h"
#include "frontends/libretro/rkeyboard.h"

#include <memory>
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

    void keyboardCallback(bool down, unsigned keycode, uint32_t character, uint16_t key_modifiers);

    static constexpr size_t FPS = 60;
    static unsigned ourInputDevices[MAX_PADS];
    static constexpr retro_usec_t ourFrameTime = 1000000 / FPS;

  private:
    // keep them in this order!
    std::shared_ptr<LoggerContext> myLoggerContext;
    std::shared_ptr<common2::PTreeRegistry> myRegistry;
    std::shared_ptr<RegistryContext> myRegistryContext;
    std::shared_ptr<RetroFrame> myFrame;

    common2::ControllerQuit myControllerQuit;

    std::vector<int> myButtonStates;

    size_t myAudioChannelsSelected;

    KeyboardType myKeyboardType;

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
    void refreshVariables();

    void processKeyDown(unsigned keycode, uint32_t character, uint16_t key_modifiers);
    void processKeyUp(unsigned keycode, uint32_t character, uint16_t key_modifiers);
  };

}
