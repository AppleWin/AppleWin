#include "frontends/sdl/imgui/glselector.h"
#include "Debugger/Debug.h"
#include "Debugger/Debugger_Console.h"

#include <unordered_map>

namespace sa2
{

  class SDLFrame;

  class ImGuiDebugger
  {
  public:
    bool showDebugger = false;

    void drawDebugger(SDLFrame* frame);
    void resetDebuggerCycles();
  private:
    bool mySyncCPU = true;

    bool myScrollConsole = true;
    char myInputBuffer[CONSOLE_WIDTH] = "";

    uint64_t myBaseDebuggerCycles;
    std::unordered_map<DWORD, uint64_t> myAddressCycles;

    void debuggerCommand(SDLFrame * frame, const char * s);

    void drawDisassemblyTable(SDLFrame * frame);
    void drawConsole();
    void drawBreakpoints();
    void drawRegisters();
    void drawAnnunciators();
    void drawSwitches();
 };

}