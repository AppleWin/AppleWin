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
    bool myShowMemory = false;
    bool myShowCPU = false;
    bool mySyncCPU = false;

    int mySpeakerVolume = 50;
    int myMockingboardVolume = 50;

    MemoryEditor myMainMemoryEditor;
    MemoryEditor myAuxMemoryEditor;

    void showSettings();
    void showMemory();
    void showCPU();
  };

}
