#include "StdAfx.h"
#include "CardManager.h"
#include "Registry.h"
#include "Core.h"
#include "Memory.h"
#include "Interface.h"
#include "Debugger/Debug.h"

#include "frontends/sdl/imgui/settingshelper.h"

#include "frontends/sdl/imgui/glselector.h"
#include "imgui_internal.h"

void CreateLanguageCard(void);

namespace
{
  const std::map<SS_CARDTYPE, std::string> cards =
    {
     {CT_Empty, "Empty"},
     {CT_Disk2, "Disk2"},
     {CT_SSC, "SSC"},
     {CT_MockingboardC, "MockingboardC"},
     {CT_GenericPrinter, "GenericPrinter"},
     {CT_GenericHDD, "GenericHDD"},
     {CT_GenericClock, "GenericClock"},
     {CT_MouseInterface, "MouseInterface"},
     {CT_Z80, "Z80"},
     {CT_Phasor, "Phasor"},
     {CT_Echo, "Echo"},
     {CT_SAM, "SAM"},
     {CT_80Col, "80Col"},
     {CT_Extended80Col, "Extended80Col"},
     {CT_RamWorksIII, "RamWorksIII"},
     {CT_Uthernet, "Uthernet"},
     {CT_LanguageCard, "LanguageCard"},
     {CT_LanguageCardIIe, "LanguageCardIIe"},
     {CT_Saturn128K, "Saturn128K"},
     {CT_FourPlay, "FourPlay"},
     {CT_SNESMAX, "SNESMAX"},
     {CT_VidHD, "VidHD"},
     {CT_Uthernet2, "Uthernet2"},
    };

  const std::map<eApple2Type, std::string> apple2Types =
    {
     {A2TYPE_APPLE2, "APPLE2"},
     {A2TYPE_APPLE2PLUS, "APPLE2PLUS"},
     {A2TYPE_APPLE2JPLUS, "APPLE2JPLUS"},
     {A2TYPE_APPLE2E, "APPLE2E"},
     {A2TYPE_APPLE2EENHANCED, "APPLE2EENHANCED"},
     {A2TYPE_APPLE2C, "APPLE2C"},
     {A2TYPE_PRAVETS8M, "PRAVETS8M"},
     {A2TYPE_PRAVETS82, "PRAVETS82"},
     {A2TYPE_BASE64A, "BASE64A"},
     {A2TYPE_PRAVETS8A, "PRAVETS8A"},
     {A2TYPE_TK30002E, "TK30002E"},
    };

  const std::map<eCpuType, std::string> cpuTypes =
    {
     {CPU_6502, "6502"},
     {CPU_65C02, "65C02"},
     {CPU_Z80, "Z80"},
    };

  const std::map<AppMode_e, std::string> appModes =
    {
     {MODE_LOGO, "LOGO"},
     {MODE_PAUSED, "PAUSED"},
     {MODE_RUNNING, "RUNNING"},
     {MODE_DEBUG, "DEBUG"},
     {MODE_STEPPING, "STEPPING"},
     {MODE_BENCHMARK, "BENCHMARCK"},
    };

  const std::map<Disk_Status_e, std::string> statuses =
    {
      {DISK_STATUS_OFF, "OFF"},
      {DISK_STATUS_READ, "READ"},
      {DISK_STATUS_WRITE, "WRITE"},
      {DISK_STATUS_PROT, "PROT"},
    };

  const std::map<VideoType_e, std::string> videoTypes =
  {
    {VT_MONO_CUSTOM, "Monochrome (Custom)"},
    {VT_COLOR_IDEALIZED, "Color (Composite Idealized)"},
    {VT_COLOR_VIDEOCARD_RGB, "Color (RGB Card/Monitor)"},
    {VT_COLOR_MONITOR_NTSC, "Color (Composite Monitor)"},
    {VT_COLOR_TV, "Color TV"},
    {VT_MONO_TV, "B&W TV"},
    {VT_MONO_AMBER, "Monochrome (Amber)"},
    {VT_MONO_GREEN, "Monochrome (Green)"},
    {VT_MONO_WHITE, "Monochrome (White)"},
  };

  const std::map<size_t, std::vector<SS_CARDTYPE>> cardsForSlots =
    {
      {0, {CT_Empty, CT_LanguageCard, CT_Saturn128K}},
      {1, {CT_Empty, CT_GenericPrinter, CT_Uthernet2}},
      {2, {CT_Empty, CT_SSC, CT_Uthernet2}},
      {3, {CT_Empty, CT_Uthernet, CT_Uthernet2, CT_VidHD}},
      {4, {CT_Empty, CT_MockingboardC, CT_MouseInterface, CT_Phasor, CT_Uthernet2}},
      {5, {CT_Empty, CT_MockingboardC, CT_Z80, CT_SAM, CT_Disk2, CT_FourPlay, CT_SNESMAX, CT_Uthernet2}},
      {6, {CT_Empty, CT_Disk2, CT_Uthernet2}},
      {7, {CT_Empty, CT_GenericHDD, CT_Uthernet2}},
    };

