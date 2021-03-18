#include "StdAfx.h"
#include "frontends/sdl/imgui/sdlsettings.h"
#include "frontends/sdl/imgui/settingshelper.h"
#include "frontends/sdl/sdlframe.h"

#include "Interface.h"
#include "CardManager.h"
#include "Core.h"
#include "CPU.h"
#include "CardManager.h"
#include "Speaker.h"
#include "Mockingboard.h"
#include "Registry.h"
#include "Memory.h"

#include "Debugger/Debug.h"
#include "Debugger/DebugDefs.h"

namespace
{

  void HelpMarker(const char* desc)
  {
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered())
    {
      ImGui::BeginTooltip();
      ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
      ImGui::TextUnformatted(desc);
      ImGui::PopTextWrapPos();
      ImGui::EndTooltip();
    }
  }

  void formatDisassemblyLine(const DisasmLine_t& line, const int bDisasmFormatFlags, char* sDisassembly, const int nBufferSize)
  {
    sDisassembly[0] = 0;

    const char* pMnemonic = g_aOpcodes[line.iOpcode].sMnemonic;
    strcat(sDisassembly, pMnemonic);
    strcat(sDisassembly, " ");

    if (line.bTargetImmediate)
    {
      strcat(sDisassembly, "#$");
    }

    if (line.bTargetIndexed || line.bTargetIndirect)
    {
      strcat(sDisassembly, "(");
    }

    strcat(sDisassembly, line.sTarget);

    if (bDisasmFormatFlags & DISASM_FORMAT_OFFSET)
    {
      if (line.nTargetOffset > 0)
      {
        strcat(sDisassembly, "+");
      }
      else if (line.nTargetOffset < 0)
      {
        strcat(sDisassembly, "-");
      }
      strcat(sDisassembly, line.sTargetOffset);
    }

    if (line.bTargetX)
    {
      strcat(sDisassembly, ",X");
    }
    else if ((line.bTargetY) && (!line.bTargetIndirect))
    {
      strcat(sDisassembly, ",Y");
    }

    if (line.bTargetIndexed || line.bTargetIndirect)
    {
      strcat(sDisassembly, ")");
    }

    if (line.bTargetIndexed && line.bTargetY)
    {
      strcat(sDisassembly, ",Y");
    }
  }

}

namespace sa2
{

