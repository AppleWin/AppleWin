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

    void syncDebuggerState(SDLFrame* frame);

  private:
    bool mySyncCPU = true;

    bool myScrollConsole = true;
    char myInputBuffer[CONSOLE_WIDTH] = "";

    int64_t myBaseDebuggerCycles;
    std::unordered_map<DWORD, int64_t> myAddressCycles;

    void debuggerCommand(SDLFrame * frame, const char * s);

    void drawDisassemblyTable(SDLFrame * frame);
    void drawConsole();
    void drawBreakpoints();
    void drawRegisters();
    void drawAnnunciators();
    void drawSwitches();
 };

}