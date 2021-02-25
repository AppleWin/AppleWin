#include "StdAfx.h"
#include "frontends/common2/ptreeregistry.h"
#include "frontends/libretro/environment.h"

#include "Common.h"
#include "Card.h"
#include "Video.h"

#include "libretro.h"

#include <list>
#include <vector>
#include <sstream>

namespace
{

  const std::string ourScope = "applewin_";

  struct Variable
  {
    std::string name;
    std::string description;
    std::string section;
    std::string key;
    std::vector<std::pair<std::string, DWORD> > values;
  };

  const std::vector<Variable> ourVariables =
    {
     {
      "machine",
      "Apple ][ type",
      REG_CONFIG,
      REGVALUE_APPLE2_TYPE,
      {
       {"Enhanced Apple //e", A2TYPE_APPLE2EENHANCED},
       {"Apple ][ (Original)", A2TYPE_APPLE2},
       {"Apple ][+", A2TYPE_APPLE2PLUS},
       {"Apple ][ J-Plus", A2TYPE_APPLE2JPLUS},
       {"Apple //e", A2TYPE_APPLE2E},
       {"Pravets 82", A2TYPE_PRAVETS82},
       {"Pravets 8M", A2TYPE_PRAVETS8M},
       {"Pravets 8A", A2TYPE_PRAVETS8A},
       {"Base64A", A2TYPE_BASE64A},
       {"TK3000 //e", A2TYPE_TK30002E},
      }
     },
     {
      "slot4",
      "Card in slot 4",
      REG_CONFIG,
      REGVALUE_SLOT4,
      {
       {"Empty", CT_Empty},
       {"Mouse", CT_MouseInterface},
       {"Mockingboard", CT_MockingboardC},
       {"Phasor", CT_Phasor},
      }
     },
     {
      "slot5",
      "Card in slot 5",
      REG_CONFIG,
      REGVALUE_SLOT5,
      {
       {"Empty", CT_Empty},
       {"CP/M", CT_Z80},
       {"Mockingboard", CT_MockingboardC},
       {"SAM/DAC", CT_SAM},
      }
     },
     {
      "video",
      "Video mode",
      REG_CONFIG,
      REGVALUE_VIDEO_MODE,
      {
       {"Color (Composite Idealized)", VT_COLOR_IDEALIZED},
       {"Color (RGB Card/Monitor)", VT_COLOR_VIDEOCARD_RGB},
       {"Color (Composite Monitor)", VT_COLOR_MONITOR_NTSC},
       {"Color TV", VT_COLOR_TV},
       {"B&W TV", VT_MONO_TV},
       {"Monochrome (Amber)", VT_MONO_AMBER},
       {"Monochrome (Green)", VT_MONO_GREEN},
       {"Monochrome (White)", VT_MONO_WHITE},
      }
     },
    };

  std::string getKey(const Variable & var)
  {
    std::ostringstream ss;
    ss << var.description << "; ";
    for (size_t i = 0; i < var.values.size(); ++i)
    {
      if (i > 0)
      {
	ss << "|";
      }
      ss << var.values[i].first;
    }
    return ss.str();
  }

}

namespace ra2
{

  void SetupRetroVariables()
  {
    const size_t numberOfVariables = ourVariables.size();
    std::vector<retro_variable> retroVariables(numberOfVariables + 1);
    std::list<std::string> workspace; // so objects do not move when it resized

    // we need to keep the char * alive till after the call to RETRO_ENVIRONMENT_SET_VARIABLES
    const auto c_str = [&workspace] (const auto & s)
                       {
                         workspace.push_back(s);
                         return workspace.back().c_str();
                       };

    for (size_t i = 0; i < numberOfVariables; ++i)
    {
      const Variable & variable = ourVariables[i];
      retro_variable & retroVariable = retroVariables[i];

      retroVariable.key = c_str(ourScope + variable.name);
      retroVariable.value = c_str(getKey(variable));
    }

    environ_cb(RETRO_ENVIRONMENT_SET_VARIABLES, retroVariables.data());
  }

  void InitialiseRetroRegistry()
  {
    const auto registry = std::make_shared<common2::PTreeRegistry>();

    for (const Variable & variable : ourVariables)
    {
      const std::string retroKey = ourScope + variable.name;
      retro_variable retroVariable;
      retroVariable.key = retroKey.c_str();
      retroVariable.value = nullptr;
      if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &retroVariable) && retroVariable.value)
      {
        const std::string value(retroVariable.value);
        const auto check = [&value] (const auto & x)
                           {
                             return x.first == value;
                           };
        const auto it = std::find_if(variable.values.begin(), variable.values.end(), check);
        if (it != variable.values.end())
        {
          registry->putDWord(variable.section, variable.key, it->second);
        }
      }
    }

    Registry::instance = registry;
  }

}
