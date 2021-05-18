#pragma once

#include "frontends/sdl/imgui/gles.h"
#include "frontends/sdl/sdirectsound.h"
#include "Debugger/Debug.h"
#include "Debugger/Debugger_Console.h"

namespace sa2
{

  class SDLFrame;

  class ImGuiSettings
  {
  public:
    void show(SDLFrame* frame);
    float drawMenuBar(SDLFrame* frame);

    bool windowed = false;

  private:
    bool myShowDemo = false;
    bool myShowSettings = false;
    bool myShowMemory = false;
    bool myShowDebugger = false;
    bool mySyncCPU = true;
    bool myShowAbout = false;

    bool myScrollConsole = true;
    char myInputBuffer[CONSOLE_WIDTH] = "";

    int mySpeakerVolume;
    int myMockingboardVolume;

    MemoryEditor myMainMemoryEditor;
    MemoryEditor myAuxMemoryEditor;

    std::vector<SoundInfo> myAudioInfo;

    void showSettings(SDLFrame* frame);
    void showDebugger(SDLFrame* frame);
    void showMemory();
    void showAboutWindow();

    void drawDisassemblyTable(SDLFrame * frame);
    void drawConsole();

    void debuggerCommand(SDLFrame * frame, const char * s);
  };

}
