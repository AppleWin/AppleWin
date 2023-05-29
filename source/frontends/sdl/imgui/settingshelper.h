#pragma once

#include "frontends/sdl/imgui/glselector.h"

#include "Card.h"
#include "CPU.h"
#include "Common.h"
#include "DiskImage.h"
#include "Video.h"

#include <string>
#include <vector>
#include <map>

class FrameBase;

namespace sa2
{

  const std::string & getCardName(SS_CARDTYPE card);
  const std::string & getApple2Name(eApple2Type type);
  const std::string & getCPUName(eCpuType cpu);
  const std::string & getAppModeName(AppMode_e mode);
  const std::string & getDiskStatusName(Disk_Status_e status);
  const std::string & getVideoTypeName(VideoType_e type);

  const std::vector<SS_CARDTYPE> & getCardsForSlot(size_t slot);
  const std::vector<SS_CARDTYPE> & getExpansionCards();
  const std::map<eApple2Type, std::string> & getAapple2Types();

  void insertCard(size_t slot, SS_CARDTYPE card, FrameBase * frame);
  void setExpansionCard(SS_CARDTYPE card);
  void setVideoStyle(Video & video, const VideoStyle_e style, const bool enabled);

  void saveTFEEnabled(const int enabled);

  void changeBreakpoint(const DWORD nAddress, const bool enableAndSet);

  ImVec4 colorrefToImVec4(const COLORREF cr);
  COLORREF imVec4ToColorref(const ImVec4 & color);

}

namespace ImGui
{
  bool CheckBoxTristate(const char* label, int* v_tristate);
  void PushStyleCompact();
  void PopStyleCompact();
}
