#include "frontends/common2/ptreeregistry.h"
#include "frontends/common2/yamlmap.h"

#include <stdexcept>

namespace common2
{

    std::string PTreeRegistry::getString(const std::string &section, const std::string &key) const
    {
        const std::string &value = myData.at(section).at(key);
        return value;
    }

    uint32_t PTreeRegistry::getDWord(const std::string &section, const std::string &key) const
    {
        const std::string &value = myData.at(section).at(key);
        size_t pos = 0;
        unsigned long result = std::stoul(value, &pos);

        if (pos != value.size())
        {
            throw std::invalid_argument("Trailing characters in numeric value: " + section + "." + key + "=" + value);
        }

        return result;
    }

    bool PTreeRegistry::getBool(const std::string &section, const std::string &key) const
    {
        return getDWord(section, key) != 0;
    }

    void PTreeRegistry::putString(const std::string &section, const std::string &key, const std::string &value)
    {
        myData[section][key] = value;
    }

    void PTreeRegistry::putDWord(const std::string &section, const std::string &key, const uint32_t value)
    {
        myData[section][key] = std::to_string(value);
    }

    const std::map<std::string, std::map<std::string, std::string>> &PTreeRegistry::getAllValues() const
    {
        return myData;
    }

    std::string PTreeRegistry::getLocation() const
    {
        return std::string();
    }

    void PTreeRegistry::saveToYamlFile(const std::string &filename) const
    {
        writeMapToYaml(filename, myData);
    }

} // namespace common2
