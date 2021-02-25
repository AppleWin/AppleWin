#include "StdAfx.h"
#include "frontends/sdl/imgui/settingshelper.h"

namespace
{
  const std::map<SS_CARDTYPE, std::string> cards =
    {
     {CT_Empty, "CT_Empty"},
     {CT_Disk2, "CT_Disk2"},
     {CT_SSC, "CT_SSC"},
     {CT_MockingboardC, "CT_MockingboardC"},
     {CT_GenericPrinter, "CT_GenericPrinter"},
     {CT_GenericHDD, "CT_GenericHDD"},
     {CT_GenericClock, "CT_GenericClock"},
     {CT_MouseInterface, "CT_MouseInterface"},
     {CT_Z80, "CT_Z80"},
     {CT_Phasor, "CT_Phasor"},
     {CT_Echo, "CT_Echo"},
     {CT_SAM, "CT_SAM"},
     {CT_80Col, "CT_80Col"},
     {CT_Extended80Col, "CT_Extended80Col"},
     {CT_RamWorksIII, "CT_RamWorksIII"},
     {CT_Uthernet, "CT_Uthernet"},
     {CT_LanguageCard, "CT_LanguageCard"},
     {CT_LanguageCardIIe, "CT_LanguageCardIIe"},
     {CT_Saturn128K, "CT_Saturn128K"},
    };

  const std::map<eApple2Type, std::string> apple2types =
    {
     {A2TYPE_APPLE2, "A2TYPE_APPLE2"},
     {A2TYPE_APPLE2PLUS, "A2TYPE_APPLE2PLUS"},
     {A2TYPE_APPLE2JPLUS, "A2TYPE_APPLE2JPLUS"},
     {A2TYPE_APPLE2E, "A2TYPE_APPLE2E"},
     {A2TYPE_APPLE2EENHANCED, "A2TYPE_APPLE2EENHANCED"},
     {A2TYPE_PRAVETS8M, "A2TYPE_PRAVETS8M"},
     {A2TYPE_PRAVETS82, "A2TYPE_PRAVETS82"},
     {A2TYPE_BASE64A, "A2TYPE_BASE64A"},
     {A2TYPE_PRAVETS8A, "A2TYPE_PRAVETS8A"},
     {A2TYPE_TK30002E, "A2TYPE_TK30002E"},
    };

  const std::map<eCpuType, std::string> cpuTypes =
    {
     {CPU_6502, "CPU_6502"},
     {CPU_65C02, "CPU_65C02"},
     {CPU_Z80, "CPU_Z80"},
    };
}

namespace sa2
{

  const std::string & getCardName(SS_CARDTYPE card)
  {
    return cards.at(card);
  }

  const std::string & getApple2Name(eApple2Type type)
  {
    return apple2types.at(type);
  }

  const std::string & getCPUName(eCpuType cpu)
  {
    return cpuTypes.at(cpu);
  }

}
