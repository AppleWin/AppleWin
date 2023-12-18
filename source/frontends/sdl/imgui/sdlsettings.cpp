#include "StdAfx.h"
#include "frontends/sdl/imgui/sdlsettings.h"
#include "frontends/sdl/imgui/settingshelper.h"
#include "frontends/sdl/processfile.h"
#include "frontends/sdl/sdirectsound.h"
#include "frontends/sdl/sdlframe.h"
#include "linux/registryclass.h"
#include "linux/version.h"
#include "linux/cassettetape.h"
#include "linux/network/slirp2.h"

#include "Interface.h"
#include "CardManager.h"
#include "Speaker.h"
#include "Registry.h"
#include "Utilities.h"
#include "Memory.h"
#include "ParallelPrinter.h"
#include "SaveState.h"
#include "Uthernet2.h"
#include "CopyProtectionDongles.h"

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

}

namespace sa2
{

  void ImGuiSettings::showSettings(SDLFrame* frame)
  {
    if (ImGui::Begin("Settings", &myShowSettings))
    {
      CardManager & cardManager = GetCardMgr();

      if (ImGui::BeginTabBar("Settings"))
      {
        if (ImGui::BeginTabItem("General"))
        {
          ImGui::Checkbox("Apple Video windowed", &windowed);
          ImGui::SameLine(); HelpMarker("Show Apple video in a separate window.");

          ImGui::Checkbox("Preserve aspect ratio", &frame->getPreserveAspectRatio());

          ImGui::Checkbox("Memory", &myShowMemory);
          ImGui::SameLine(); HelpMarker("Show Apple memory.");

          if (ImGui::Checkbox("Debugger", &myDebugger.showDebugger) && myDebugger.showDebugger)
          {
            frame->ChangeMode(MODE_DEBUG);
          }
          ImGui::SameLine(); HelpMarker("Show Apple CPU.");

          ImGui::Checkbox("Show Demo", &myShowDemo);
          ImGui::SameLine(); HelpMarker("Show Dear ImGui DemoWindow.");

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
            frame->FrameResetMachineState();
          }

          ImGui::SameLine();
          if (ImGui::Button("CtrlReset"))
          {
            CtrlReset();
          }

          ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Speed"))
        {
          ImGui::BeginDisabled();
          ImGui::Checkbox("Full speed", &g_bFullSpeed);
          ImGui::EndDisabled();

          ImGui::LabelText("Mode", "%s", getAppModeName(g_nAppMode).c_str());

          ImGui::Separator();

          int speedMultiplier = g_dwSpeed;
          if (ImGui::SliderInt("Speed", &speedMultiplier, SPEED_MIN, SPEED_MAX))
          {
            g_dwSpeed = speedMultiplier;
            SetCurrentCLK6502();
            REGSAVE(TEXT(REGVALUE_EMULATION_SPEED), g_dwSpeed);
            frame->ResetSpeed();
          }

          const common2::Speed::Stats stats = frame->getSpeed().getSpeedStats();
          ImGui::LabelText("Clock",          "%15.2f Hz", stats.nominal);
          ImGui::LabelText("Audio adjusted", "%12.0f    Hz", stats.audio);
          ImGui::LabelText("Actual",         "%12.0f    Hz", stats.actual);
          ImGui::LabelText("Feedback",       "%12.0f    Hz", stats.netFeedback);
          if (ImGui::Button("Reset stats"))
          {
            frame->ResetSpeed();
          }

          ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Hardware"))
        {
          ImGui::LabelText("Option", "Value");

          comboIterator("Apple 2", GetApple2Type(), getAapple2Types(), [] (eApple2Type x) {
            SetApple2Type(x);
            REGSAVE(REGVALUE_APPLE2_TYPE, x);
          });

          ImGui::LabelText("CPU", "%s", getCPUName(GetMainCpu()).c_str());
          ImGui::LabelText("Mode", "%s", getAppModeName(g_nAppMode).c_str());

          ImGui::Separator();

          ImGui::LabelText("Slot", "Card");
          for (size_t slot = SLOT1; slot < NUM_SLOTS; ++slot)
          {
            const SS_CARDTYPE current = cardManager.QuerySlot(slot);
            comboIterator(std::to_string(slot).c_str(), current, getCardsForSlot(slot), getCardNames(), [slot, &frame] (SS_CARDTYPE x) {
              insertCard(slot, x, frame);
            });
          }

          ImGui::Separator();

          {
            // Expansion
            const SS_CARDTYPE expansion = GetCurrentExpansionMemType();
            comboIterator("Expansion", expansion, getExpansionCards(), getCardNames(), [] (SS_CARDTYPE x) {
              setExpansionCard(x);
            });

            int ramWorksMemorySize = GetRamWorksMemorySize();
            if (ImGui::SliderInt("RamWorks size", &ramWorksMemorySize, 1, kMaxExMemoryBanks))
            {
              SetRamWorksMemorySize(ramWorksMemorySize);
            }
          }

          ImGui::Separator();

          comboIterator("Copy protection", GetCopyProtectionDongleType(), getDongleTypes(), [] (DONGLETYPE x) {
            SetCopyProtectionDongleType(x);
            RegSetConfigGameIOConnectorNewDongleType(GAME_IO_CONNECTOR, x);
          });

          const UINT uthernetSlot = SLOT3;

          const std::string current_interface = PCapBackend::GetRegistryInterface(uthernetSlot);

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
                  PCapBackend::SetRegistryInterface(uthernetSlot, iface);
                }
                if (isSelected)
                {
                  ImGui::SetItemDefaultFocus();
                }
              }
            }
            ImGui::EndCombo();
          }

          bool virtualDNS = Uthernet2::GetRegistryVirtualDNS(uthernetSlot);
          if (ImGui::Checkbox("Virtual DNS", &virtualDNS))
          {
            Uthernet2::SetRegistryVirtualDNS(uthernetSlot, virtualDNS);
          }

          ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Disks"))
        {
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

          if (ImGui::BeginTable("Disk2", 13, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit))
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
            ImGui::TableSetupColumn("Open", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("Filename", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();

            // only scan disk slots.
            for (int slot = SLOT5; slot < NUM_SLOTS; ++slot)
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
                  if (ImGui::RadioButton("##Sel", (dragAndDropSlot == slot) && (dragAndDropDrive == drive)))
                  {
                    frame->setDragDropSlotAndDrive(slot, drive);
                  }

                  ImGui::TableNextColumn();
                  if (ImGui::SmallButton("Open"))
                  {
                    const std::string & diskName = card2->DiskGetFullPathName(drive);
                    openFileDialog(diskName, slot, drive);
                  }

                  ImGui::TableNextColumn();
                  ImGui::TextUnformatted(card2->GetFullDiskFilename(drive).c_str());

                  ImGui::PopID();
                }
              }
              else if (cardManager.QuerySlot(slot) == CT_GenericHDD)
              {
                HarddiskInterfaceCard* pHarddiskCard = dynamic_cast<HarddiskInterfaceCard*>(cardManager.GetObj(slot));
                Disk_Status_e disk1Status_;
                pHarddiskCard->GetLightStatus(&disk1Status_);
                for (size_t drive = HARDDISK_1; drive < NUM_HARDDISKS; ++drive)
                {
                  ImGui::PushID(drive);
                  ImGui::TableNextRow();
                  ImGui::TableNextColumn();
                  ImGui::Text("%d", slot);
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
                  if (ImGui::RadioButton("##Sel", (dragAndDropSlot == slot) && (dragAndDropDrive == drive)))
                  {
                    frame->setDragDropSlotAndDrive(slot, drive);
                  }

                  ImGui::TableNextColumn();
                  if (ImGui::SmallButton("Open"))
                  {
                    const std::string & diskName = pHarddiskCard->HarddiskGetFullPathName(drive);
                    openFileDialog(diskName, slot, drive);
                  }

                  ImGui::TableNextColumn();
                  ImGui::TextUnformatted(pHarddiskCard->GetFullName(drive).c_str());

                  ImGui::PopID();
                }
              }
              ImGui::PopID();
            }
            ImGui::EndTable();

            myFileDialog.Display();
            if (myFileDialog.HasSelected())
            {
              sa2::processFile(frame, myFileDialog.GetSelected().string().c_str(), myOpenSlot, myOpenDrive);
              myFileDialog.ClearSelected();
            }

          }
          ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Printer"))
        {
          if (cardManager.IsParallelPrinterCardInstalled())
          {
            ParallelPrinterCard* card = cardManager.GetParallelPrinterCard();
            ImGui::LabelText("Printer file", "%s", card->GetFilename().c_str());
            ImGui::SameLine();
            if (ImGui::Button("Reset"))
            {
              card->Reset(true);
            }
            ImGui::Separator();
            bool printerAppend = card->GetPrinterAppend();
            if (ImGui::Checkbox("Append", &printerAppend))
            {
              card->SetPrinterAppend(printerAppend);
            }

            bool filterUnprintable = card->GetFilterUnprintable();
            if (ImGui::Checkbox("Filter unprintable", &filterUnprintable))
            {
              card->SetFilterUnprintable(filterUnprintable);
            }

            bool convertEncoding = card->GetConvertEncoding();
            if (ImGui::Checkbox("Convert encoding", &convertEncoding))
            {
              card->SetConvertEncoding(convertEncoding);
            }
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

          MockingboardCardManager& mockingboard = cardManager.GetMockingboardCardMgr();

          myMockingboardVolume = volumeMax - mockingboard.GetVolume();
          if (ImGui::SliderInt("Mockingboard volume", &myMockingboardVolume, 0, volumeMax))
          {
            mockingboard.SetVolume(volumeMax - myMockingboardVolume, volumeMax);
            REGSAVE(TEXT(REGVALUE_MB_VOLUME), mockingboard.GetVolume());
          }

          ImGui::Separator();

          if (ImGui::BeginTable("Devices", 5, ImGuiTableFlags_RowBg))
          {
            myAudioInfo = getAudioInfo();
            ImGui::TableSetupColumn("Running");
            ImGui::TableSetupColumn("Channels");
            ImGui::TableSetupColumn("Volume");
            ImGui::TableSetupColumn("Buffer (ms)");
            ImGui::TableSetupColumn("Underruns");
            ImGui::TableHeadersRow();

            ImGui::BeginDisabled();
            for (SoundInfo & device : myAudioInfo)
            {
              ImGui::TableNextColumn();
              ImGui::Checkbox("##Running", &device.running);
              ImGui::TableNextColumn();
              ImGui::Text("%d", device.channels);
              ImGui::TableNextColumn();
              int volume = device.volume * 100;
              ImGui::SliderInt("##Volume", &volume, 0, 100, "%3d");
              ImGui::TableNextColumn();
              float buffer = device.buffer * 1000;
              float size = device.size * 1000;
              ImGui::SliderFloat("##Buffer", &buffer, 0, size, "%4.0f");
              ImGui::TableNextColumn();
              ImGui::Text("%zu", device.numberOfUnderruns);
            }
            ImGui::EndDisabled();

            ImGui::EndTable();
          }

          if (ImGui::Button("Reset underruns"))
          {
            resetAudioUnderruns();
          }

          ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Video"))
        {
          Video & video = GetVideo();
          comboIterator("Video mode", video.GetVideoType(), getVideoTypes(), [&video, &frame] (VideoType_e x) {
            video.SetVideoType(x);
            frame->ApplyVideoModeChange();
          });

          ImVec4 color = colorrefToImVec4(video.GetMonochromeRGB());
          if (ImGui::ColorEdit3("Monochrome Color", (float*)&color, 0))
          {
            const COLORREF cr = imVec4ToColorref(color);
            video.SetMonochromeRGB(cr);
            frame->ApplyVideoModeChange();
          }

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

#ifdef U2_USE_SLIRP
        if (ImGui::BeginTabItem("Network"))
        {
          if (ImGui::BeginTabBar("Uthernet"))
          {
            for (size_t slot = SLOT1; slot < NUM_SLOTS; ++slot)
            {
              const SS_CARDTYPE card = cardManager.QuerySlot(slot);
              if (card == CT_Uthernet || card == CT_Uthernet2)
              {
                const std::string slotNumber = StrFormat("Slot %" SIZE_T_FMT, slot);
                if (ImGui::BeginTabItem(slotNumber.c_str()))
                {
                  ImGui::LabelText("Card", "%s", getCardName(card).c_str());
                  ImGui::Separator();
                  const NetworkCard & networkCard = dynamic_cast<NetworkCard &>(cardManager.GetRef(slot));
                  const std::shared_ptr<NetworkBackend> & backend = networkCard.GetNetworkBackend();
                  const std::shared_ptr<SlirpBackend> slirp = std::dynamic_pointer_cast<SlirpBackend>(backend);
                  if (slirp)
                  {
                    const std::string info = slirp->getNeighborInfo();
                    ImGui::TextUnformatted(info.c_str());
                  }
                  ImGui::EndTabItem();
                }
              }
            }

            ImGui::EndTabBar();
          }
          ImGui::EndTabItem();
        }
#endif

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

      int nMajor, nMinor, nFixMajor, nFixMinor;
      UnpackVersion(DEBUGGER_VERSION, nMajor, nMinor, nFixMajor, nFixMinor);
      ImGui::Text("Debugger %d.%d.%d.%d", nMajor, nMinor, nFixMajor, nFixMinor);

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

  void ImGuiSettings::show(SDLFrame * frame, ImFont * debuggerFont)
  {
    if (myShowSettings)
    {
      showSettings(frame);
    }

    if (myShowMemory)
    {
      showMemory();
    }

    if (myDebugger.showDebugger)
    {
      ImGui::PushFont(debuggerFont);
      myDebugger.drawDebugger(frame);
      ImGui::PopFont();
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
        if (ImGui::MenuItem("Debugger", nullptr, &myDebugger.showDebugger) && myDebugger.showDebugger)
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

  void ImGuiSettings::resetDebuggerCycles()
  {
    myDebugger.resetDebuggerCycles();
  }

  void ImGuiSettings::openFileDialog(const std::string & diskName, const size_t slot, const size_t drive)
  {
    if (!diskName.empty())
    {
      const std::filesystem::path path(diskName);
      myFileDialog.SetPwd(path.parent_path());
    }
    myFileDialog.Open();
    myOpenSlot = slot;
    myOpenDrive = drive;
  }

}