    const std::vector<SS_CARDTYPE> expansionCards =
      {CT_Empty, CT_LanguageCard, CT_Extended80Col, CT_Saturn128K, CT_RamWorksIII};

}

namespace sa2
{

  const std::string & getCardName(SS_CARDTYPE card)
  {
    return cards.at(card);
  }

  const std::string & getApple2Name(eApple2Type type)
  {
    return apple2Types.at(type);
  }

  const std::string & getCPUName(eCpuType cpu)
  {
    return cpuTypes.at(cpu);
  }

  const std::string & getAppModeName(AppMode_e mode)
  {
    return appModes.at(mode);
  }

  const std::string & getVideoTypeName(VideoType_e type)
  {
    return videoTypes.at(type);
  }

  const std::vector<SS_CARDTYPE> & getCardsForSlot(size_t slot)
  {
    return cardsForSlots.at(slot);
  }

  const std::vector<SS_CARDTYPE> & getExpansionCards()
  {
    return expansionCards;
  }

  const std::map<eApple2Type, std::string> & getAapple2Types()
  {
    return apple2Types;
  }

  const std::string & getDiskStatusName(Disk_Status_e status)
  {
    return statuses.at(status);
  }

  void insertCard(size_t slot, SS_CARDTYPE card, FrameBase * frame)
  {
    CardManager & cardManager = GetCardMgr();
    Video & video = GetVideo();
    const bool oldHasVid = video.HasVidHD();
    switch (slot)
    {
      case 3:
      {
        if (cardManager.QuerySlot(slot) == CT_VidHD)
        {
          // the old card was a VidHD, which will be removed
          // reset it
          video.SetVidHD(false);
        }
        break;
      }
      case 4:
      case 5:
      {
        if (card == CT_MockingboardC)
        {
          cardManager.Insert(9 - slot, card);  // the other
        }
        else
        {
          if (cardManager.QuerySlot(slot) == CT_MockingboardC)
          {
            cardManager.Insert(9 - slot, CT_Empty);  // the other
          }
        }
        break;
      }
    };

    cardManager.Insert(slot, card);

    // keep everything consistent
    // a bit of a heavy call, but nothing simpler is available now
    MemInitializeIO();

    if (oldHasVid != video.HasVidHD())
    {
      frame->Destroy();
      frame->Initialize(true);
    }
  }

  void setExpansionCard(SS_CARDTYPE card)
  {
    SetExpansionMemType(card);
    CreateLanguageCard();
    MemInitializeIO();
  }

  void setVideoStyle(Video & video, const VideoStyle_e style, const bool enabled)
  {
    VideoStyle_e currentVideoStyle = video.GetVideoStyle();
    if (enabled)
    {
      currentVideoStyle = VideoStyle_e(currentVideoStyle | style);
    }
    else
    {
      currentVideoStyle = VideoStyle_e(currentVideoStyle & (~style));
    }
    video.SetVideoStyle(currentVideoStyle);
  }

  void saveTFEEnabled(const int enabled)
  {
    REGSAVE(TEXT(REGVALUE_UTHERNET_ACTIVE), enabled);
  }

  void changeBreakpoint(const DWORD nAddress, const bool enableAndSet)
  {
    // see _BWZ_RemoveOne
    for (Breakpoint_t & bp : g_aBreakpoints)
    {
      if (bp.bSet && bp.nLength && (nAddress >= bp.nAddress) && (nAddress < bp.nAddress + bp.nLength))
      {
        bp.bSet = enableAndSet;
        bp.bEnabled = enableAndSet;
        if (!enableAndSet)
        {
          bp.nLength = 0;
          --g_nBreakpoints;
        }
      }
    }
  }


}

namespace ImGui
{

  bool CheckBoxTristate(const char* label, int* v_tristate)
  {
    bool ret;
    if (*v_tristate == -1)
    {
      ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, true);
      bool b = false;
      ret = ImGui::Checkbox(label, &b);
      if (ret)
        *v_tristate = 1;
      ImGui::PopItemFlag();
    }
    else
    {
      bool b = (*v_tristate != 0);
      ret = ImGui::Checkbox(label, &b);
      if (ret)
        *v_tristate = (int)b;
    }
    return ret;
  }

  void PushStyleCompact()
  {
    ImGuiStyle& style = ImGui::GetStyle();
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(style.FramePadding.x, (float)(int)(style.FramePadding.y * 0.60f)));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(style.ItemSpacing.x, (float)(int)(style.ItemSpacing.y * 0.60f)));
  }

  void PopStyleCompact()
  {
    ImGui::PopStyleVar(2);
  }

}
