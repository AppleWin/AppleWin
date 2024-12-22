#include "StdAfx.h"
#include "frontends/libretro/retroregistry.h"
#include "frontends/common2/ptreeregistry.h"
#include "frontends/libretro/environment.h"

#include "Common.h"
#include "Card.h"
#include "Video.h"

#include "libretro.h"

#include <list>
#include <sstream>

namespace
{

  const std::string ourScope = "applewin_";

  const char * REG_RA2 = "ra2";
  const char * REGVALUE_AUDIO_SOURCE = "Audio source";
  const char * REGVALUE_KEYBOARD_TYPE = "Keyboard type";
  const char * REGVALUE_PLAYLIST_START = "Playlist start";

  struct Variable
  {
    std::string name;
    std::string description;
    std::string section;
    std::string key;
    std::vector<std::pair<std::string, uint32_t> > values;
  };

  const std::vector<Variable> ourVariables =
    {
     {
      "machine",
      "Apple ][ Type",
      REG_CONFIG,
      REGVALUE_APPLE2_TYPE, // reset required
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
      "slot3",
      "Card in Slot 3",
      "Configuration\\Slot 3",
      REGVALUE_CARD_TYPE, // reset required
      {
       {"Empty", CT_Empty},
       {"Video HD", CT_VidHD},
      }
     },
     {
      "slot4",
      "Card in Slot 4",
      "Configuration\\Slot 4",
      REGVALUE_CARD_TYPE, // reset required
      {
       {"Empty", CT_Empty},
       {"Mouse", CT_MouseInterface},
       {"Mockingboard", CT_MockingboardC},
       {"Phasor", CT_Phasor},
      }
     },
     {
      "slot5",
      "Card in Slot 5",
      "Configuration\\Slot 5",
      REGVALUE_CARD_TYPE, // reset required
      {
       {"Empty", CT_Empty},
       {"CP/M", CT_Z80},
       {"Mockingboard", CT_MockingboardC},
       {"Phasor", CT_Phasor},
       {"SAM/DAC", CT_SAM},
      }
     },
     {
      "video_mode",
      "Video Mode",
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
     {
      "video_style",
      "Video Style",
      REG_CONFIG,
      REGVALUE_VIDEO_STYLE,
      {
       {"Half Scanlines", VS_HALF_SCANLINES},
       {"None", VS_NONE},
      }
     },
     {
      "video_refresh_rate",
      "Video Refresh Rate",
      REG_CONFIG,
      REGVALUE_VIDEO_REFRESH_RATE, // reset required
      {
       {"60Hz", VR_60HZ},
       {"50Hz", VR_50HZ},
      }
     },
     {
      "audio_source",
      "Audio Source",
      REG_RA2,
      REGVALUE_AUDIO_SOURCE,
      {
       {REGVALUE_AUDIO_SPEAKER, static_cast<uint32_t>(ra2::AudioSource::SPEAKER)},
       {REGVALUE_AUDIO_MOCKINGBOARD, static_cast<uint32_t>(ra2::AudioSource::MOCKINGBOARD)},
      }
     },
     {
      "keyboard_type",
      "Keyboard Type",
      REG_RA2,
      REGVALUE_KEYBOARD_TYPE,
      {
       {"ASCII", static_cast<uint32_t>(ra2::KeyboardType::ASCII)},
       {"Original", static_cast<uint32_t>(ra2::KeyboardType::Original)},
      }
     },
     {
      "playlist_start",
      "Playlist start disk",
      REG_RA2,
      REGVALUE_PLAYLIST_START,
      {
       {"First", static_cast<uint32_t>(ra2::PlaylistStartDisk::First)},
       {"Previous", static_cast<uint32_t>(ra2::PlaylistStartDisk::Previous)},
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

  void PopulateRegistry(const std::shared_ptr<Registry> & registry)
  {
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
  }

  std::shared_ptr<common2::PTreeRegistry> CreateRetroRegistry()
  {
    const auto registry = std::make_shared<common2::PTreeRegistry>();
    PopulateRegistry(registry);
    return registry;
  }

  AudioSource GetAudioSource()
  {
    uint32_t value = 1;
    RegLoadValue(REG_RA2, REGVALUE_AUDIO_SOURCE, TRUE, &value);
    const AudioSource source = value <= uint32_t(AudioSource::UNKNOWN) ? AudioSource(value) : AudioSource::UNKNOWN;
    return source;
  }

  KeyboardType GetKeyboardEmulationType()
  {
    uint32_t value = static_cast<uint32_t>(KeyboardType::ASCII);
    RegLoadValue(REG_RA2, REGVALUE_KEYBOARD_TYPE, TRUE, &value);
    return static_cast<KeyboardType>(value);
  }

  PlaylistStartDisk GetPlaylistStartDisk()
  {
    uint32_t value = static_cast<uint32_t>(PlaylistStartDisk::First);
    RegLoadValue(REG_RA2, REGVALUE_PLAYLIST_START, TRUE, &value);
    return static_cast<PlaylistStartDisk>(value);
  }

}
