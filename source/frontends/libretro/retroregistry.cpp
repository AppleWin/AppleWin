#include "StdAfx.h"
#include "frontends/libretro/retroregistry.h"
#include "frontends/libretro/environment.h"
#include "frontends/libretro/game.h"
#include "frontends/common2/ptreeregistry.h"

#include "Registry.h"
#include "Common.h"
#include "Card.h"
#include "Video.h"

#include "libretro.h"

#include <list>
#include <sstream>

namespace
{

    const std::string ourScope = "applewin_";

    const char *REG_RA2 = "ra2";
    const char *REGVALUE_KEYBOARD_TYPE = "Keyboard type";
    const char *REGVALUE_PLAYLIST_START = "Playlist start";
    const char *REGVALUE_MOUSE_SPEED_00 = "Mouse speed";

    struct Variable
    {
        const std::string name;
        const std::string description;
        const std::vector<std::pair<std::string, uint32_t>> values;

        std::string getKey() const
        {
            std::ostringstream ss;
            ss << description << "; ";
            for (size_t i = 0; i < values.size(); ++i)
            {
                if (i > 0)
                {
                    ss << "|";
                }
                ss << values[i].first;
            }
            return ss.str();
        }

        std::vector<std::pair<std::string, uint32_t>>::const_iterator findValue(
            const retro_variable &retroVariable) const
        {
            const std::string value(retroVariable.value);
            const auto check = [&value](const auto &x) { return x.first == value; };
            const auto it = std::find_if(values.begin(), values.end(), check);
            return it;
        }
    };

    struct RegistryVariable : public Variable
    {
        const std::string section;
        const std::string key;

        void apply(ra2::Game &game, const retro_variable &retroVariable) const
        {
            Registry &registry = game.getRegistry();
            const auto it = findValue(retroVariable);
            if (it != values.end())
            {
                registry.putDWord(section, key, it->second);
            }
        }
    };

    const std::vector<std::pair<std::string, uint32_t>> joypadMappingValues = {
        {"<default>", 0x00}, {"0", '0'},      {"1", '1'},     {"2", '2'},    {"3", '3'},     {"4", '4'},
        {"5", '5'},          {"6", '6'},      {"7", '7'},     {"8", '8'},    {"9", '9'},     {"A", 'A'},
        {"B", 'B'},          {"C", 'C'},      {"D", 'D'},     {"E", 'E'},    {"F", 'F'},     {"G", 'G'},
        {"H", 'H'},          {"I", 'I'},      {"J", 'J'},     {"K", 'K'},    {"L", 'L'},     {"M", 'M'},
        {"N", 'N'},          {"O", 'O'},      {"P", 'P'},     {"Q", 'Q'},    {"R", 'R'},     {"S", 'S'},
        {"T", 'T'},          {"U", 'U'},      {"V", 'V'},     {"W", 'W'},    {"X", 'X'},     {"Y", 'Y'},
        {"Z", 'Z'},          {"Enter", 0x0d}, {"Space", ' '}, {"ESC", 0x0b}, {"Left", 0x08}, {"Right", 0x15},
        {"Down", 0x0a},      {"Up", 0x0b},
    };

    struct JoypadMappingVariable : public Variable
    {
        const size_t button;

        void apply(ra2::Game &game, const retro_variable &retroVariable) const
        {
            const auto it = findValue(retroVariable);
            if (it != values.end())
            {
                const uint32_t value = it->second;
                game.getInputRemapper().remapButton(button, value);
            }
        }
    };

