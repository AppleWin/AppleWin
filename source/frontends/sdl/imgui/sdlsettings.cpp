#include "StdAfx.h"
#include "frontends/sdl/imgui/sdlsettings.h"
#include "frontends/sdl/imgui/settingshelper.h"
#include "frontends/sdl/sdlframe.h"
#include "linux/registry.h"
#include "linux/version.h"

#include "Interface.h"
#include "CardManager.h"
#include "Core.h"
#include "CPU.h"
#include "CardManager.h"
#include "Disk.h"
#include "Harddisk.h"
#include "Speaker.h"
#include "Mockingboard.h"
#include "Registry.h"
#include "Utilities.h"
#include "Memory.h"

#include "Debugger/Debug.h"
#include "Debugger/DebugDefs.h"

#include "Tfe/tfe.h"
#include "Tfe/tfesupp.h"
#include "linux/network/uthernet2.h"

#include "imgui_internal.h"

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

  ImVec4 colorrefToImVec4(const COLORREF cr)
  {
    const float coeff = 1.0 / 255.0;
    const bgra_t * bgra = reinterpret_cast<const bgra_t *>(&cr);
    const ImVec4 color(bgra->b * coeff, bgra->g * coeff, bgra->r * coeff, 1);
    return color;
  }

  uint8_t roundToRGB(float x)
  {
    // c++ cast truncates
    return uint8_t(x * 255 + 0.5);
  }

  COLORREF imVec4ToColorref(const ImVec4 & color)
  {
    const bgra_t bgra = {roundToRGB(color.x), roundToRGB(color.y), roundToRGB(color.z), roundToRGB(color.w)};
    const COLORREF * cr = reinterpret_cast<const COLORREF *>(&bgra);
    return *cr;
  }

  ImVec4 debuggerGetColor(int iColor)
  {
    const COLORREF cr = DebuggerGetColor(iColor);
    const ImVec4 color = colorrefToImVec4(cr);
    return color;
  }

  void debuggerTextColored(int iColor, const char* text)
  {
    const ImVec4 color = debuggerGetColor(iColor);
    ImGui::TextColored(color, "%s", text);
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

  void standardLabelText(const char * label, const char * text)
  {
    // this is "better/different" that ImGui::LabelText();
    // because it shows the text in the same style as other options
    // it will not display the ARROW_DOWN, to indicate the option cannot be changed
    ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
    if (ImGui::BeginCombo(label, text, ImGuiComboFlags_NoArrowButton))
    {
      ImGui::EndCombo();
    }
    ImGui::PushItemFlag(ImGuiItemFlags_Disabled, false);
  }

}

namespace sa2
{

  void ImGuiSettings::showSettings(SDLFrame* frame)
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

          ImGui::Separator();

          ImGui::Text("FPS: %d", int(io.Framerate));

          ImGui::Separator();

          if (ImGui::Button("Restart"))
          {
            frame->Restart();
          }

          ImGui::SameLine();
          if (ImGui::Button("ResetMachineState"))
          {
            ResetMachineState();
          }

