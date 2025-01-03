#include "StdAfx.h"
#include "frontends/sdl/imgui/sdldebugger.h"
#include "frontends/sdl/imgui/settingshelper.h"
#include "frontends/sdl/sdlframe.h"

#include "Interface.h"
#include "CPU.h"
#include "Memory.h"

#include <cinttypes>

namespace
{

  void printBoolean(const char * label, const bool value, const char * falseValue, const char * trueValue)
  {
    int elem = value;
    const char * name = value ? trueValue : falseValue;
    ImGui::SliderInt(label, &elem, 0, 1, name);
  }

  void printOnOff(const char * label, const bool value)
  {
    printBoolean(label, value, "OFF", "ON");
  }

  void printReg(const char label, const BYTE value)
  {
    ImGui::Text("%c  '%c'  %02X", label, sa2::getPrintableChar(value), value);
  }

  ImVec4 debuggerGetColor(int iColor)
  {
    const COLORREF cr = DebuggerGetColor(iColor);
    const ImVec4 color = sa2::colorrefToImVec4(cr);
    return color;
  }

  void debuggerTextColored(int iColor, const char* text)
  {
    const ImVec4 color = debuggerGetColor(iColor);
    ImGui::TextColored(color, "%s", text);
  }

  void adjustMouseText(const char* text, size_t length, char* out)
  {
    for (size_t i = 0; i < length; ++i)
    {
      const auto ch = *(text + i) & 0x7F;  // same as DebuggerPrint()
      // to understand the conversion, look at
      // resource/Debug_Font.bmp vs resource/debug6502.ttf
      // the latter comes from https://fontstruct.com/fontstructions/show/1912741/debug6502
      switch (ch)
      {
      case 0x00 ... 0x1F:  // mouse text -> U+00C0
        *out = 0xC3;
        ++out;
        *out = 0x80 + (ch - 0x00);
        break;
      case 0x7F:  // -> U+00BF
        *out = 0xC2;
        ++out;
        *out = 0xBF;
        break;
      case 0x80 ... 0x8F:  // bookmarks, not currently used -> U+00F0
        *out = 0xC3;
        ++out;
        *out = 0xB0 + (ch - 0x80);
        break;
      default:
        *out = ch;
      }
      ++out;
    }
    *out = 0x00;
  }

  // use this if "text" may contain mouse text
  void safeDebuggerTextColored(int iColor, const char* text)
  {
    const size_t length = strlen(text);
    char utf8[2 * length + 1];  // worst case is 2-bytes utf8 encoding
    adjustMouseText(text, length, utf8);
    debuggerTextColored(iColor, utf8);
  }

  void displayDisassemblyLine(const DisasmLine_t& line, const int bDisasmFormatFlags)
  {
    const char* pMnemonic = g_aOpcodes[line.iOpcode].sMnemonic;
    ImGui::Text("%s ", pMnemonic);

    if (line.bTargetImmediate)
    {
      ImGui::SameLine(0, 0);
      debuggerTextColored(FG_DISASM_OPERATOR, "#$");
    }

    if (line.bTargetIndexed || line.bTargetIndirect)
    {
      ImGui::SameLine(0, 0);
      debuggerTextColored(FG_DISASM_OPERATOR, "(");
    }

    int targetColor;
    if (bDisasmFormatFlags & DISASM_FORMAT_SYMBOL)
    {
      targetColor = FG_DISASM_SYMBOL;
    }
    else
    {
      if (line.iOpmode == AM_M)
      {
        targetColor = FG_DISASM_OPCODE;
      }
      else
      {
        targetColor = FG_DISASM_TARGET;
      }
    }

    const char * target = line.sTarget;
    if (target[0] == '$')
    {
      ImGui::SameLine(0, 0);
      debuggerTextColored(FG_DISASM_OPERATOR, "$");
      ++target;
    }
    ImGui::SameLine(0, 0);
    debuggerTextColored(targetColor, target);

    if (bDisasmFormatFlags & DISASM_FORMAT_OFFSET)
    {
      if (line.nTargetOffset > 0)
      {
        ImGui::SameLine(0, 0);
        debuggerTextColored(FG_DISASM_OPERATOR, "+");
      }
      else if (line.nTargetOffset < 0)
      {
        ImGui::SameLine(0, 0);
        debuggerTextColored(FG_DISASM_OPERATOR, "-");
      }
      ImGui::SameLine(0, 0);
      debuggerTextColored(FG_DISASM_OPCODE, line.sTargetOffset);
    }

    if (line.bTargetX)
    {
      ImGui::SameLine(0, 0);
      debuggerTextColored(FG_DISASM_OPERATOR, ",");
      ImGui::SameLine(0, 0);
      debuggerTextColored(FG_INFO_REG, "X");
    }
    else if ((line.bTargetY) && (!line.bTargetIndirect))
    {
      ImGui::SameLine(0, 0);
      debuggerTextColored(FG_DISASM_OPERATOR, ",");
      ImGui::SameLine(0, 0);
      debuggerTextColored(FG_INFO_REG, "Y");
    }

    if (line.bTargetIndexed || line.bTargetIndirect)
    {
      ImGui::SameLine(0, 0);
      debuggerTextColored(FG_DISASM_OPERATOR, ")");
    }

    if (line.bTargetIndexed && line.bTargetY)
    {
      ImGui::SameLine(0, 0);
      debuggerTextColored(FG_DISASM_OPERATOR, ",");
      ImGui::SameLine(0, 0);
      debuggerTextColored(FG_INFO_REG, "Y");
    }
  }

}