    template <typename T> void applyVariables(const std::vector<T> &variables, ra2::Game &game)
    {
        for (const T &variable : variables)
        {
            const std::string retroKey = ourScope + variable.name;
            retro_variable retroVariable{retroKey.c_str(), nullptr};
            if (ra2::environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &retroVariable) && retroVariable.value)
            {
                variable.apply(game, retroVariable);
            }
        }
    }

    template <typename T, typename W>
    void setupVariables(const std::vector<T> &variables, const W &c_str, std::vector<retro_variable> &retroVariables)
    {
        retroVariables.reserve(retroVariables.size() + variables.size());
        for (const T &variable : variables)
        {
            retroVariables.push_back({c_str(ourScope + variable.name), c_str(variable.getKey())});
        }
    }

    const std::vector<RegistryVariable> ourRegistryVariables = {
        {
            {
                "machine",
                "Apple ][ Type",
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
                },
            },
            REG_CONFIG,
            REGVALUE_APPLE2_TYPE, // reset required
        },
        {
            {
                "slot3",
                "Card in Slot 3",
                {
                    {"Empty", CT_Empty},
                    {"Video HD", CT_VidHD},
                },
            },
            "Configuration\\Slot 3",
            REGVALUE_CARD_TYPE, // reset required
        },
        {
            {
                "slot4",
                "Card in Slot 4",
                {
                    {"Empty", CT_Empty},
                    {"Mouse", CT_MouseInterface},
                    {"Mockingboard", CT_MockingboardC},
                    {"Phasor", CT_Phasor},

                },
            },
            "Configuration\\Slot 4",
            REGVALUE_CARD_TYPE, // reset required
        },
        {
            {
                "slot5",
                "Card in Slot 5",
                {
                    {"Empty", CT_Empty},
                    {"CP/M", CT_Z80},
                    {"Mockingboard", CT_MockingboardC},
                    {"Phasor", CT_Phasor},
                    {"SAM/DAC", CT_SAM},
                },
            },
            "Configuration\\Slot 5",
            REGVALUE_CARD_TYPE, // reset required
        },
        {
            {
                "video_mode",
                "Video Mode",
                {
                    {"Color (Composite Idealized)", VT_COLOR_IDEALIZED},
                    {"Color (RGB Card/Monitor)", VT_COLOR_VIDEOCARD_RGB},
                    {"Color (Composite Monitor)", VT_COLOR_MONITOR_NTSC},
                    {"Color TV", VT_COLOR_TV},
                    {"B&W TV", VT_MONO_TV},
                    {"Monochrome (Amber)", VT_MONO_AMBER},
                    {"Monochrome (Green)", VT_MONO_GREEN},
                    {"Monochrome (White)", VT_MONO_WHITE},
                },
            },
            REG_CONFIG,
            REGVALUE_VIDEO_MODE,
        },
        {
            {
                "video_style",
                "Video Style",
                {
                    {"Half Scanlines", VS_HALF_SCANLINES},
                    {"None", VS_NONE},
                },
            },
            REG_CONFIG,
            REGVALUE_VIDEO_STYLE,
        },
        {
            {
                "video_refresh_rate",
                "Video Refresh Rate",
                {
                    {"60Hz", VR_60HZ},
                    {"50Hz", VR_50HZ},
                },
            },
            REG_CONFIG,
            REGVALUE_VIDEO_REFRESH_RATE, // reset required
        },
        {
            {
                "keyboard_type",
                "Keyboard Type",
                {
                    {"ASCII", static_cast<uint32_t>(ra2::KeyboardType::ASCII)},
                    {"Original", static_cast<uint32_t>(ra2::KeyboardType::Original)},
                },
            },
            REG_RA2,
            REGVALUE_KEYBOARD_TYPE,
        },
        {
            {
                "playlist_start",
                "Playlist start disk",
                {
                    {"First", static_cast<uint32_t>(ra2::PlaylistStartDisk::First)},
                    {"Previous", static_cast<uint32_t>(ra2::PlaylistStartDisk::Previous)},
                },
            },
            REG_RA2,
            REGVALUE_PLAYLIST_START,
        },
        {
            "mouse_speed",
            "Mouse pointer speed",
            {
                {"0.25", 25},  {"0.50", 50},  {"0.75", 75},  {"1.00", 100}, {"1.25", 125}, {"1.50", 150}, {"1.75", 175},
                {"2.00", 200}, {"2.25", 225}, {"2.50", 250}, {"2.75", 275}, {"3.00", 300}, {"3.25", 325}, {"3.50", 350},
                {"3.75", 375}, {"4.00", 400}, {"4.25", 425}, {"4.50", 450}, {"4.75", 475}, {"5.00", 500},
            },
            REG_RA2,
            REGVALUE_MOUSE_SPEED_00,
        },
    };

    const std::vector<JoypadMappingVariable> ourJoypadMappingVariables = {
        {
            {
                "joypad_a",
                "Joypad A [button 0]",
                joypadMappingValues,
            },
            RETRO_DEVICE_ID_JOYPAD_A,
        },
        {
            {
                "joypad_b",
                "Joypad B [button 1]",
                joypadMappingValues,
            },
            RETRO_DEVICE_ID_JOYPAD_B,
        },
        {
            {
                "joypad_x",
                "Joypad X",
                joypadMappingValues,
            },
            RETRO_DEVICE_ID_JOYPAD_X,
        },
        {
            {
                "joypad_y",
                "Joypad Y",
                joypadMappingValues,
            },
            RETRO_DEVICE_ID_JOYPAD_Y,
        },
        {
            {
                "joypad_select",
                "Joypad Select",
                joypadMappingValues,
            },
            RETRO_DEVICE_ID_JOYPAD_SELECT,
        },
        {
            {
                "joypad_start",
                "Joypad Start",
                joypadMappingValues,
            },
            RETRO_DEVICE_ID_JOYPAD_START,
        },
        {
            {
                "joypad_down",
                "Joypad Down [paddle 1 standard]",
                joypadMappingValues,
            },
            RETRO_DEVICE_ID_JOYPAD_DOWN,
        },
        {
            {
                "joypad_up",
                "Joypad Up [paddle 1 standard]",
                joypadMappingValues,
            },
            RETRO_DEVICE_ID_JOYPAD_UP,
        },
        {
            {
                "joypad_left",
                "Joypad Left [paddle 0 standard]",
                joypadMappingValues,
            },
            RETRO_DEVICE_ID_JOYPAD_LEFT,
        },
        {
            {
                "joypad_right",
                "Joypad Right [paddle 0 standard]",
                joypadMappingValues,
            },
            RETRO_DEVICE_ID_JOYPAD_RIGHT,
        },
        {
            {
                "joypad_l",
                "Joypad L",
                joypadMappingValues,
            },
            RETRO_DEVICE_ID_JOYPAD_L,
        },
        {
            {
                "joypad_r",
                "Joypad R",
                joypadMappingValues,
            },
            RETRO_DEVICE_ID_JOYPAD_R,
        },
        {
            {
                "joypad_l2",
                "Joypad L2",
                joypadMappingValues,
            },
            RETRO_DEVICE_ID_JOYPAD_L2,
        },
        {
            {
                "joypad_r2",
                "Joypad R2",
                joypadMappingValues,
            },
            RETRO_DEVICE_ID_JOYPAD_R2,
        },
        {
            {
                "joypad_l3",
                "Joypad L3",
                joypadMappingValues,
            },
            RETRO_DEVICE_ID_JOYPAD_L3,
        },
        {
            {
                "joypad_r3",
                "Joypad R3",
                joypadMappingValues,
            },
            RETRO_DEVICE_ID_JOYPAD_R3,
        },
    };

} // namespace

