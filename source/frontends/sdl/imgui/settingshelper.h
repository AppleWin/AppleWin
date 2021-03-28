#pragma once

#include "Card.h"
#include "CPU.h"
#include "Common.h"

#include <string>
#include <vector>
#include <map>

namespace sa2
{

  const std::string & getCardName(SS_CARDTYPE card);
  const std::string & getApple2Name(eApple2Type type);
  const std::string & getCPUName(eCpuType cpu);
  const std::string & getModeName(AppMode_e mode);

  const std::vector<SS_CARDTYPE> & getCardsForSlot(size_t slot);
  const std::vector<SS_CARDTYPE> & getExpansionCards();
  const std::map<eApple2Type, std::string> & getAapple2Types();

  void insertCard(size_t slot, SS_CARDTYPE card);
}
