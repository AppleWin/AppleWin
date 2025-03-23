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

    const char *CATEGORY_SYSTEM = "system";
    const char *CATEGORY_INPUT = "input";
    const char *CATEGORY_RETROPAD_MAPPING = "retropad";

    retro_core_option_v2_category ourOptionCatsUS[] = {
        {CATEGORY_SYSTEM, "System", "Configure system options."},
        {CATEGORY_INPUT, "Input", "Configure input options."},
        {CATEGORY_RETROPAD_MAPPING, "RetroPad Mapping", "Configure RetroPad mapping options."},
        {nullptr, nullptr, nullptr},
    };

    struct Variable
    {
        const char *name;
        const char *description;
        const char *category;
        const std::vector<std::pair<const char *, uint32_t>> values;
        const char *defaultValue;

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

        void fillValues(retro_core_option_value values[RETRO_NUM_CORE_OPTION_VALUES_MAX]) const
        {
            for (size_t i = 0; i < this->values.size(); ++i)
            {
                values[i].value = this->values[i].first;
            }
            const size_t last = this->values.size();
            values[last].value = nullptr;
            values[last].label = nullptr;
        }

        std::vector<std::pair<const char *, uint32_t>>::const_iterator findValue(
            const retro_variable &retroVariable) const
        {
            const char *value = retroVariable.value;
            const auto check = [&value](const auto &x) { return strcmp(x.first, value) == 0; };
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

    const std::vector<std::pair<const char *, uint32_t>> joypadMappingValues = {
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

    template <typename T, typename W>
    void setupVariables(
        const std::vector<T> &variables, const W &c_str, std::vector<retro_core_option_v2_definition> &retroVariables)
    {
        retroVariables.reserve(retroVariables.size() + variables.size());
        for (const T &variable : variables)
        {
            retro_core_option_v2_definition &def = retroVariables.emplace_back();
            def.key = c_str(ourScope + variable.name);
            def.desc = variable.description;
            def.category_key = variable.category;
            variable.fillValues(def.values);
            def.default_value = variable.defaultValue ? variable.defaultValue : def.values[0].value;
        }
    }

    const std::vector<RegistryVariable> ourRegistryVariables = {
        {
            {
                "machine",
                "Apple ][ Type",
                CATEGORY_SYSTEM,
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
                CATEGORY_SYSTEM,
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
                CATEGORY_SYSTEM,
                {
                    {"Empty", CT_Empty},
                    {"Mockingboard", CT_MockingboardC},
                    {"Mouse", CT_MouseInterface},
                    {"Phasor", CT_Phasor},
                },
                "Mockingboard",
            },
            "Configuration\\Slot 4",
            REGVALUE_CARD_TYPE, // reset required
        },
        {
            {
                "slot5",
                "Card in Slot 5",
                CATEGORY_SYSTEM,
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
                CATEGORY_SYSTEM,
                {
                    {"Color (RGB Card/Monitor)", VT_COLOR_VIDEOCARD_RGB},
                    {"Color (Composite Idealized)", VT_COLOR_IDEALIZED},
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
                CATEGORY_SYSTEM,
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
                CATEGORY_SYSTEM,
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
                "playlist_start",
                "Playlist Start Disk",
                CATEGORY_SYSTEM,
                {
                    {"First", static_cast<uint32_t>(ra2::PlaylistStartDisk::First)},
                    {"Previous", static_cast<uint32_t>(ra2::PlaylistStartDisk::Previous)},
                },
            },
            REG_RA2,
            REGVALUE_PLAYLIST_START,
        },
        {
            {
                "keyboard_type",
                "Keyboard Type",
                CATEGORY_INPUT,
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
                "mouse_speed",
                "Mouse Speed",
                CATEGORY_INPUT,
                {
                    {"0.25", 25},  {"0.50", 50},  {"0.75", 75},  {"1.00", 100}, {"1.25", 125},
                    {"1.50", 150}, {"1.75", 175}, {"2.00", 200}, {"2.25", 225}, {"2.50", 250},
                    {"2.75", 275}, {"3.00", 300}, {"3.25", 325}, {"3.50", 350}, {"3.75", 375},
                    {"4.00", 400}, {"4.25", 425}, {"4.50", 450}, {"4.75", 475}, {"5.00", 500},
                },
                "1.00",
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
                CATEGORY_RETROPAD_MAPPING,

                joypadMappingValues,
            },
            RETRO_DEVICE_ID_JOYPAD_A,
        },
        {
            {
                "joypad_b",
                "Joypad B [button 1]",
                CATEGORY_RETROPAD_MAPPING,
                joypadMappingValues,
            },
            RETRO_DEVICE_ID_JOYPAD_B,
        },
        {
            {
                "joypad_x",
                "Joypad X",
                CATEGORY_RETROPAD_MAPPING,
                joypadMappingValues,
            },
            RETRO_DEVICE_ID_JOYPAD_X,
        },
        {
            {
                "joypad_y",
                "Joypad Y",
                CATEGORY_RETROPAD_MAPPING,
                joypadMappingValues,
            },
            RETRO_DEVICE_ID_JOYPAD_Y,
        },
        {
            {
                "joypad_select",
                "Joypad Select",
                CATEGORY_RETROPAD_MAPPING,
                joypadMappingValues,
            },
            RETRO_DEVICE_ID_JOYPAD_SELECT,
        },
        {
            {
                "joypad_start",
                "Joypad Start",
                CATEGORY_RETROPAD_MAPPING,
                joypadMappingValues,
            },
            RETRO_DEVICE_ID_JOYPAD_START,
        },
        {
            {
                "joypad_down",
                "Joypad Down [paddle 1 standard]",
                CATEGORY_RETROPAD_MAPPING,
                joypadMappingValues,
            },
            RETRO_DEVICE_ID_JOYPAD_DOWN,
        },
        {
            {
                "joypad_up",
                "Joypad Up [paddle 1 standard]",
                CATEGORY_RETROPAD_MAPPING,
                joypadMappingValues,
            },
            RETRO_DEVICE_ID_JOYPAD_UP,
        },
        {
            {
                "joypad_left",
                "Joypad Left [paddle 0 standard]",
                CATEGORY_RETROPAD_MAPPING,
                joypadMappingValues,
            },
            RETRO_DEVICE_ID_JOYPAD_LEFT,
        },
        {
            {
                "joypad_right",
                "Joypad Right [paddle 0 standard]",
                CATEGORY_RETROPAD_MAPPING,
                joypadMappingValues,
            },
            RETRO_DEVICE_ID_JOYPAD_RIGHT,
        },
        {
            {
                "joypad_l",
                "Joypad L",
                CATEGORY_RETROPAD_MAPPING,
                joypadMappingValues,
            },
            RETRO_DEVICE_ID_JOYPAD_L,
        },
        {
            {
                "joypad_r",
                "Joypad R",
                CATEGORY_RETROPAD_MAPPING,
                joypadMappingValues,
            },
            RETRO_DEVICE_ID_JOYPAD_R,
        },
        {
            {
                "joypad_l2",
                "Joypad L2",
                CATEGORY_RETROPAD_MAPPING,
                joypadMappingValues,
            },
            RETRO_DEVICE_ID_JOYPAD_L2,
        },
        {
            {
                "joypad_r2",
                "Joypad R2",
                CATEGORY_RETROPAD_MAPPING,
                joypadMappingValues,
            },
            RETRO_DEVICE_ID_JOYPAD_R2,
        },
        {
            {
                "joypad_l3",
                "Joypad L3",
                CATEGORY_RETROPAD_MAPPING,
                joypadMappingValues,
            },
            RETRO_DEVICE_ID_JOYPAD_L3,
        },
        {
            {
                "joypad_r3",
                "Joypad R3",
                CATEGORY_RETROPAD_MAPPING,
                joypadMappingValues,
            },
            RETRO_DEVICE_ID_JOYPAD_R3,
        },
    };

    bool setupRetroVariablesV2(const std::function<const char *(const std::string &)> &c_str)
    {
        const size_t numberOfVariables = ourRegistryVariables.size() + ourJoypadMappingVariables.size();
        std::vector<retro_core_option_v2_definition> retroVariables;
        retroVariables.reserve(numberOfVariables + 1);

        setupVariables(ourRegistryVariables, c_str, retroVariables);
        setupVariables(ourJoypadMappingVariables, c_str, retroVariables);

        retroVariables.push_back({nullptr, nullptr});

        retro_core_options_v2 optionsUS = {ourOptionCatsUS, retroVariables.data()};
        retro_core_options_v2_intl optionsIntl = {&optionsUS, nullptr};

        const bool res = ra2::environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2_INTL, &optionsIntl);
        return res;
    }

    bool setupRetroVariablesV0(const std::function<const char *(const std::string &)> &c_str)
    {
        const size_t numberOfVariables = ourRegistryVariables.size() + ourJoypadMappingVariables.size();
        std::vector<retro_variable> retroVariables;
        retroVariables.reserve(numberOfVariables + 1);

        setupVariables(ourRegistryVariables, c_str, retroVariables);
        setupVariables(ourJoypadMappingVariables, c_str, retroVariables);

        retroVariables.push_back({nullptr, nullptr});

        const bool res = ra2::environ_cb(RETRO_ENVIRONMENT_SET_VARIABLES, retroVariables.data());
        return res;
    }

} // namespace

namespace ra2
{

    void setupRetroVariables()
    {
        std::list<std::string> workspace; // so objects do not move when it resized

        // we need to keep the char * alive till after the call to RETRO_ENVIRONMENT_SET_VARIABLES
        const auto c_str = [&workspace](const auto &s)
        {
            workspace.push_back(s);
            return workspace.back().c_str();
        };

        unsigned version = 0;
        if (environ_cb(RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION, &version) && (version >= 2))
        {
            setupRetroVariablesV2(c_str);
        }
        else
        {
            setupRetroVariablesV0(c_str);
        }
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