namespace ra2
{

    void setupRetroVariables()
    {
        const size_t numberOfVariables = ourRegistryVariables.size() + ourJoypadMappingVariables.size();
        std::vector<retro_variable> retroVariables;
        retroVariables.reserve(numberOfVariables + 1);

        std::list<std::string> workspace; // so objects do not move when it resized

        // we need to keep the char * alive till after the call to RETRO_ENVIRONMENT_SET_VARIABLES
        const auto c_str = [&workspace](const auto &s)
        {
            workspace.push_back(s);
            return workspace.back().c_str();
        };

        setupVariables(ourRegistryVariables, c_str, retroVariables);
        setupVariables(ourJoypadMappingVariables, c_str, retroVariables);

        retroVariables.push_back({nullptr, nullptr});

        const bool res = environ_cb(RETRO_ENVIRONMENT_SET_VARIABLES, retroVariables.data());
        retroVariables.clear();
    }

    void applyRetroVariables(Game &game)
    {
        applyVariables(ourRegistryVariables, game);
        applyVariables(ourJoypadMappingVariables, game);
    }

    std::shared_ptr<common2::PTreeRegistry> createRetroRegistry()
    {
        const auto registry = std::make_shared<common2::PTreeRegistry>();
        return registry;
    }

    KeyboardType getKeyboardEmulationType()
    {
        uint32_t value = static_cast<uint32_t>(KeyboardType::ASCII);
        RegLoadValue(REG_RA2, REGVALUE_KEYBOARD_TYPE, TRUE, &value);
        return static_cast<KeyboardType>(value);
    }

    PlaylistStartDisk getPlaylistStartDisk()
    {
        uint32_t value = static_cast<uint32_t>(PlaylistStartDisk::First);
        RegLoadValue(REG_RA2, REGVALUE_PLAYLIST_START, TRUE, &value);
        return static_cast<PlaylistStartDisk>(value);
    }

    double getMouseSpeed()
    {
        uint32_t value = 75;
        RegLoadValue(REG_RA2, REGVALUE_MOUSE_SPEED_00, TRUE, &value);
        return value / 100.0;
    }

} // namespace ra2