          ImGui::SameLine();
          if (ImGui::Button("CtrlReset"))
          {
            CtrlReset();
          }

          ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Hardware"))
        {
          ImGui::LabelText("Option", "Value");
          const eApple2Type a2e = GetApple2Type();
          const std::map<eApple2Type, std::string> & apple2Types = getAapple2Types();

          if (ImGui::BeginCombo("Apple 2", getApple2Name(a2e).c_str()))
          {
            for (const auto & it : apple2Types)
            {
              const bool isSelected = it.first == a2e;
              if (ImGui::Selectable(getApple2Name(it.first).c_str(), isSelected))
              {
                SetApple2Type(it.first);
                REGSAVE(REGVALUE_APPLE2_TYPE, it.first);
              }
              if (isSelected)
              {
                ImGui::SetItemDefaultFocus();
              }
            }
            ImGui::EndCombo();
          }

          standardLabelText("CPU", getCPUName(GetMainCpu()).c_str());
          standardLabelText("Mode", getAppModeName(g_nAppMode).c_str());

          ImGui::Separator();

          ImGui::LabelText("Slot", "Card");
          CardManager & manager = GetCardMgr();
          for (size_t slot = 1; slot < 8; ++slot)
          {
            const SS_CARDTYPE current = manager.QuerySlot(slot);
            if (ImGui::BeginCombo(std::to_string(slot).c_str(), getCardName(current).c_str()))
            {
              const std::vector<SS_CARDTYPE> & cards = getCardsForSlot(slot);
              for (SS_CARDTYPE card : cards)
              {
                const bool isSelected = card == current;
                if (ImGui::Selectable(getCardName(card).c_str(), isSelected))
                {
                  insertCard(slot, card);
                }
                if (isSelected)
                {
                  ImGui::SetItemDefaultFocus();
                }
              }
              ImGui::EndCombo();
            }
          }

          ImGui::Separator();

          {
            // Expansion
            const SS_CARDTYPE expansion = GetCurrentExpansionMemType();
            if (ImGui::BeginCombo("Expansion", getCardName(expansion).c_str()))
            {
              const std::vector<SS_CARDTYPE> & cards = getExpansionCards();
              for (SS_CARDTYPE card : cards)
              {
                const bool isSelected = card == expansion;
                if (ImGui::Selectable(getCardName(card).c_str(), isSelected))
                {
                  SetExpansionMemType(card);
                }
                if (isSelected)
                {
                  ImGui::SetItemDefaultFocus();
                }
              }
              ImGui::EndCombo();
            }
          }

          ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Disks"))
        {
          size_t dragAndDropSlot;
          size_t dragAndDropDrive;
          frame->getDragDropSlotAndDrive(dragAndDropSlot, dragAndDropDrive);

          CardManager & cardManager = GetCardMgr();
          if (ImGui::BeginTable("Disk2", 12, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit))
          {
            ImGui::TableSetupColumn("Slot", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("Drive", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("Firmware", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("Track", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("Track", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("Phase", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("Eject", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("Swap", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("D&D", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("Filename", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();

            for (int slot = SLOT5; slot < SLOT7; ++slot)
            {
              ImGui::PushID(slot);
              if (cardManager.QuerySlot(slot) == CT_Disk2)
              {
                Disk2InterfaceCard * card2 = dynamic_cast<Disk2InterfaceCard*>(cardManager.GetObj(slot));

                const int currentDrive = card2->GetCurrentDrive();
                Disk_Status_e statuses[NUM_DRIVES] = {};
                card2->GetLightStatus(statuses + 0, statuses + 1);
                const UINT firmware = card2->GetCurrentFirmware();

                for (size_t drive = DRIVE_1; drive < NUM_DRIVES; ++drive)
                {
                  ImGui::PushID(drive);
                  ImGui::TableNextRow();
                  ImGui::TableNextColumn();
                  ImGui::Text("%d", slot);
                  ImGui::TableNextColumn();
                  ImGui::Text("%d", drive + 1);
                  ImGui::TableNextColumn();
                  ImGui::Text("%d", firmware);
                  ImGui::TableNextColumn();
                  ImGui::Text("%d", card2->GetTrack(drive));
                  ImGui::TableNextColumn();
                  if (drive == currentDrive)
                  {
                    ImGui::TextUnformatted(card2->GetCurrentTrackString().c_str());
                  }
                  ImGui::TableNextColumn();
                  if (drive == currentDrive)
                  {
                    ImGui::TextUnformatted(card2->GetCurrentPhaseString().c_str());
                  }
                  ImGui::TableNextColumn();
                  ImGui::TextUnformatted(getDiskStatusName(statuses[drive]).c_str());

                  ImGui::TableNextColumn();
                  if (ImGui::SmallButton("Eject"))
                  {
                    card2->EjectDisk(drive);
                  }

                  ImGui::TableNextColumn();
                  if (ImGui::SmallButton("Swap"))
                  {
                    card2->DriveSwap();
                  }

                  ImGui::TableNextColumn();
                  if (ImGui::RadioButton("", (dragAndDropSlot == slot) && (dragAndDropDrive == drive)))
                  {
                    frame->setDragDropSlotAndDrive(slot, drive);
                  }

                  ImGui::TableNextColumn();
                  ImGui::TextUnformatted(card2->GetFullDiskFilename(drive).c_str());
                  ImGui::PopID();
                }
              }
              ImGui::PopID();
            }

            if (HD_CardIsEnabled())
            {
              ImGui::PushID(7);
              Disk_Status_e disk1Status_;
              HD_GetLightStatus(&disk1Status_);
              for (size_t drive = HARDDISK_1; drive < NUM_HARDDISKS; ++drive)
              {
                ImGui::PushID(drive);
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("%d", SLOT7);
                ImGui::TableNextColumn();
                ImGui::Text("%d", drive + 1);
                ImGui::TableNextColumn();
                ImGui::TextUnformatted("HD");
                ImGui::TableNextColumn();
                ImGui::TableNextColumn();
                ImGui::TableNextColumn();
                ImGui::TableNextColumn();

                ImGui::TextUnformatted(getDiskStatusName(disk1Status_).c_str());
                ImGui::TableNextColumn();
                if (ImGui::SmallButton("Eject"))
                {
                  HD_Unplug(drive);
                }
                ImGui::TableNextColumn();
                if (ImGui::SmallButton("Swap"))
                {
                  HD_ImageSwap();
                }
                ImGui::TableNextColumn();
                if (ImGui::RadioButton("", (dragAndDropSlot == SLOT7) && (dragAndDropDrive == drive)))
                {
                  frame->setDragDropSlotAndDrive(SLOT7, drive);
                }

                ImGui::TableNextColumn();
                ImGui::TextUnformatted(HD_GetFullName(drive).c_str());
                ImGui::PopID();
              }
              ImGui::PopID();
            }
            ImGui::EndTable();
          }
          ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Audio"))
        {
          const int volumeMax = GetPropertySheet().GetVolumeMax();
          mySpeakerVolume = volumeMax - SpkrGetVolume();
          if (ImGui::SliderInt("Speaker volume", &mySpeakerVolume, 0, volumeMax))
          {
            SpkrSetVolume(volumeMax - mySpeakerVolume, volumeMax);
            REGSAVE(TEXT(REGVALUE_SPKR_VOLUME), SpkrGetVolume());
          }

          myMockingboardVolume = volumeMax - MB_GetVolume();
          if (ImGui::SliderInt("Mockingboard volume", &myMockingboardVolume, 0, volumeMax))
          {
            MB_SetVolume(volumeMax - myMockingboardVolume, volumeMax);
            REGSAVE(TEXT(REGVALUE_MB_VOLUME), MB_GetVolume());
          }

          ImGui::Separator();

          if (ImGui::BeginTable("Devices", 5, ImGuiTableFlags_RowBg))
          {
            myAudioInfo = getAudioInfo();
            ImGui::TableSetupColumn("Running");
            ImGui::TableSetupColumn("Channels");
            ImGui::TableSetupColumn("Volume");
            ImGui::TableSetupColumn("Buffer");
            ImGui::TableSetupColumn("Queue");
            ImGui::TableHeadersRow();

            ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true); // this requires imgui_internal.h
            for (SoundInfo & device : myAudioInfo)
            {
              ImGui::TableNextColumn();
              ImGui::Checkbox("", &device.running);
              ImGui::TableNextColumn();
              ImGui::Text("%d", device.channels);
              ImGui::TableNextColumn();
              ImGui::SliderFloat("", &device.volume, 0.0f, 1.0f, "%.2f");
              ImGui::TableNextColumn();
              ImGui::SliderFloat("", &device.buffer, 0.0f, device.size, "%.3f");
              ImGui::TableNextColumn();
              ImGui::SliderFloat("", &device.queue, 0.0f, device.size, "%.3f");
            }
            ImGui::PushItemFlag(ImGuiItemFlags_Disabled, false);

            ImGui::EndTable();
          }

          ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Video"))
        {
          Video & video = GetVideo();
          const VideoType_e videoType = video.GetVideoType();
          if (ImGui::BeginCombo("Video mode", getVideoTypeName(videoType).c_str()))
          {
            for (size_t value = VT_MONO_CUSTOM; value < NUM_VIDEO_MODES; ++value)
            {
              const bool isSelected = value == videoType;
              if (ImGui::Selectable(getVideoTypeName(VideoType_e(value)).c_str(), isSelected))
              {
                video.SetVideoType(VideoType_e(value));
                frame->ApplyVideoModeChange();
              }
              if (isSelected)
              {
                ImGui::SetItemDefaultFocus();
              }
            }
            ImGui::EndCombo();
          }

          ImVec4 color = colorrefToImVec4(video.GetMonochromeRGB());
          ImGui::ColorEdit3("Monochrome Color", (float*)&color, 0);
          const COLORREF cr = imVec4ToColorref(color);
          video.SetMonochromeRGB(cr);
          frame->ApplyVideoModeChange();

          bool scanLines = video.IsVideoStyle(VS_HALF_SCANLINES);
          if (ImGui::Checkbox("50% Scan lines", &scanLines))
          {
            setVideoStyle(video, VS_HALF_SCANLINES, scanLines);
            frame->ApplyVideoModeChange();
          }

          bool verticalBlend = video.IsVideoStyle(VS_COLOR_VERTICAL_BLEND);
          if (ImGui::Checkbox("Vertical blend", &verticalBlend))
          {
            setVideoStyle(video, VS_COLOR_VERTICAL_BLEND, verticalBlend);
            frame->ApplyVideoModeChange();
          }

          bool hertz50 = video.GetVideoRefreshRate() == VR_50HZ;
          if (ImGui::Checkbox("50Hz video", &hertz50))
          {
            video.SetVideoRefreshRate(hertz50 ? VR_50HZ : VR_60HZ);
            frame->ApplyVideoModeChange();
          }

          ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Uthernet"))
        {
          if (ImGui::RadioButton("Disabled", tfe_enabled == 0)) { saveTFEEnabled(0); } ImGui::SameLine();
          if (ImGui::RadioButton("Uthernet I", tfe_enabled == 1)) { saveTFEEnabled(1); } ImGui::SameLine();
          if (ImGui::RadioButton("Uthernet II", tfe_enabled == 2)) { saveTFEEnabled(2); }

          if (ImGui::BeginCombo("Interface", static_cast<const char *>(get_tfe_interface())))
          {
            std::vector<char *> ifaces;
            if (tfe_enumadapter_open())
            {
              char *pname;
              char *pdescription;

              while (tfe_enumadapter(&pname, &pdescription))
              {
                ifaces.push_back(pname);
                lib_free(pdescription);
              }
              tfe_enumadapter_close();

              for (const auto & iface : ifaces)
              {
                // must call it each time
                // as update_tfe_interface() will invalidate it
                const char * current = static_cast<const char *>(get_tfe_interface());
                const bool isSelected = strcmp(iface, current) == 0;
                if (ImGui::Selectable(iface, isSelected))
                {
                  // the following line interacts with tfe_enumadapter, so we must run it outside the above loop
                  update_tfe_interface(iface, nullptr);
                  RegSaveString(TEXT(REG_CONFIG), TEXT(REGVALUE_UTHERNET_INTERFACE), 1, iface);
                }
                if (isSelected)
                {
                  ImGui::SetItemDefaultFocus();
                }
                lib_free(iface);
              }
            }
            ImGui::EndCombo();
          }
          ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Debugger"))
        {
          if (ImGui::RadioButton("Color", g_iColorScheme == SCHEME_COLOR)) { g_iColorScheme = SCHEME_COLOR; } ImGui::SameLine();
          if (ImGui::RadioButton("Mono", g_iColorScheme == SCHEME_MONO)) { g_iColorScheme = SCHEME_MONO; } ImGui::SameLine();
          if (ImGui::RadioButton("BW", g_iColorScheme == SCHEME_BW)) { g_iColorScheme = SCHEME_BW; }

          ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Registry"))
        {
          if (ImGui::BeginTable("Registry", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit))
          {
            ImGui::TableSetupColumn("Option", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            if (ImGui::TreeNode("Registry"))
            {
              const std::map<std::string, std::map<std::string, std::string>> values = Registry::instance->getAllValues();

              for (const auto & it1 : values)
              {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                if (ImGui::TreeNodeEx(it1.first.c_str(), ImGuiTreeNodeFlags_SpanFullWidth))
                {
                  for (const auto & it2 : it1.second)
                  {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TreeNodeEx(it2.first.c_str(), ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanFullWidth);
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(it2.second.c_str());
                  }
                  ImGui::TreePop();
                }
              }
              ImGui::TreePop();
            }
            ImGui::EndTable();
          }
          ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
      }

    }
    ImGui::End();
  }

  void ImGuiSettings::showAboutWindow()
  {
    if (ImGui::Begin("About", &myShowAbout, ImGuiWindowFlags_AlwaysAutoResize))
    {
      ImGui::TextUnformatted("sa2: Apple ][ emulator for Linux");
      ImGui::Text("Based on AppleWin %s", getVersion().c_str());

      ImGui::Separator();
      SDL_version sdl;
      SDL_GetVersion(&sdl);
      ImGui::Text("SDL version %d.%d.%d", sdl.major, sdl.minor, sdl.patch);
      ImGui::Text("Dear ImGui %s", ImGui::GetVersion());
    }
    ImGui::End();
  }

  void ImGuiSettings::show(SDLFrame * frame)
  {
    if (myShowSettings)
    {
      showSettings(frame);
    }

    if (myShowMemory)
    {
      showMemory();
    }

    if (myShowDebugger)
    {
      showDebugger(frame);
    }

    if (myShowAbout)
    {
      showAboutWindow();
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
        ImGui::EndMenu();
      }

      if (ImGui::BeginMenu("Help"))
      {
        ImGui::MenuItem("Demo", nullptr, &myShowDemo);
        ImGui::Separator();
        ImGui::MenuItem("About", nullptr, &myShowAbout);
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
          myAuxMemoryEditor.DrawContents(auxBase, _6502_MEM_LEN);
          ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
      }
    }
    ImGui::End();
  }

  void ImGuiSettings::drawDisassemblyTable()
  {
    const ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_ScrollY;
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

          ImGui::TableNextRow();

          if (nAddress == regs.pc)
          {
            const ImU32 currentBgColor = ImGui::GetColorU32(ImVec4(0, 0, 1, 1));
            ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, currentBgColor);
          }
          else if (nAddress == g_nDisasmCurAddress)
          {
            const ImU32 currentBgColor = ImGui::GetColorU32(ImVec4(0, 0, 0.5, 1));
            ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, currentBgColor);
          }

          ImGui::TableNextColumn();
          ImGui::Selectable(line.sAddress, false, ImGuiSelectableFlags_SpanAllColumns);

          ImGui::TableNextColumn();
          debuggerTextColored(FG_DISASM_OPCODE, line.sOpCodes);

          ImGui::TableNextColumn();
          if (pSymbol)
          {
            debuggerTextColored(FG_DISASM_SYMBOL, pSymbol);
          }

          ImGui::TableNextColumn();
          displayDisassemblyLine(line, bDisasmFormatFlags);

          ImGui::TableNextColumn();
          if (bDisasmFormatFlags & DISASM_FORMAT_TARGET_POINTER)
          {
            debuggerTextColored(FG_DISASM_ADDRESS, line.sTargetPointer);
          }

          ImGui::TableNextColumn();
          if (bDisasmFormatFlags & DISASM_FORMAT_TARGET_VALUE)
          {
            debuggerTextColored(FG_DISASM_OPCODE, line.sTargetValue);
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
      for (int i = 0; i < CONSOLE_HEIGHT; ++i)
      {
        const conchar_t * src = g_aConsoleDisplay[CONSOLE_HEIGHT - i - 1];
        char line[CONSOLE_WIDTH + 1];
        COLORREF currentColor = ConsoleColor_GetColor(src[0]);
        line[0] = ConsoleChar_GetChar(src[0]);
        size_t length = 1;
        if (!line[0])
        {
          continue;
        }

        ImGui::TableNextRow();
        ImGui::TableNextColumn();

        const auto textAndReset = [&line, &length, & currentColor] () {
          line[length] = 0;
          length = 0;
          const ImVec4 color = colorrefToImVec4(currentColor);
          ImGui::TextColored(color, "%s", line);
        };

        for (size_t j = length; j < CONSOLE_WIDTH; ++j)
        {
          const conchar_t g = src[j];
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
