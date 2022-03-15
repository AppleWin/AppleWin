#include "StdAfx.h"
#include "frontends/sdl/imgui/sdlsettings.h"
#include "frontends/sdl/imgui/settingshelper.h"
#include "frontends/sdl/sdlframe.h"
#include "linux/registry.h"
#include "linux/version.h"
#include "linux/tape.h"

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
#include "ParallelPrinter.h"
#include "SaveState.h"

#include "Debugger/Debugger_Types.h"

#include "Tfe/tfesupp.h"
#include "Tfe/PCapBackend.h"

namespace
{

  struct MemoryTab
  {
    void * basePtr;
    size_t baseAddr;
    size_t length;
    std::string name;
  };

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

  char getPrintableChar(const uint8_t x)
  {
    if (x >= 0x20 && x <= 0x7e)
    {
      return x;
    }
    else
    {
      return '.';
    }
  }

  void printReg(const char label, const BYTE value)
  {
    ImGui::Text("%c  '%c'  %02X", label, getPrintableChar(value), value);
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

}

namespace sa2
{

  void ImGuiSettings::showSettings(SDLFrame* frame)
  {
    if (ImGui::Begin("Settings", &myShowSettings))
    {
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
            frame->ChangeMode(MODE_DEBUG);
          }
          ImGui::SameLine(); HelpMarker("Show Apple CPU.");

          ImGui::Checkbox("Show Demo", &myShowDemo);
          ImGui::SameLine(); HelpMarker("Show Dear ImGui DemoWindow.");

          ImGui::Separator();

          ImGui::BeginDisabled();
          ImGui::Checkbox("Full speed", &g_bFullSpeed);
          ImGui::EndDisabled();

          int speed = g_dwSpeed;
          if (ImGui::SliderInt("Speed", &speed, SPEED_MIN, SPEED_MAX))
          {
            g_dwSpeed = speed;
            SetCurrentCLK6502();
            REGSAVE(TEXT(REGVALUE_EMULATION_SPEED), g_dwSpeed);
            frame->ResetSpeed();
          }
          ImGui::LabelText("Clock", "%15.2f Hz", g_fCurrentCLK6502);

          ImGui::Separator();
          ImGui::LabelText("Save state", "%s", Snapshot_GetPathname().c_str());
          ImGui::Separator();

          if (frame->HardwareChanged())
          {
            ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor(180, 0, 0));
          }
          else
          {
            ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor(0, 180, 0));
          }
          if (ImGui::Button("Restart"))
          {
            frame->Restart();
          }
          ImGui::PopStyleColor(1);

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

          ImGui::LabelText("CPU", "%s", getCPUName(GetMainCpu()).c_str());
          ImGui::LabelText("Mode", "%s", getAppModeName(g_nAppMode).c_str());

          ImGui::Separator();

          ImGui::LabelText("Slot", "Card");
          CardManager & manager = GetCardMgr();
          for (size_t slot = SLOT1; slot < NUM_SLOTS; ++slot)
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
                  insertCard(slot, card, frame);
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
                  setExpansionCard(card);
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

          const std::string current_interface = PCapBackend::tfe_interface;