namespace sa2
{
  void ImGuiDebugger::drawDebugger(SDLFrame * frame)
  {
    myAddressCycles[regs.pc] = g_nCumulativeCycles - myBaseDebuggerCycles;

    ImGui::PushTabStop(false);  // natural ImGui tabbing interacts with Tab switching
    if (ImGui::Begin("Debugger", &showDebugger))
    {
      myCycleTabItems.init();
      if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows))
      {
        // unfortunately if we put this where it belongs (i.e. a few lines below with the rest of the key handling)
        // there are some focus-related issues, and we no longer capture keys
        // at the same time, there is a potential double counting with InputText,
        // but, since it does not handle TABs (see ImGuiInputTextFlags_CallbackCompletion & ImGuiInputTextFlags_AllowTabInput)
        // this is not an issue now
        ImGui::SetNextFrameWantCaptureKeyboard(true);
        if (ImGui::IsKeyChordPressed(ImGuiKey_Tab | ImGuiMod_Shift))
        {
          myCycleTabItems.prev();
        }
        else if (ImGui::IsKeyChordPressed(ImGuiKey_Tab))
        {
          myCycleTabItems.next();
        }
      }

      ImGui::BeginChild("Console", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()));
      if (myCycleTabItems.beginTabBar("Tabs"))
      {
        bool debuggerShortcutEnabled = false;
        if (g_nAppMode == MODE_DEBUG)
        {
          if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows))
          {
            debuggerShortcutEnabled = true;
            ImGui::SetNextFrameWantCaptureKeyboard(true);
            processDebuggerKeys();
          }
        }

        if (myCycleTabItems.beginTabItem("CPU"))
        {
          if (ImGui::RadioButton("Color", g_iColorScheme == SCHEME_COLOR)) { g_iColorScheme = SCHEME_COLOR; } ImGui::SameLine();
          if (ImGui::RadioButton("Mono", g_iColorScheme == SCHEME_MONO)) { g_iColorScheme = SCHEME_MONO; } ImGui::SameLine();
          if (ImGui::RadioButton("BW", g_iColorScheme == SCHEME_BW)) { g_iColorScheme = SCHEME_BW; } ImGui::SameLine();

          ImGui::BeginDisabled();
          ImGui::Checkbox("Shortcuts", &debuggerShortcutEnabled);
          ImGui::EndDisabled();
          ImGui::SameLine();

          ImGui::Checkbox("Auto-sync cursor", &mySyncCursor);

          bool recalc = false;
          if (mySyncCursor || (ImGui::SameLine(), ImGui::Button("Sync cursor")))
          {
            recalc = true;
            g_nDisasmCurAddress = regs.pc;
          }

          if (ImGui::Button("Step"))
          {
            frame->ChangeMode(MODE_DEBUG);
            frame->SingleStep();
          }
          ImGui::SameLine();

          if ((ImGui::SameLine(), ImGui::Button("Debug")))
          {
            frame->ChangeMode(MODE_DEBUG);
            resetDebuggerCycles();
          }
          if ((ImGui::SameLine(), ImGui::Button("Continue")))
          {
            frame->ChangeMode(MODE_RUNNING);
          }
          ImGui::SameLine();
          ImGui::Text("%016llu - %04X", g_nCumulativeCycles, regs.pc);

          if (!mySyncCursor)
          {
            ImGui::SameLine();
            ImGui::PushItemWidth(ImGui::GetFontSize() * 5);
            if (ImGui::InputScalar("Cursor", ImGuiDataType_U16, &g_nDisasmCurAddress, nullptr, nullptr, "%04X", ImGuiInputTextFlags_CharsHexadecimal))
            {
              recalc = true;
            }
            ImGui::PopItemWidth();
          }

          // do not flip next ||
          if (ImGui::SliderInt("Cursor row", &g_nDisasmCurLine, 0, 100) || recalc)
          {
            DisasmCalcTopBotAddress();
          }

          // this is about right to contain the fixed-size register child
          const ImVec2 registerTextSize = ImGui::CalcTextSize("Annunciators012345");

          ImGui::BeginChild("Disasm", ImVec2(-registerTextSize.x, 0));
          drawDisassemblyTable(frame);
          ImGui::EndChild();

          ImGui::SameLine();

          ImGui::BeginChild("Flags");
          const ImVec2 gap(0.0f, 20.0f);
          drawRegisters();
          ImGui::Dummy(gap);
          drawStackReturnAddress();
          ImGui::Dummy(gap);
          drawAnnunciators();
          ImGui::Dummy(gap);
          drawSwitches();
          ImGui::EndChild();

          ImGui::EndTabItem();
        }
        if (myCycleTabItems.beginTabItem("Console"))
        {
          drawConsole();
          ImGui::EndTabItem();
        }
        if (myCycleTabItems.beginTabItem("Breakpoints"))
        {
          drawBreakpoints();
          ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
      }
      ImGui::EndChild();

      if (g_nAppMode == MODE_DEBUG)
      {
        if (myInputTextHistory.inputText("Prompt"))
        {
          const std::string & command = myInputTextHistory.execute();
          debuggerCommand(frame, command.c_str());
        }
      }
    }
    ImGui::PopTabStop();  // natural ImGui tabbing interacts with Tab switching

    if (!showDebugger)
    {
      // this happens when the window is closed
      syncDebuggerState(frame);
    }

    ImGui::End();
  }

  void ImGuiDebugger::syncDebuggerState(SDLFrame * frame)
  {
    const AppMode_e mode = showDebugger ? MODE_DEBUG : MODE_RUNNING;
    frame->ChangeMode(mode);
  }

  void ImGuiDebugger::debuggerCommand(SDLFrame * frame, const char * s)
  {
    for (; *s; ++s)
    {
      DebuggerInputConsoleChar(*s);
    }
    // this might trigger a GO
    frame->ResetSpeed();
    DebuggerProcessKey(VK_RETURN);
    myScrollConsole = true;
  }

  void ImGuiDebugger::drawDisassemblyTable(SDLFrame * frame)
  {
    const ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_ScrollY;
    if (ImGui::BeginTable("Disassembly", 9, flags))
    {
      ImGui::PushStyleCompact();
      // weigths proportional to column width (including header)
      ImGui::TableSetupScrollFreeze(0, 1); // Make top row always visible
      ImGui::TableSetupColumn("", 0, 1);
      ImGui::TableSetupColumn("Address", 0, 5);
      ImGui::TableSetupColumn("Opcode", 0, 6);
      ImGui::TableSetupColumn("Symbol", 0, 10);
      ImGui::TableSetupColumn("Disassembly", 0, 15);
      ImGui::TableSetupColumn("Target", 0, 4);
      ImGui::TableSetupColumn("Branch", 0, 3);
      ImGui::TableSetupColumn("Immediate", 0, 4);
      ImGui::TableSetupColumn("Cycles", 0, 6);
      ImGui::TableHeadersRow();

      ImGuiListClipper clipper;
      clipper.Begin(1000);
      int row = 0;
      WORD nAddress = g_nDisasmTopAddress;
      while (clipper.Step())
      {
        for (; row < clipper.DisplayStart; ++row)
        {
          int iOpcode, iOpmode, nOpbyte;
          _6502_GetOpmodeOpbyte(nAddress, iOpmode, nOpbyte, nullptr);
          nAddress += nOpbyte;
        }
        IM_ASSERT(row == clipper.DisplayStart && "Clipper position mismatch");
        for (; row < clipper.DisplayEnd; ++row)
        {
          ImGui::PushID(nAddress);
          DisasmLine_t line;
          std::string const* pSymbol = FindSymbolFromAddress(nAddress);
          const int bDisasmFormatFlags = GetDisassemblyLine(nAddress, line);

          ImGui::TableNextRow();

          bool breakpointActive;
          bool breakpointEnabled;
          GetBreakpointInfo(nAddress, breakpointActive, breakpointEnabled);

          float red = 0.0;
          int state = 0;
          if (breakpointEnabled)
          {
            red = 0.5;
            state = 1;
          }
          else if (breakpointActive)
          {
            red = 0.25;
            state = -1;
          }

          ImGui::TableNextColumn();

          ImGui::BeginDisabled(g_nAppMode != MODE_DEBUG);
          if (ImGui::CheckBoxTristate("##State", &state))
          {
            if (!breakpointActive && state == 1)
            {
              const std::string command = std::string("bpx ") + line.sAddress;
              debuggerCommand(frame, command.c_str());
            }
            else
            {
              changeBreakpoint(nAddress, state == 1);
            }
          }
          ImGui::EndDisabled();

          if (nAddress == regs.pc)
          {
            const ImU32 currentBgColor = ImGui::GetColorU32(ImVec4(red, 0, 1, 1));
            ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, currentBgColor);
          }
          else if (nAddress == g_nDisasmCurAddress)
          {
            const ImU32 currentBgColor = ImGui::GetColorU32(ImVec4(red, 0, 0.5, 1));
            ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, currentBgColor);
          }
          else if (breakpointActive || breakpointEnabled)
          {
            const ImU32 currentBgColor = ImGui::GetColorU32(ImVec4(red, 0, 0, 1));
            ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, currentBgColor);
          }

          ImGui::TableNextColumn();
          if (ImGui::Selectable(line.sAddress, g_nDisasmCurAddress == nAddress, ImGuiSelectableFlags_SpanAllColumns))
          {
            g_nDisasmCurAddress = nAddress;
            mySyncCursor = false;
          }

          ImGui::TableNextColumn();
          debuggerTextColored(FG_DISASM_OPCODE, line.sOpCodes);

          ImGui::TableNextColumn();
          if (pSymbol)
          {
            debuggerTextColored(FG_DISASM_SYMBOL, pSymbol->c_str());
          }

          ImGui::TableNextColumn();
          displayDisassemblyLine(line, bDisasmFormatFlags);

          ImGui::TableNextColumn();
          if (bDisasmFormatFlags & DISASM_FORMAT_TARGET_POINTER)
          {
            debuggerTextColored(FG_DISASM_ADDRESS, line.sTargetPointer);
            
            if (bDisasmFormatFlags & DISASM_FORMAT_TARGET_VALUE)
            {
              ImGui::SameLine(0, 0);
              debuggerTextColored(FG_DISASM_OPERATOR, ":");
              ImGui::SameLine(0, 0);
              debuggerTextColored(FG_DISASM_OPCODE, line.sTargetValue);
            }
          }
          else if (line.bTargetImmediate && line.nImmediate)
          {
            debuggerTextColored(FG_DISASM_OPERATOR, "#");
            ImGui::SameLine(0, 0);
            debuggerTextColored(FG_DISASM_SINT8, line.sImmediateSignedDec);
          }

          ImGui::TableNextColumn();
          if (bDisasmFormatFlags & DISASM_FORMAT_BRANCH)
          {
            safeDebuggerTextColored(FG_DISASM_BRANCH, line.sBranch);
          }

          ImGui::TableNextColumn();
          if (bDisasmFormatFlags & DISASM_FORMAT_CHAR)
          {
            DebugColors_e color;
            const unsigned int c = (unsigned char)line.nImmediate;
            if ((c & 0x7F) < 0x20)
            {
              // Invisible control characters 0x00..0x1F are mapped to a visible range
              // in sImmediate, but we want to specially color it.
              // Mimics "bCtrlBit" logic in Windows.
              color = FG_INFO_CHAR_LO;
            }
            else if (c > 0x7F)
            {
              // Characters with high bit set are mapped back down to 0x00..0x7F in
              // sImmediate, but we want to specially color it.
              // Mimics "bHighBit" logic in Windows.
              color = FG_INFO_CHAR_HI;
            }
            else
            {
              color = FG_DISASM_CHAR;
            }
            safeDebuggerTextColored(color, line.sImmediate);
          }

          ImGui::TableNextColumn();
          if (g_nAppMode == MODE_DEBUG)
          {
            if (myAddressCycles.find(nAddress) != myAddressCycles.end())
            {
              ImGui::Text("%" PRIu64, myAddressCycles[nAddress]);
            }
          }

          nAddress += line.nOpbyte;
          ImGui::PopID();
        }
      }
      ImGui::PopStyleCompact();
      ImGui::EndTable();
    }
  }

  void ImGuiDebugger::drawRegisters()
  {
    ImGui::TextUnformatted("Registers");
    ImGui::Separator();
    printReg('A', regs.a);
    printReg('X', regs.x);
    printReg('Y', regs.y);
    printReg('P', regs.ps);
    ImGui::Text("PC    %04X", regs.pc);
    ImGui::Text("SP    %04X", regs.sp);
    if (regs.bJammed)
    {
      ImGui::Separator();
      ImGui::Selectable("CPU Jammed");
    }
  }

  void ImGuiDebugger::drawStackReturnAddress()
  {
    ImGui::TextUnformatted("Stack return");
    ImGui::Separator();
    const WORD nAddress = _6502_GetStackReturnAddress();
    const std::string s = FormatAddress(nAddress, 3);
    if (ImGui::Button(s.c_str()))
    {
      setCurrentAddress(nAddress);
    }
  }

  void ImGuiDebugger::drawAnnunciators()
  {
    ImGui::TextUnformatted("Annunciators");
    ImGui::Separator();
    ImGui::BeginDisabled();
    for (UINT i = 0; i < 4; ++i)
    {
      char buffer[20];
      sprintf(buffer, "Ann %d", i);
      printOnOff(buffer, MemGetAnnunciator(i));
    }
    ImGui::EndDisabled();
  }

  void ImGuiDebugger::drawSwitches()
  {
    Video & video = GetVideo();
    ImGui::TextUnformatted("Switches C0xx");
    ImGui::Separator();
    ImGui::BeginDisabled();
    printBoolean("50", video.VideoGetSWTEXT(), "GR", "TEXT");
    printBoolean("52", video.VideoGetSWMIXED(), "FULL", "MIX");
    printBoolean("54", video.VideoGetSWPAGE2(), "PAGE 1", "PAGE 2");
    printBoolean("56", video.VideoGetSWHIRES(), "RES LO", "RES HI");
    printBoolean("5E", !video.VideoGetSWDHIRES(), "DHGR", "HGR");
    ImGui::Separator();
    printBoolean("00", video.VideoGetSW80STORE(), "80sto 0", "80sto 1");
    printBoolean("02", GetMemMode() & MF_AUXREAD, "R m", "R x");
    printBoolean("02", GetMemMode() & MF_AUXWRITE, "W m", "R x");
    printBoolean("0C", video.VideoGetSW80COL(), "Col 40", "Col 80");
    printBoolean("0E", video.VideoGetSWAltCharSet(), "ASC", "MOUS");
    ImGui::EndDisabled();
  }

  void ImGuiDebugger::drawBreakpoints()
  {
    if (ImGui::BeginTable("Breakpoints", 10, ImGuiTableFlags_RowBg))
    {
      ImGui::TableSetupColumn("ID");
      ImGui::TableSetupColumn("First");
      ImGui::TableSetupColumn("Last");
      ImGui::TableSetupColumn("Source");
      ImGui::TableSetupColumn("Operator");
      ImGui::TableSetupColumn("Enabled");
      ImGui::TableSetupColumn("Stop");
      ImGui::TableSetupColumn("Temporary");
      ImGui::TableSetupColumn("Counter");
      ImGui::TableSetupColumn("Hit");
      ImGui::TableHeadersRow();

      for (int i = 0; i < MAX_BREAKPOINTS; ++i)
      {
        Breakpoint_t & bp = g_aBreakpoints[i];
        if (bp.bSet)
        {
          ImGui::PushID(i);
          ImGui::TableNextRow();
          ImGui::TableNextColumn();
          ImGui::Text("%2d", i);
          ImGui::TableNextColumn();
          ImGui::Text("%04X", bp.nAddress);
          ImGui::TableNextColumn();
          ImGui::Text("%04X", bp.nAddress + bp.nLength - 1);
          ImGui::TableNextColumn();
          ImGui::Text("%2d", bp.eSource);
          ImGui::TableNextColumn();
          ImGui::Text("%2d", bp.eOperator);
          ImGui::TableNextColumn();
          ImGui::Checkbox("##Enabled", &bp.bEnabled);
          ImGui::TableNextColumn();
          ImGui::Checkbox("##Stop", &bp.bStop);
          ImGui::TableNextColumn();
          ImGui::Checkbox("##Temp", &bp.bTemp);
          ImGui::TableNextColumn();
          ImGui::Text("%08X", bp.nHitCount);
          ImGui::TableNextColumn();
          ImGui::BeginDisabled();
          ImGui::Checkbox("##Hit", &bp.bHit);
          ImGui::EndDisabled();
          ImGui::PopID();
        }
      }

      ImGui::EndTable();
    }
  }

  void ImGuiDebugger::drawConsole()
  {
    const ImGuiTableFlags flags = ImGuiTableFlags_BordersV | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_ScrollY;
    if (ImGui::BeginTable("Console", 1, flags))
    {
      for (int i = g_nConsoleDisplayTotal; i >= CONSOLE_FIRST_LINE; --i)
      {
        const conchar_t * src = g_aConsoleDisplay[i];

        char line[CONSOLE_WIDTH + 1];
        char lineMouseText[2 * CONSOLE_WIDTH + 1];
        size_t length = 0;

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        COLORREF currentColor = DebuggerGetColor( FG_CONSOLE_OUTPUT );

        const auto textAndReset = [&line, &lineMouseText, &length, &currentColor] () {
          line[length] = 0;
          const ImVec4 color = colorrefToImVec4(currentColor);
          adjustMouseText(line, length, lineMouseText);
          ImGui::TextColored(color, "%s", lineMouseText);
          length = 0;
        };

        for (size_t j = 0; j < CONSOLE_WIDTH; ++j)
        {
          const conchar_t g = src[j];
          if (!g)
          {
            break;
          }
          if (ConsoleColor_IsColorOrMouse(g))
          {
            // colors propagate till next color information
            // left in, right out = [ )
            textAndReset();
            ImGui::SameLine(0, 0);
            currentColor = ConsoleColor_GetColor(g);
          }
          line[length] = ConsoleChar_GetChar(g);
          ++length;
        }
        textAndReset();
      }
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::TextUnformatted(g_aConsoleInput);

      if (myScrollConsole)
      {
        ImGui::SetScrollHereY(1.0f);
        myScrollConsole = false;
      }
      ImGui::EndTable();
    }
  }

  void ImGuiDebugger::resetDebuggerCycles()
  {
    myAddressCycles.clear();
    myBaseDebuggerCycles = g_nCumulativeCycles;
  }

  void ImGuiDebugger::processDebuggerKeys()
  {
    if (ImGui::IsKeyChordPressed(ImGuiKey_Space | ImGuiMod_Ctrl))
    {
      CmdStepOver(0);
    }
    else if (ImGui::IsKeyChordPressed(ImGuiKey_Space | ImGuiMod_Shift))
    {
      CmdStepOut(0);
    }
    else if (ImGui::IsKeyChordPressed(ImGuiKey_Space))
    {
      CmdTrace(0);
    }
    else if (ImGui::IsKeyChordPressed(ImGuiKey_DownArrow | ImGuiMod_Ctrl))
    {
      CmdCursorRunUntil(0);
    }
    else if (ImGui::IsKeyChordPressed(ImGuiKey_RightArrow | ImGuiMod_Ctrl))
    {
      CmdCursorSetPC(0);
    }
    else if (ImGui::IsKeyChordPressed(ImGuiKey_LeftArrow))
    {
    	WORD nAddress = _6502_GetStackReturnAddress();
      setCurrentAddress(nAddress);
    }
  }

  void ImGuiDebugger::setCurrentAddress(const uint32_t nAddress)
  {
    g_nDisasmCurAddress = nAddress;
    DisasmCalcTopBotAddress();
    mySyncCursor = false;
  }

}
