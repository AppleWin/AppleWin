#pragma once

#include "frontends/sdl/imgui/glselector.h"
#include "frontends/sdl/imgui/sdldebugger.h"
#include "frontends/sdl/sdirectsound.h"

namespace sa2
{

  class SDLFrame;

  class ImGuiSettings
  {
  public:
    void show(SDLFrame* frame);
    float drawMenuBar(SDLFrame* frame);
    void resetDebuggerCycles();

    bool windowed = false;

  private:
    bool myShowDemo = false;
    bool myShowSettings = false;
    bool myShowMemory = false;
    bool myShowAbout = false;

    int mySpeakerVolume;
    int myMockingboardVolume;

    ImGuiDebugger myDebugger;

    std::vector<MemoryEditor> myMemoryEditors;

    std::vector<SoundInfo> myAudioInfo;

    void showSettings(SDLFrame* frame);
    void showMemory();
    void showAboutWindow();
  };

}
