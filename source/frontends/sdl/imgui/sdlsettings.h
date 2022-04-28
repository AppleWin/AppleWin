#pragma once

#include "frontends/sdl/imgui/glselector.h"
#include "frontends/sdl/sdirectsound.h"
#include "Debugger/Debug.h"
#include "Debugger/Debugger_Console.h"

#include <unordered_map>

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
    bool myShowDebugger = false;
    bool mySyncCPU = true;
    bool myShowAbout = false;

    bool myScrollConsole = true;
    char myInputBuffer[CONSOLE_WIDTH] = "";

    int mySpeakerVolume;
    int myMockingboardVolume;

    uint64_t myBaseDebuggerCycles;
    std::unordered_map<DWORD, uint64_t> myAddressCycles;

    std::vector<MemoryEditor> myMemoryEditors;

    std::vector<SoundInfo> myAudioInfo;

    void showSettings(SDLFrame* frame);
    void showDebugger(SDLFrame* frame);
    void showMemory();
    void showAboutWindow();

    void drawDisassemblyTable(SDLFrame * frame);
    void drawConsole();
    void drawBreakpoints();
    void drawRegisters();
    void drawAnnunciators();
    void drawSwitches();

    void debuggerCommand(SDLFrame * frame, const char * s);
  };

}
