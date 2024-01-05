#pragma once

#include "frontends/sdl/imgui/glselector.h"
#include "frontends/sdl/imgui/sdldebugger.h"
#include "frontends/sdl/imgui/imgui-filebrowser/imfilebrowser.h"
#include "frontends/sdl/sdirectsound.h"

namespace sa2
{

  class SDLFrame;

  class ImGuiSettings
  {
  public:
    void show(SDLFrame* frame, ImFont * debuggerFont);
    float drawMenuBar(SDLFrame* frame);
    void resetDebuggerCycles();

    bool windowed = false;
    bool quit = false;

  private:
    bool myShowDemo = false;
    bool myShowSettings = false;
    bool myShowMemory = false;
    bool myShowAbout = false;

    int mySpeakerVolume;
    int myMockingboardVolume;

    size_t myOpenSlot = 0;
    size_t myOpenDrive = 0;

    ImGuiDebugger myDebugger;
    ImGui::FileBrowser myFileDialog;

    std::vector<MemoryEditor> myMemoryEditors;

    std::vector<SoundInfo> myAudioInfo;

    void showSettings(SDLFrame* frame);
    void showMemory();
    void showAboutWindow();
    void openFileDialog(const std::string & diskName, const size_t slot, const size_t drive);
  };

}
