#include "StdAfx.h"
#include "frontends/common2/utils.h"
#include "frontends/common2/programoptions.h"

#include "SaveState.h"
#include "Registry.h"

namespace
{

    std::string getEnvOrDefault(const char *var, const char *fallback = nullptr)
    {
        const char *value = getenv(var);
        if (value)
        {
            return value;
        }
        if (fallback)
        {
            return fallback;
        }
        throw std::runtime_error("${" + std::string(var) + "} not set");
    }

} // namespace

namespace common2
{

    std::filesystem::path getSettingsRootDir()
    {
#ifdef _WIN32
        const std::string profile = getEnvOrDefault("LOCALAPPDATA");
        return profile;
#else
        // https://specifications.freedesktop.org/basedir-spec/latest/
        const std::filesystem::path home = getHomeDir();
        const std::filesystem::path config = getEnvOrDefault("XDG_CONFIG_HOME", ".config");

        return home / config;
#endif
    }

    void setSnapshotFilename(const std::filesystem::path &filename)
    {
        if (std::filesystem::exists(filename))
        {
            std::filesystem::current_path(filename.parent_path());
            Snapshot_SetFilename(filename.string());
            RegSaveString(REG_CONFIG, REGVALUE_SAVESTATE_FILENAME, 1, Snapshot_GetPathname());
        }
    }

    void loadGeometryFromRegistry(const std::string &section, Geometry &geometry)
    {
        const std::string path = section + "\\geometry";
        const auto loadValue = [&path](const char *name, int &dest)
        {
            uint32_t value;
            if (RegLoadValue(path.c_str(), name, TRUE, &value))
            {
                // uint32_t and int have the same size
                // but if they did not, this would be necessary
                typedef std::make_signed<uint32_t>::type signed_t;
                dest = static_cast<signed_t>(value);
            }
        };

        loadValue("width", geometry.width);
        loadValue("height", geometry.height);
        loadValue("x", geometry.x);
        loadValue("y", geometry.y);
    }

    void saveGeometryToRegistry(const std::string &section, const Geometry &geometry)
    {
        const std::string path = section + "\\geometry";
        const auto saveValue = [&path](const char *name, const int source)
        {
            // this seems to already do the right thing for negative numbers
            RegSaveValue(path.c_str(), name, TRUE, source);
        };
        saveValue("width", geometry.width);
        saveValue("height", geometry.height);
        saveValue("x", geometry.x);
        saveValue("y", geometry.y);
    }

    std::filesystem::path getHomeDir()
    {
#ifdef _WIN32
        const char *var = "USERPROFILE";
#else
        const char *var = "HOME";
#endif
        const std::filesystem::path home = getEnvOrDefault(var);
        return home;
    }

    std::filesystem::path getConfigFile(const std::string &filename)
    {
        const std::filesystem::path dir = getSettingsRootDir() / "applewin";
        std::filesystem::create_directories(dir);

        return dir / filename;
    }

    RestoreCurrentDirectory::RestoreCurrentDirectory()
        : myCurrentDirectory(std::filesystem::current_path())
    {
    }

    RestoreCurrentDirectory::~RestoreCurrentDirectory()
    {
        std::filesystem::current_path(myCurrentDirectory);
    }

} // namespace common2
