#pragma once

#include "Card.h"
#include "CPU.h"
#include "Common.h"

#include <string>

const std::string & getCardName(SS_CARDTYPE card);
const std::string & getApple2Name(eApple2Type type);
const std::string & getCPUName(eCpuType cpu);
