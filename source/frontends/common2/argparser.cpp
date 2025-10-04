#include "StdAfx.h"
#include "frontends/common2/argparser.h"
#include "frontends/common2/programoptions.h"
#include "frontends/common2/utils.h"
#include "linux/version.h"

#include "Memory.h"

#include <getopt.h>
#include <regex>
#include <iostream>
#include <iomanip>
#include <sstream>

namespace
{

    constexpr int PAUSED = 1001;
    constexpr int FIXED_SPEED = 1002;
    constexpr int HEADLESS = 1003;
    constexpr int NO_SQUARING = 1004;
    constexpr int SLIRP_NAT = 1005;

    constexpr int DISK_H1 = 1006;
    constexpr int DISK_H2 = 1007;

    constexpr int MEM_CLEAR = 1008;
    constexpr int ROM = 1009;
    constexpr int F8ROM = 1010;

    constexpr int NO_AUDIO = 1011;
    constexpr int AUDIO_BUFFER = 1012;
    constexpr int WAV_SPEAKER = 1013;
    constexpr int WAV_MOCKINGBOARD = 1014;

    constexpr int SDL_DRIVER = 1015;
    constexpr int GL_SWAP = 1016;
    constexpr int TIMER = 1017;
    constexpr int NO_IMGUI = 1018;
    constexpr int GEOMETRY = 1019;
    constexpr int ASPECT_RATIO = 1020;
    constexpr int GAME_CONTROLLER = 1021;
    constexpr int MAPPING_FILE = 1022;
    constexpr int AUDIO_DEVICE = 1023;

    constexpr int NO_VIDEO_UPDATE = 1024;
    constexpr int EV_DEVICE_NAME = 1025;

    struct OptionData_t
    {
        const char *name;
        int has_arg;
        int val;
        const char *description;
        const char *defaultValue; // optional
    };

    bool isShort(const int val)
    {
        return val >= 0 && val <= 0xFF;
    }

    void printHelp(const std::vector<std::pair<std::string, std::vector<OptionData_t>>> &data)
    {
        for (const auto &categories : data)
        {
            std::cerr << categories.first << ":" << std::endl;
            for (const auto &option : categories.second)
            {
                std::cerr << "  ";
                std::ostringstream value;
                if (isShort(option.val))
                {
                    value << "[ -" << char(option.val) << " ] ";
                }
                else
                {
                    value << "       ";
                }
                value << "--" << option.name;
                if (option.has_arg == required_argument)
                {
                    value << " arg";
                }
                std::cerr << std::left << std::setw(30) << value.str() << "\t" << option.description << std::endl;
                if (option.defaultValue)
                {
                    std::cerr << "\t\t\t\t\t( " << option.defaultValue << " )" << std::endl;
                }
            }
            std::cerr << std::endl;
        }
    }

    void parseGeometry(const std::string &s, std::optional<common2::Geometry> &geometry)
    {
        std::smatch m;
        if (std::regex_match(s, m, std::regex("^(\\d+)x(\\d+)(\\+(\\d+)\\+(\\d+))?$")))
        {
            const size_t groups = m.size();
            if (groups == 6)
            {
                geometry = common2::Geometry();
                geometry->width = std::stoi(m.str(1));
                geometry->height = std::stoi(m.str(2));
                if (!m.str(3).empty())
                {
                    geometry->x = std::stoi(m.str(4));
                    geometry->y = std::stoi(m.str(5));
                }
                return;
            }
        }
        throw std::runtime_error("Invalid sizes: " + s);
    }

    void extractOptions(
        const std::vector<std::pair<std::string, std::vector<OptionData_t>>> &data, std::vector<option> &longOptions,
        std::string &shortOptions)
    {
        std::ostringstream shorts;
        for (const auto &categories : data)
        {
            for (const auto &option : categories.second)
            {
                longOptions.push_back({option.name, option.has_arg, nullptr, option.val});
                const int val = option.val;
                if (isShort(val))
                {
                    shorts << char(val);
                    if (option.has_arg == required_argument)
                    {
                        shorts << ":";
                    }
                }
            }
        }
        longOptions.push_back({nullptr, 0, nullptr, 0});
        shortOptions.append(shorts.str());
    }

} // namespace

namespace common2
{