  void ImGuiSettings::showSettings()
  {
    if (ImGui::Begin("Settings", &myShowSettings))
    {
      ImGuiIO& io = ImGui::GetIO();

      if (ImGui::BeginTabBar("Settings"))
      {
        if (ImGui::BeginTabItem("General"))
        {
          ImGui::Checkbox("Apple Video windowed", &windowed);
          ImGui::SameLine(); HelpMarker("Show Apple video in a separate window.");

          ImGui::Checkbox("Memory", &myShowMemory);
          ImGui::SameLine(); HelpMarker("Show Apple memory.");

          if (ImGui::Checkbox("Debugger", &myShowDebugger) && myShowDebugger)
          {
            DebugBegin();
          }
          ImGui::SameLine(); HelpMarker("Show Apple CPU.");

          ImGui::Checkbox("Show Demo", &myShowDemo);
          ImGui::SameLine(); HelpMarker("Show Dear ImGui DemoWindow.");

          ImGui::Text("FPS: %d", int(io.Framerate));
          ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Hardware"))
        {
          if (ImGui::BeginTable("Cards", 2, ImGuiTableFlags_RowBg))
          {
            CardManager & manager = GetCardMgr();
            ImGui::TableSetupColumn("Slot");
            ImGui::TableSetupColumn("Card");
            ImGui::TableHeadersRow();

            for (size_t slot = 0; slot < 8; ++slot)
            {
              ImGui::TableNextColumn();
              ImGui::Selectable(std::to_string(slot).c_str());
              ImGui::TableNextColumn();
              const SS_CARDTYPE card = manager.QuerySlot(slot);;
              ImGui::Selectable(getCardName(card).c_str());
            }
            ImGui::TableNextColumn();
            ImGui::Selectable("AUX");
            ImGui::TableNextColumn();
            const SS_CARDTYPE card = manager.QueryAux();
            ImGui::Selectable(getCardName(card).c_str());

            ImGui::EndTable();
          }
          ImGui::Separator();

          if (ImGui::BeginTable("Type", 2, ImGuiTableFlags_RowBg))
          {
            ImGui::TableNextColumn();
            ImGui::Selectable("Apple 2");
            ImGui::TableNextColumn();
            const eApple2Type a2e = GetApple2Type();
            ImGui::Selectable(getApple2Name(a2e).c_str());

            ImGui::TableNextColumn();
            ImGui::Selectable("CPU");
            ImGui::TableNextColumn();
            const eCpuType cpu = GetMainCpu();
            ImGui::Selectable(getCPUName(cpu).c_str());

            ImGui::TableNextColumn();
            ImGui::Selectable("Mode");
            ImGui::TableNextColumn();
            ImGui::Selectable(getModeName(g_nAppMode).c_str());

            ImGui::EndTable();
          }

          ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Audio"))
        {
          const int volumeMax = GetPropertySheet().GetVolumeMax();
          if (ImGui::SliderInt("Speaker volume", &mySpeakerVolume, 0, volumeMax))
          {
            SpkrSetVolume(volumeMax - mySpeakerVolume, volumeMax);
            REGSAVE(TEXT(REGVALUE_SPKR_VOLUME), SpkrGetVolume());
          }

          if (ImGui::SliderInt("Mockingboard volume", &myMockingboardVolume, 0, volumeMax))
          {
            MB_SetVolume(volumeMax - myMockingboardVolume, volumeMax);
            REGSAVE(TEXT(REGVALUE_MB_VOLUME), MB_GetVolume());
          }

          ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
      }

    }
    ImGui::End();
  }

  void ImGuiSettings::show(SDLFrame * frame)
  {
    if (myShowSettings)
    {
      showSettings();
    }

    if (myShowMemory)
    {
      showMemory();
    }

    if (myShowDebugger)
    {
      showDebugger(frame);
    }

    if (myShowDemo)
    {
      ImGui::ShowDemoWindow(&myShowDemo);
    }
  }

  float ImGuiSettings::drawMenuBar()
  {
    float menuBarHeight;

    if (ImGui::BeginMainMenuBar())
    {
      menuBarHeight = ImGui::GetWindowHeight();
      if (ImGui::BeginMenu("System"))
      {
        ImGui::MenuItem("Settings", nullptr, &myShowSettings);
        ImGui::MenuItem("Memory", nullptr, &myShowMemory);
        if (ImGui::MenuItem("Debugger", nullptr, &myShowDebugger) && myShowDebugger)
        {
          DebugBegin();
        }
        ImGui::Separator();
        ImGui::MenuItem("Demo", nullptr, &myShowDemo);
        ImGui::EndMenu();
      }
      ImGui::EndMainMenuBar();
    }
    else
    {
      menuBarHeight = 0.0;
    }

    return menuBarHeight;
  }

  void ImGuiSettings::showMemory()
  {
    if (ImGui::Begin("Memory Viewer", &myShowMemory))
    {
      if (ImGui::BeginTabBar("Memory"))
      {
        if (ImGui::BeginTabItem("Main"))
        {
          void * mainBase = MemGetMainPtr(0);
          myMainMemoryEditor.DrawContents(mainBase, _6502_MEM_LEN);
          ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("AUX"))
        {
          void * auxBase = MemGetAuxPtr(0);
          myMainMemoryEditor.DrawContents(auxBase, _6502_MEM_LEN);
          ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
      }
    }
    ImGui::End();
  }

  void ImGuiSettings::drawDisassemblyTable()
  {
    const ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersV | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_ScrollY;
    if (ImGui::BeginTable("Disassembly", 8, flags))
    {
      // weigths proportional to column width (including header)
      ImGui::TableSetupScrollFreeze(0, 1); // Make top row always visible
      ImGui::TableSetupColumn("Address", 0, 7);
      ImGui::TableSetupColumn("Opcode", 0, 8);
      ImGui::TableSetupColumn("Symbol", 0, 10);
      ImGui::TableSetupColumn("Disassembly", 0, 20);
      ImGui::TableSetupColumn("Target", 0, 6);
      ImGui::TableSetupColumn("Value", 0, 6);
      ImGui::TableSetupColumn("Immediate", 0, 9);
      ImGui::TableSetupColumn("Branch", 0, 6);
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
          DisasmLine_t line;
          const char* pSymbol = FindSymbolFromAddress(nAddress);
          const int bDisasmFormatFlags = GetDisassemblyLine(nAddress, line);

          char buffer[256];
          formatDisassemblyLine(line, bDisasmFormatFlags, buffer, sizeof(buffer));

          ImGui::TableNextRow();

          if (nAddress == regs.pc)
          {
            const ImU32 currentBgColor = ImGui::GetColorU32(ImVec4(0, 0, 1, 1));
            ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, currentBgColor);
          }
          ImGui::TableNextColumn();
          ImGui::TextUnformatted(line.sAddress);

          ImGui::TableNextColumn();
          ImGui::TextUnformatted(line.sOpCodes);

          ImGui::TableNextColumn();
          if (pSymbol)
          {
            ImGui::TextUnformatted(pSymbol);
          }

          ImGui::TableNextColumn();
          ImGui::Selectable(buffer, false, ImGuiSelectableFlags_SpanAllColumns);

          ImGui::TableNextColumn();
          if (bDisasmFormatFlags & DISASM_FORMAT_TARGET_POINTER)
          {
            ImGui::TextUnformatted(line.sTargetPointer);
          }

          ImGui::TableNextColumn();
          if (bDisasmFormatFlags & DISASM_FORMAT_TARGET_VALUE)
          {
            ImGui::TextUnformatted(line.sTargetValue);
          }

          ImGui::TableNextColumn();
          if (bDisasmFormatFlags & DISASM_FORMAT_CHAR)
          {
            ImGui::TextUnformatted(line.sImmediate);
          }

          ImGui::TableNextColumn();
          if (bDisasmFormatFlags & DISASM_FORMAT_BRANCH)
          {
            ImGui::TextUnformatted(line.sBranch);
          }

          nAddress += line.nOpbyte;
        }
      }
      ImGui::EndTable();
    }
  }

  void ImGuiSettings::drawConsole()
  {
    const ImGuiTableFlags flags = ImGuiTableFlags_BordersV | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_ScrollY;
    if (ImGui::BeginTable("Console", 1, flags))
    {
      {
        for (int i = 0; i < CONSOLE_HEIGHT; ++i)
        {
          char line[CONSOLE_WIDTH + 1];
          line[CONSOLE_WIDTH] = 0;
          const conchar_t * src = g_aConsoleDisplay[CONSOLE_HEIGHT - i - 1];
          for (size_t j = 0; j < CONSOLE_WIDTH; ++j)
          {
            line[j] = ConsoleChar_GetChar(src[j]);
            if (!line[j])
            {
              break;
            }
          }
          if (line[0])
          {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(line);
          }
        }
      }
      ImGui::EndTable();
    }
  }

  void ImGuiSettings::showDebugger(SDLFrame * frame)
  {
    if (ImGui::Begin("Debugger", &myShowDebugger))
    {
      if (ImGui::BeginTabBar("Settings"))
      {
        if (ImGui::BeginTabItem("CPU"))
        {
          ImGui::Checkbox("Auto-sync PC", &mySyncCPU);

          // complicated if condition to preserve widget order
          const bool recalc = mySyncCPU || (ImGui::SameLine(), ImGui::Button("Sync PC"));

          if (ImGui::Button("Step"))
          {
            frame->ChangeMode(MODE_STEPPING);
            frame->Execute(myStepCycles);
          }
          ImGui::SameLine();
          ImGui::PushItemWidth(150);
          ImGui::DragInt("cycles", &myStepCycles, 0.2f, 0, 256, "%d");
          ImGui::PopItemWidth();

          if ((ImGui::SameLine(), ImGui::Button("Run")))
          {
            frame->ChangeMode(MODE_RUNNING);
          }
          if ((ImGui::SameLine(), ImGui::Button("Pause")))
          {
            frame->ChangeMode(MODE_PAUSED);
          }
          ImGui::SameLine(); ImGui::Text("%016llu - %04X", g_nCumulativeCycles, regs.pc);

          if (ImGui::SliderInt("PC position", &g_nDisasmCurLine, 0, 100) || recalc)
          {
            g_nDisasmCurAddress = regs.pc;
            DisasmCalcTopBotAddress();
          }
          drawDisassemblyTable();
          ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Console"))
        {
          drawConsole();
          ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
      }
    }
    ImGui::End();
  }


}