          if (ImGui::BeginCombo("pcap", current_interface.c_str()))
          {
            std::vector<std::string> ifaces;
            if (PCapBackend::tfe_enumadapter_open())
            {
              std::string name;
              std::string description;

              while (PCapBackend::tfe_enumadapter(name, description))
              {
                ifaces.push_back(name);
              }
              PCapBackend::tfe_enumadapter_close();

              for (const auto & iface : ifaces)
              {
                const bool isSelected = iface == current_interface;
                if (ImGui::Selectable(iface.c_str(), isSelected))
                {
                  // the following line interacts with tfe_enumadapter, so we must run it outside the above loop
                  PCapBackend::tfe_interface = iface;
                  PCapBackend::tfe_SetRegistryInterface(SLOT3, PCapBackend::tfe_interface);
                }
                if (isSelected)
                {
                  ImGui::SetItemDefaultFocus();
                }
              }
            }
            ImGui::EndCombo();
          }

          ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Disks"))
        {
          CardManager & cardManager = GetCardMgr();

          bool enhancedSpeed = cardManager.GetDisk2CardMgr().GetEnhanceDisk();
          if (ImGui::Checkbox("Enhanced speed", &enhancedSpeed))
          {
            cardManager.GetDisk2CardMgr().SetEnhanceDisk(enhancedSpeed);
            REGSAVE(TEXT(REGVALUE_ENHANCE_DISK_SPEED), (DWORD)enhancedSpeed);
          }

          ImGui::Separator();

          size_t dragAndDropSlot;
          size_t dragAndDropDrive;
          frame->getDragDropSlotAndDrive(dragAndDropSlot, dragAndDropDrive);

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
                  ImGui::Text("%zu", drive + 1);
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

            HarddiskInterfaceCard* pHarddiskCard = dynamic_cast<HarddiskInterfaceCard*>(cardManager.GetObj(SLOT7));
            if (pHarddiskCard)
            {
              ImGui::PushID(7);
              Disk_Status_e disk1Status_;
              pHarddiskCard->GetLightStatus(&disk1Status_);
              for (size_t drive = HARDDISK_1; drive < NUM_HARDDISKS; ++drive)
              {
                ImGui::PushID(drive);
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("%d", SLOT7);
                ImGui::TableNextColumn();
                ImGui::Text("%zu", drive + 1);
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
                  pHarddiskCard->Unplug(drive);
                }
                ImGui::TableNextColumn();
                if (ImGui::SmallButton("Swap"))
                {
                  pHarddiskCard->ImageSwap();
                }
                ImGui::TableNextColumn();
                if (ImGui::RadioButton("", (dragAndDropSlot == SLOT7) && (dragAndDropDrive == drive)))
                {
                  frame->setDragDropSlotAndDrive(SLOT7, drive);
                }

                ImGui::TableNextColumn();
                ImGui::TextUnformatted(pHarddiskCard->GetFullName(drive).c_str());
                ImGui::PopID();
              }
              ImGui::PopID();
            }
            ImGui::EndTable();
          }
          ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Printer"))
        {
          ImGui::LabelText("Printer file", "%s", Printer_GetFilename().c_str());
          ImGui::SameLine();
          if (ImGui::Button("Reset"))
          {
            PrintReset();
          }
          ImGui::Separator();
          ImGui::Checkbox("Append", &g_bPrinterAppend);
          ImGui::Checkbox("Filter unprintable", &g_bFilterUnprintable);
          ImGui::Checkbox("Convert encoding", &g_bConvertEncoding);
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

            ImGui::BeginDisabled();
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
            ImGui::EndDisabled();

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

        if (ImGui::BeginTabItem("Tape"))
        {
          CassetteTape & tape = CassetteTape::instance();

          CassetteTape::TapeInfo info;
          tape.getTapeInfo(info);

          if (info.size)
          {
            const float remaining = float(info.size - (info.pos + 1)) / float(info.frequency);
            const float fraction = float(info.pos + 1) / float(info.size);
            char buf[32];
            sprintf(buf, "-%.1f s", remaining);
            const ImU32 color = info.bit ? IM_COL32(200, 0, 0, 100) : IM_COL32(0, 200, 0, 100);

            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, color);
            ImGui::ProgressBar(fraction, ImVec2(-FLT_MIN, 0), buf);
            ImGui::PopStyleColor();

            ImGui::LabelText("Filename", "%s", info.filename.c_str());
            ImGui::LabelText("Frequency", "%d Hz", info.frequency);
            ImGui::LabelText("Auto Play", "%s", "ON");

            ImGui::Separator();

            if (ImGui::Button("Rewind"))
            {
              tape.rewind();
            }
            ImGui::SameLine();
            if (ImGui::Button("Eject"))
            {
              tape.eject();
            }
          }
          else
          {
            ImGui::TextUnformatted("Drop a .wav file.");
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

    ImGui::Separator();
    ImGuiIO& io = ImGui::GetIO();
    ImGui::Text("FPS: %d", int(io.Framerate));

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

  float ImGuiSettings::drawMenuBar(SDLFrame* frame)
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
          frame->ChangeMode(MODE_DEBUG);
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
        std::vector<MemoryTab> banks;

        banks.push_back({mem, 0, _6502_MEM_LEN, "Memory"});
        banks.push_back({MemGetCxRomPeripheral(), _6502_IO_BEGIN, 4 * 1024, "Cx ROM"});

        size_t i = 0;
        void * bank;
        while ((bank = MemGetBankPtr(i)))
        {
          const std::string name = "Bank " + std::to_string(i);
          banks.push_back({bank, 0, _6502_MEM_LEN, name});
          ++i;
        }

        myMemoryEditors.resize(banks.size());

        for (i = 0; i < banks.size(); ++i)
        {
          if (ImGui::BeginTabItem(banks[i].name.c_str()))
          {
            myMemoryEditors[i].DrawContents(banks[i].basePtr, banks[i].length, banks[i].baseAddr);
            ImGui::EndTabItem();
          }
        }

        ImGui::EndTabBar();
      }
    }
    ImGui::End();
  }

  void ImGuiSettings::drawDisassemblyTable(SDLFrame * frame)
  {
    const ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_ScrollY;
    if (ImGui::BeginTable("Disassembly", 10, flags))
    {
      ImGui::PushStyleCompact();
      // weigths proportional to column width (including header)
      ImGui::TableSetupScrollFreeze(0, 1); // Make top row always visible
      ImGui::TableSetupColumn("", 0, 1);
      ImGui::TableSetupColumn("Address", 0, 5);
      ImGui::TableSetupColumn("Opcode", 0, 8);
      ImGui::TableSetupColumn("Symbol", 0, 10);
      ImGui::TableSetupColumn("Disassembly", 0, 20);
      ImGui::TableSetupColumn("Target", 0, 4);
      ImGui::TableSetupColumn("Value", 0, 6);
      ImGui::TableSetupColumn("Immediate", 0, 4);
      ImGui::TableSetupColumn("Branch", 0, 4);
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
          if (ImGui::CheckBoxTristate("", &state))
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
          ImGui::Selectable(line.sAddress, false, ImGuiSelectableFlags_SpanAllColumns);

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

  void ImGuiSettings::drawRegisters()
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

  void ImGuiSettings::drawAnnunciators()
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

  void ImGuiSettings::drawSwitches()
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

  void ImGuiSettings::drawConsole()
  {
    const ImGuiTableFlags flags = ImGuiTableFlags_BordersV | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_ScrollY;
    if (ImGui::BeginTable("Console", 1, flags))
    {
      for (int i = g_nConsoleDisplayTotal; i >= CONSOLE_FIRST_LINE; --i)
      {
        const conchar_t * src = g_aConsoleDisplay[i];
        char line[CONSOLE_WIDTH + 1];
        size_t length = 0;

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        COLORREF currentColor = DebuggerGetColor( FG_CONSOLE_OUTPUT );

        const auto textAndReset = [&line, &length, & currentColor] () {
          line[length] = 0;
          length = 0;
          const ImVec4 color = colorrefToImVec4(currentColor);
          ImGui::TextColored(color, "%s", line);
        };

        for (size_t j = 0; j < CONSOLE_WIDTH; ++j)
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
          if (line[length])
          {
            ++length;
          }
          else
          {
            break;
          }
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

  void ImGuiSettings::resetDebuggerCycles()
  {
    myAddressCycles.clear();
    myBaseDebuggerCycles = g_nCumulativeCycles;
  }

  void ImGuiSettings::showDebugger(SDLFrame * frame)
  {
    myAddressCycles[regs.pc] = g_nCumulativeCycles - myBaseDebuggerCycles;

    if (ImGui::Begin("Debugger", &myShowDebugger))
    {
      ImGui::BeginChild("Console", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()));
      if (ImGui::BeginTabBar("Tabs"))
      {
        if (ImGui::BeginTabItem("CPU"))
        {
          ImGui::Checkbox("Auto-sync PC", &mySyncCPU);

          bool recalc = false;
          if (mySyncCPU || (ImGui::SameLine(), ImGui::Button("Sync PC")))
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

          if (!mySyncCPU)
          {
            ImGui::SameLine();
            ImGui::PushItemWidth(ImGui::GetFontSize() * 5);
            if (ImGui::InputScalar("Reference", ImGuiDataType_U16, &g_nDisasmCurAddress, nullptr, nullptr, "%04X", ImGuiInputTextFlags_CharsHexadecimal))
            {
              recalc = true;
            }
            ImGui::PopItemWidth();
          }

          // do not flip next ||
          if (ImGui::SliderInt("PC position", &g_nDisasmCurLine, 0, 100) || recalc)
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
          drawRegisters();
          ImGui::Dummy(ImVec2(0.0f, 20.0f));
          drawAnnunciators();
          ImGui::Dummy(ImVec2(0.0f, 20.0f));
          drawSwitches();
          ImGui::EndChild();

          ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Console"))
        {
          drawConsole();
          ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
      }
      ImGui::EndChild();
      if (g_nAppMode == MODE_DEBUG)
      {
        if (ImGui::InputText("Prompt", myInputBuffer, IM_ARRAYSIZE(myInputBuffer), ImGuiInputTextFlags_EnterReturnsTrue))
        {
          debuggerCommand(frame, myInputBuffer);
          myInputBuffer[0] = 0;
          ImGui::SetKeyboardFocusHere(-1);
        }
      }
    }
    ImGui::End();
  }

  void ImGuiSettings::debuggerCommand(SDLFrame * frame, const char * s)
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

}
