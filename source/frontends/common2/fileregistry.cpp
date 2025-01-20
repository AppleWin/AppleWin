#include "StdAfx.h"

#include "frontends/common2/fileregistry.h"
#include "frontends/common2/ptreeregistry.h"
#include "frontends/common2/programoptions.h"
#include "frontends/common2/utils.h"

#include "Log.h"
#include "frontends/qt/applicationname.h"

#include <boost/property_tree/ini_parser.hpp>

namespace
{

    void parseOption(const std::string &s, std::string &path, std::string &value)
    {
        const size_t pos = s.find('=');
        if (pos == std::string::npos)
        {
            throw std::runtime_error("Invalid option format: " + s + ", expected: section.key=value");
        }
        path = s.substr(0, pos);
        std::replace(path.begin(), path.end(), '_', ' ');
        value = s.substr(pos + 1);
        std::replace(value.begin(), value.end(), '_', ' ');
    }

    class Configuration : public common2::PTreeRegistry
    {
    public:
        Configuration(const std::filesystem::path &filename, const bool saveOnExit);
        ~Configuration();

        void addExtraOptions(const std::vector<std::string> &options);

        std::string getLocation() const override;

    private:
        const std::filesystem::path myFilename;
        bool mySaveOnExit;
    };

    Configuration::Configuration(const std::filesystem::path &filename, const bool saveOnExit)
        : myFilename(filename)
        , mySaveOnExit(saveOnExit)
    {
        if (std::filesystem::exists(myFilename))
        {
            boost::property_tree::ini_parser::read_ini(myFilename.string(), myINI);
        }
        else
        {
            LogFileOutput("Registry: configuration file '%s' not found\n", myFilename.string().c_str());
        }
    }

    Configuration::~Configuration()
    {
        if (mySaveOnExit)
        {
            try
            {
                saveToINIFile(myFilename.string());
            }
            catch (const std::exception &e)
            {
                LogFileOutput("Registry: cannot save settings to '%s': %s\n", myFilename.string().c_str(), e.what());
            }
        }
    }

    void Configuration::addExtraOptions(const std::vector<std::string> &options)
    {
        for (const std::string &option : options)
        {
            std::string path, value;
            parseOption(option, path, value);
            myINI.put(path, value);
        }
    }

    std::string Configuration::getLocation() const
    {
        return myFilename.string();
    }

} // namespace

namespace common2
{

    std::shared_ptr<Registry> CreateFileRegistry(const EmulatorOptions &options)
    {
        std::filesystem::path filename;
        bool saveOnExit;

        if (options.useQtIni)
        {
            filename = getSettingsRootDir() / ORGANIZATION_NAME / (std::string(APPLICATION_NAME) + ".conf");
            saveOnExit = false;
        }
        else
        {
            filename = options.configurationFile;
            saveOnExit = true;
        }

        LogFileOutput("Reading configuration from: '%s'\n", filename.string().c_str());
        std::shared_ptr<Configuration> config = std::make_shared<Configuration>(filename, saveOnExit);
        config->addExtraOptions(options.registryOptions);

        return config;
    }

} // namespace common2
