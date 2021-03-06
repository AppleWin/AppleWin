#pragma once

#include "frontends/sdl/imgui/gles.h"

namespace sa2
{

  class ImGuiSettings
  {
  public:
    void show();
    float drawMenuBar();

    bool windowed = false;

  private:
    bool myShowDemo = false;
    bool myShowSettings = false;
    int mySpeakerVolume = 50;
    int myMockingboardVolume = 50;

    void showSettings();
  };

}
