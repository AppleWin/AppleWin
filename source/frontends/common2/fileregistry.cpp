#include "StdAfx.h"

#include "frontends/common2/fileregistry.h"
#include "frontends/common2/ptreeregistry.h"
#include "frontends/common2/programoptions.h"
#include "frontends/common2/utils.h"
#include "frontends/common2/yamlmap.h"

#include "Log.h"
#include <regex>

namespace
{

    void parseOption(const std::string &s, std::string &section, std::string &key, std::string &value)
    {
        static const std::regex re(R"(^([^.=]+)\.([^.=]+)=(.*)$)");
        std::smatch m;
        if (!std::regex_match(s, m, re))
        {
            throw std::runtime_error("Invalid option format: " + s + ", expected: section.key=value");
        }

        section = m[1].str();
        key = m[2].str();
        value = m[3].str();
    }

    class Configuration : public common2::PTreeRegistry
    {
    public:
        Configuration(const std::filesystem::path &filename);
        ~Configuration();

        void addExtraOptions(const std::vector<std::string> &options);

        std::string getLocation() const override;

    private:
        const std::filesystem::path myFilename;
    };

    Configuration::Configuration(const std::filesystem::path &filename)
        : myFilename(filename)
    {
        if (std::filesystem::exists(myFilename))
        {
            myData = common2::readMapFromYaml(myFilename.string());
        }
        else
        {
            LogFileOutput("Registry: configuration file '%s' not found\n", myFilename.string().c_str());
        }
    }

    Configuration::~Configuration()
    {
        try
        {
            saveToYamlFile(myFilename.string());
        }
        catch (const std::exception &e)
        {
            LogFileOutput("Registry: cannot save settings to '%s': %s\n", myFilename.string().c_str(), e.what());
        }
    }

    void Configuration::addExtraOptions(const std::vector<std::string> &options)
    {
        for (const std::string &option : options)
        {
            std::string section, key, value;
            parseOption(option, section, key, value);
            putString(section, key, value);
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
        const std::filesystem::path &filename = options.configurationFile;

        LogFileOutput("Reading configuration from: '%s'\n", filename.string().c_str());
        std::shared_ptr<Configuration> config = std::make_shared<Configuration>(filename);
        config->addExtraOptions(options.registryOptions);

        return config;
    }

} // namespace common2