    bool getEmulatorOptions(
        int argc, char *const argv[], OptionsType type, const std::string &edition, EmulatorOptions &options)
    {
        const std::string name = "Apple Emulator for " + edition + " (based on AppleWin " + getVersion() + ")";

        options.configurationFile = getConfigFile("applewin.conf");
        const std::string configurationFileDefault = options.configurationFile.string();
        const std::string audioBufferDefault = std::to_string(options.audioBuffer);

        // clang-format off

        std::vector<std::pair<std::string, std::vector<OptionData_t>>> allOptions = {
            {name.c_str(),
             {
                 {"help",                    no_argument,          'h',              "Print this help message"},
             }},
            {"Configuration",
             {
                 {"conf",                    required_argument,    'c',              "Select configuration file", configurationFileDefault.c_str()},
                 {"qt-ini",                  no_argument,          'q',              "Use Qt ini file (read only)"},
                 {"registry",                required_argument,    'r',              "Registry options section.path=value"},
             }},
            {"Emulator",
             {
                 {"log",                     no_argument,          'l',              "Log to AppleWin.log"},
                 {"paused",                  no_argument,          PAUSED,           "Start paused"},
                 {"fixed-speed",             no_argument,          FIXED_SPEED,      "Fixed (non-adaptive) speed"},
                 {"headless",                no_argument,          HEADLESS,         "Headless: disable video (freewheel)"},
                 {"benchmark",               no_argument,          'b',              "Benchmark emulator"},
                 {"no-squaring",             no_argument,          NO_SQUARING,      "Gamepad range is (already) a square"},
                 {"nat",                     required_argument,    SLIRP_NAT,        "SLIRP PortFwd (e.g. 0,tcp,,8080,,http)"},
             }},
            {"Disk",
             {
                 {"d1",                      required_argument,    '1',              "Disk in S6D1 drive"},
                 {"d2",                      required_argument,    '2',              "Disk in S6D2 drive"},
                 {"h1",                      required_argument,    DISK_H1,          "Hard Disk in 1st drive"},
                 {"h2",                      required_argument,    DISK_H2,          "Hard Disk in 1st drive"},
             }},
            {"Snapshot",
             {
                 {"state-filename",          required_argument,    'f',              "Set snapshot filename"},
                 {"load-state",              required_argument,    's',              "Load snapshot from file"},
             }},
            {"Memory",
             {
                 {"memclear",                required_argument,    MEM_CLEAR,        "Memory initialization pattern [0..7]"},
                 {"rom",                     required_argument,    ROM,              "Custom 12k/16k ROM"},
                 {"f8rom",                   required_argument,    F8ROM,            "Custom 2k ROM"},
             }},
            {"Audio",
             {
                 {"no-audio",                no_argument,          NO_AUDIO,         "Disable audio"},
                 {"audio-buffer",            required_argument,    AUDIO_BUFFER,     "Audio buffer (ms)", audioBufferDefault.c_str()},
                 {"wav-speaker",             required_argument,    WAV_SPEAKER,      "Speaker wav output filename"},
                 {"wav-mockingboard",        required_argument,    WAV_MOCKINGBOARD, "Mockingboard wav output filename"},
             }},
        };

        const std::vector<std::pair<std::string, std::vector<OptionData_t>>> sa2Options = {
            {"sa2",
             {
                 {"sdl-driver",              required_argument,    SDL_DRIVER,       "SDL driver"},
                 {"gl-swap",                 required_argument,    GL_SWAP,          "SDL_GL_SwapInterval"},
                 {"timer",                   no_argument,          TIMER,            "Synchronise with timer"},
                 {"no-imgui",                no_argument,          NO_IMGUI,         "Plain SDL2 renderer"},
                 {"geometry",                required_argument,    GEOMETRY,         "WxH[+X+Y]"},
                 {"aspect-ratio",            no_argument,          ASPECT_RATIO,     "Always preserve correct aspect ratio"},
                 {"game-controller",         required_argument,    GAME_CONTROLLER,  "SDL_GameControllerOpen"},
                 {"game-mapping-file",       required_argument,    MAPPING_FILE,     "SDL_GameControllerAddMappingsFromFile"},
                 {"audio-device",            required_argument,    AUDIO_DEVICE,     "Audio device name"},
             }},
        };

        const std::vector<std::pair<std::string, std::vector<OptionData_t>>> applenOptions = {
            {"applen",
             {
                 {"no-video-update",         no_argument,          NO_VIDEO_UPDATE,  "Do not execute NTSC code"},
                 {"ev-device-name",          required_argument,    EV_DEVICE_NAME,   "Gamepad ev-device name"},
             }},
        };

        // clang-format on

        if (type == OptionsType::sa2)
        {
            allOptions.insert(allOptions.end(), sa2Options.begin(), sa2Options.end());
        }
        else if (type == OptionsType::applen)
        {
            allOptions.insert(allOptions.end(), applenOptions.begin(), applenOptions.end());
        }

        std::vector<option> longOptions;
        std::string shortOptions;
        extractOptions(allOptions, longOptions, shortOptions);

        while (true)
        {
            int optionIndex = 0;
            const int c = getopt_long(argc, argv, shortOptions.c_str(), longOptions.data(), &optionIndex);

            switch (c)
            {
            case -1:
            {
                if (optind < argc)
                {
                    std::cerr << "Uexpected positional argument: '" << argv[optind] << "'" << std::endl << std::endl;
                    printHelp(allOptions);
                    return false;
                }
                return true;
            }
            case '?':
            {
                std::cerr << std::endl;
                printHelp(allOptions);
                return false;
            }
            case 'h':
            {
                printHelp(allOptions);
                return false;
            }
            case 'c':
            {
                options.configurationFile = optarg;
                break;
            }
            case 'l':
            {
                options.log = true;
                break;
            }
            case '1':
            {
                options.disk1 = optarg;
                break;
            }
            case '2':
            {
                options.disk2 = optarg;
                break;
            }
            case 'f':
            {
                options.snapshotFilename = optarg;
                options.loadSnapshot = false;
                break;
            }
            case 's':
            {
                options.snapshotFilename = optarg;
                options.loadSnapshot = true;
                break;
            }
            case 'q':
            {
                options.useQtIni = true;
                break;
            }
            case 'r':
            {
                options.registryOptions.emplace_back(optarg);
                break;
            }
            case 'b':
            {
                options.benchmark = true;
                break;
            }
            case PAUSED:
            {
                options.autoBoot = false;
                break;
            }
            case FIXED_SPEED:
            {
                options.fixedSpeed = true;
                break;
            }
            case HEADLESS:
            {
                options.headless = true;
                break;
            }
            case NO_SQUARING:
            {
                options.paddleSquaring = false;
                break;
            }
            case SLIRP_NAT:
            {
                options.natPortFwds.emplace_back(optarg);
                break;
            }
            case DISK_H1:
            {
                options.hardDisk1 = optarg;
                break;
            }
            case DISK_H2:
            {
                options.hardDisk2 = optarg;
                break;
            }
            case MEM_CLEAR:
            {
                const int memclear = std::stoi(optarg);
                if (memclear >= 0 && memclear < NUM_MIP)
                {
                    options.memclear = memclear;
                }
                break;
            }
            case ROM:
            {
                options.customRom = optarg;
                break;
            }
            case F8ROM:
            {
                options.customRomF8 = optarg;
                break;
            }
            case NO_AUDIO:
            {
                options.noAudio = true;
                break;
            }
            case AUDIO_BUFFER:
            {
                options.audioBuffer = std::stoul(optarg);
                break;
            }
            case WAV_SPEAKER:
            {
                options.wavFileSpeaker = optarg;
                break;
            }
            case WAV_MOCKINGBOARD:
            {
                options.wavFileMockingboard = optarg;
                break;
            }
            case SDL_DRIVER:
            {
                options.sdlDriver = std::stoi(optarg);
                break;
            }
            case GL_SWAP:
            {
                options.glSwapInterval = std::stoi(optarg);
                break;
            }
            case TIMER:
            {
                options.syncWithTimer = true;
                break;
            }
            case NO_IMGUI:
            {
                options.imgui = false;
                break;
            }
            case GEOMETRY:
            {
                parseGeometry(optarg, options.geometry);
                break;
            }
            case ASPECT_RATIO:
            {
                options.aspectRatio = true;
                break;
            }
            case GAME_CONTROLLER:
            {
                options.gameControllerIndex = std::stoi(optarg);
                break;
            }
            case MAPPING_FILE:
            {
                options.gameControllerMappingFile = optarg;
                break;
            }
            case AUDIO_DEVICE:
            {
                options.audioDeviceName = optarg;
                break;
            }
            case NO_VIDEO_UPDATE:
            {
                options.noVideoUpdate = true;
                break;
            }
            case EV_DEVICE_NAME:
            {
                options.paddleDeviceName = optarg;
                break;
            }
            default:
            {
                printHelp(allOptions);
                return false;
            }
            }
        }

        return true;
    }

} // namespace common2
